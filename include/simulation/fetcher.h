#ifndef FETCHER_H
#define FETCHER_H

#include <systemc.h>
#include <vector>
#include <map>
#include <queue>

#include "instruction.h"
#include "instruction_schedule.h"
#include "queue.h"


// struct Inst {
//     uint32_t  id;
//     InstrType type;
//     bool pull_prev, pull_next, push_prev, push_next;
//     int dep_count;

//     Inst () {

//     }
    
//     Inst(const sc_int<64>& data) { 
//         this->id = data.range(63, 32).to_uint();

//         this->type = static_cast<InstrType>(
//             data.range(5, 4).to_uint()
//         );

//         this->pull_prev = data[3];
//         this->pull_next = data[2];
//         this->push_prev = data[1];
//         this->push_next = data[0];
//     }

//     Inst(int id, InstrType type, bool pull_prev, bool pull_next, bool push_prev, bool push_next) {
//         this->id = id;
//         this->type = type;
//         this->pull_prev = pull_prev;
//         this->pull_next = pull_next;
//         this->push_prev = push_prev;
//         this->push_next = push_next;
//         this->dep_count = (pull_prev ? 1 : 0) + (pull_next ? 1 : 0);
//     }

//     sc_int<64> encode() {
//         sc_int<64> data = 0;
        
//             // ID → bits [63:32]
//         data.range(63, 32) = this->id;

//         // Type → bits [5:4]
//         data.range(5, 4) = static_cast<unsigned int>(this->type);

//         // Flags
//         data[3] = this->pull_prev;
//         data[2] = this->pull_next;
//         data[1] = this->push_prev;
//         data[0] = this->push_next;

//         return data;
//     }

// };


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

    sc_out<sc_uint<32>> ARADDR;
    sc_out<sc_uint<8>>  ARLEN;
    sc_out<bool>        ARVALID;
    sc_in<bool>         ARREADY;

    sc_in<sc_uint<32>>  RDATA;
    sc_in<sc_uint<2>>   RRESP;
    sc_in<bool>         RVALID;
    sc_out<bool>        RREADY;
    sc_in<bool>         RLAST;

    // --- Validation Signals for VCD Tracing ---
    sc_signal<sc_uint<64>> parser_inst_part0;
    sc_signal<sc_uint<64>> parser_inst_part1;
    sc_signal<sc_uint<64>> axi_fetch_part0;
    sc_signal<sc_uint<64>> axi_fetch_part1;

    Fetcher(
        sc_core::sc_module_name nm,
        const std::vector<std::string>& keys,
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
    void axi_read_thread();

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