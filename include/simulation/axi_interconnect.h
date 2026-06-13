#ifndef AXI_INTERCONNECT_H
#define AXI_INTERCONNECT_H

#include <systemc.h>

SC_MODULE(axi_interconnect) {
    sc_in<bool> ACLK, ARESETN;

    // --- WRITE CHANNELS ---
    sc_in<sc_uint<32>> AWADDR_M0, WDATA_M0; sc_in<sc_uint<8>> AWLEN_M0; sc_in<bool> AWVALID_M0, WVALID_M0, WLAST_M0, BREADY_M0;
    sc_in<bool> AWLOCK_M0; // <-- NEW: Dispatcher Lock for M0
    sc_out<bool> AWREADY_M0, WREADY_M0, BVALID_M0; sc_out<sc_uint<2>> BRESP_M0;

    sc_in<sc_uint<32>> AWADDR_M1, WDATA_M1; sc_in<sc_uint<8>> AWLEN_M1; sc_in<bool> AWVALID_M1, WVALID_M1, WLAST_M1, BREADY_M1;
    sc_in<bool> AWLOCK_M1; // <-- NEW: Dispatcher Lock for M1
    sc_out<bool> AWREADY_M1, WREADY_M1, BVALID_M1; sc_out<sc_uint<2>> BRESP_M1;

    sc_in<sc_uint<32>> AWADDR_M2, WDATA_M2; sc_in<sc_uint<8>> AWLEN_M2; sc_in<bool> AWVALID_M2, WVALID_M2, WLAST_M2, BREADY_M2;
    sc_in<bool> AWLOCK_M2; // <-- NEW: Dispatcher Lock for M2
    sc_out<bool> AWREADY_M2, WREADY_M2, BVALID_M2; sc_out<sc_uint<2>> BRESP_M2;

    sc_out<sc_uint<32>> AWADDR_OUT, WDATA_OUT; sc_out<sc_uint<8>> AWLEN_OUT; sc_out<bool> AWVALID_OUT, WVALID_OUT, WLAST_OUT, BREADY_OUT;
    sc_in<bool> AWREADY_IN, WREADY_IN, BVALID_IN; sc_in<sc_uint<2>> BRESP_IN;

    // --- READ CHANNELS ---
    sc_in<sc_uint<32>> ARADDR_M0; sc_in<bool> ARVALID_M0; sc_out<bool> ARREADY_M0; sc_in<sc_uint<8>> ARLEN_M0;
    sc_in<bool> ARLOCK_M0; // <-- NEW: Dispatcher Lock for M0
    sc_out<sc_uint<32>> RDATA_M0; sc_out<sc_uint<2>> RRESP_M0; sc_out<bool> RVALID_M0; sc_in<bool> RREADY_M0; sc_out<bool> RLAST_M0;

    sc_in<sc_uint<32>> ARADDR_M1; sc_in<bool> ARVALID_M1; sc_out<bool> ARREADY_M1; sc_in<sc_uint<8>> ARLEN_M1;
    sc_in<bool> ARLOCK_M1; // <-- NEW: Dispatcher Lock for M1
    sc_out<sc_uint<32>> RDATA_M1; sc_out<sc_uint<2>> RRESP_M1; sc_out<bool> RVALID_M1; sc_in<bool> RREADY_M1; sc_out<bool> RLAST_M1;

    sc_in<sc_uint<32>> ARADDR_M2; sc_in<bool> ARVALID_M2; sc_out<bool> ARREADY_M2; sc_in<sc_uint<8>> ARLEN_M2;
    sc_in<bool> ARLOCK_M2; // <-- NEW: Dispatcher Lock for M2
    sc_out<sc_uint<32>> RDATA_M2; sc_out<sc_uint<2>> RRESP_M2; sc_out<bool> RVALID_M2; sc_in<bool> RREADY_M2; sc_out<bool> RLAST_M2;

    sc_out<sc_uint<32>> ARADDR_OUT; sc_out<bool> ARVALID_OUT; sc_in<bool> ARREADY_IN; sc_out<sc_uint<8>> ARLEN_OUT;
    sc_in<sc_uint<32>> RDATA_IN; sc_in<sc_uint<2>> RRESP_IN; sc_in<bool> RVALID_IN; sc_out<bool> RREADY_OUT; sc_in<bool> RLAST_IN;

    // --- STATE VARIABLES ---
    bool m0_queued = false, m1_queued = false, m2_queued = false;
    sc_uint<32> m0_saved_addr, m1_saved_addr, m2_saved_addr;
    int active_master = -1;
    bool w_addr_accepted = false; 

    bool r_m0_queued = false, r_m1_queued = false, r_m2_queued = false;
    sc_uint<32> r_m0_saved_addr, r_m1_saved_addr, r_m2_saved_addr;
    int active_read_master = -1;
    bool r_addr_accepted = false; 

    void process_interconnect() {
        if (!ARESETN.read()) {
            active_master = -1; active_read_master = -1;
            w_addr_accepted = false; r_addr_accepted = false;
            m0_queued = false; m1_queued = false; m2_queued = false;
            r_m0_queued = false; r_m1_queued = false; r_m2_queued = false;
            AWVALID_OUT.write(0); WVALID_OUT.write(0); ARVALID_OUT.write(0);
            return;
        }

        // ========================================================
        // 1. THE WRITE ARBITER (WITH DISPATCHER INTERRUPTS)
        // ========================================================
        if (AWVALID_M0.read() == 1 && !m0_queued) { m0_saved_addr = AWADDR_M0.read(); m0_queued = true; AWREADY_M0.write(1); } else { AWREADY_M0.write(0); }
        if (AWVALID_M1.read() == 1 && !m1_queued) { m1_saved_addr = AWADDR_M1.read(); m1_queued = true; AWREADY_M1.write(1); } else { AWREADY_M1.write(0); }
        if (AWVALID_M2.read() == 1 && !m2_queued) { m2_saved_addr = AWADDR_M2.read(); m2_queued = true; AWREADY_M2.write(1); } else { AWREADY_M2.write(0); }

        if (active_master == -1) {
            // PRIORITY 1: DISPATCHER VIP LOCKS (Preemptive Override)
            if (AWLOCK_M0.read() == 1 && m0_queued) active_master = 0;
            else if (AWLOCK_M1.read() == 1 && m1_queued) active_master = 1;
            else if (AWLOCK_M2.read() == 1 && m2_queued) active_master = 2;
            
            // PRIORITY 2: STANDARD QUEUE (Runs if no Dispatcher Interrupts are active)
            else if (m0_queued) active_master = 0; 
            else if (m1_queued) active_master = 1; 
            else if (m2_queued) active_master = 2;
        }

        // --- WRITE ROUTING LOGIC (Unchanged) ---
        if (active_master == 0) {
            AWADDR_OUT.write(m0_saved_addr); 
            if (!w_addr_accepted) { AWVALID_OUT.write(1); if (AWREADY_IN.read() == 1) w_addr_accepted = true; } else { AWVALID_OUT.write(0); }
            WDATA_OUT.write(WDATA_M0.read()); WVALID_OUT.write(WVALID_M0.read()); WLAST_OUT.write(WLAST_M0.read()); BREADY_OUT.write(BREADY_M0.read()); WREADY_M0.write(WREADY_IN.read()); BVALID_M0.write(BVALID_IN.read());
            WREADY_M1.write(0); BVALID_M1.write(0); WREADY_M2.write(0); BVALID_M2.write(0);
            if (BVALID_IN.read() && BREADY_M0.read()) { active_master = -1; m0_queued = false; w_addr_accepted = false; AWVALID_OUT.write(0); }
        } else if (active_master == 1) {
            AWADDR_OUT.write(m1_saved_addr); 
            if (!w_addr_accepted) { AWVALID_OUT.write(1); if (AWREADY_IN.read() == 1) w_addr_accepted = true; } else { AWVALID_OUT.write(0); }
            WDATA_OUT.write(WDATA_M1.read()); WVALID_OUT.write(WVALID_M1.read()); WLAST_OUT.write(WLAST_M1.read()); BREADY_OUT.write(BREADY_M1.read()); WREADY_M1.write(WREADY_IN.read()); BVALID_M1.write(BVALID_IN.read());
            WREADY_M0.write(0); BVALID_M0.write(0); WREADY_M2.write(0); BVALID_M2.write(0);
            if (BVALID_IN.read() && BREADY_M1.read()) { active_master = -1; m1_queued = false; w_addr_accepted = false; AWVALID_OUT.write(0); }
        } else if (active_master == 2) {
            AWADDR_OUT.write(m2_saved_addr); 
            if (!w_addr_accepted) { AWVALID_OUT.write(1); if (AWREADY_IN.read() == 1) w_addr_accepted = true; } else { AWVALID_OUT.write(0); }
            WDATA_OUT.write(WDATA_M2.read()); WVALID_OUT.write(WVALID_M2.read()); WLAST_OUT.write(WLAST_M2.read()); BREADY_OUT.write(BREADY_M2.read()); WREADY_M2.write(WREADY_IN.read()); BVALID_M2.write(BVALID_IN.read());
            WREADY_M0.write(0); BVALID_M0.write(0); WREADY_M1.write(0); BVALID_M1.write(0);
            if (BVALID_IN.read() && BREADY_M2.read()) { active_master = -1; m2_queued = false; w_addr_accepted = false; AWVALID_OUT.write(0); }
        } else {
            AWVALID_OUT.write(0); WVALID_OUT.write(0); WLAST_OUT.write(0);
            WREADY_M0.write(0); BVALID_M0.write(0); WREADY_M1.write(0); BVALID_M1.write(0); WREADY_M2.write(0); BVALID_M2.write(0);
        }

        // ========================================================
        // 2. THE READ ARBITER (WITH DISPATCHER INTERRUPTS)
        // ========================================================
        if (ARVALID_M0.read() == 1 && !r_m0_queued) { r_m0_saved_addr = ARADDR_M0.read(); r_m0_queued = true; ARREADY_M0.write(1); } else { ARREADY_M0.write(0); }
        if (ARVALID_M1.read() == 1 && !r_m1_queued) { r_m1_saved_addr = ARADDR_M1.read(); r_m1_queued = true; ARREADY_M1.write(1); } else { ARREADY_M1.write(0); }
        if (ARVALID_M2.read() == 1 && !r_m2_queued) { r_m2_saved_addr = ARADDR_M2.read(); r_m2_queued = true; ARREADY_M2.write(1); } else { ARREADY_M2.write(0); }

        if (active_read_master == -1) {
            // PRIORITY 1: DISPATCHER VIP LOCKS (Preemptive Override)
            if (ARLOCK_M0.read() == 1 && r_m0_queued) active_read_master = 0;
            else if (ARLOCK_M1.read() == 1 && r_m1_queued) active_read_master = 1;
            else if (ARLOCK_M2.read() == 1 && r_m2_queued) active_read_master = 2;
            
            // PRIORITY 2: STANDARD QUEUE
            else if (r_m0_queued) active_read_master = 0; 
            else if (r_m1_queued) active_read_master = 1; 
            else if (r_m2_queued) active_read_master = 2;
        }

        // --- READ ROUTING LOGIC (Unchanged) ---
        if (active_read_master == 0) {
            ARADDR_OUT.write(r_m0_saved_addr); 
            if (!r_addr_accepted) { ARVALID_OUT.write(1); if (ARREADY_IN.read() == 1) r_addr_accepted = true; } else { ARVALID_OUT.write(0); }
            RDATA_M0.write(RDATA_IN.read()); RLAST_M0.write(RLAST_IN.read()); RVALID_M0.write(RVALID_IN.read()); RRESP_M0.write(RRESP_IN.read()); RREADY_OUT.write(RREADY_M0.read());
            RVALID_M1.write(0); RVALID_M2.write(0); 
            if (RVALID_IN.read() && RREADY_M0.read() && RLAST_IN.read()) { active_read_master = -1; r_m0_queued = false; r_addr_accepted = false; ARVALID_OUT.write(0); }
        } else if (active_read_master == 1) {
            ARADDR_OUT.write(r_m1_saved_addr); 
            if (!r_addr_accepted) { ARVALID_OUT.write(1); if (ARREADY_IN.read() == 1) r_addr_accepted = true; } else { ARVALID_OUT.write(0); }
            RDATA_M1.write(RDATA_IN.read()); RLAST_M1.write(RLAST_IN.read()); RVALID_M1.write(RVALID_IN.read()); RRESP_M1.write(RRESP_IN.read()); RREADY_OUT.write(RREADY_M1.read());
            RVALID_M0.write(0); RVALID_M2.write(0);
            if (RVALID_IN.read() && RREADY_M1.read() && RLAST_IN.read()) { active_read_master = -1; r_m1_queued = false; r_addr_accepted = false; ARVALID_OUT.write(0); }
        } else if (active_read_master == 2) {
            ARADDR_OUT.write(r_m2_saved_addr); 
            if (!r_addr_accepted) { ARVALID_OUT.write(1); if (ARREADY_IN.read() == 1) r_addr_accepted = true; } else { ARVALID_OUT.write(0); }
            RDATA_M2.write(RDATA_IN.read()); RLAST_M2.write(RLAST_IN.read()); RVALID_M2.write(RVALID_IN.read()); RRESP_M2.write(RRESP_IN.read()); RREADY_OUT.write(RREADY_M2.read());
            RVALID_M0.write(0); RVALID_M1.write(0);
            if (RVALID_IN.read() && RREADY_M2.read() && RLAST_IN.read()) { active_read_master = -1; r_m2_queued = false; r_addr_accepted = false; ARVALID_OUT.write(0); }
        } else {
            ARVALID_OUT.write(0); RVALID_M0.write(0); RVALID_M1.write(0); RVALID_M2.write(0);
        }
    }

    SC_CTOR(axi_interconnect) { SC_METHOD(process_interconnect); sensitive << ACLK.pos() << ARESETN.neg(); }
};
#endif