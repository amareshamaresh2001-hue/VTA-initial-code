#include "compute_module.h"


int ComputeModule::current_layer = 0;

ComputeModule::ComputeModule(sc_module_name n) : Module(n) {

    SC_METHOD(send_signal_handler);
    dont_initialize();
    sensitive << send_signal;

    SC_METHOD(activate_push_prev_vld_handler);
    dont_initialize();
    sensitive << activate_push_prev_vld;

    SC_METHOD(activate_push_prev_end_handler);
    dont_initialize();
    sensitive << activate_push_prev_end;

    SC_METHOD(activate_push_next_vld_handler);
    dont_initialize();
    sensitive << activate_push_next_vld;

    SC_METHOD(activate_push_next_end_handler);
    dont_initialize();
    sensitive << activate_push_next_end;

    SC_METHOD(activate_pull_prev_rdy_handler);
    dont_initialize();
    sensitive << activate_pull_prev_rdy;

    SC_METHOD(activate_pull_next_rdy_handler);
    dont_initialize();
    sensitive << activate_pull_next_rdy;


    SC_METHOD(write_push_prev_data_handler);
    dont_initialize();
    sensitive << write_push_prev_data;

    SC_METHOD(write_push_next_data_handler);
    dont_initialize();
    sensitive << write_push_next_data;

    SC_METHOD(read_pull_prev_data_handler);
    dont_initialize();
    sensitive << read_pull_prev_data;

    SC_METHOD(read_pull_next_data_handler);
    dont_initialize();
    sensitive << read_pull_next_data;


    SC_METHOD(pull_prev_vld_handler);
    dont_initialize();
    sensitive << pull_prev_vld.pos();

    SC_METHOD(pull_prev_end_handler);
    dont_initialize();
    sensitive << pull_prev_end.pos();

    SC_METHOD(push_prev_rdy_handler);
    dont_initialize();
    sensitive << push_prev_rdy.pos();

    SC_METHOD(pull_next_vld_handler);
    dont_initialize();
    sensitive << pull_next_vld.pos();

    SC_METHOD(pull_next_end_handler);
    dont_initialize();
    sensitive << pull_next_end.pos();

    SC_METHOD(push_next_rdy_handler);
    dont_initialize();
    sensitive << push_next_rdy.pos();

}

sc_time ComputeModule::latency() {
    int inst_exec_time = 15;
    if (current->get_y_size() != 0) {
        if (current->get_name() == "LOAD ACC") {
            inst_exec_time = current->get_y_size() * 100 + PlatformConfig::getInstance().load_cycles * std::ceil(current->get_y_size() * current->get_x_size() * 64.0 / 256.0);
        } else {
            inst_exec_time = current->get_y_size() * 100 + PlatformConfig::getInstance().load_cycles * std::ceil(current->get_y_size() * current->get_x_size() * 16.0 / 256.0);
        }
    } else {
        if (current->get_name() == "GEMM") {
            inst_exec_time = (current->get_range_1() - current->get_range_0()) * current->get_outer_loop_iter() * current->get_inner_loop_iter() + 100;
        }
        else if (current->get_name() == "NOP-COMPUTE-STAGE") {
            inst_exec_time = 15;
        }
        else if (current->get_name() == "FINISH") {
            inst_exec_time = 10;
        }
        else {
            inst_exec_time = 2 * ((current->get_range_1() - current->get_range_0()) * current->get_outer_loop_iter() * current->get_inner_loop_iter()) + 100;
        }
    }
    return sc_time(inst_exec_time, SC_NS);
}


void ComputeModule::check_dependencies() {
    if (current->get_pop_prev() && !current->get_pop_next()) {
        if (this->pull_prev_vld.read()) {
            this->pull_prev_rdy_state = true;
            activate_pull_prev_rdy.notify(1, SC_NS);
        } else {
            this->is_waiting_prev = true;
        }
    }
    else if (!current->get_pop_prev() && current->get_pop_next()) {
        if (this->pull_next_vld.read()) {
            this->pull_next_rdy_state = true;
            activate_pull_next_rdy.notify(1, SC_NS);
        } else {
            this->is_waiting_next = true;
        }
    } 
    else if (current->get_pop_prev() && current->get_pop_next()) {
        if (this->pull_prev_vld.read()) {
            this->pull_prev_rdy_state = true;
            activate_pull_prev_rdy.notify(1, SC_NS);
        } else {
            this->is_waiting_prev = true;
        }
        if (this->pull_next_vld.read()) {
            this->pull_next_rdy_state = true;
            activate_pull_next_rdy.notify(1, SC_NS);
        } else {
            this->is_waiting_next = true;
        }
    } else {
        this->dep_received.notify(SC_ZERO_TIME);
    }
}

void ComputeModule::receive_dependencies() {
    if (current->get_pop_prev() && !current->get_pop_next() && this->prev_data != nullptr) {
        this->dep_received.notify(SC_ZERO_TIME);
    }
    else if (!current->get_pop_prev() && current->get_pop_next() && this->next_data != nullptr) {
        this->dep_received.notify(SC_ZERO_TIME);
    } 
    else if (current->get_pop_prev() && current->get_pop_next() && this->prev_data != nullptr && this->next_data != nullptr) {
        this->dep_received.notify(SC_ZERO_TIME);
    }
}

void ComputeModule::dependencies_received() {
    // std::cout << sc_time_stamp() << " START COMPUTE ID=" << current->id << std::endl;
    // std::cout << sc_time_stamp() << " " << this->name() << " START EXECUTING " << current->get_layer() << " " << current->get_pc() << std::endl;
    finish.notify(latency());
}

void ComputeModule::finalize_instruction() {
    this->result_data = new sc_int<32>(5);
    
    if (this->current->get_name() =="FINISH") {
        std::cout << sc_time_stamp() << "\t\t" << "---------------------------- FINISH LAYER " << ComputeModule::current_layer++ << " ----------------------------" << std::endl;
        if (result_data != nullptr)
            delete result_data;
        if (prev_data != nullptr)
            delete prev_data;
        if (next_data != nullptr)
            delete next_data;
        delete current;

        this->current = nullptr;
        this->result_data = nullptr;
        this->prev_data = nullptr;
        this->next_data = nullptr;
        this->did_push_prev = false;
        this->did_push_next = false;

        this->out_signal = true;
        this->send_signal.notify(SC_ZERO_TIME);
    } else {
        this->push_dep.notify(SC_ZERO_TIME);
    }
}

void ComputeModule::push_dependencies() {
    // check push dependency
    if (current->get_push_prev() && !current->get_push_next()) {
        this->push_prev_vld_state = true;
        this->activate_push_prev_vld.notify(1, SC_NS);
    } 
    else if (!current->get_push_prev() && current->get_push_next()) {
        this->push_next_vld_state = true;
        this->activate_push_next_vld.notify(1, SC_NS);
    } 
    else if (current->get_push_prev() && current->get_push_next()) {
        this->push_prev_vld_state = true;
        this->activate_push_prev_vld.notify(1, SC_NS);
        
        this->push_next_vld_state = true;
        this->activate_push_next_vld.notify(1, SC_NS);
    } else {
        // std::cout << sc_time_stamp() << " FINISH COMPUTE ID=" << current->get_pc() << std::endl;
        if (result_data != nullptr)
            delete result_data;
        if (prev_data != nullptr)
            delete prev_data;
        if (next_data != nullptr)
            delete next_data;
        delete current;

        this->current = nullptr;
        this->result_data = nullptr;
        this->prev_data = nullptr;
        this->next_data = nullptr;
        this->did_push_prev = false;
        this->did_push_next = false;

        this->fetch.notify(1, SC_NS);
    }
}

void ComputeModule::dependencies_pushed() {
    if (
        (!current->get_push_prev() && !current->get_push_next()) || 
        (current->get_push_prev() && !current->get_push_next() && this->did_push_prev) ||
        (!current->get_push_prev() && current->get_push_next() && this->did_push_next) ||
        (current->get_push_prev() && current->get_push_next() && this->did_push_prev && this->did_push_next)
    ) {
        // std::cout << sc_time_stamp() << " FINISH COMPUTE ID=" << current->get_pc() << std::endl;

        if (result_data != nullptr)
            delete result_data;
        if (prev_data != nullptr)
            delete prev_data;
        if (next_data != nullptr)
            delete next_data;
        delete current;

        this->current = nullptr;
        this->result_data = nullptr;
        this->prev_data = nullptr;
        this->next_data = nullptr;
        this->did_push_prev = false;
        this->did_push_next = false;

        this->push_prev_vld_state = false;
        this->activate_push_prev_vld.notify(1, SC_NS);
        
        this->push_next_vld_state = false;
        this->activate_push_next_vld.notify(1, SC_NS);
        
        this->fetch.notify(1, SC_NS);
    }
}


// write signals

void ComputeModule::send_signal_handler() {
    this->out_trig.write(this->out_signal);
    if (this->out_signal) {
        this->out_signal = false;
        this->send_signal.notify(1, SC_NS);
    }
}

void ComputeModule::activate_push_prev_vld_handler() {
    // std::cout << sc_time_stamp() << " COMPUTE 2 PUSH_PREV_QUEUE VLD=" << this->push_prev_vld_state << std::endl;
    this->push_prev_vld.write(this->push_prev_vld_state);
}

void ComputeModule::activate_push_prev_end_handler() {
    // std::cout << sc_time_stamp() << " COMPUTE 2 PUSH_PREV_QUEUE VLD=" << this->push_prev_vld_state << std::endl;
    this->push_prev_end.write(this->push_prev_end_state);

    if (push_prev_end_state) {
        this->push_prev_end_state = false;
        this->activate_push_prev_end.notify(1, SC_NS);

        this->did_push_prev = true;
        this->dep_pushed.notify(SC_ZERO_TIME);
    }
}

void ComputeModule::activate_push_next_vld_handler() {
    // std::cout << sc_time_stamp() << " COMPUTE 2 PUSH_NEXT_QUEUE VLD=" << this->push_next_vld_state << std::endl;
    this->push_next_vld.write(this->push_next_vld_state);
}

void ComputeModule::activate_push_next_end_handler() {
    // std::cout << sc_time_stamp() << " COMPUTE 2 PUSH_NEXT_QUEUE VLD=" << this->push_next_vld_state << std::endl;
    this->push_next_end.write(this->push_next_end_state);

    if (this->push_next_end_state) {
        this->push_next_end_state = false;
        this->activate_push_next_end.notify(1, SC_NS);

        this->did_push_next = true;
        this->dep_pushed.notify(SC_ZERO_TIME);
    }
}

void ComputeModule::activate_pull_prev_rdy_handler() {
    // std::cout << sc_time_stamp() << " COMPUTE 2 PULL_PREV_QUEUE RDY=" << this->pull_prev_rdy_state << std::endl;
    this->pull_prev_rdy.write(this->pull_prev_rdy_state);
    if (this->pull_prev_rdy_state) {
        read_pull_prev_data.notify(1, SC_NS);
    }
}

void ComputeModule::activate_pull_next_rdy_handler() {
    // std::cout << sc_time_stamp() << " COMPUTE 2 PULL_NEXT_QUEUE RDY=" << this->pull_next_rdy_state << std::endl;
    this->pull_next_rdy.write(this->pull_next_rdy_state);
    if (this->pull_next_rdy_state) {
        read_pull_next_data.notify(1, SC_NS);
    }
}


// pull prev
void ComputeModule::pull_prev_vld_handler() {
    if (current && current->get_pop_prev() && this->is_waiting_prev) {
        this->is_waiting_prev = false;
        this->pull_prev_rdy_state = true;
        activate_pull_prev_rdy.notify(1, SC_NS);
    }
}

void ComputeModule::read_pull_prev_data_handler() {
    // std::cout << sc_time_stamp() << " COMPUTE RECEIVE DATA FROM LOAD " << this->pull_prev_data.read() << std::endl;
    this->prev_data = new sc_int<64>(this->pull_prev_data.read());
}

void ComputeModule::pull_prev_end_handler() {
    this->pull_prev_rdy_state = false;
    activate_pull_prev_rdy.notify(SC_ZERO_TIME);

    this->receive_dep.notify(SC_ZERO_TIME);
}

// push prev
void ComputeModule::push_prev_rdy_handler() {
    write_push_prev_data.notify(SC_ZERO_TIME);
}

void ComputeModule::write_push_prev_data_handler() {
    // std::cout << sc_time_stamp() << " COMPUTE SEND DATA TO LOAD " << this->current->id << std::endl;
    this->push_prev_data.write(this->current->get_pc());

    this->push_prev_end_state = true;
    this->activate_push_prev_end.notify(1, SC_NS);
}

// pull next
void ComputeModule::pull_next_vld_handler() {
    if (current && current->get_pop_next() && this->is_waiting_next) {
        this->is_waiting_next = false;
        this->pull_next_rdy_state = true;
        activate_pull_next_rdy.notify(1, SC_NS);
    }
}

void ComputeModule::read_pull_next_data_handler() {
    // std::cout << sc_time_stamp() << " COMPUTE RECEIVE DATA FROM STORE " << this->pull_next_data.read() << std::endl;
    this->next_data = new sc_int<64>(this->pull_next_data.read());
}

void ComputeModule::pull_next_end_handler() {
    this->pull_next_rdy_state = false;
    activate_pull_next_rdy.notify(1, SC_NS);

    this->receive_dep.notify(SC_ZERO_TIME);
}

// push next
void ComputeModule::push_next_rdy_handler() {
    write_push_next_data.notify(SC_ZERO_TIME);
}

void ComputeModule::write_push_next_data_handler() {
    // std::cout << sc_time_stamp() << " COMPUTE SEND DATA TO STORE " << this->current->id << std::endl;
    this->push_next_data.write(this->current->get_pc());
    this->push_next_end_state = true;
    this->activate_push_next_end.notify(1, SC_NS);
}
