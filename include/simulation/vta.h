#ifndef VTA_H
#define VTA_H

#include <systemc.h>
#include "arm.h"
#include "load_module.h"
#include "compute_module.h"
#include "store_module.h"
#include "instruction.h"


class VTA : public sc_module {
public:
    SC_HAS_PROCESS(VTA);
    VTA(
        sc_module_name n,
        const std::vector<std::string>& keys,
        const std::map<std::string, std::vector<std::tuple<InstrType, sc_int<64>, sc_int<64>>>>& encoded_splited_instructions);

private:
    ARM *arm;
    Fetcher *fetcher;

    Queue *l_instructions_queue;
    Queue *c_instructions_queue;
    Queue *s_instructions_queue;

    LoadModule *load;
    ComputeModule *compute;
    StoreModule *store;

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