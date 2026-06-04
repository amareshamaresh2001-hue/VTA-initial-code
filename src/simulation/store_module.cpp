#include "store_module.h"


StoreModule::StoreModule(sc_module_name n) : Module(n) {

    SC_METHOD(activate_push_prev_vld_handler);
    dont_initialize();
    sensitive << activate_push_prev_vld;

    SC_METHOD(activate_push_prev_end_handler);
    dont_initialize();
    sensitive << activate_push_prev_end;

    SC_METHOD(activate_pull_prev_rdy_handler);
    dont_initialize();
    sensitive << activate_pull_prev_rdy;


    SC_METHOD(write_push_prev_data_handler);
    dont_initialize();
    sensitive << write_push_prev_data;

    SC_METHOD(read_pull_prev_data_handler);
    dont_initialize();
    sensitive << read_pull_prev_data;

    
    SC_METHOD(pull_prev_vld_handler);
    dont_initialize();
    sensitive << pull_prev_vld.pos();
    
    SC_METHOD(pull_prev_end_handler);
    dont_initialize();
    sensitive << pull_prev_end.pos();
    
    SC_METHOD(push_prev_rdy_handler);
    dont_initialize();
    sensitive << push_prev_rdy.pos();
}

sc_time StoreModule::latency() {
    int inst_exec_time = 15;
    if (current->get_y_size() != 0) {
        inst_exec_time = current->get_y_size() * 100 + PlatformConfig::getInstance().store_cycles * std::ceil(current->get_y_size() * current->get_x_size() * 16.0 / 256.0);
    } 
    return sc_time(inst_exec_time, SC_NS);
}

void StoreModule::check_dependencies() {
    if (current->get_pop_prev()) {
        if (this->pull_prev_vld.read()) {
            this->pull_prev_rdy_state = true;
            activate_pull_prev_rdy.notify(1, SC_NS);
        } else {
            this->is_waiting_prev = true;
        }
    } else {
        this->dep_received.notify(SC_ZERO_TIME);
    }
}

void StoreModule::receive_dependencies() {
    if (current->get_pop_prev() && this->prev_data != nullptr) {
        this->dep_received.notify(SC_ZERO_TIME);
    }
}

void StoreModule::dependencies_received() {
    // std::cout << sc_time_stamp() << " START STORE ID=" << current->id << std::endl;
    // std::cout << sc_time_stamp() << " " << this->name() << " START EXECUTING " << current->get_layer() << " " << current->get_pc() << std::endl;
    finish.notify(latency());
}

void StoreModule::finalize_instruction() {
    this->result_data = new sc_int<32>(5);
    this->push_dep.notify(SC_ZERO_TIME);
}

void StoreModule::push_dependencies() {
    // check push dependency
    if (current->get_push_prev()) {
        this->push_prev_vld_state = true;
        activate_push_prev_vld.notify(1, SC_NS);
    } else {
        // std::cout << sc_time_stamp() << " FINISH STORE ID=" << current->get_pc() << std::endl;
        if (result_data != nullptr)
            delete result_data;
        if (prev_data != nullptr)
            delete prev_data;
        delete current;

        this->current = nullptr;
        this->result_data = nullptr;
        this->prev_data = nullptr;
        this->did_push_prev = false;

        this->fetch.notify(1, SC_NS);
    }
}

void StoreModule::dependencies_pushed() {
    if (!current->get_push_prev() || (current->get_push_prev() && this->did_push_prev)) {
        // std::cout << sc_time_stamp() << " FINISH STORE ID=" << current->get_pc() << std::endl;

        if (result_data != nullptr)
            delete result_data;
        if (prev_data != nullptr)
            delete prev_data;
        delete current;

        this->current = nullptr;
        this->result_data = nullptr;
        this->prev_data = nullptr;
        this->did_push_prev = false;

        this->push_prev_vld_state = false;
        activate_push_prev_vld.notify(1, SC_NS);

        this->fetch.notify(1, SC_NS);
    }
}


// write signals
void StoreModule::activate_push_prev_vld_handler() {
    // std::cout << sc_time_stamp() << " STORE 2 PUSH_PREV_QUEUE VLD=" << this->push_prev_vld_state << std::endl;
    this->push_prev_vld.write(this->push_prev_vld_state);
}

void StoreModule::activate_push_prev_end_handler() {
    // std::cout << sc_time_stamp() << " STORE 2 PUSH_PREV_QUEUE VLD=" << this->push_prev_vld_state << std::endl;
    this->push_prev_end.write(this->push_prev_end_state);

    if (push_prev_end_state) {
        this->push_prev_end_state = false;
        this->activate_push_prev_end.notify(1, SC_NS);

        this->did_push_prev = true;
        this->dep_pushed.notify(SC_ZERO_TIME);
    }
}

void StoreModule::activate_pull_prev_rdy_handler() {
    // std::cout << sc_time_stamp() << " STORE 2 PULL_PREV_QUEUE RDY=" << this->pull_prev_rdy_state << std::endl;
    this->pull_prev_rdy.write(this->pull_prev_rdy_state);
    if (this->pull_prev_rdy_state) {
        read_pull_prev_data.notify(1, SC_NS);
    }
}


// pull prev
void StoreModule::pull_prev_vld_handler() {
    if (current && current->get_pop_prev() && this->is_waiting_prev) {
        this->is_waiting_prev = false;
        this->pull_prev_rdy_state = true;
        activate_pull_prev_rdy.notify(1, SC_NS);
    }
}

void StoreModule::read_pull_prev_data_handler() {
    // std::cout << sc_time_stamp() << " STORE RECEIVE DATA " << this->pull_prev_data.read() << std::endl;
    this->prev_data = new sc_int<64>(this->pull_prev_data.read());
}

void StoreModule::pull_prev_end_handler() {
    this->pull_prev_rdy_state = false;
    activate_pull_prev_rdy.notify(1, SC_NS);

    this->receive_dep.notify(SC_ZERO_TIME);
}

// push prev
void StoreModule::push_prev_rdy_handler() {
    write_push_prev_data.notify(SC_ZERO_TIME);
}

void StoreModule::write_push_prev_data_handler() {
    // std::cout << sc_time_stamp() << " STORE SEND DATA " << this->current->id << std::endl;
    this->push_prev_data.write(this->current->get_pc());

    this->push_prev_end_state = true;
    this->activate_push_prev_end.notify(1, SC_NS);
}
