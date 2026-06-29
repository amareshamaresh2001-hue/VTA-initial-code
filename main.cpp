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

    // --- SETUP MEMORY (DUMP INSTRUCTIONS TO DRAM) ---
    // The supervisor wants Fetcher to read from memory.
    // Fetcher start address is 0x0000C000.
    uint32_t dram_offset = 0x0000C000;
    for (const auto& key : parser.keys) {
        for (const auto& instruction : parser.encoded_splited_instructions[key]) {
            uint64_t part0 = std::get<1>(instruction).to_uint64();
            uint64_t part1 = std::get<2>(instruction).to_uint64();

            // Write first 8 bytes
            for(int i = 0; i < 8; i++) {
                vta.dram->memory_array[dram_offset++] = (part0 >> (i * 8)) & 0xFF;
            }
            // Write second 8 bytes
            for(int i = 0; i < 8; i++) {
                vta.dram->memory_array[dram_offset++] = (part1 >> (i * 8)) & 0xFF;
            }
        }
    }

    // --- SETUP WAVEFORM TRACING ---
    sc_trace_file *tf = sc_create_vcd_trace_file("vta_waveforms");
    tf->set_time_unit(1, SC_NS);

    // ==========================================
    // 1. SYSTEM STATUS
    // ==========================================
    sc_trace(tf, vta.sys_clk,   "1_SYS/clk");
    sc_trace(tf, vta.sys_reset, "1_SYS/reset_n");
    sc_trace(tf, vta.compute_arm_trig, "1_SYS/FINISH_layer_done");

    // ==========================================
    // 2. FETCHER MODULE (Instruction Fetch)
    // ==========================================
    sc_trace(tf, vta.m3_ARVALID, "2_FETCHER/AXI_ARVALID");
    sc_trace(tf, vta.m3_ARREADY, "2_FETCHER/AXI_ARREADY");
    sc_trace(tf, vta.m3_RVALID,  "2_FETCHER/AXI_RVALID");
    sc_trace(tf, vta.m3_RREADY,  "2_FETCHER/AXI_RREADY");
    sc_trace(tf, vta.m3_RDATA,   "2_FETCHER/AXI_RDATA");

    // ==========================================
    // 3. DISPATCH ROUTING (Fetcher -> Modules)
    // ==========================================
    sc_trace(tf, vta.fetcher_l_queue_vld_sig, "3_DISPATCH/to_LOAD_vld");
    sc_trace(tf, vta.l_queue_load_data,       "3_DISPATCH/to_LOAD_data");
    
    sc_trace(tf, vta.fetcher_c_queue_vld_sig, "3_DISPATCH/to_COMPUTE_vld");
    sc_trace(tf, vta.c_queue_compute_data,    "3_DISPATCH/to_COMPUTE_data");
    
    sc_trace(tf, vta.fetcher_s_queue_vld_sig, "3_DISPATCH/to_STORE_vld");
    sc_trace(tf, vta.s_queue_store_data,      "3_DISPATCH/to_STORE_data");

    // ==========================================
    // 4. LOAD MODULE (Data Fetch)
    // ==========================================
    sc_trace(tf, vta.m0_ARVALID, "4_LOAD/AXI_ARVALID");
    sc_trace(tf, vta.m0_ARREADY, "4_LOAD/AXI_ARREADY");
    sc_trace(tf, vta.m0_RVALID,  "4_LOAD/AXI_RVALID");
    sc_trace(tf, vta.m0_RREADY,  "4_LOAD/AXI_RREADY");
    sc_trace(tf, vta.m0_RDATA,   "4_LOAD/AXI_RDATA");

    // ==========================================
    // 5. COMPUTE MODULE (UOP/Bias Fetch)
    // ==========================================
    sc_trace(tf, vta.m1_ARVALID, "5_COMPUTE/AXI_ARVALID");
    sc_trace(tf, vta.m1_ARREADY, "5_COMPUTE/AXI_ARREADY");
    sc_trace(tf, vta.m1_RVALID,  "5_COMPUTE/AXI_RVALID");
    sc_trace(tf, vta.m1_RREADY,  "5_COMPUTE/AXI_RREADY");

    // ==========================================
    // 6. STORE MODULE (Result Write)
    // ==========================================
    sc_trace(tf, vta.m2_AWVALID, "6_STORE/AXI_AWVALID");
    sc_trace(tf, vta.m2_AWREADY, "6_STORE/AXI_AWREADY");
    sc_trace(tf, vta.m2_WVALID,  "6_STORE/AXI_WVALID");
    sc_trace(tf, vta.m2_WDATA,   "6_STORE/AXI_WDATA");
    sc_trace(tf, vta.m2_BVALID,  "6_STORE/AXI_BVALID");

    // ==========================================
    // 7. PIPELINE DEPENDENCY QUEUES
    // ==========================================
    sc_trace(tf, vta.load_l2c_vld_sig,    "7_DEPENDENCIES/l2c_Load_Ready");
    sc_trace(tf, vta.compute_c2l_vld_sig, "7_DEPENDENCIES/c2l_Compute_Done");
    sc_trace(tf, vta.compute_c2s_vld_sig, "7_DEPENDENCIES/c2s_Compute_Ready");
    sc_trace(tf, vta.store_s2c_vld_sig,   "7_DEPENDENCIES/s2c_Store_Done");

    // Run for 50 microseconds
    sc_start(50000, SC_NS);

    sc_close_vcd_trace_file(tf);

    return 0;
}
