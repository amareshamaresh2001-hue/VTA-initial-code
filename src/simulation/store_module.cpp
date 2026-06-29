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
        
        // Only proceed if this is actually a STORE instruction (not a NOP)
        if (name.find("STORE") != std::string::npos && name != "NOP-STORE-STAGE") {
            
            try {
                // =====================================================================
                // FIX 1: Read the DRAM byte address FROM THE INSTRUCTION, not from
                // START_ADDR. The STORE instruction carries the exact physical address
                // in DRAM where the output tensor must be written.
                // HLS ref: memop_dram_T dram_idx = insn.dram_base;
                // =====================================================================
                std::string dram_str = current->get_dram();
                std::string sram_str = current->get_sram();
                uint32_t dram_base = dram_str.empty() ? 0 : std::stoul(dram_str, nullptr, 16);
                uint32_t sram_base = sram_str.empty() ? 0 : std::stoul(sram_str, nullptr, 16);
                uint32_t y_size    = current->get_y_size();
                uint32_t x_size    = current->get_x_size();
                uint32_t stride    = current->get_stride();

                if (x_size == 0 || y_size == 0) {
                    finish.notify(latency());
                    continue;
                }

                // FIX 2: Use the instruction's dram_base as the starting DRAM byte address.
                uint32_t current_address = dram_base;

                // FIX 3: sram_idx is a simple linear counter.
                // HLS ref: sram_idx = insn.sram_base; sram_idx += x_size per row.
                uint32_t sram_idx = sram_base;

                // Each OUT tile = VTA_BLOCK_OUT int8 values = 16 bytes = 4 x 32-bit AXI beats = 1 burst
                const uint32_t OUT_TILE_BYTES = VTA_BLOCK_OUT; // 16

                // 2D DMA TRANSFER LOOP
                // HLS ref: for (int y = 0; y < y_size; y++) { memcpy(..., x_size * VTA_OUT_ELEM_BYTES); dram_idx += x_stride; }
                for (uint32_t y = 0; y < y_size; y++) {
                    
                    for (uint32_t x = 0; x < x_size; x++) {
                        
                        // --- AXI ADDRESS PHASE ---
                        // AWADDR = row_base + x * tile_bytes (16 bytes per OUT tile)
                        AWADDR.write(current_address + x * OUT_TILE_BYTES);
                        AWLEN.write(3); // Always 4 beats per OUT tile

                        // STRICT ADDRESS HANDSHAKE
                        AWVALID.write(1);
                        do { wait(); } while (AWREADY.read() == 0);
                        AWVALID.write(0);

                        // --- AXI DATA PHASE ---
                        // FIX 4: Write WDATA FIRST, then assert WVALID.
                        // Each 4-beat burst sends 16 bytes of one OUT tile (sram_idx + x).
                        // FIX (Bug 3): sram_idx is a running counter; use (sram_idx + x) per tile.
                        for (uint32_t i = 0; i < 4; i++) {
                            uint32_t cur_sram = sram_idx + x;
                            uint32_t data_chunk = 0;
                            if (cur_sram < ACC_BUFF_DEPTH) {
                                data_chunk |= ((uint32_t)(out_mem[cur_sram][i*4+0] & 0xFF)) << 0;
                                data_chunk |= ((uint32_t)(out_mem[cur_sram][i*4+1] & 0xFF)) << 8;
                                data_chunk |= ((uint32_t)(out_mem[cur_sram][i*4+2] & 0xFF)) << 16;
                                data_chunk |= ((uint32_t)(out_mem[cur_sram][i*4+3] & 0xFF)) << 24;
                            }
                            // Set data stable on the bus first, THEN assert valid
                            WDATA.write(data_chunk);
                            WLAST.write(i == 3 ? 1 : 0);
                            WVALID.write(1);
                            // Hold WVALID until slave accepts this beat
                            do { wait(); } while (WREADY.read() == 0);
                        }
                        WVALID.write(0);
                        WLAST.write(0);

                        // --- AXI RESPONSE PHASE ---
                        // Wait for the slave (memory) to confirm the write was accepted
                        BREADY.write(1);
                        while (BVALID.read() == 0) { wait(); }
                        BREADY.write(0);
                    }

                    // FIX 5: Advance DRAM by stride TILES (stride * 16 bytes) per row.
                    // HLS ref: dram_idx += x_stride
                    current_address += stride * OUT_TILE_BYTES;

                    // FIX 6: Advance sram_idx linearly by x_size (NOT recalculated per beat).
                    // HLS ref: sram_idx += x_size
                    sram_idx += x_size;
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
