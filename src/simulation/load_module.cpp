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

    // --- STAGE 2: REGISTER AXI READ THREAD ---
    // We register the new axi_read_thread as an SC_THREAD so it can use wait()
    // to synchronize with the AXI clock. It is sensitive to the positive edge of ACLK.
    SC_THREAD(axi_read_thread);
    sensitive << ACLK.pos();

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
    // std::cout << sc_time_stamp() << " START LOAD ID=" << current->id << std::endl;
    // std::cout << sc_time_stamp() << " " << this->name() << " START EXECUTING " << current->get_layer() << " " << current->get_pc() << std::endl;
    
    // --- STAGE 2: AXI TRIGGER ---
    // Instead of finishing immediately or using memcpy, we trigger our new AXI thread.
    // This bridges our event-driven architecture with the cycle-accurate hardware.
    start_axi_read.notify(SC_ZERO_TIME);
}

// =========================================================================
// --- STAGE 2: AXI READ THREAD IMPLEMENTATION ---
// This is the cycle-accurate hardware model for the Load module.
// It uses 4-beat bursts to remain compatible with the teammate's Memory.
// =========================================================================
void LoadModule::axi_read_thread() {
    // 0. HARDWARE INITIALIZATION
    ARVALID.write(0); 
    RREADY.write(0);

    while (true) {
        // 1. SLEEP UNTIL INSTRUCTION ARRIVES
        wait(start_axi_read);

        std::string name = current->get_name();
        
        if (name == "LOAD INP" || name == "LOAD WGT") {
            
            try {
                std::string sram_str = current->get_sram();
                uint32_t sram_base = sram_str.empty() ? 0 : std::stoul(sram_str, nullptr, 16);
                uint32_t y_size = current->get_y_size();
                uint32_t x_size = current->get_x_size();
                uint32_t stride = current->get_stride();
                
                // --- NEW: Tensor Padding Parameters ---
                uint32_t y0_pad = current->get_y0_pad();
                uint32_t y1_pad = current->get_y1_pad();
                uint32_t x0_pad = current->get_x0_pad();
                uint32_t x1_pad = current->get_x1_pad();

                if (x_size == 0 || y_size == 0) {
                    finish.notify(latency());
                    continue;
                }

                sc_uint<32> current_address = START_ADDR.read(); 
                uint32_t sram_idx = sram_base;
                uint32_t x_width = x0_pad + x_size + x1_pad;

                if (name == "LOAD INP") {
                    // ==========================================
                    // 1. TOP PADDING (y0_pad rows)
                    // ==========================================
                    uint32_t top_pad_tiles = y0_pad * x_width;
                    for (uint32_t i = 0; i < top_pad_tiles; i++) {
                        if (sram_idx < INP_BUFF_DEPTH) {
                            for (int c = 0; c < 16; c++) inp_mem[sram_idx][c] = 0;
                        }
                        sram_idx++;
                    }

                    // ==========================================
                    // 2. 2D DMA TRANSFER WITH ROW PADDING
                    // ==========================================
                    for (uint32_t y = 0; y < y_size; y++) {
                        
                        // --- Left Padding ---
                        for (uint32_t i = 0; i < x0_pad; i++) {
                            if (sram_idx < INP_BUFF_DEPTH) {
                                for (int c = 0; c < 16; c++) inp_mem[sram_idx][c] = 0;
                            }
                            sram_idx++;
                        }

                        // --- AXI Data Fetch ---
                        uint32_t beats_processed = 0;
                        while (beats_processed < x_size) {
                            
                            ARADDR.write(current_address + (beats_processed * 4)); 
                            ARLEN.write(3); 
                            ARVALID.write(1);
                            
                            do { 
                                wait(); 
                            } while (ARREADY.read() == 0);
                            ARVALID.write(0);

                            int chunk_count = 0;
                            RREADY.write(0); 
                            
                            while (chunk_count < 4) {
                                wait(); 
                                if (RVALID.read() == 1) {
                                    RREADY.write(1);
                                    wait(); 
                                    
                                    uint32_t data_chunk = RDATA.read().to_uint();
                                    
                                    if (sram_idx < INP_BUFF_DEPTH) {
                                        inp_mem[sram_idx][0] = (data_chunk >> 0) & 0xFF;
                                        inp_mem[sram_idx][1] = (data_chunk >> 8) & 0xFF;
                                        inp_mem[sram_idx][2] = (data_chunk >> 16) & 0xFF;
                                        inp_mem[sram_idx][3] = (data_chunk >> 24) & 0xFF;
                                    }
                                    
                                    chunk_count++;
                                    sram_idx++; // Increment linear SRAM pointer
                                    RREADY.write(0); 
                                }
                            }
                            beats_processed += 4;
                        }
                        
                        current_address += stride;

                        // --- Right Padding ---
                        for (uint32_t i = 0; i < x1_pad; i++) {
                            if (sram_idx < INP_BUFF_DEPTH) {
                                for (int c = 0; c < 16; c++) inp_mem[sram_idx][c] = 0;
                            }
                            sram_idx++;
                        }
                    }

                    // ==========================================
                    // 3. BOTTOM PADDING (y1_pad rows)
                    // ==========================================
                    uint32_t bot_pad_tiles = y1_pad * x_width;
                    for (uint32_t i = 0; i < bot_pad_tiles; i++) {
                        if (sram_idx < INP_BUFF_DEPTH) {
                            for (int c = 0; c < 16; c++) inp_mem[sram_idx][c] = 0;
                        }
                        sram_idx++;
                    }
                } 
                else if (name == "LOAD WGT") {
                    // Standard 2D load without padding
                    for (uint32_t y = 0; y < y_size; y++) {
                        uint32_t beats_processed = 0;
                        while (beats_processed < x_size) {
                            ARADDR.write(current_address + (beats_processed * 4)); 
                            ARLEN.write(3); 
                            ARVALID.write(1);
                            
                            do { 
                                wait(); 
                            } while (ARREADY.read() == 0);
                            ARVALID.write(0);

                            int chunk_count = 0;
                            RREADY.write(0); 
                            
                            while (chunk_count < 4) {
                                wait(); 
                                if (RVALID.read() == 1) {
                                    RREADY.write(1);
                                    wait(); 
                                    
                                    uint32_t data_chunk = RDATA.read().to_uint();
                                    
                                    if (sram_idx < WGT_BUFF_DEPTH) {
                                        wgt_mem[sram_idx][0] = (data_chunk >> 0) & 0xFF;
                                        wgt_mem[sram_idx][1] = (data_chunk >> 8) & 0xFF;
                                        wgt_mem[sram_idx][2] = (data_chunk >> 16) & 0xFF;
                                        wgt_mem[sram_idx][3] = (data_chunk >> 24) & 0xFF;
                                    }
                                    
                                    chunk_count++;
                                    sram_idx++; // Increment linear SRAM pointer
                                    RREADY.write(0); 
                                }
                            }
                            beats_processed += 4;
                        }
                        current_address += stride;
                    }
                }
            } catch (...) {
                std::cout << "[AXI READ ERROR] Failed to parse memory addresses." << std::endl;
            }
        }
        finish.notify(latency());
    }
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
