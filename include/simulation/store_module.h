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
    
    // =========================================================================
    // --- STAGE 2: AXI HARDWARE INTEGRATION PORTS ---
    // The following ports allow this StoreModule to act as an AXI Master.
    // It will push completed neural network calculations back into main memory.
    // =========================================================================

    // ACLK (AXI Clock): The global system clock that synchronizes all AXI transfers.
    sc_in<bool> ACLK;

    // ARESETN (AXI Reset, Active Low): Resets the AXI state machine when pulled to 0.
    sc_in<bool> ARESETN;

    // START_ADDR: The dynamic starting memory address assigned to this module by the
    // Time-Triggered Dispatcher. Tells the Store module exactly where it is safe to write.
    sc_in<sc_uint<32>> START_ADDR; 

    // --- AXI ADDRESS WRITE CHANNEL (AW) ---
    // Used by the Store module to tell the memory *where* it is about to write data.

    // AWADDR (Address Write): The 32-bit physical address the module wants to write to.
    sc_out<sc_uint<32>> AWADDR; 

    // AWLEN (Address Write Length): Defines how many data beats (bursts) are in this transfer.
    sc_out<sc_uint<8>>  AWLEN;

    // AWVALID (Address Write Valid): The Store module sets this HIGH (1) to announce: 
    // "I have placed a valid address on AWADDR. Please prepare to receive data."
    sc_out<bool>        AWVALID;

    // AWREADY (Address Write Ready): The Arbiter sets this HIGH (1) to tell the Store module:
    // "I have accepted your address. You can begin sending the data now."
    sc_in<bool>         AWREADY;

    // --- AXI DATA WRITE CHANNEL (W) ---
    // Used by the Store module to send the actual computed data to the memory.

    // WDATA (Write Data): The 32-bit chunk of actual calculated neural network data.
    sc_out<sc_uint<32>> WDATA;

    // WVALID (Write Valid): The Store module sets this HIGH (1) to announce:
    // "I have placed valid data on the WDATA wire. Please capture it."
    sc_out<bool>        WVALID;

    // WREADY (Write Ready): The Memory sets this HIGH (1) to tell the Store module:
    // "I have successfully captured the data you sent."
    sc_in<bool>         WREADY;

    // WLAST (Write Last): The Store module sets this HIGH (1) during the final data beat,
    // signaling to the Memory and Arbiter that it is finished writing for this burst.
    sc_out<bool>        WLAST;

    // --- AXI WRITE RESPONSE CHANNEL (B) ---
    // Used by the memory to confirm the entire burst write was successful.

    // BRESP (Write Response): Status of the write (e.g., OKAY, ERROR).
    sc_in<sc_uint<2>>   BRESP;

    // BVALID (Response Valid): The Memory sets this HIGH (1) to announce:
    // "I have finished processing your burst write, here is the final status."
    sc_in<bool>         BVALID;

    // BREADY (Response Ready): The Store module sets this HIGH (1) to tell the Memory:
    // "I am ready to receive your final confirmation."
    sc_out<bool>        BREADY;


    SC_HAS_PROCESS(StoreModule);

    StoreModule(sc_module_name n);

private:

    // =========================================================================
    // --- STAGE 2: AXI TO EVENT-DRIVEN BRIDGE ---
    // We cannot use wait() inside dependencies_received() because it is an SC_METHOD.
    // Therefore, we use this sc_event to wake up a separate SC_THREAD that handles AXI.
    // =========================================================================

    // Phase 2: AXI SC_METHOD handlers for STORE OUT
    enum store_axi_state { s_idle, s_out_addr, s_out_data, s_out_resp };
    sc_signal<store_axi_state, SC_MANY_WRITERS> axi_state;

    uint32_t s_axi_sram_idx;
    uint32_t s_axi_dram_offset;
    uint32_t s_axi_chunk_count;

    uint32_t s_axi_y;
    uint32_t s_axi_x;
    uint32_t s_axi_x_size;
    uint32_t s_axi_y_size;
    uint32_t s_axi_stride;

    uint32_t s_axi_base_addr;

    bool aw_pending = false;
    bool w_pending  = false;

    void process_axi_write_fsm();

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