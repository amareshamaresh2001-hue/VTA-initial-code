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

    // =========================================================================
    // --- STAGE 2: AXI HARDWARE INTEGRATION PORTS ---
    // The following ports allow this LoadModule to act as an AXI Master.
    // It will request data from the shared Arbiter/Memory across the bus.
    // =========================================================================

    // ACLK (AXI Clock): The global system clock that synchronizes all AXI transfers.
    // Every handshake (VALID & READY) is evaluated on the rising edge of this clock.
    sc_in<bool> ACLK;

    // ARESETN (AXI Reset, Active Low): When this is 0, the module must immediately
    // abort any active transfers and return to its initial idle state.
    sc_in<bool> ARESETN;

    // START_ADDR: The dynamic starting memory address assigned to this module by the
    // Time-Triggered Dispatcher. This ensures the module doesn't overwrite other data.
    sc_in<sc_uint<32>> START_ADDR; 

    // --- AXI ADDRESS READ CHANNEL (AR) ---
    // Used by the Load module to tell the memory *where* it wants to read from.

    // ARADDR (Address Read): The 32-bit physical address the module wants to read from.
    sc_out<sc_uint<32>> ARADDR; 

    // ARLEN (Address Read Length): Defines how many data beats (bursts) are in this transfer.
    // ARLEN=3 means 4 total beats of data will be transferred (0, 1, 2, 3).
    sc_out<sc_uint<8>>  ARLEN;

    // ARVALID (Address Read Valid): The Load module sets this HIGH (1) to announce: 
    // "I have placed a valid address on ARADDR. Please accept my read request."
    sc_out<bool>        ARVALID;

    // ARREADY (Address Read Ready): The Arbiter sets this HIGH (1) to tell the Load module:
    // "I have accepted your read request. You can now wait for the data."
    sc_in<bool>         ARREADY;

    // --- AXI DATA READ CHANNEL (R) ---
    // Used by the memory to send the requested data back to the Load module.

    // RDATA (Read Data): The 32-bit chunk of actual neural network data arriving from memory.
    sc_in<sc_uint<32>>  RDATA;

    // RRESP (Read Response): Status of the read (e.g., OKAY, ERROR). Usually ignored in simple sims.
    sc_in<sc_uint<2>>   RRESP;

    // RVALID (Read Valid): The Memory sets this HIGH (1) to announce:
    // "I have placed valid data on the RDATA wire for you to capture."
    sc_in<bool>         RVALID;

    // RREADY (Read Ready): The Load module sets this HIGH (1) to tell the Memory:
    // "I am ready to accept the data you are sending me."
    sc_out<bool>        RREADY;

    // RLAST (Read Last): The Memory sets this HIGH (1) during the final data beat of the burst,
    // signaling to the Load module that the requested transaction is completely finished.
    sc_in<bool>         RLAST;


    SC_HAS_PROCESS(LoadModule);

    LoadModule(sc_module_name n);

private:

    // =========================================================================
    // --- STAGE 2: AXI TO EVENT-DRIVEN BRIDGE ---
    // We cannot use wait() inside dependencies_received() because it is an SC_METHOD.
    // Therefore, we use this sc_event to wake up a separate SC_THREAD that handles AXI.
    // =========================================================================

    // start_axi_read: This event is fired by dependencies_received() when an instruction arrives.
    sc_event start_axi_read;

    // axi_read_thread: A clock-driven SystemC thread. It sleeps until start_axi_read is triggered.
    // Once awake, it handles the complex multi-cycle AXI handshakes and loop logic.
    void axi_read_thread();

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