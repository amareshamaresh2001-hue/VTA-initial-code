#include "c2l_queue.h"


void Queue::receive_signal() {
    triggers_queue.push(new sc_event);
    if (receiver_is_waiting) {
        triggers_queue.pop();
        // notify the compute module
        receiver_is_waiting = false;
        sending = true;
        send.notify(SC_ZERO_TIME);
    }
}

void Queue::send_signal() {
    if (sending) {
        sending = false;
        this->out_sig.write(true);
        next_trigger(sc_time(1, SC_NS));
    } else {
        this->out_sig.write(false);
    }
}

void Queue::receive_pull_signal() {
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







void C2LQueue::receive_signal() {
    triggers_queue.push(new sc_event);
    if (receiver_is_waiting) {
        triggers_queue.pop();
        // notify the compute module
        receiver_is_waiting = false;
        sending = true;
        send.notify(SC_ZERO_TIME);
    }
}

void C2LQueue::send_signal() {
    if (sending) {
        sending = false;
        this->out_sig.write(true);
        next_trigger(sc_time(1, SC_NS));
    } else {
        this->out_sig.write(false);
    }
}

void C2LQueue::receive_pull_signal() {
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





// void C2LQueue::push() {
//     triggers_queue.push(new sc_event);
//     if (load_is_waiting) {
//         triggers_queue.pop();
//         load_is_waiting = false;
//         // notify the compute module
//         send_signal();
//     }
// }

// void C2LQueue::pop() {
//     if (triggers_queue.size() > 0) {
//         triggers_queue.pop();
//         // notify the compute module
//         send_signal();
//     } else {
//         load_is_waiting = true;
//     }
// }

// void C2LQueue::send_signal() {
//     load_module->compute_signal_arrived = true;
//     load_module->kick.notify(SC_ZERO_TIME);
// }