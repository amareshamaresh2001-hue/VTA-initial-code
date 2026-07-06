#ifndef AXI4_FULL_SLAVE_H
#define AXI4_FULL_SLAVE_H

#include <systemc.h>
#include <cstdlib> // Required for rand()
#include <vector>

enum fsm_state { state_idle, state_ready, state_burst_write, state_response };
enum read_fsm { r_idle, r_burst };

SC_MODULE(axi4_full_slave) {
    sc_in<bool> ACLK, ARESETN;

    // --- CHANNELS ---
    sc_in<sc_uint<32>> AWADDR; sc_in<bool> AWVALID; sc_out<bool> AWREADY; sc_in<sc_uint<8>> AWLEN;
    sc_in<sc_uint<32>> WDATA; sc_in<bool> WVALID; sc_out<bool> WREADY; sc_in<bool> WLAST;
    sc_out<sc_uint<2>> BRESP; sc_out<bool> BVALID; sc_in<bool> BREADY;

    sc_in<sc_uint<32>> ARADDR; sc_in<bool> ARVALID; sc_out<bool> ARREADY; sc_in<sc_uint<8>> ARLEN;
    sc_out<sc_uint<32>> RDATA; sc_out<sc_uint<2>> RRESP; sc_out<bool> RVALID; sc_in<bool> RREADY; sc_out<bool> RLAST;

    // --- 1D MEMORY ARCHITECTURE (2GB) ---
    std::vector<uint8_t> memory_array; 
    
    
    sc_signal<fsm_state> write_state; sc_signal<read_fsm> read_state;
    
    // --- Linear Address Trackers ONLY ---
    sc_uint<32> current_w_addr; 
    sc_uint<32> current_r_addr;

    int read_burst_count, active_arlen;

    // ==========================================
    // WRITE STATE MACHINE
    // ==========================================
    void process_write_fsm() {
        if (!ARESETN.read()) { write_state.write(state_idle); AWREADY.write(0); WREADY.write(0); BVALID.write(0); return; }

        switch (write_state.read()) {
            case state_idle: 
                AWREADY.write(0); WREADY.write(0); BVALID.write(0); BRESP.write(0);
                if (AWVALID.read() == 1) { write_state.write(state_ready); } 
                break;
            case state_ready:
                AWREADY.write(1); 
                if (AWVALID.read() && AWREADY.read()) { 
                    current_w_addr = AWADDR.read();     
                    AWREADY.write(0); WREADY.write(1); write_state.write(state_burst_write); 
                }
                break;
            case state_burst_write:
                WREADY.write(1); 
                if (WVALID.read() == 1) {
                    sc_uint<32> data = WDATA.read(); 
                    
                    if (current_w_addr + 3 < 2147483648ULL) {
                        memory_array[current_w_addr + 0] = data.range(7, 0); 
                        memory_array[current_w_addr + 1] = data.range(15, 8);   
                        memory_array[current_w_addr + 2] = data.range(23, 16); 
                        memory_array[current_w_addr + 3] = data.range(31, 24);
                    }
                    
                    // Strict linear increment
                    current_w_addr += 4; 
                    
                    if (WLAST.read() == 1) { WREADY.write(0); write_state.write(state_response); }
                }
                break;
            case state_response:
                BRESP.write(0); BVALID.write(1);
                if (BVALID.read() && BREADY.read()) { BVALID.write(0); write_state.write(state_idle); }
                break;
        }
    }

    // ==========================================
    // READ STATE MACHINE
    // ==========================================
    void process_read_fsm() {
        if (!ARESETN.read()) { read_state.write(r_idle); ARREADY.write(0); RVALID.write(0); RLAST.write(0); return; }

        switch (read_state.read()) {
            case r_idle:
                RVALID.write(0); RLAST.write(0); // Delta-cycle fix safely retained
                if (ARVALID.read() == 1) {
                    current_r_addr = ARADDR.read();    
                    active_arlen = ARLEN.read(); // Latch the dynamic burst length!
                    ARREADY.write(1); read_burst_count = 0; read_state.write(r_burst); 
                } else { ARREADY.write(0); }
                break;

            case r_burst:
                ARREADY.write(0); 
                sc_uint<32> mem_data = 0;
                
                if (current_r_addr + 3 < 2147483648ULL) {
                    mem_data = (memory_array[current_r_addr + 3] << 24) | 
                               (memory_array[current_r_addr + 2] << 16) | 
                               (memory_array[current_r_addr + 1] << 8)  | 
                               (memory_array[current_r_addr + 0]);
                }
                RDATA.write(mem_data); RRESP.write(0); RVALID.write(1); 
                
                // Assert RLAST dynamically on the final beat (read_burst_count == active_arlen)
                if (read_burst_count == active_arlen) RLAST.write(1); else RLAST.write(0);

                if (RREADY.read() == 1) {
                    // Strict linear increment
                    current_r_addr += 4;
                    
                    read_burst_count++; 
                    
                    // Transition to r_idle once all beats (active_arlen + 1) are complete
                    if (read_burst_count == active_arlen + 1) { read_state.write(r_idle); }
                }
                break;
        }
    }

    SC_CTOR(axi4_full_slave) { 
        memory_array.resize(2147483648ULL);
        // Power-On SRAM Randomization using a fast LCG (Linear Congruential Generator)
        // to avoid the huge 40-second overhead of calling standard rand() 2.1 billion times.
        uint32_t seed = 0x12345678;
        for (size_t i = 0; i < 2147483648ULL; i++) {
            seed = seed * 1103515245 + 12345;
            memory_array[i] = (seed >> 16) & 0xFF;
        }

        SC_METHOD(process_write_fsm); sensitive << ACLK.pos() << ARESETN.neg(); 
        SC_METHOD(process_read_fsm); sensitive << ACLK.pos() << ARESETN.neg(); 
    }
};
#endif