#ifndef AXI_LITE_SLAVE_H
#define AXI_LITE_SLAVE_H

#include <systemc.h>
#include <cstdlib> // Required for rand()

enum fsm_state { state_idle, state_ready, state_burst_write, state_response };
enum read_fsm { r_idle, r_burst };

SC_MODULE(axi_lite_slave) {
    sc_in<bool> ACLK, ARESETN;

    // --- CHANNELS ---
    sc_in<sc_uint<32>> AWADDR; sc_in<bool> AWVALID; sc_out<bool> AWREADY; sc_in<sc_uint<8>> AWLEN;
    sc_in<sc_uint<32>> WDATA; sc_in<bool> WVALID; sc_out<bool> WREADY; sc_in<bool> WLAST;
    sc_out<sc_uint<2>> BRESP; sc_out<bool> BVALID; sc_in<bool> BREADY;

    sc_in<sc_uint<32>> ARADDR; sc_in<bool> ARVALID; sc_out<bool> ARREADY; sc_in<sc_uint<8>> ARLEN;
    sc_out<sc_uint<32>> RDATA; sc_out<sc_uint<2>> RRESP; sc_out<bool> RVALID; sc_in<bool> RREADY; sc_out<bool> RLAST;

    // --- 1D MEMORY ARCHITECTURE (64KB) ---
    sc_uint<8> memory_array[65536]; 
    
    sc_signal<fsm_state> write_state; sc_signal<read_fsm> read_state;
    
    // --- Linear Address Trackers ONLY ---
    sc_uint<32> current_w_addr; 
    sc_uint<32> current_r_addr;

    int write_delay_counter, read_delay_counter, read_burst_count;

    // ==========================================
    // WRITE STATE MACHINE
    // ==========================================
    void process_write_fsm() {
        if (!ARESETN.read()) { write_state.write(state_idle); AWREADY.write(0); WREADY.write(0); BVALID.write(0); return; }

        switch (write_state.read()) {
            case state_idle: if (AWVALID.read()) write_state.write(state_ready); break;
            case state_ready:
                AWREADY.write(1); 
                if (AWVALID.read() && AWREADY.read()) { 
                    current_w_addr = AWADDR.read();     
                    AWREADY.write(0); write_delay_counter = 0; write_state.write(state_burst_write); 
                }
                break;
            case state_burst_write:
                if (write_delay_counter == 0) { WREADY.write(0); write_delay_counter = 1; } 
                else {
                    WREADY.write(1); 
                    if (WVALID.read() == 1) {
                        sc_uint<32> data = WDATA.read(); 
                        
                        if (current_w_addr + 3 < 65536) {
                            memory_array[current_w_addr + 0] = data.range(7, 0); 
                            memory_array[current_w_addr + 1] = data.range(15, 8);   
                            memory_array[current_w_addr + 2] = data.range(23, 16); 
                            memory_array[current_w_addr + 3] = data.range(31, 24);
                        }
                        
                        // Strict linear increment
                        current_w_addr += 4; 
                        
                        write_delay_counter = 0; 
                        if (WLAST.read() == 1) { WREADY.write(0); write_state.write(state_response); }
                    }
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
                    ARREADY.write(1); read_burst_count = 0; read_delay_counter = 0; read_state.write(r_burst); 
                } else { ARREADY.write(0); }
                break;

            case r_burst:
                ARREADY.write(0); 
                if (read_delay_counter == 0) { RVALID.write(0); read_delay_counter = 1; } 
                else {
                    sc_uint<32> mem_data = 0;
                    
                    if (current_r_addr + 3 < 65536) {
                        mem_data = (memory_array[current_r_addr + 3] << 24) | 
                                   (memory_array[current_r_addr + 2] << 16) | 
                                   (memory_array[current_r_addr + 1] << 8)  | 
                                   (memory_array[current_r_addr + 0]);
                    }
                    RDATA.write(mem_data); RRESP.write(0); RVALID.write(1); 
                    if (read_burst_count == 3) RLAST.write(1); else RLAST.write(0);

                    if (RREADY.read() == 1) {
                        // Strict linear increment
                        current_r_addr += 4;
                        
                        read_burst_count++; read_delay_counter = 0; 
                        
                        if (read_burst_count == 4) { read_state.write(r_idle); }
                    }
                }
                break;
        }
    }

    SC_CTOR(axi_lite_slave) { 
        // Power-On SRAM Randomization 
        for (int i = 0; i < 65536; i++) {
            memory_array[i] = rand() % 256;
        }

        SC_METHOD(process_write_fsm); sensitive << ACLK.pos() << ARESETN.neg(); 
        SC_METHOD(process_read_fsm); sensitive << ACLK.pos() << ARESETN.neg(); 
    }
};
#endif