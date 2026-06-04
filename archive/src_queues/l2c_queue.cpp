#include "l2c_queue.h"


void L2CQueue::receive_signal() {
    triggers_queue.push(new sc_event);
    if (receiver_is_waiting) {
        triggers_queue.pop();
        // notify the compute module
        receiver_is_waiting = false;
        sending = true;
        send.notify(SC_ZERO_TIME);
    }
}

void L2CQueue::send_signal() {
    if (receiver_is_waiting) {
        sending = false;
        this->out_sig.write(true);
        next_trigger(sc_time(1, SC_NS));
    } else {
        this->out_sig.write(false);
    }
}

void L2CQueue::receive_pull_signal() {
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




// void L2CQueue::push() {
//     triggers_queue.push(new sc_event);
//     if (compute_is_waiting) {
//         triggers_queue.pop();
//         compute_is_waiting = false;
//         // notify the compute module
//         send_signal();
//     }
// }

// void L2CQueue::pop() {
//     if (triggers_queue.size() > 0) {
//         triggers_queue.pop();
//         // notify the compute module
//         send_signal();
//     } else {
//         compute_is_waiting = true;
//     }
// }

// void L2CQueue::send_signal() {
//     compute_module->load_signal_arrived = true;
//     compute_module->kick.notify(SC_ZERO_TIME);
// }