#include "store_module.h"
#include "vta_config.h"

// =========================================================================
// --- STAGE 2: EXTERNAL SRAM ARRAYS ---
// By declaring this 'extern', the Store module can access the results computed
// by the Compute module and send them across the AXI bus back to main memory.
// =========================================================================
constexpr int VTA_BLOCK_OUT = 16;
constexpr int ACC_BUFF_DEPTH = (1 << vta_config::UOP_DST_WIDTH); // 2048

extern int8_t out_mem[ACC_BUFF_DEPTH][VTA_BLOCK_OUT];


StoreModule::StoreModule(sc_module_name n) : Module(n) {

    // --- STAGE 2: REGISTER AXI    // Pure SC_METHOD design: no SC_THREAD, no wait(), no while loops.
    // All logic runs in event-driven SC_METHODs only.

    SC_METHOD(process_axi_write_fsm);
    sensitive << ACLK.pos() << ARESETN.neg();

    SC_METHOD(activate_push_prev_vld_handler);
    dont_initialize();
    sensitive << activate_push_prev_vld;

    SC_METHOD(activate_push_prev_end_handler);
    dont_initialize();
    sensitive << activate_push_prev_end;

    SC_METHOD(activate_pull_prev_rdy_handler);
    dont_initialize();
    sensitive << activate_pull_prev_rdy;


    SC_METHOD(write_push_prev_data_handler);
    dont_initialize();
    sensitive << write_push_prev_data;

    SC_METHOD(read_pull_prev_data_handler);
    dont_initialize();
    sensitive << read_pull_prev_data;

    
    SC_METHOD(pull_prev_vld_handler);
    dont_initialize();
    sensitive << pull_prev_vld.pos();
    
    SC_METHOD(pull_prev_end_handler);
    dont_initialize();
    sensitive << pull_prev_end.pos();
    
    SC_METHOD(push_prev_rdy_handler);
    dont_initialize();
    sensitive << push_prev_rdy.pos();
}

sc_time StoreModule::latency() {
    int inst_exec_time = 15;
    if (current->get_y_size() != 0) {
        inst_exec_time = current->get_y_size() * 100 + PlatformConfig::getInstance().store_cycles * std::ceil(current->get_y_size() * current->get_x_size() * 16.0 / 256.0);
    } 
    return sc_time(inst_exec_time, SC_NS);
}

void StoreModule::check_dependencies() {
    if (current->get_pop_prev()) {
        if (this->pull_prev_vld.read()) {
            this->pull_prev_rdy_state = true;
            activate_pull_prev_rdy.notify(1, SC_NS);
        } else {
            this->is_waiting_prev = true;
        }
    } else {
        this->dep_received.notify(SC_ZERO_TIME);
    }
}

void StoreModule::receive_dependencies() {
    if (current->get_pop_prev() && this->prev_data != nullptr) {
        this->dep_received.notify(SC_ZERO_TIME);
    }
}

void StoreModule::dependencies_received() {
    // std::cout << sc_time_stamp() << " START STORE ID=" << current->id << std::endl;
    // std::cout << sc_time_stamp() << " " << this->name() << " START EXECUTING " << current->get_layer() << " " << current->get_pc() << std::endl;
    
    // --- STAGE 2: AXI TRIGGER ---
    if (current->get_name().find("STORE") != std::string::npos && current->get_name() != "NOP-STORE-STAGE") {
        std::string dram_str = current->get_dram();
        std::string sram_str = current->get_sram();
        uint32_t dram_base = dram_str.empty() ? 0 : std::stoul(dram_str, nullptr, 16);
        uint32_t sram_base = sram_str.empty() ? 0 : std::stoul(sram_str, nullptr, 16);
        uint32_t y_size    = current->get_y_size();
        uint32_t x_size    = current->get_x_size();
        uint32_t stride    = current->get_stride();

        if (x_size == 0 || y_size == 0) {
            finish.notify(latency());
            return;
        }

        s_axi_sram_idx = sram_base;
        s_axi_dram_offset = dram_base * VTA_BLOCK_OUT; // 16 bytes per tile
        s_axi_y = 0;
        s_axi_x = 0;
        s_axi_x_size = x_size;
        s_axi_y_size = y_size;
        s_axi_stride = stride;
        s_axi_base_addr = START_ADDR.read();

        axi_state.write(s_out_addr);
        return; // Let FSM take over
    }

    finish.notify(latency());
}

// =========================================================================
// --- STAGE 2: AXI WRITE THREAD IMPLEMENTATION ---
// This is the cycle-accurate hardware model for the Store module.
// It uses 4-beat bursts to remain compatible with the teammate's Memory.
// =========================================================================

void StoreModule::finalize_instruction() {
    this->result_data = new sc_int<32>(5);
    this->push_dep.notify(SC_ZERO_TIME);
}

void StoreModule::push_dependencies() {
    // check push dependency
    if (current->get_push_prev()) {
        this->push_prev_vld_state = true;
        activate_push_prev_vld.notify(1, SC_NS);
    } else {
        std::cout << sc_time_stamp() << " FINISH STORE ID=" << current->get_pc() << " (AXI Phase 2)" << std::endl;
        if (result_data != nullptr)
            delete result_data;
        if (prev_data != nullptr)
            delete prev_data;
        delete current;

        this->current = nullptr;
        this->result_data = nullptr;
        this->prev_data = nullptr;
        this->did_push_prev = false;

        this->fetch.notify(1, SC_NS);
    }
}

void StoreModule::dependencies_pushed() {
    if (!current->get_push_prev() || (current->get_push_prev() && this->did_push_prev)) {
        // std::cout << sc_time_stamp() << " FINISH STORE ID=" << current->get_pc() << std::endl;

        if (result_data != nullptr)
            delete result_data;
        if (prev_data != nullptr)
            delete prev_data;
        delete current;

        this->current = nullptr;
        this->result_data = nullptr;
        this->prev_data = nullptr;
        this->did_push_prev = false;

        this->push_prev_vld_state = false;
        activate_push_prev_vld.notify(1, SC_NS);

        this->fetch.notify(1, SC_NS);
    }
}


// write signals
void StoreModule::activate_push_prev_vld_handler() {
    // std::cout << sc_time_stamp() << " STORE 2 PUSH_PREV_QUEUE VLD=" << this->push_prev_vld_state << std::endl;
    this->push_prev_vld.write(this->push_prev_vld_state);
}

void StoreModule::activate_push_prev_end_handler() {
    // std::cout << sc_time_stamp() << " STORE 2 PUSH_PREV_QUEUE VLD=" << this->push_prev_vld_state << std::endl;
    this->push_prev_end.write(this->push_prev_end_state);

    if (push_prev_end_state) {
        this->push_prev_end_state = false;
        this->activate_push_prev_end.notify(1, SC_NS);

        this->did_push_prev = true;
        this->dep_pushed.notify(SC_ZERO_TIME);
    }
}

void StoreModule::activate_pull_prev_rdy_handler() {
    // std::cout << sc_time_stamp() << " STORE 2 PULL_PREV_QUEUE RDY=" << this->pull_prev_rdy_state << std::endl;
    this->pull_prev_rdy.write(this->pull_prev_rdy_state);
    if (this->pull_prev_rdy_state) {
        read_pull_prev_data.notify(1, SC_NS);
    }
}


// pull prev
void StoreModule::pull_prev_vld_handler() {
    if (current && current->get_pop_prev() && this->is_waiting_prev) {
        this->is_waiting_prev = false;
        this->pull_prev_rdy_state = true;
        activate_pull_prev_rdy.notify(1, SC_NS);
    }
}

void StoreModule::read_pull_prev_data_handler() {
    // std::cout << sc_time_stamp() << " STORE RECEIVE DATA " << this->pull_prev_data.read() << std::endl;
    this->prev_data = new sc_int<64>(this->pull_prev_data.read());
}

void StoreModule::pull_prev_end_handler() {
    this->pull_prev_rdy_state = false;
    activate_pull_prev_rdy.notify(1, SC_NS);

    this->receive_dep.notify(SC_ZERO_TIME);
}

// push prev
void StoreModule::push_prev_rdy_handler() {
    write_push_prev_data.notify(SC_ZERO_TIME);
}

void StoreModule::write_push_prev_data_handler() {
    this->push_prev_data.write(this->current->get_pc());
    
    this->push_prev_end_state = true;
    this->activate_push_prev_end.notify(1, SC_NS);
}

// Phase 2: process_axi_write_fsm
void StoreModule::process_axi_write_fsm() {
    if (!ARESETN.read()) {
        axi_state.write(s_idle);
        AWVALID.write(0);
        WVALID.write(0);
        WLAST.write(0);
        BREADY.write(0);
        return;
    }

    switch (axi_state.read()) {
        case s_idle:
            break;

        case s_out_addr:
            if (s_axi_y < s_axi_y_size) {
                if (s_axi_x < s_axi_x_size) {
                    AWADDR.write(s_axi_base_addr + s_axi_dram_offset);
                    AWLEN.write(3); // 4-beat burst = 16 bytes = 1 tile
                    AWVALID.write(1);

                    if (AWREADY.read() == 1 && AWVALID.read() == 1) {
                        AWVALID.write(0);
                        s_axi_chunk_count = 0;
                        BREADY.write(1);
                        axi_state.write(s_out_data);
                    }
                } else {
                    s_axi_dram_offset += (s_axi_stride - s_axi_x_size) * VTA_BLOCK_OUT; // 16 bytes per tile
                    s_axi_x = 0;
                    s_axi_y++;
                }
            } else {
                axi_state.write(s_idle);
                finish.notify(latency());
            }
            break;

        case s_out_data:
            {
                uint32_t c0 = out_mem[s_axi_sram_idx][s_axi_chunk_count * 4 + 0] & 0xFF;
                uint32_t c1 = out_mem[s_axi_sram_idx][s_axi_chunk_count * 4 + 1] & 0xFF;
                uint32_t c2 = out_mem[s_axi_sram_idx][s_axi_chunk_count * 4 + 2] & 0xFF;
                uint32_t c3 = out_mem[s_axi_sram_idx][s_axi_chunk_count * 4 + 3] & 0xFF;
                uint32_t word = c0 | (c1 << 8) | (c2 << 16) | (c3 << 24);
                
                WDATA.write(word);
                WVALID.write(1);
                
                if (s_axi_chunk_count == 3) {
                    WLAST.write(1);
                } else {
                    WLAST.write(0);
                }

                if (WREADY.read() == 1 && WVALID.read() == 1) {
                    s_axi_chunk_count++;
                    s_axi_dram_offset += 4;
                    if (s_axi_chunk_count == 4) {
                        WVALID.write(0);
                        WLAST.write(0);
                        axi_state.write(s_out_resp);
                    }
                }
            }
            break;

        case s_out_resp:
            if (BVALID.read() == 1 && BREADY.read() == 1) {
                BREADY.write(0);
                s_axi_sram_idx++;
                s_axi_x++;
                axi_state.write(s_out_addr);
            }
            break;
    }
}
