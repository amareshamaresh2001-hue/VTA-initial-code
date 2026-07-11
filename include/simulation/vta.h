#ifndef VTA_H
#define VTA_H

#include <systemc.h>
#include "arm.h"
#include "load_module.h"
#include "compute_module.h"
#include "store_module.h"
#include "instruction.h"

// --- AXI HARDWARE IMPORTS ---
#include "axi_interconnect.h"
#include "axi4_full_slave.h"


class VTA : public sc_module {
public:
    SC_HAS_PROCESS(VTA);
    VTA(
        sc_module_name n,
        const std::vector<std::string>& keys,
        const std::map<std::string, std::vector<std::tuple<InstrType, sc_int<64>, sc_int<64>>>>& encoded_splited_instructions);

    ARM *arm;
    Fetcher *fetcher;


public: // Unavoidable: main.cpp needs to trace these for the VCD file
    Queue *l_instructions_queue;
    Queue *c_instructions_queue;
    Queue *s_instructions_queue;

    LoadModule *load;
    ComputeModule *compute;
    StoreModule *store;

    // =========================================================================
    // --- AXI HARDWARE COMPONENTS ---
    // These are the physical hardware blocks provided by the teammate.
    // =========================================================================
    axi_interconnect *arbiter;
    axi4_full_slave *dram;
    
public:
    sc_clock sys_clk;          // The global 10ns clock driving all AXI state machines
    sc_signal<bool> sys_reset; // The global reset signal

    
    // We must declare physical signals (wires) to connect the ports of our modules.
    

    // 1. Traces between the Arbiter and the Main Memory
    sc_signal<sc_uint<32>> sys_AWADDR, sys_WDATA, sys_ARADDR, sys_RDATA;
    sc_signal<sc_uint<8>>  sys_AWLEN, sys_ARLEN;
    sc_signal<sc_uint<2>>  sys_BRESP, sys_RRESP;
    sc_signal<bool> sys_AWVALID, sys_AWREADY, sys_WVALID, sys_WREADY, sys_WLAST;
    sc_signal<bool> sys_BVALID, sys_BREADY, sys_ARVALID, sys_ARREADY;
    sc_signal<bool> sys_RVALID, sys_RREADY, sys_RLAST;
    
    // Dynamic Memory Config traces (from Dispatcher/System to Memory)
    sc_signal<int> sys_cfg_width;
    sc_signal<int> sys_cfg_stride;

    // 2. Traces for Master 0 (Load Module -> Arbiter)
    sc_signal<sc_uint<32>> m0_AWADDR, m0_WDATA, m0_ARADDR, m0_RDATA;
    sc_signal<sc_uint<8>>  m0_AWLEN, m0_ARLEN;
    sc_signal<sc_uint<2>>  m0_BRESP, m0_RRESP;
    sc_signal<bool> m0_AWVALID, m0_AWREADY, m0_WVALID, m0_WREADY, m0_WLAST;
    sc_signal<bool> m0_BVALID, m0_BREADY, m0_ARVALID, m0_ARREADY;
    sc_signal<bool> m0_RVALID, m0_RREADY, m0_RLAST;
    sc_signal<bool> m0_AWLOCK, m0_ARLOCK; // Dispatcher lock pins
    sc_signal<sc_uint<32>> sys_start_m0;  // Dynamic start address for Load

    // 3. Traces for Master 1 (Compute Module -> Arbiter)
    sc_signal<sc_uint<32>> m1_AWADDR, m1_WDATA, m1_ARADDR, m1_RDATA;
    sc_signal<sc_uint<8>>  m1_AWLEN, m1_ARLEN;
    sc_signal<sc_uint<2>>  m1_BRESP, m1_RRESP;
    sc_signal<bool> m1_AWVALID, m1_AWREADY, m1_WVALID, m1_WREADY, m1_WLAST;
    sc_signal<bool> m1_BVALID, m1_BREADY, m1_ARVALID, m1_ARREADY;
    sc_signal<bool> m1_RVALID, m1_RREADY, m1_RLAST;
    sc_signal<bool> m1_AWLOCK, m1_ARLOCK;
    sc_signal<sc_uint<32>> sys_start_m1;  // Dynamic start address for Compute

    // 4. Traces for Master 2 (Store Module -> Arbiter)
    sc_signal<sc_uint<32>> m2_AWADDR, m2_WDATA, m2_ARADDR, m2_RDATA;
    sc_signal<sc_uint<8>>  m2_AWLEN, m2_ARLEN;
    sc_signal<sc_uint<2>>  m2_BRESP, m2_RRESP;
    sc_signal<bool> m2_AWVALID, m2_AWREADY, m2_WVALID, m2_WREADY, m2_WLAST;
    sc_signal<bool> m2_BVALID, m2_BREADY, m2_ARVALID, m2_ARREADY;
    sc_signal<bool> m2_RVALID, m2_RREADY, m2_RLAST;
    sc_signal<bool> m2_AWLOCK, m2_ARLOCK; // Dispatcher lock pins
    sc_signal<sc_uint<32>> sys_start_m2;  // Dynamic start address for Store

    // 5. Traces for Master 3 (Fetcher -> Arbiter)
    sc_signal<sc_uint<32>> m3_AWADDR, m3_WDATA, m3_ARADDR, m3_RDATA;
    sc_signal<sc_uint<8>>  m3_AWLEN, m3_ARLEN;
    sc_signal<sc_uint<2>>  m3_BRESP, m3_RRESP;
    sc_signal<bool> m3_AWVALID, m3_AWREADY, m3_WVALID, m3_WREADY, m3_WLAST;
    sc_signal<bool> m3_BVALID, m3_BREADY, m3_ARVALID, m3_ARREADY;
    sc_signal<bool> m3_RVALID, m3_RREADY, m3_RLAST;
    sc_signal<bool> m3_AWLOCK, m3_ARLOCK;
    sc_signal<sc_uint<32>> sys_start_m3;  // Dynamic start address for Fetcher

    void power_on_sequence(); // --- Power-On Reset ---

    Queue *l2c_queue;
    Queue *c2l_queue;
    Queue *s2c_queue;
    Queue *c2s_queue;

    sc_signal<bool> arm_fetcher_trig;
    sc_signal<bool> compute_arm_trig;

    // fetcher 2 modules
    /// fetcher 2 load queue
    sc_signal<bool> fetcher_l_queue_vld_sig;
    sc_signal<bool> fetcher_l_queue_rdy_sig;
    sc_signal<sc_int<64>> fetcher_l_queue_data;
    sc_signal<bool> fetcher_l_queue_end_sig;
    /// load queue 2 load
    sc_signal<bool> l_queue_load_vld_sig;
    sc_signal<bool> l_queue_load_rdy_sig;
    sc_signal<sc_int<64>> l_queue_load_data;
    sc_signal<bool> l_queue_load_end_sig;

    /// fetcher 2 compute queue
    sc_signal<bool> fetcher_c_queue_vld_sig;
    sc_signal<bool> fetcher_c_queue_rdy_sig;
    sc_signal<sc_int<64>> fetcher_c_queue_data;
    sc_signal<bool> fetcher_c_queue_end_sig;
    /// compute queue 2 compute
    sc_signal<bool> c_queue_compute_vld_sig;
    sc_signal<bool> c_queue_compute_rdy_sig;
    sc_signal<sc_int<64>> c_queue_compute_data;
    sc_signal<bool> c_queue_compute_end_sig;

    /// fetcher 2 store queue
    sc_signal<bool> fetcher_s_queue_vld_sig;
    sc_signal<bool> fetcher_s_queue_rdy_sig;
    sc_signal<sc_int<64>> fetcher_s_queue_data;
    sc_signal<bool> fetcher_s_queue_end_sig;
    /// store queue 2 store
    sc_signal<bool> s_queue_store_vld_sig;
    sc_signal<bool> s_queue_store_rdy_sig;
    sc_signal<sc_int<64>> s_queue_store_data;
    sc_signal<bool> s_queue_store_end_sig;


    // load 2 compute
    /// load 2 queue
    sc_signal<bool> load_l2c_vld_sig;
    sc_signal<bool> l2c_load_rdy_sig;
    sc_signal<sc_int<64>> load_l2c_data;
    sc_signal<bool> load_l2c_end_sig;
    /// queue 2 compute
    sc_signal<bool> l2c_compute_vld_sig;
    sc_signal<bool> compute_l2c_rdy_sig;
    sc_signal<sc_int<64>> l2c_compute_data;
    sc_signal<bool> l2c_compute_end_sig;


    // compute 2 load 
    /// compute 2 queue
    sc_signal<bool> compute_c2l_vld_sig;
    sc_signal<bool> c2l_compute_rdy_sig;
    sc_signal<sc_int<64>> compute_c2l_data;
    sc_signal<bool> compute_c2l_end_sig;
    /// queue 2 load
    sc_signal<bool> c2l_load_vld_sig;
    sc_signal<bool> load_c2l_rdy_sig;
    sc_signal<sc_int<64>> c2l_load_data;
    sc_signal<bool> c2l_load_end_sig;


    // store 2 compute
    /// store 2 queue
    sc_signal<bool> store_s2c_vld_sig;
    sc_signal<bool> s2c_store_rdy_sig;
    sc_signal<sc_int<64>> store_s2c_data;
    sc_signal<bool> store_s2c_end_sig;
    /// queue 2 compute
    sc_signal<bool> s2c_compute_vld_sig;
    sc_signal<bool> compute_s2c_rdy_sig;
    sc_signal<sc_int<64>> s2c_compute_data;
    sc_signal<bool> s2c_compute_end_sig;


    // compute 2 store 
    /// compute 2 queue
    sc_signal<bool> compute_c2s_vld_sig;
    sc_signal<bool> c2s_compute_rdy_sig;
    sc_signal<sc_int<64>> compute_c2s_data;
    sc_signal<bool> compute_c2s_end_sig;
    /// queue 2 store
    sc_signal<bool> c2s_store_vld_sig;
    sc_signal<bool> store_c2s_rdy_sig;
    sc_signal<sc_int<64>> c2s_store_data;
    sc_signal<bool> c2s_store_end_sig;

    void start();
};



#endif;