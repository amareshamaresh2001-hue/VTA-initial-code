#include "queue.h"


Queue::Queue(sc_core::sc_module_name nm, bool instruction_queue): sc_module(nm) {
    this->instruction_queue = instruction_queue;

    SC_METHOD(in_vld_handler);
    dont_initialize();
    sensitive << in_vld.pos();
    
    SC_METHOD(in_end_handler);
    dont_initialize();
    sensitive << in_end.pos();
    
    SC_METHOD(out_rdy_handler);
    dont_initialize();
    sensitive << out_rdy.pos();
    
    SC_METHOD(activate_in_rdy_handler);
    dont_initialize();
    sensitive << activate_in_rdy;
    
    SC_METHOD(activate_out_vld_handler);
    dont_initialize();
    sensitive << activate_out_vld;
    
    SC_METHOD(activate_out_end_handler);
    dont_initialize();
    sensitive << activate_out_end;
    
    SC_METHOD(write_out_data_handler);
    dont_initialize();
    sensitive << write_out_data;
    
    SC_METHOD(read_in_data_handler);
    dont_initialize();
    sensitive << read_in_data;

}

// input
void Queue::in_vld_handler() {
    if (this->data.size() < capacity) {
        this->in_rdy_state = true;
        activate_in_rdy.notify(1, SC_NS);
    }
}

void Queue::in_end_handler() {
    this->in_rdy_state = false;
    this->activate_in_rdy.notify(SC_ZERO_TIME);
}

// output
void Queue::out_rdy_handler() {
    if (instruction_queue) 
    {
        out_instruction_parts_count = 0;
    }
    this->write_out_data.notify(SC_ZERO_TIME);
}


// write signals
void Queue::activate_in_rdy_handler() {
    // std::cout << sc_time_stamp() << " " << this->name() << " IN RDY=" << this->in_rdy_state << std::endl;
    this->in_rdy.write(this->in_rdy_state);
    if (this->in_rdy_state) {
        read_in_data.notify(1, SC_NS);
    }
}

void Queue::activate_out_vld_handler() {
    // std::cout << sc_time_stamp() << " " << this->name() << " OUT VLD=" << this->out_vld_state << std::endl;
    this->out_vld.write(this->out_vld_state);
}

void Queue::activate_out_end_handler() {
    // std::cout << sc_time_stamp() << " " << this->name() << " OUT VLD=" << this->out_vld_state << std::endl;
    this->out_end.write(this->out_end_state);

    if (this->out_end_state) {
        this->out_end_state = false;
        this->activate_out_end.notify(1, SC_NS);
        this->out_vld_state = false;
        this->activate_out_vld.notify(SC_ZERO_TIME);

        if (this->data.size() > 0) {
            this->out_vld_state = true;
            this->activate_out_vld.notify(1, SC_NS);
        }
    }
    
}


// read & write data
void Queue::write_out_data_handler() {
    if(std::string(this->name()) == "vta.Compute_Instructions") {
        std::cout<<"";
    }
    // std::cout << sc_time_stamp() << " " << this->name() << " SEND DATA " << this->data.front() << std::endl;
    if (instruction_queue) {
        this->out_data.write(this->data.front());
        out_instruction_parts_count++;
        this->data.pop();

        if (out_instruction_parts_count < 2) {
            this->write_out_data.notify(1, SC_NS);
        } else {
            if ((this->data.size() + out_instruction_parts_count) == capacity && this->in_vld.read()) {
                this->in_rdy_state = true;
                activate_in_rdy.notify(1, SC_NS);
            }

            this->out_end_state = true;
            this->activate_out_end.notify(1, SC_NS);
        }
    } else {
        this->out_data.write(this->data.front());

        if (this->data.size() == capacity && this->in_vld.read()) {
            this->in_rdy_state = true;
            activate_in_rdy.notify(1, SC_NS);
        }

        this->data.pop();

        this->out_end_state = true;
        this->activate_out_end.notify(1, SC_NS);
    }
    
    if(std::string(this->name()) == "vta.Compute_Instructions") {
        std::cout<<"";
    }
}

void Queue::read_in_data_handler() {
    if(std::string(this->name()) == "vta.Compute_Instructions") {
        std::cout<<"";
    }

    if(!in_vld.read()) {
        return;
    }

    // std::cout << sc_time_stamp() << " " << this->name() << " RECEIVE DATA " << this->in_data.read() << std::endl;
    if (instruction_queue) {
        current_instruction_part.push_back(this->in_data.read());
        if (current_instruction_part.size() == 2) {
            this->data.push(current_instruction_part[0]); 
            this->data.push(current_instruction_part[1]); 
            
            Instruction *new_current = new Instruction({current_instruction_part[0], current_instruction_part[1]});
            if (new_current->get_pc() > 175) {
                std::cout<<"";
            }

            current_instruction_part.clear(); 

            this->out_vld_state = true;
            this->activate_out_vld.notify(1, SC_NS);
        } else {
            read_in_data.notify(1, SC_NS);
            return;
        }
    } else {
        this->data.push(this->in_data.read());
        
        this->out_vld_state = true;
        this->activate_out_vld.notify(1, SC_NS);
    }

    if(std::string(this->name()) == "vta.Compute_Instructions") {
        std::cout<<"";
    }
}
