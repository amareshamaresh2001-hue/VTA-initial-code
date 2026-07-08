#ifndef FETCHER_H
#define FETCHER_H

#include <systemc.h>
#include <vector>
#include <map>
#include <queue>

#include "instruction.h"
#include "instruction_schedule.h"
#include "queue.h"




class Fetcher : public sc_module
{
public:

    // trigger
    sc_in<bool> in_trig;

    // load queue
    sc_out<bool> load_queue_vld;
    sc_in<bool> load_queue_rdy;
    sc_out<sc_int<64>> load_queue_data;
    sc_out<bool> load_queue_end;

    // compute queue
    sc_out<bool> compute_queue_vld;
    sc_in<bool> compute_queue_rdy;
    sc_out<sc_int<64>> compute_queue_data;
    sc_out<bool> compute_queue_end;

    // store queue
    sc_out<bool> store_queue_vld;
    sc_in<bool> store_queue_rdy;
    sc_out<sc_int<64>> store_queue_data;
    sc_out<bool> store_queue_end;

    // =========================================================================
    // --- AXI MASTER READ PORT (M3) ---
    // Allows the Fetcher to load instructions directly from DRAM via M3.
    // Mirrors the LoadModule AXI pattern exactly.
    // =========================================================================
    sc_in<bool>         ACLK;
    sc_in<bool>         ARESETN;
    sc_in<sc_uint<32>>  START_ADDR;

    sc_out<sc_uint<32>> ARADDR;// the address that we want the data from
    sc_out<sc_uint<8>>  ARLEN;//how many 32bits we want
    sc_out<bool>        ARVALID;// turn high to inform that we put an address on ARADDR
    sc_in<bool>         ARREADY;//the pin fetcher listens to, arbitrer turns this high to say it received the data

    sc_in<sc_uint<32>>  RDATA;// pin where the 32bit data comes from memory
    sc_in<sc_uint<2>>   RRESP;
    sc_in<bool>         RVALID;//pin we listen to- memory turns this hihg when RDATA has a valid data
    sc_out<bool>        RREADY;//ready to accept data
    sc_in<bool>         RLAST;// inform that its the last data

    // --- Validation Signals for VCD Tracing ---
    sc_signal<sc_uint<64>> parser_inst_part0;
    sc_signal<sc_uint<64>> parser_inst_part1;
    sc_signal<sc_uint<64>> axi_fetch_part0;
    sc_signal<sc_uint<64>> axi_fetch_part1;

    sc_signal<uint64_t> trace_expected_part0;
    sc_signal<uint64_t> trace_expected_part1;
    sc_signal<uint64_t> trace_fetched_part0;
    sc_signal<uint64_t> trace_fetched_part1;

    Fetcher(sc_module_name n,
            const std::vector<std::string>& layers,
        const std::map<std::string, std::vector<std::tuple<InstrType, sc_int<64>, sc_int<64>>>>& encoded_splited_instructions);

    ~Fetcher() {}

    SC_HAS_PROCESS(Fetcher);

protected:
    
    int current_layer;
    std::vector<std::string> layers;
    std::map<std::string, std::vector<std::tuple<InstrType, sc_int<64>, sc_int<64>>>> encoded_splited_instructions;
    std::queue<std::tuple<InstrType, sc_int<64>, sc_int<64>>> instructions;
    InstrType current_instruction_type = InstrType::LOAD;
    sc_int<64> bits;
    std::vector<sc_int<64>> current_instruction_part;

    bool do_fetch_layer = false;

    // Guards the shared current_instruction_part/current_instruction_type
    // buffer: true from the moment load_instruction() pops an instruction
    // until its dispatch to the target queue fully completes (the
    // activate_*_queue_end_handler() that used to be the only trigger for
    // load.notify()). Without this, firing load.notify() from the AXI
    // fetch side (see process_axi_read_fsm's f_data state) as well as from
    // dispatch-completion can pop a NEW instruction into this buffer while
    // the PREVIOUS one's dispatch is still draining it -- which happens
    // whenever a target instruction queue is at its 128-entry capacity and
    // backpressures a dispatch mid-flight. Corrupts the shared buffer and
    // crashes. See AGENT_CONTEXT.md Fix 7 for the full incident writeup.
    bool dispatch_busy = false;

    bool load_queue_vld_state = false;
    bool load_queue_end_state = false;
    bool compute_queue_vld_state = false;
    bool compute_queue_end_state = false;
    bool store_queue_vld_state = false;
    bool store_queue_end_state = false;

    sc_event load_layer;
    sc_event load;
    sc_event send;
    sc_event start_axi_read;

    sc_event activate_load_queue_vld;
    sc_event activate_load_queue_end;
    sc_event activate_compute_queue_vld;
    sc_event activate_compute_queue_end;
    sc_event activate_store_queue_vld;
    sc_event activate_store_queue_end;

    sc_event write_load_data;
    sc_event write_compute_data;
    sc_event write_store_data;
    
    void in_trig_handler();

    void load_layer_instructions();

    void load_instruction();
    void send_instruction();
    enum fetch_axi_state { f_idle, f_addr, f_data };
    sc_signal<fetch_axi_state, SC_MANY_WRITERS> axi_state;

    uint32_t f_axi_dram_offset;
    uint32_t f_axi_chunk_count;
    uint32_t f_axi_inst_idx;
    uint32_t f_axi_num_inst;
    uint64_t f_axi_part0;
    uint64_t f_axi_part1;

    void process_axi_read_fsm();

    void activate_load_queue_vld_handler();
    void activate_load_queue_end_handler();
    void activate_compute_queue_vld_handler();
    void activate_compute_queue_end_handler();
    void activate_store_queue_vld_handler();
    void activate_store_queue_end_handler();

    void load_queue_rdy_handler();
    void compute_queue_rdy_handler();
    void store_queue_rdy_handler();

    void write_load_data_handler();
    void write_compute_data_handler();
    void write_store_data_handler();

};

#endif