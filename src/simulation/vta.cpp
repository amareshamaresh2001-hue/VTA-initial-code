#include "vta.h"


VTA::VTA(
    sc_module_name n,
    const std::vector<std::string>& keys,
    const std::map<std::string, std::vector<std::tuple<InstrType, sc_int<64>, sc_int<64>>>>& encoded_splited_instructions) : sc_module(n) {
    arm = new ARM("ARM");
    fetcher = new Fetcher("Fetcher", keys, encoded_splited_instructions);

    l_instructions_queue = new Queue("Load_Instructions", true);
    c_instructions_queue = new Queue("Compute_Instructions", true);
    s_instructions_queue = new Queue("Store_Instructions", true);

    load = new LoadModule("Load");
    compute = new ComputeModule("Compute");
    store = new StoreModule("Store");

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

    SC_METHOD(start);
}

void VTA::start() {
    load->start();
    compute->start();
    store->start();
    arm->start();
}