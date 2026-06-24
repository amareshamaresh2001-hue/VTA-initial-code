#include <systemc.h>
#include <chrono>
#include <iostream>
#include <stdexcept>

#include "config.h"
#include "files_manager.h"
#include "parser.h"
#include "vta.h"

int sc_main(int, char*[]) {
    auto& config = PlatformConfig::getInstance();
    FilesManager files_manager(config.granulatity, const_cast<char*>(config.model_name.c_str()));
    Parser parser(files_manager.model_path);

    VTA vta("vta", parser.keys, parser.encoded_splited_instructions);

    // --- SETUP WAVEFORM TRACING ---
    sc_trace_file *tf = sc_create_vcd_trace_file("vta_waveforms");
    tf->set_time_unit(1, SC_NS);
    
    // Trace the clock and reset
    sc_trace(tf, vta.sys_clk, "sys_clk");
    sc_trace(tf, vta.sys_reset, "sys_reset");
    
    // Trace AXI Arbiter signals for Load Module (Master 0)
    sc_trace(tf, vta.m0_ARVALID, "m0_ARVALID");
    sc_trace(tf, vta.m0_ARREADY, "m0_ARREADY");
    sc_trace(tf, vta.m0_RVALID, "m0_RVALID");
    sc_trace(tf, vta.m0_RREADY, "m0_RREADY");
    
    // Trace AXI Arbiter signals for Store Module (Master 2)
    sc_trace(tf, vta.m2_AWVALID, "m2_AWVALID");
    sc_trace(tf, vta.m2_AWREADY, "m2_AWREADY");
    sc_trace(tf, vta.m2_WVALID, "m2_WVALID");
    sc_trace(tf, vta.m2_WREADY, "m2_WREADY");

    // Trace the Compute Module's internal output queue
    sc_trace(tf, vta.c2s_queue->in_vld, "Compute_Output_Valid");

    // Trace the 64-bit instructions from the Fetcher to the Compute Module
    sc_trace(tf, vta.c_instructions_queue->in_data, "Fetcher_to_Compute_Data");
    sc_trace(tf, vta.c_instructions_queue->out_data, "Compute_Received_Data");

    // The final layer finishes at ~232,338,376 ns. 
    // Run for 50 microseconds (50,000 ns) since our simple ALU program finishes instantly.
    sc_start(50000, SC_NS);

    sc_close_vcd_trace_file(tf);

    return 0;
}
