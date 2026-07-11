#ifndef COMPUTE_MODULE_H
#define COMPUTE_MODULE_H

#include <systemc.h>
#include <queue>

#include "module.h"


class ComputeModule : public Module {
public:

    // trigger
    sc_out<bool> out_trig;

    // =========================================================================
    // --- AXI MASTER READ PORT (M1) ---
    // Allows the Compute module to load Micro-Ops and Biases directly from DRAM.
    // Mirrors the LoadModule AXI pattern exactly.
    // =========================================================================
    sc_in<bool>         ACLK;
    sc_in<bool>         ARESETN;
    sc_in<sc_uint<32>>  START_ADDR;

    sc_out<sc_uint<32>> ARADDR;
    sc_out<sc_uint<8>>  ARLEN;
    sc_out<bool>        ARVALID;
    sc_in<bool>         ARREADY;

    sc_in<sc_uint<32>>  RDATA;
    sc_in<sc_uint<2>>   RRESP;
    sc_in<bool>         RVALID;
    sc_out<bool>        RREADY;
    sc_in<bool>         RLAST;

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

    SC_HAS_PROCESS(ComputeModule);

    ComputeModule(sc_module_name n);

    static int current_layer;
private:

    sc_int<64> *prev_data = nullptr;
    sc_int<64> *next_data = nullptr;

    bool is_waiting_prev = false;
    bool is_waiting_next = false;

    bool out_signal = false;

    bool did_push_prev = false;
    bool did_push_next = false;
    bool push_prev_vld_state = false;
    bool push_prev_end_state = false;
    bool push_next_vld_state = false;
    bool push_next_end_state = false;
    bool pull_next_rdy_state = false;
    bool pull_prev_rdy_state = false;

    sc_event send_signal;

    // AXI SC_METHOD handlers for LOAD UOP and LOAD ACC
    enum compute_axi_state { c_idle, c_uop_addr, c_uop_data, c_acc_addr, c_acc_data };
    sc_signal<compute_axi_state, SC_MANY_WRITERS> axi_state;

    uint32_t c_axi_beats_remaining;
    uint32_t c_axi_sram_idx;
    uint32_t c_axi_addr_offset;
    uint32_t c_axi_chunk_count;

    uint32_t c_axi_y;
    uint32_t c_axi_x;
    uint32_t c_axi_burst_idx;
    uint32_t c_axi_elem_base;
    uint32_t c_axi_base_addr;

    uint32_t c_axi_x0_pad;
    uint32_t c_axi_x1_pad;
    uint32_t c_axi_y1_pad;
    uint32_t c_axi_x_width;
    bool c_axi_left_pad_done;

    void process_axi_read_fsm();
    sc_event activate_push_prev_vld;
    sc_event activate_push_prev_end;
    sc_event activate_push_next_vld;
    sc_event activate_push_next_end;
    sc_event activate_pull_prev_rdy;
    sc_event activate_pull_next_rdy;

    sc_event write_push_prev_data;
    sc_event read_pull_prev_data;
    sc_event write_push_next_data;
    sc_event read_pull_next_data;

    sc_time latency() override;

    void send_signal_handler();

    void check_dependencies() override;
    void receive_dependencies() override;
    void dependencies_received() override;
    void finalize_instruction() override;
    void push_dependencies() override;
    void dependencies_pushed() override;

    void activate_push_prev_vld_handler();
    void activate_push_prev_end_handler();
    void activate_push_next_vld_handler();
    void activate_push_next_end_handler();
    void activate_pull_prev_rdy_handler();
    void activate_pull_next_rdy_handler();

    // pull prev
    void pull_prev_vld_handler();
    void pull_prev_end_handler();
    void read_pull_prev_data_handler();

    // push prev
    void push_prev_rdy_handler();
    void write_push_prev_data_handler();

    // pull next
    void pull_next_vld_handler();
    void pull_next_end_handler();
    void read_pull_next_data_handler();

    // push next
    void push_next_rdy_handler();
    void write_push_next_data_handler();

};

#endif;