#include "arm.h"


ARM::ARM(sc_module_name nm) : sc_module(nm) {

    SC_METHOD(send_signal_handler);
    dont_initialize();
    sensitive << send_signal;


    SC_METHOD(in_trig_handler);
    dont_initialize();
    sensitive << in_trig.pos();

}

void ARM::start() {
    out_signal = true;
    send_signal.notify(1, SC_NS);
}

void ARM::in_trig_handler() {
    out_signal = true;
    send_signal.notify(1, SC_NS);
}

void ARM::send_signal_handler() {
    this->out_trig.write(out_signal);
    if (out_signal) {
        out_signal = false;
        send_signal.notify(1, SC_NS);
    }
}