#include "load_module.h"


LoadModule::LoadModule(sc_module_name n) : Module(n) {

    SC_METHOD(activate_push_next_vld_handler);
    dont_initialize();
    sensitive << activate_push_next_vld;

    SC_METHOD(activate_push_next_end_handler);
    dont_initialize();
    sensitive << activate_push_next_end;

    SC_METHOD(activate_pull_next_rdy_handler);
    dont_initialize();
    sensitive << activate_pull_next_rdy;


    SC_METHOD(write_push_next_data_handler);
    dont_initialize();
    sensitive << write_push_next_data;

    SC_METHOD(read_pull_next_data_handler);
    dont_initialize();
    sensitive << read_pull_next_data;


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

sc_time LoadModule::latency() {
    int inst_exec_time = 15;
    if (current->get_y_size() != 0) {
        if (current->get_name() == "LOAD INP") {
            inst_exec_time = current->get_y_size() * 100 + PlatformConfig::getInstance().load_cycles * std::ceil(current->get_y_size() * current->get_x_size() * 16.0 / 256.0);
        } else {
            inst_exec_time = current->get_y_size() * 100 + PlatformConfig::getInstance().load_cycles * current->get_y_size() * current->get_x_size();
        }
    }
    return sc_time(inst_exec_time, SC_NS);
}


void LoadModule::check_dependencies() { //
    if (current->get_pop_next()) {
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

void LoadModule::receive_dependencies() { //
    if (current->get_pop_next() && this->next_data != nullptr) {
        this->dep_received.notify(SC_ZERO_TIME);
    }
}

void LoadModule::dependencies_received() { //
    // std::cout << sc_time_stamp() << " START LOAD ID=" << current->id << std::endl;
    // std::cout << sc_time_stamp() << " " << this->name() << " START EXECUTING " << current->get_layer() << " " << current->get_pc() << std::endl;
    finish.notify(latency());
}

void LoadModule::finalize_instruction() { //
    this->result_data = new sc_int<32>(5);
    this->push_dep.notify(SC_ZERO_TIME);
}

void LoadModule::push_dependencies() {
    // check push dependency
    if (current->get_push_next()) {
        this->push_next_vld_state = true;
        this->activate_push_next_vld.notify(1, SC_NS);
    } else {
        // std::cout << sc_time_stamp() << " FINISH LOAD ID=" << current->get_pc() << std::endl;

        if (result_data != nullptr)
            delete result_data;
        if (next_data != nullptr)
            delete next_data;
        delete current;

        this->current = nullptr;
        this->result_data = nullptr;
        this->next_data = nullptr;
        this->did_push_next = false;

        this->fetch.notify(1, SC_NS);
    }
}

void LoadModule::dependencies_pushed() {
    if (!current->get_push_next() || (current->get_push_next() && this->did_push_next)) {
        // std::cout << sc_time_stamp() << " FINISH LOAD ID=" << current->get_pc() << std::endl;

        if (result_data != nullptr)
            delete result_data;
        if (next_data != nullptr)
            delete next_data;
        delete current;

        this->current = nullptr;
        this->result_data = nullptr;
        this->next_data = nullptr;
        this->did_push_next = false;

        this->push_next_vld_state = false;
        this->activate_push_next_vld.notify(1, SC_NS);

        this->fetch.notify(1, SC_NS);
    } 
}


// write signals
void LoadModule::activate_push_next_vld_handler() {
    // std::cout << sc_time_stamp() << " LOAD 2 PUSH_NEXT_QUEUE VLD=" << this->push_next_vld_state << std::endl;
    this->push_next_vld.write(this->push_next_vld_state);
}

void LoadModule::activate_push_next_end_handler() {
    // std::cout << sc_time_stamp() << " LOAD 2 PUSH_NEXT_QUEUE VLD=" << this->push_next_vld_state << std::endl;
    this->push_next_end.write(this->push_next_end_state);
    
    if (this->push_next_end_state) {
        this->push_next_end_state = false;
        this->activate_push_next_end.notify(1, SC_NS);

        this->did_push_next = true;
        this->dep_pushed.notify(SC_ZERO_TIME);
    }
    
}

void LoadModule::activate_pull_next_rdy_handler() { //
    // std::cout << sc_time_stamp() << " LOAD 2 PULL_NEXT_QUEUE RDY=" << this->pull_next_rdy_state << std::endl;
    this->pull_next_rdy.write(this->pull_next_rdy_state);
    if (this->pull_next_rdy_state) {
        read_pull_next_data.notify(1, SC_NS);
    }
}


// pull next
void LoadModule::pull_next_vld_handler() { //
    if (current && current->get_pop_next() && this->is_waiting_next) {
        this->is_waiting_next = false;
        this->pull_next_rdy_state = true;
        activate_pull_next_rdy.notify(1, SC_NS);
    }
}

void LoadModule::read_pull_next_data_handler() { //
    // std::cout << sc_time_stamp() << " LOAD RECEIVE DATA " << this->pull_next_data.read() << std::endl;
    this->next_data = new sc_int<64>(this->pull_next_data.read());
}

void LoadModule::pull_next_end_handler() { //
    this->pull_next_rdy_state = false;
    activate_pull_next_rdy.notify(1, SC_NS);
    this->receive_dep.notify(SC_ZERO_TIME);
}

// push next
void LoadModule::push_next_rdy_handler() {
    write_push_next_data.notify(SC_ZERO_TIME);
}

void LoadModule::write_push_next_data_handler() {
    // std::cout << sc_time_stamp() << " LOAD SEND DATA " << this->current->id << std::endl;
    this->push_next_data.write(this->current->get_pc());
    
    this->push_next_end_state = true;
    this->activate_push_next_end.notify(1, SC_NS);
}
