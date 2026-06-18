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

    // --- STAGE 2: REGISTER AXI WRITE THREAD ---
    SC_THREAD(axi_write_thread);
    sensitive << ACLK.pos();

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
    // Instead of finishing immediately, trigger the AXI write thread to push data back to DRAM.
    start_axi_write.notify(SC_ZERO_TIME);
}

// =========================================================================
// --- STAGE 2: AXI WRITE THREAD IMPLEMENTATION ---
// This is the cycle-accurate hardware model for the Store module.
// It uses 4-beat bursts to remain compatible with the teammate's Memory.
// =========================================================================
void StoreModule::axi_write_thread() {
    // 0. HARDWARE INITIALIZATION
    AWVALID.write(0); 
    WVALID.write(0); 
    WLAST.write(0); 
    BREADY.write(0);

    while (true) {
        // 1. SLEEP UNTIL INSTRUCTION ARRIVES
        wait(start_axi_write);

        std::string name = current->get_name();
        
        // Only proceed if this is actually a STORE instruction (Parser names it "STORE STORE")
        if (name.find("STORE") != std::string::npos && name != "NOP-STORE-STAGE") {
            
            try {
                std::string sram_str = current->get_sram();
                uint32_t sram_base = sram_str.empty() ? 0 : std::stoul(sram_str, nullptr, 16);
                uint32_t y_size = current->get_y_size();
                uint32_t x_size = current->get_x_size();
                uint32_t stride = current->get_stride();

                if (x_size == 0 || y_size == 0) {
                    finish.notify(latency());
                    continue;
                }

                sc_uint<32> current_address = START_ADDR.read(); 

                // 2. 2D DMA TRANSFER LOOP
                for (uint32_t y = 0; y < y_size; y++) {
                    
                    uint32_t beats_processed = 0;
                    
                    while (beats_processed < x_size) {
                        
                        // --- AXI ADDRESS PHASE ---
                        AWADDR.write(current_address + (beats_processed * 4)); 
                        AWLEN.write(3); // Always 4 beats
                        
                        // 1. STRICT ADDRESS HANDSHAKE
                        AWVALID.write(1);
                        do { wait(); } while (AWREADY.read() == 0);
                        AWVALID.write(0);

                        // --- AXI DATA PHASE ---
                        for (uint32_t i = 0; i < 4; i++) {
                            uint32_t sram_idx = sram_base + (y * x_size) + beats_processed + i;
                            uint32_t data_chunk = 0;
                            
                            if (sram_idx < ACC_BUFF_DEPTH) {
                                data_chunk |= ((uint32_t)out_mem[sram_idx][0] & 0xFF) << 0;
                                data_chunk |= ((uint32_t)out_mem[sram_idx][1] & 0xFF) << 8;
                                data_chunk |= ((uint32_t)out_mem[sram_idx][2] & 0xFF) << 16;
                                data_chunk |= ((uint32_t)out_mem[sram_idx][3] & 0xFF) << 24;
                            }
                            
                            WDATA.write(data_chunk); 
                            
                            // 2. STRICT DATA HANDSHAKE
                            WVALID.write(1);
                            if (i == 3) WLAST.write(1); else WLAST.write(0);
                            
                            // Hold WVALID high until Arbiter/Slave asserts WREADY
                            do { 
                                wait(); 
                            } while (WREADY.read() == 0);
                        }
                        
                        // Deassert valid only after successful completion of the entire burst
                        WVALID.write(0); 
                        WLAST.write(0);

                        // --- AXI RESPONSE PHASE ---
                        BREADY.write(1); // Hold BREADY continuously high
                        while (BVALID.read() == 0) { wait(); }
                        BREADY.write(0);

                        beats_processed += 4;
                    }
                    
                    current_address += stride; 
                }
            } catch (...) {
                std::cout << "[AXI WRITE ERROR] Failed to parse memory addresses." << std::endl;
            }
        }
        finish.notify(latency());
    }
}

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
    // std::cout << sc_time_stamp() << " STORE SEND DATA " << this->current->id << std::endl;
    this->push_prev_data.write(this->current->get_pc());

    this->push_prev_end_state = true;
    this->activate_push_prev_end.notify(1, SC_NS);
}
