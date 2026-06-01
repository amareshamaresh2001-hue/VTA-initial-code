#ifndef QUEUE_H
#define QUEUE_H

#include <systemc.h>
#include <queue>

#include "instruction.h"


class Queue : public sc_module {
public:

    // input
    sc_in<bool> in_vld;
    sc_out<bool> in_rdy;
    sc_in<sc_int<64>> in_data;
    sc_in<bool> in_end;

    // output
    sc_out<bool> out_vld;
    sc_in<bool> out_rdy;
    sc_out<sc_int<64>> out_data;
    sc_out<bool> out_end;

    SC_HAS_PROCESS(Queue);

    Queue(sc_core::sc_module_name nm, bool instruction_queue);

protected:

    bool instruction_queue = false;
    int capacity = 128;
    std::queue<sc_int<64>> data;
    std::vector<sc_int<64>> current_instruction_part;
    int out_instruction_parts_count = 0;

    // internal events
    sc_event activate_in_rdy;
    sc_event activate_out_vld;
    sc_event activate_out_end;

    sc_event write_out_data;
    sc_event read_in_data;

    bool out_vld_state = false;
    bool out_end_state = false;
    bool in_rdy_state = false;

    // input
    void in_vld_handler();
    void in_end_handler();

    // output
    void out_rdy_handler();

    // internal events
    void activate_in_rdy_handler();

    void activate_out_vld_handler();

    void activate_out_end_handler();

    void write_out_data_handler();

    void read_in_data_handler();

};

#endif;