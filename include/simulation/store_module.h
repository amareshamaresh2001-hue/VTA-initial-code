#ifndef STORE_MODULE_H
#define STORE_MODULE_H

#include <systemc.h>
#include <queue>

#include "module.h"


class StoreModule : public Module {
public:

    // pull prev
    sc_in<bool> pull_prev_vld;
    sc_out<bool> pull_prev_rdy;
    sc_in<sc_int<64>> pull_prev_data;
    sc_in<bool> pull_prev_end;

    // push prev
    sc_out<bool> push_prev_vld;
    sc_in<bool> push_prev_rdy;
    sc_out<sc_int<64>> push_prev_data;
    sc_out<bool> push_prev_end;
    
    SC_HAS_PROCESS(StoreModule);

    StoreModule(sc_module_name n);

private:

    sc_int<64> *prev_data = nullptr;

    bool is_waiting_prev = false;

    bool did_push_prev = false;
    bool push_prev_vld_state = false;
    bool push_prev_end_state = false;
    bool pull_prev_rdy_state = false;

    sc_event activate_push_prev_vld;
    sc_event activate_push_prev_end;
    sc_event activate_pull_prev_rdy;

    sc_event write_push_prev_data;
    sc_event read_pull_prev_data;

    sc_time latency() override;

    void check_dependencies() override;
    void receive_dependencies() override;
    void dependencies_received() override;
    void finalize_instruction() override;
    void push_dependencies() override;
    void dependencies_pushed() override;

    void activate_push_prev_vld_handler();
    void activate_push_prev_end_handler();
    void activate_pull_prev_rdy_handler();

     // pull prev
    void pull_prev_vld_handler();
    void read_pull_prev_data_handler();
    void pull_prev_end_handler();

    // push prev
    void push_prev_rdy_handler();
    void write_push_prev_data_handler();

};

#endif;