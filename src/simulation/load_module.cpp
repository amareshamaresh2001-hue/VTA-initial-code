#include "load_module.h"
#include "vta_config.h"

// =========================================================================
// --- STAGE 2: EXTERNAL SRAM ARRAYS ---
// The following arrays are physically located in the Compute module.
// By declaring them 'extern', the Load module can access them to store 
// the neural network inputs and weights it fetches from the AXI memory.
// =========================================================================
constexpr int VTA_BLOCK_IN  = 16;
constexpr int VTA_BLOCK_OUT = 16;
constexpr int INP_BUFF_DEPTH = (1 << vta_config::UOP_SRC_WIDTH); // 2048
constexpr int WGT_BUFF_DEPTH = (1 << vta_config::UOP_WGT_WIDTH); // 1024

extern int8_t inp_mem[INP_BUFF_DEPTH][VTA_BLOCK_IN];
extern int8_t wgt_mem[WGT_BUFF_DEPTH][VTA_BLOCK_OUT * VTA_BLOCK_IN];

LoadModule::LoadModule(sc_module_name n) : Module(n) {

    SC_METHOD(process_axi_read_fsm);
    sensitive << ACLK.pos() << ARESETN.neg();

    SC_METHOD(activate_push_next_vld_handler);
    dont_initialize();
    sensitive << activate_push_next_vld;

    SC_METHOD(activate_push_next_end_handler);
    dont_initialize();
    sensitive << activate_push_next_end;

    SC_METHOD(activate_pull_next_rdy_handler);
    dont_initialize();
    sensitive << activate_pull_next_rdy;

    SC_METHOD(write_push_next_data_handler);
    dont_initialize();
    sensitive << write_push_next_data;

    SC_METHOD(read_pull_next_data_handler);
    dont_initialize();
    sensitive << read_pull_next_data;

    SC_METHOD(pull_next_vld_handler);
    dont_initialize();
    sensitive << pull_next_vld.pos();

    SC_METHOD(pull_next_end_handler);
    dont_initialize();
    sensitive << pull_next_end.pos();

    SC_METHOD(push_next_rdy_handler);
    dont_initialize();
    sensitive << push_next_rdy.pos();
}

sc_time LoadModule::latency() {
    int inst_exec_time = 15;
    if (current->get_y_size() != 0) {
        if (current->get_name() == "LOAD INP") {
            inst_exec_time = current->get_y_size() * 100 + PlatformConfig::getInstance().load_cycles * std::ceil(current->get_y_size() * current->get_x_size() * 16.0 / 256.0);
        } else {
            inst_exec_time = current->get_y_size() * 100 + PlatformConfig::getInstance().load_cycles * current->get_y_size() * current->get_x_size();
        }
    }
    return sc_time(inst_exec_time, SC_NS);
}


void LoadModule::check_dependencies() { //
    if (current->get_pop_next()) {
        if (this->pull_next_vld.read()) {
            this->pull_next_rdy_state = true;
            activate_pull_next_rdy.notify(1, SC_NS);
        } else {
            this->is_waiting_next = true;
        }
    } else {
        this->dep_received.notify(SC_ZERO_TIME);
    }
}

void LoadModule::receive_dependencies() { //
    if (current->get_pop_next() && this->next_data != nullptr) {
        this->dep_received.notify(SC_ZERO_TIME);
    }
}

void LoadModule::dependencies_received() { //
    std::string name = current->get_name();

    if (name == "LOAD INP" || name == "LOAD WGT") {
        try {
            std::string dram_str = current->get_dram();
            std::string sram_str = current->get_sram();
            uint32_t dram_base = dram_str.empty() ? 0 : std::stoul(dram_str, nullptr, 16);
            uint32_t sram_base = sram_str.empty() ? 0 : std::stoul(sram_str, nullptr, 16);
            uint32_t y_size    = current->get_y_size();
            uint32_t x_size    = current->get_x_size();
            uint32_t stride    = current->get_stride();
            uint32_t y0_pad    = current->get_y0_pad();
            uint32_t y1_pad    = current->get_y1_pad();
            uint32_t x0_pad    = current->get_x0_pad();
            uint32_t x1_pad    = current->get_x1_pad();

            if (name == "LOAD WGT" && (x_size == 0 || y_size == 0)) {
                std::cout << sc_time_stamp() << " " << this->name() << " SKIPPED INSTRUCTION: " << name
                          << " x_size=" << x_size << " y_size=" << y_size << " pc=" << current->get_pc() << std::endl;
                finish.notify(latency());
                return;
            }

            // sram_idx: linear SRAM tile counter, matches HLS ref: sram_idx = insn.sram_base
            uint32_t sram_idx = sram_base;

            if (name == "LOAD INP") {
                l_axi_x_width = x0_pad + x_size + x1_pad;

                // TOP PADDING: zero-fill y0_pad rows in SRAM synchronously
                for (uint32_t i = 0; i < y0_pad * l_axi_x_width; i++) {
                    if (sram_idx < INP_BUFF_DEPTH)
                        for (int c = 0; c < VTA_BLOCK_IN; c++) inp_mem[sram_idx][c] = 0;
                    sram_idx++;
                }

                l_axi_sram_idx = sram_idx;
                l_axi_dram_offset = dram_base * VTA_BLOCK_IN; // 16 bytes per INP tile
                l_axi_y = 0;
                l_axi_x = 0;
                l_axi_x_size = x_size;
                l_axi_y_size = y_size;
                l_axi_stride = stride;
                l_axi_x0_pad = x0_pad;
                l_axi_x1_pad = x1_pad;
                l_axi_y1_pad = y1_pad;
                l_axi_base_addr = START_ADDR.read();
                l_axi_left_pad_done = false;

                axi_state.write(l_inp_addr);
                return; // Let FSM take over

            } else if (name == "LOAD WGT") {
                l_axi_sram_idx = sram_idx;
                l_axi_dram_offset = dram_base * VTA_BLOCK_IN * VTA_BLOCK_OUT; // 256 bytes per WGT tile
                l_axi_y = 0;
                l_axi_x = 0;
                l_axi_x_size = x_size;
                l_axi_y_size = y_size;
                l_axi_stride = stride;
                l_axi_base_addr = START_ADDR.read();

                axi_state.write(l_wgt_addr);
                return; // Let FSM take over
            }

        } catch (...) {
            std::cout << "[LOAD ERROR] Failed to parse instruction fields." << std::endl;
        }
    }

    finish.notify(latency());
}

void LoadModule::finalize_instruction() { //
    this->result_data = new sc_int<32>(5);
    this->push_dep.notify(SC_ZERO_TIME);
}

void LoadModule::push_dependencies() {
    // check push dependency
    if (current->get_push_next()) {
        this->push_next_vld_state = true;
        this->activate_push_next_vld.notify(1, SC_NS);
    } else {
        // std::cout << sc_time_stamp() << " FINISH LOAD ID=" << current->get_pc() << " (AXI Phase 2)" << std::endl;

        if (result_data != nullptr)
            delete result_data;
        if (next_data != nullptr)
            delete next_data;
        delete current;

        this->current = nullptr;
        this->result_data = nullptr;
        this->next_data = nullptr;
        this->did_push_next = false;

        this->fetch.notify(1, SC_NS);
    }
}

void LoadModule::dependencies_pushed() {
    if (!current->get_push_next() || (current->get_push_next() && this->did_push_next)) {
        // std::cout << sc_time_stamp() << " FINISH LOAD ID=" << current->get_pc() << std::endl;

        if (result_data != nullptr)
            delete result_data;
        if (next_data != nullptr)
            delete next_data;
        delete current;

        this->current = nullptr;
        this->result_data = nullptr;
        this->next_data = nullptr;
        this->did_push_next = false;

        this->push_next_vld_state = false;
        this->activate_push_next_vld.notify(1, SC_NS);

        this->fetch.notify(1, SC_NS);
    } 
}


// write signals
void LoadModule::activate_push_next_vld_handler() {
    // std::cout << sc_time_stamp() << " LOAD 2 PUSH_NEXT_QUEUE VLD=" << this->push_next_vld_state << std::endl;
    this->push_next_vld.write(this->push_next_vld_state);
}

void LoadModule::activate_push_next_end_handler() {
    // std::cout << sc_time_stamp() << " LOAD 2 PUSH_NEXT_QUEUE VLD=" << this->push_next_vld_state << std::endl;
    this->push_next_end.write(this->push_next_end_state);
    
    if (this->push_next_end_state) {
        this->push_next_end_state = false;
        this->activate_push_next_end.notify(1, SC_NS);

        this->did_push_next = true;
        this->dep_pushed.notify(SC_ZERO_TIME);
    }
    
}

void LoadModule::activate_pull_next_rdy_handler() { //
    // std::cout << sc_time_stamp() << " LOAD 2 PULL_NEXT_QUEUE RDY=" << this->pull_next_rdy_state << std::endl;
    this->pull_next_rdy.write(this->pull_next_rdy_state);
    if (this->pull_next_rdy_state) {
        read_pull_next_data.notify(1, SC_NS);
    }
}


// pull next
void LoadModule::pull_next_vld_handler() { //
    if (current && current->get_pop_next() && this->is_waiting_next) {
        this->is_waiting_next = false;
        this->pull_next_rdy_state = true;
        activate_pull_next_rdy.notify(1, SC_NS);
    }
}

void LoadModule::read_pull_next_data_handler() { //
    // std::cout << sc_time_stamp() << " LOAD RECEIVE DATA " << this->pull_next_data.read() << std::endl;
    this->next_data = new sc_int<64>(this->pull_next_data.read());
}

void LoadModule::pull_next_end_handler() { //
    this->pull_next_rdy_state = false;
    activate_pull_next_rdy.notify(1, SC_NS);
    this->receive_dep.notify(SC_ZERO_TIME);
}

// push next
void LoadModule::push_next_rdy_handler() {
    write_push_next_data.notify(SC_ZERO_TIME);
}

void LoadModule::write_push_next_data_handler() {
    // std::cout << sc_time_stamp() << " LOAD SEND DATA " << this->current->id << std::endl;
    this->push_next_data.write(this->current->get_pc());
    
    this->push_next_end_state = true;
    this->activate_push_next_end.notify(1, SC_NS);
}

// Phase 2: process_axi_read_fsm
void LoadModule::process_axi_read_fsm() {
    if (!ARESETN.read()) {
        axi_state.write(l_idle);
        ARVALID.write(0);
        RREADY.write(0);
        return;
    }

    switch (axi_state.read()) {
        case l_idle:
            break;

        case l_inp_addr:
            if (l_axi_y < l_axi_y_size) {
                if (l_axi_x == 0 && !l_axi_left_pad_done) {
                    // LEFT PADDING synchronously (fires exactly once per row: guarded so
                    // repeated FSM evaluations while waiting for ARREADY don't re-run it)
                    for (uint32_t i = 0; i < l_axi_x0_pad; i++) {
                        if (l_axi_sram_idx < INP_BUFF_DEPTH)
                            for (int c = 0; c < VTA_BLOCK_IN; c++) inp_mem[l_axi_sram_idx][c] = 0;
                        l_axi_sram_idx++;
                    }
                    l_axi_left_pad_done = true;
                }

                if (l_axi_x < l_axi_x_size) {
                    ARADDR.write(l_axi_base_addr + l_axi_dram_offset);
                    ARLEN.write(3); // 4-beat burst = 16 bytes = 1 tile
                    ARVALID.write(1);

                    if (ARREADY.read() == 1 && ARVALID.read() == 1) {
                        ARVALID.write(0);
                        l_axi_chunk_count = 0;
                        l_axi_elem_base = 0;
                        axi_state.write(l_inp_data);
                    }
                } else {
                    // RIGHT PADDING synchronously
                    for (uint32_t i = 0; i < l_axi_x1_pad; i++) {
                        if (l_axi_sram_idx < INP_BUFF_DEPTH)
                            for (int c = 0; c < VTA_BLOCK_IN; c++) inp_mem[l_axi_sram_idx][c] = 0;
                        l_axi_sram_idx++;
                    }
                    l_axi_dram_offset += (l_axi_stride - l_axi_x_size) * VTA_BLOCK_IN; // jump to next row
                    l_axi_x = 0;
                    l_axi_y++;
                    l_axi_left_pad_done = false; // next row needs its own left padding
                }
            } else {
                // BOTTOM PADDING synchronously
                for (uint32_t i = 0; i < l_axi_y1_pad * l_axi_x_width; i++) {
                    if (l_axi_sram_idx < INP_BUFF_DEPTH)
                        for (int c = 0; c < VTA_BLOCK_IN; c++) inp_mem[l_axi_sram_idx][c] = 0;
                    l_axi_sram_idx++;
                }
                axi_state.write(l_idle);
                finish.notify(latency());
            }
            break;

        case l_inp_data:
            RREADY.write(1);
            if (RVALID.read() == 1 && RREADY.read() == 1 && l_axi_chunk_count < 4) {
                uint32_t data = RDATA.read().to_uint();
                if (l_axi_sram_idx < INP_BUFF_DEPTH) {
                    // data is 4 bytes (4 elements)
                    inp_mem[l_axi_sram_idx][l_axi_elem_base + 0] = (int8_t)((data >> 0) & 0xFF);
                    inp_mem[l_axi_sram_idx][l_axi_elem_base + 1] = (int8_t)((data >> 8) & 0xFF);
                    inp_mem[l_axi_sram_idx][l_axi_elem_base + 2] = (int8_t)((data >> 16) & 0xFF);
                    inp_mem[l_axi_sram_idx][l_axi_elem_base + 3] = (int8_t)((data >> 24) & 0xFF);
                }
                l_axi_dram_offset += 4;
                l_axi_elem_base += 4;
                l_axi_chunk_count++;
            }

            if (RLAST.read() == 1) {
                RREADY.write(0);
                l_axi_sram_idx++;
                l_axi_x++;
                axi_state.write(l_inp_addr);
            }
            break;

        case l_wgt_addr:
            if (l_axi_y < l_axi_y_size) {
                if (l_axi_x < l_axi_x_size) {
                    ARADDR.write(l_axi_base_addr + l_axi_dram_offset);
                    ARLEN.write(63); // 64-beat burst = 256 bytes = 1 tile
                    ARVALID.write(1);

                    if (ARREADY.read() == 1 && ARVALID.read() == 1) {
                        ARVALID.write(0);
                        l_axi_chunk_count = 0;
                        l_axi_elem_base = 0;
                        axi_state.write(l_wgt_data);
                    }
                } else {
                    l_axi_dram_offset += (l_axi_stride - l_axi_x_size) * VTA_BLOCK_IN * VTA_BLOCK_OUT;
                    l_axi_x = 0;
                    l_axi_y++;
                }
            } else {
                axi_state.write(l_idle);
                finish.notify(latency());
            }
            break;

        case l_wgt_data:
            RREADY.write(1);
            if (RVALID.read() == 1 && RREADY.read() == 1 && l_axi_chunk_count < 64) {
                uint32_t data = RDATA.read().to_uint();
                if (l_axi_sram_idx < WGT_BUFF_DEPTH) {
                    wgt_mem[l_axi_sram_idx][l_axi_elem_base + 0] = (int8_t)((data >> 0) & 0xFF);
                    wgt_mem[l_axi_sram_idx][l_axi_elem_base + 1] = (int8_t)((data >> 8) & 0xFF);
                    wgt_mem[l_axi_sram_idx][l_axi_elem_base + 2] = (int8_t)((data >> 16) & 0xFF);
                    wgt_mem[l_axi_sram_idx][l_axi_elem_base + 3] = (int8_t)((data >> 24) & 0xFF);
                }
                l_axi_dram_offset += 4;
                l_axi_elem_base += 4;
                l_axi_chunk_count++;
            }

            if (RLAST.read() == 1) {
                RREADY.write(0);
                l_axi_sram_idx++;
                l_axi_x++;
                axi_state.write(l_wgt_addr);
            }
            break;
    }
}
