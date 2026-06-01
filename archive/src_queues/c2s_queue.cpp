#include "c2s_queue.h"


void C2SQueue::receive_signal() {
    triggers_queue.push(new sc_event);
    if (receiver_is_waiting) {
        triggers_queue.pop();
        // notify the compute module
        receiver_is_waiting = false;
        sending = true;
        send.notify(SC_ZERO_TIME);
    }
}

void C2SQueue::send_signal() {
    if (receiver_is_waiting) {
        sending = false;
        this->out_sig.write(true);
        next_trigger(sc_time(1, SC_NS));
    } else {
        this->out_sig.write(false);
    }
}

void C2SQueue::receive_pull_signal() {
    if (triggers_queue.size() > 0) {
        triggers_queue.pop();
        // notify the compute module
        receiver_is_waiting = false;
        sending = true;
        send.notify(SC_ZERO_TIME);
    } else {
        receiver_is_waiting = true;
    }
}









// void C2SQueue::push() {
//     triggers_queue.push(new sc_event);
//     if (store_is_waiting) {
//         triggers_queue.pop();
//         store_is_waiting = false;
//         // notify the compute module
//         send_signal();
//     }
// }

// void C2SQueue::pop() {
//     if (triggers_queue.size() > 0) {
//         triggers_queue.pop();
//         // notify the compute module
//         send_signal();
//     } else {
//         store_is_waiting = true;
//     }
// }

// void C2SQueue::send_signal() {
//     store_module->compute_signal_arrived = true;
//     store_module->kick.notify(SC_ZERO_TIME);
// }