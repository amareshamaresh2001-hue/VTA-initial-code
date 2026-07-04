#include <zmq.h>
#include <nlohmann/json.hpp>

#include "module.h"
#include "utilities.h"
// #include "consts.h"


Module::Module(sc_core::sc_module_name nm): sc_module(nm) { 
    
    SC_METHOD(i_queue_vld_handler);
    dont_initialize();
    sensitive << i_queue_vld.pos();
    
    SC_METHOD(i_queue_end_handler);
    dont_initialize();
    sensitive << i_queue_end.pos();

    SC_METHOD(activate_i_queue_rdy_handler);
    dont_initialize();
    sensitive << activate_i_queue_rdy;
    
    SC_METHOD(read_i_queue_data_handler);
    dont_initialize();
    sensitive << read_i_queue_data;

    SC_METHOD(fetch_instruction);
    dont_initialize();
    sensitive << fetch;

    SC_METHOD(check_dependencies);
    dont_initialize();
    sensitive << check_dep;

    SC_METHOD(receive_dependencies);
    dont_initialize();
    sensitive << receive_dep;

    SC_METHOD(dependencies_received);
    dont_initialize();
    sensitive << dep_received;

    SC_METHOD(finalize_instruction);
    dont_initialize();
    sensitive << finish;

    SC_METHOD(push_dependencies);
    dont_initialize();
    sensitive << push_dep;

    SC_METHOD(dependencies_pushed);
    dont_initialize();
    sensitive << dep_pushed;
}

void Module::start() {
    fetch.notify(SC_ZERO_TIME);
}

void Module::fetch_instruction() {
    // if (!current) { 
    //     // no more instructioons in the queue
    //     if (instructions.empty()) {
    //         std::cout << "------------" << this->name() << " MODULE FINISHED" << "------------" << std::endl;
    //         return;
    //     }

    //     // just started execution the load new instruction
    //     current = new Inst(instructions.front());
    //     instructions.pop();

    //     std::cout << sc_time_stamp() << " " << this->name() << " START EXECUTING " << current->id << std::endl;

    //     check_dep.notify(SC_ZERO_TIME);
    // }

    if (i_queue_vld.read()) {
        i_queue_rdy_state = true;
        activate_i_queue_rdy.notify(1, SC_NS);
    } 
}


// fetch instruction 
void Module::i_queue_vld_handler() {
    if (!current) {
        i_queue_rdy_state = true;
        activate_i_queue_rdy.notify(1, SC_NS);
    }
}

void Module::i_queue_end_handler() {
    i_queue_rdy_state = false;
    activate_i_queue_rdy.notify(SC_ZERO_TIME);
}


void Module::activate_i_queue_rdy_handler() {
    this->i_queue_rdy.write(i_queue_rdy_state);
    if (i_queue_rdy_state) {
        read_i_queue_data.notify(1, SC_NS);
    }
}

void Module::read_i_queue_data_handler() {

    if (!i_queue_vld.read()) {
        return;
    }

    current_instruction_part.push_back(i_queue_data.read());
    if (current_instruction_part.size() == 2) {
        current = new Instruction({current_instruction_part[0], current_instruction_part[1]});
        current_instruction_part.clear();

        check_dep.notify(SC_ZERO_TIME);
    } else {
        read_i_queue_data.notify(1, SC_NS);
    }
}
