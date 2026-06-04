#ifndef LOAD_MODULE_H
#define LOAD_MODULE_H

#include <systemc.h>
#include <queue>

#include "module.h"


class LoadModule : public Module {
public:
    // pull next
    sc_in<bool> pull_next_vld;
    sc_out<bool> pull_next_rdy;
    sc_in<sc_int<64>> pull_next_data;
    sc_in<bool> pull_next_end;

    // push next
    sc_out<bool> push_next_vld;
    sc_in<bool> push_next_rdy;
    sc_out<sc_int<64>> push_next_data;
    sc_out<bool> push_next_end;

    SC_HAS_PROCESS(LoadModule);

    LoadModule(sc_module_name n);

private:

    sc_int<64> *next_data = nullptr;

    bool is_waiting_next = false;

    bool did_push_next = false;
    bool push_next_vld_state = false;
    bool push_next_end_state = false;
    bool pull_next_rdy_state = false;

    sc_event activate_push_next_vld;
    sc_event activate_push_next_end;
    sc_event activate_pull_next_rdy;

    sc_event write_push_next_data;
    sc_event read_pull_next_data;

    sc_time latency() override;

    void check_dependencies() override;
    void receive_dependencies() override;
    void dependencies_received() override;
    void finalize_instruction() override;
    void push_dependencies() override;
    void dependencies_pushed() override;

    void activate_push_next_vld_handler();
    void activate_push_next_end_handler();
    void activate_pull_next_rdy_handler();

    // pull next
    void pull_next_vld_handler();
    void pull_next_end_handler();
    void read_pull_next_data_handler();

    // push next
    void push_next_rdy_handler();
    void write_push_next_data_handler();

};

#endif;