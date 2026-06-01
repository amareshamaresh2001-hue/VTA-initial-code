#ifndef ARM_H
#define ARM_H

#include <systemc.h>


class ARM : public sc_module
{
public:

    // triggers
    sc_in<bool> in_trig;
    sc_out<bool> out_trig;

    ARM(sc_core::sc_module_name nm);

    ~ARM() {}

    SC_HAS_PROCESS(ARM);

    void start();

protected:
    
    bool out_signal = false;

    sc_event send_signal;

    void in_trig_handler();
    void send_signal_handler();

};

#endif