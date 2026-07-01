#include "vta.h"


VTA::VTA(
    sc_module_name n,
    const std::vector<std::string>& keys,
    const std::map<std::string, std::vector<std::tuple<InstrType, sc_int<64>, sc_int<64>>>>& encoded_splited_instructions) 
    : sc_module(n), sys_clk("sys_clk", 10, SC_NS) { // Instantiate 10ns clock
    
    arm = new ARM("ARM");
    fetcher = new Fetcher("Fetcher", keys, encoded_splited_instructions);

    l_instructions_queue = new Queue("Load_Instructions", true);
    c_instructions_queue = new Queue("Compute_Instructions", true);
    s_instructions_queue = new Queue("Store_Instructions", true);

    load = new LoadModule("Load");
    compute = new ComputeModule("Compute");
    store = new StoreModule("Store");

    // =========================================================================
    // --- STAGE 2: AXI HARDWARE INSTANTIATION & WIRING ---
    // =========================================================================

    // Turn off Reset (Active Low) so the hardware turns on immediately
    sys_reset.write(1);

    // Provide default dynamic config values for now.
    // In Stage 3, the Dispatcher would drive these.
    sys_cfg_width.write(16);
    sys_cfg_stride.write(16);
    sys_start_m0.write(0x00000000);
    sys_start_m1.write(0x00004000); // Compute module reads UOPs and biases from this region
    sys_start_m2.write(0x00008000);
    sys_start_m3.write(0x0000C000); // Fetcher reads instructions from this region
    
    // --- INSTANTIATE ARBITER AND MAIN MEMORY ---
    dram = new axi4_full_slave("main_memory"); 
    dram->ACLK(sys_clk); 
    dram->ARESETN(sys_reset);

    // --- PRE-LOAD MEMORY ---
    // Pre-load the memory with instructions so the Fetcher can read them via AXI.
    // The memory powers on randomized, so we must flash the code into it.
    uint32_t mem_offset = 0x0000C000;
    for (const auto& layer : encoded_splited_instructions) {
        for (const auto& inst : layer.second) {
            uint64_t part0 = std::get<1>(inst).to_uint64();
            uint64_t part1 = std::get<2>(inst).to_uint64();
            
            for (int i = 0; i < 8; i++) dram->memory_array[mem_offset++] = (part0 >> (i * 8)) & 0xFF;
            for (int i = 0; i < 8; i++) dram->memory_array[mem_offset++] = (part1 >> (i * 8)) & 0xFF;
        }
    }
    
    arbiter = new axi_interconnect("axi_arbiter");
    arbiter->ACLK(sys_clk);
    arbiter->ARESETN(sys_reset);

    // --- ARBITER TO SLAVE CONNECTION ---
    arbiter->AWADDR_OUT(sys_AWADDR); arbiter->AWVALID_OUT(sys_AWVALID); arbiter->AWREADY_IN(sys_AWREADY); arbiter->AWLEN_OUT(sys_AWLEN);
    arbiter->WDATA_OUT(sys_WDATA);   arbiter->WVALID_OUT(sys_WVALID);   arbiter->WREADY_IN(sys_WREADY);   arbiter->WLAST_OUT(sys_WLAST);
    arbiter->BRESP_IN(sys_BRESP);    arbiter->BVALID_IN(sys_BVALID);    arbiter->BREADY_OUT(sys_BREADY);
    arbiter->ARADDR_OUT(sys_ARADDR); arbiter->ARVALID_OUT(sys_ARVALID); arbiter->ARREADY_IN(sys_ARREADY); arbiter->ARLEN_OUT(sys_ARLEN);
    arbiter->RDATA_IN(sys_RDATA);    arbiter->RRESP_IN(sys_RRESP);      arbiter->RVALID_IN(sys_RVALID);   arbiter->RREADY_OUT(sys_RREADY); arbiter->RLAST_IN(sys_RLAST);

    dram->AWADDR(sys_AWADDR); dram->AWVALID(sys_AWVALID); dram->AWREADY(sys_AWREADY); dram->AWLEN(sys_AWLEN);
    dram->WDATA(sys_WDATA);   dram->WVALID(sys_WVALID);   dram->WREADY(sys_WREADY);   dram->WLAST(sys_WLAST);
    dram->BRESP(sys_BRESP);   dram->BVALID(sys_BVALID);   dram->BREADY(sys_BREADY);
    dram->ARADDR(sys_ARADDR); dram->ARVALID(sys_ARVALID); dram->ARREADY(sys_ARREADY); dram->ARLEN(sys_ARLEN);
    dram->RDATA(sys_RDATA);   dram->RRESP(sys_RRESP);     dram->RVALID(sys_RVALID);   dram->RREADY(sys_RREADY); dram->RLAST(sys_RLAST);

    // --- SOLDER LOAD MODULE TO ARBITER MASTER 0 ---
    load->ACLK(sys_clk);
    load->ARESETN(sys_reset);
    load->START_ADDR(sys_start_m0);
    load->ARADDR(m0_ARADDR); load->ARLEN(m0_ARLEN); load->ARVALID(m0_ARVALID); load->RREADY(m0_RREADY);
    load->ARREADY(m0_ARREADY); load->RVALID(m0_RVALID); load->RLAST(m0_RLAST); load->RDATA(m0_RDATA); load->RRESP(m0_RRESP);

    arbiter->ARADDR_M0(m0_ARADDR); arbiter->ARLEN_M0(m0_ARLEN); arbiter->ARVALID_M0(m0_ARVALID); arbiter->RREADY_M0(m0_RREADY);
    arbiter->ARREADY_M0(m0_ARREADY); arbiter->RVALID_M0(m0_RVALID); arbiter->RLAST_M0(m0_RLAST); arbiter->RDATA_M0(m0_RDATA); arbiter->RRESP_M0(m0_RRESP);
    
    // Master 0 Write pins are dummy (Load only reads)
    arbiter->AWADDR_M0(m0_AWADDR); arbiter->AWLEN_M0(m0_AWLEN); arbiter->AWVALID_M0(m0_AWVALID); arbiter->WDATA_M0(m0_WDATA); arbiter->WVALID_M0(m0_WVALID); arbiter->WLAST_M0(m0_WLAST); arbiter->BREADY_M0(m0_BREADY);
    arbiter->AWREADY_M0(m0_AWREADY); arbiter->WREADY_M0(m0_WREADY); arbiter->BRESP_M0(m0_BRESP); arbiter->BVALID_M0(m0_BVALID);

    // --- SOLDER STORE MODULE TO ARBITER MASTER 2 ---
    store->ACLK(sys_clk);
    store->ARESETN(sys_reset);
    store->START_ADDR(sys_start_m2);
    store->AWADDR(m2_AWADDR); store->AWLEN(m2_AWLEN); store->AWVALID(m2_AWVALID); store->BREADY(m2_BREADY);
    store->AWREADY(m2_AWREADY); store->WDATA(m2_WDATA); store->WVALID(m2_WVALID); store->WLAST(m2_WLAST); store->WREADY(m2_WREADY);
    store->BRESP(m2_BRESP); store->BVALID(m2_BVALID);

    arbiter->AWADDR_M2(m2_AWADDR); arbiter->AWLEN_M2(m2_AWLEN); arbiter->AWVALID_M2(m2_AWVALID); arbiter->WDATA_M2(m2_WDATA); arbiter->WVALID_M2(m2_WVALID); arbiter->WLAST_M2(m2_WLAST); arbiter->BREADY_M2(m2_BREADY);
    arbiter->AWREADY_M2(m2_AWREADY); arbiter->WREADY_M2(m2_WREADY); arbiter->BRESP_M2(m2_BRESP); arbiter->BVALID_M2(m2_BVALID);

    // Master 2 Read pins are dummy (Store only writes)
    arbiter->ARADDR_M2(m2_ARADDR); arbiter->ARLEN_M2(m2_ARLEN); arbiter->ARVALID_M2(m2_ARVALID); arbiter->RREADY_M2(m2_RREADY);
    arbiter->ARREADY_M2(m2_ARREADY); arbiter->RVALID_M2(m2_RVALID); arbiter->RLAST_M2(m2_RLAST); arbiter->RDATA_M2(m2_RDATA); arbiter->RRESP_M2(m2_RRESP);

    // --- SOLDER COMPUTE MODULE TO ARBITER MASTER 1 ---
    compute->ACLK(sys_clk);
    compute->ARESETN(sys_reset);
    compute->START_ADDR(sys_start_m1);
    compute->ARADDR(m1_ARADDR); compute->ARLEN(m1_ARLEN); compute->ARVALID(m1_ARVALID); compute->RREADY(m1_RREADY);
    compute->ARREADY(m1_ARREADY); compute->RVALID(m1_RVALID); compute->RLAST(m1_RLAST); compute->RDATA(m1_RDATA); compute->RRESP(m1_RRESP);

    // Master 1 Write pins are dummy (Compute only reads)
    arbiter->AWADDR_M1(m1_AWADDR); arbiter->AWLEN_M1(m1_AWLEN); arbiter->AWVALID_M1(m1_AWVALID); arbiter->WDATA_M1(m1_WDATA); arbiter->WVALID_M1(m1_WVALID); arbiter->WLAST_M1(m1_WLAST); arbiter->BREADY_M1(m1_BREADY);
    arbiter->AWREADY_M1(m1_AWREADY); arbiter->WREADY_M1(m1_WREADY); arbiter->BRESP_M1(m1_BRESP); arbiter->BVALID_M1(m1_BVALID);
    arbiter->ARADDR_M1(m1_ARADDR); arbiter->ARLEN_M1(m1_ARLEN); arbiter->ARVALID_M1(m1_ARVALID); arbiter->RREADY_M1(m1_RREADY);
    arbiter->ARREADY_M1(m1_ARREADY); arbiter->RVALID_M1(m1_RVALID); arbiter->RLAST_M1(m1_RLAST); arbiter->RDATA_M1(m1_RDATA); arbiter->RRESP_M1(m1_RRESP);

    // --- SOLDER FETCHER MODULE TO ARBITER MASTER 3 ---
    fetcher->ACLK(sys_clk);
    fetcher->ARESETN(sys_reset);
    fetcher->START_ADDR(sys_start_m3);
    fetcher->ARADDR(m3_ARADDR); fetcher->ARLEN(m3_ARLEN); fetcher->ARVALID(m3_ARVALID); fetcher->RREADY(m3_RREADY);
    fetcher->ARREADY(m3_ARREADY); fetcher->RVALID(m3_RVALID); fetcher->RLAST(m3_RLAST); fetcher->RDATA(m3_RDATA); fetcher->RRESP(m3_RRESP);

    // Master 3 Write pins are dummy (Fetcher only reads)
    arbiter->AWADDR_M3(m3_AWADDR); arbiter->AWLEN_M3(m3_AWLEN); arbiter->AWVALID_M3(m3_AWVALID); arbiter->WDATA_M3(m3_WDATA); arbiter->WVALID_M3(m3_WVALID); arbiter->WLAST_M3(m3_WLAST); arbiter->BREADY_M3(m3_BREADY);
    arbiter->AWREADY_M3(m3_AWREADY); arbiter->WREADY_M3(m3_WREADY); arbiter->BRESP_M3(m3_BRESP); arbiter->BVALID_M3(m3_BVALID);
    arbiter->ARADDR_M3(m3_ARADDR); arbiter->ARLEN_M3(m3_ARLEN); arbiter->ARVALID_M3(m3_ARVALID); arbiter->RREADY_M3(m3_RREADY);
    arbiter->ARREADY_M3(m3_ARREADY); arbiter->RVALID_M3(m3_RVALID); arbiter->RLAST_M3(m3_RLAST); arbiter->RDATA_M3(m3_RDATA); arbiter->RRESP_M3(m3_RRESP);

    arbiter->AWLOCK_M0(m0_AWLOCK); arbiter->ARLOCK_M0(m0_ARLOCK);
    arbiter->AWLOCK_M1(m1_AWLOCK); arbiter->ARLOCK_M1(m1_ARLOCK);
    arbiter->AWLOCK_M2(m2_AWLOCK); arbiter->ARLOCK_M2(m2_ARLOCK);
    arbiter->AWLOCK_M3(m3_AWLOCK); arbiter->ARLOCK_M3(m3_ARLOCK);

    // Drive remaining dummy signals low to avoid SystemC float warnings
    m0_AWVALID.write(0); m0_WVALID.write(0); m0_WLAST.write(0); m0_BREADY.write(0); m0_AWLOCK.write(0); m0_ARLOCK.write(0);
    m1_AWVALID.write(0); m1_WVALID.write(0); m1_WLAST.write(0); m1_BREADY.write(0); m1_AWLOCK.write(0); m1_ARLOCK.write(0);
    m2_ARVALID.write(0); m2_RREADY.write(0); m2_AWLOCK.write(0); m2_ARLOCK.write(0);
    m3_AWVALID.write(0); m3_WVALID.write(0); m3_WLAST.write(0); m3_BREADY.write(0); m3_AWLOCK.write(0); m3_ARLOCK.write(0);

    // =========================================================================

    l2c_queue = new Queue("l2c", false);
    c2l_queue = new Queue("c2l", false);
    s2c_queue = new Queue("s2c", false);
    c2s_queue = new Queue("c2s", false);

    arm->out_trig(arm_fetcher_trig);
    fetcher->in_trig(arm_fetcher_trig);

    compute->out_trig(compute_arm_trig);
    arm->in_trig(compute_arm_trig);

    // fetcher 2 modules
    /// fetcher 2 load queue
    fetcher->load_queue_vld(fetcher_l_queue_vld_sig);
    l_instructions_queue->in_vld(fetcher_l_queue_vld_sig);
    l_instructions_queue->in_rdy(fetcher_l_queue_rdy_sig);
    fetcher->load_queue_rdy(fetcher_l_queue_rdy_sig);
    fetcher->load_queue_data(fetcher_l_queue_data);
    l_instructions_queue->in_data(fetcher_l_queue_data);
    fetcher->load_queue_end(fetcher_l_queue_end_sig);
    l_instructions_queue->in_end(fetcher_l_queue_end_sig);
    /// load queue 2 load
    l_instructions_queue->out_vld(l_queue_load_vld_sig);
    load->i_queue_vld(l_queue_load_vld_sig);
    load->i_queue_rdy(l_queue_load_rdy_sig);
    l_instructions_queue->out_rdy(l_queue_load_rdy_sig);
    l_instructions_queue->out_data(l_queue_load_data);
    load->i_queue_data(l_queue_load_data);
    l_instructions_queue->out_end(l_queue_load_end_sig);
    load->i_queue_end(l_queue_load_end_sig);

    /// fetcher 2 compute queue
    fetcher->compute_queue_vld(fetcher_c_queue_vld_sig);
    c_instructions_queue->in_vld(fetcher_c_queue_vld_sig);
    c_instructions_queue->in_rdy(fetcher_c_queue_rdy_sig);
    fetcher->compute_queue_rdy(fetcher_c_queue_rdy_sig);
    fetcher->compute_queue_data(fetcher_c_queue_data);
    c_instructions_queue->in_data(fetcher_c_queue_data);
    fetcher->compute_queue_end(fetcher_c_queue_end_sig);
    c_instructions_queue->in_end(fetcher_c_queue_end_sig);
    /// compute queue 2 compute
    c_instructions_queue->out_vld(c_queue_compute_vld_sig);
    compute->i_queue_vld(c_queue_compute_vld_sig);
    compute->i_queue_rdy(c_queue_compute_rdy_sig);
    c_instructions_queue->out_rdy(c_queue_compute_rdy_sig);
    c_instructions_queue->out_data(c_queue_compute_data);
    compute->i_queue_data(c_queue_compute_data);
    c_instructions_queue->out_end(c_queue_compute_end_sig);
    compute->i_queue_end(c_queue_compute_end_sig);

    /// fetcher 2 store queue
    fetcher->store_queue_vld(fetcher_s_queue_vld_sig);
    s_instructions_queue->in_vld(fetcher_s_queue_vld_sig);
    s_instructions_queue->in_rdy(fetcher_s_queue_rdy_sig);
    fetcher->store_queue_rdy(fetcher_s_queue_rdy_sig);
    fetcher->store_queue_data(fetcher_s_queue_data);
    s_instructions_queue->in_data(fetcher_s_queue_data);
    fetcher->store_queue_end(fetcher_s_queue_end_sig);
    s_instructions_queue->in_end(fetcher_s_queue_end_sig);
    /// store queue 2 store
    s_instructions_queue->out_vld(s_queue_store_vld_sig);
    store->i_queue_vld(s_queue_store_vld_sig);
    store->i_queue_rdy(s_queue_store_rdy_sig);
    s_instructions_queue->out_rdy(s_queue_store_rdy_sig);
    s_instructions_queue->out_data(s_queue_store_data);
    store->i_queue_data(s_queue_store_data);
    s_instructions_queue->out_end(s_queue_store_end_sig);
    store->i_queue_end(s_queue_store_end_sig);


    // load 2 compute
    /// load 2 queue
    load->push_next_vld(load_l2c_vld_sig);
    l2c_queue->in_vld(load_l2c_vld_sig);
    l2c_queue->in_rdy(l2c_load_rdy_sig);
    load->push_next_rdy(l2c_load_rdy_sig);
    load->push_next_data(load_l2c_data);
    l2c_queue->in_data(load_l2c_data);
    load->push_next_end(load_l2c_end_sig);
    l2c_queue->in_end(load_l2c_end_sig);

    /// queue 2 compute
    l2c_queue->out_vld(l2c_compute_vld_sig);
    compute->pull_prev_vld(l2c_compute_vld_sig);
    compute->pull_prev_rdy(compute_l2c_rdy_sig);
    l2c_queue->out_rdy(compute_l2c_rdy_sig);
    l2c_queue->out_data(l2c_compute_data);
    compute->pull_prev_data(l2c_compute_data);
    l2c_queue->out_end(l2c_compute_end_sig);
    compute->pull_prev_end(l2c_compute_end_sig);


    // compute 2 load 
    /// compute 2 queue
    compute->push_prev_vld(compute_c2l_vld_sig);
    c2l_queue->in_vld(compute_c2l_vld_sig);
    c2l_queue->in_rdy(c2l_compute_rdy_sig);
    compute->push_prev_rdy(c2l_compute_rdy_sig);
    compute->push_prev_data(compute_c2l_data);
    c2l_queue->in_data(compute_c2l_data);
    compute->push_prev_end(compute_c2l_end_sig);
    c2l_queue->in_end(compute_c2l_end_sig);

    /// queue 2 load
    c2l_queue->out_vld(c2l_load_vld_sig);
    load->pull_next_vld(c2l_load_vld_sig);
    load->pull_next_rdy(load_c2l_rdy_sig);
    c2l_queue->out_rdy(load_c2l_rdy_sig);
    c2l_queue->out_data(c2l_load_data);
    load->pull_next_data(c2l_load_data);
    c2l_queue->out_end(c2l_load_end_sig);
    load->pull_next_end(c2l_load_end_sig);


    // store 2 compute
    /// store 2 queue
    store->push_prev_vld(store_s2c_vld_sig);
    s2c_queue->in_vld(store_s2c_vld_sig);
    s2c_queue->in_rdy(s2c_store_rdy_sig);
    store->push_prev_rdy(s2c_store_rdy_sig);
    store->push_prev_data(store_s2c_data);
    s2c_queue->in_data(store_s2c_data);
    store->push_prev_end(store_s2c_end_sig);
    s2c_queue->in_end(store_s2c_end_sig);

    /// queue 2 compute
    s2c_queue->out_vld(s2c_compute_vld_sig);
    compute->pull_next_vld(s2c_compute_vld_sig);
    compute->pull_next_rdy(compute_s2c_rdy_sig);
    s2c_queue->out_rdy(compute_s2c_rdy_sig);
    s2c_queue->out_data(s2c_compute_data);
    compute->pull_next_data(s2c_compute_data);
    s2c_queue->out_end(s2c_compute_end_sig);
    compute->pull_next_end(s2c_compute_end_sig);


    // compute 2 store 
    /// compute 2 queue
    compute->push_next_vld(compute_c2s_vld_sig);
    c2s_queue->in_vld(compute_c2s_vld_sig);
    c2s_queue->in_rdy(c2s_compute_rdy_sig);
    compute->push_next_rdy(c2s_compute_rdy_sig);
    compute->push_next_data(compute_c2s_data);
    c2s_queue->in_data(compute_c2s_data);
    compute->push_next_end(compute_c2s_end_sig);
    c2s_queue->in_end(compute_c2s_end_sig);

    /// queue 2 store
    c2s_queue->out_vld(c2s_store_vld_sig);
    store->pull_prev_vld(c2s_store_vld_sig);
    store->pull_prev_rdy(store_c2s_rdy_sig);
    c2s_queue->out_rdy(store_c2s_rdy_sig);
    c2s_queue->out_data(c2s_store_data);
    store->pull_prev_data(c2s_store_data);
    c2s_queue->out_end(c2s_store_end_sig);
    store->pull_prev_end(c2s_store_end_sig);

    SC_METHOD(start); // <-- RESTORED: Must be an SC_METHOD so SystemC schedules it correctly
    SC_THREAD(power_on_sequence); // --- STAGE 2: REGISTER POWER-ON THREAD ---
}

void VTA::start() {
    load->start();
    compute->start();
    store->start();
}

// =========================================================================
// --- STAGE 2: POWER-ON RESET SEQUENCE ---
// This thread handles the hardware boot-up. It pulls the Reset line LOW,
// waits for the clock to stabilize, and then releases the Reset.
// =========================================================================
void VTA::power_on_sequence() {
    // 1. Initial State: Assert Reset (Active Low = 0)
    sys_reset.write(0);
    
    // 2. Wait for 100ns (10 clock cycles) to let the signals stabilize
    wait(100, SC_NS);
    
    // 3. Release Reset (Active Low = 1)
    sys_reset.write(1);
    
    // 4. Wait 1 more clock cycle to ensure all threads see ARESETN=1
    wait(10, SC_NS);
    
    std::cout << "\n[SYSTEM] AXI Bus Reset Released. Power-On Complete." << std::endl;
    
    // 5. Trigger the ARM to start fetching instructions AFTER reset is released
    arm->start();
}