#ifndef C2S_QUEUE_H
#define C2S_QUEUE_H

#include "store_module.h"

class StoreModule;

class C2SQueue : public sc_module {
public:
    // StoreModule *store_module;

    sc_event send;           // local trigger

    sc_in<bool> in_sig;
    sc_out<bool> out_sig;
    sc_in<bool> pull_sig;

    bool sending = false;
    bool receiver_is_waiting = false;

    std::queue<sc_event*> triggers_queue;

    SC_HAS_PROCESS(C2SQueue);

    C2SQueue(sc_core::sc_module_name nm): sc_module(nm) {
        SC_METHOD(receive_signal);
        dont_initialize();
        sensitive << in_sig.pos();

        SC_METHOD(receive_pull_signal);
        dont_initialize();
        sensitive << pull_sig.pos();

        SC_METHOD(send_signal);
        dont_initialize();
        sensitive << send;
    }

    void receive_signal();

    void send_signal();

    void receive_pull_signal();

    // void push();

};

#endif;