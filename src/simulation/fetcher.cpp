#include "fetcher.h"
#include "utilities.h"

Fetcher::Fetcher(
    sc_module_name nm,
    const std::vector<std::string>& keys,
    const std::map<std::string, std::vector<std::tuple<InstrType, sc_int<64>, sc_int<64>>>>& encoded_splited_instructions) : sc_module(nm) {
    this->layers = keys;
    this->encoded_splited_instructions = encoded_splited_instructions;
    this->current_layer = 0;

    SC_METHOD(process_axi_read_fsm);
    dont_initialize();
    sensitive << ACLK.pos();

    SC_METHOD(load_layer_instructions);
    dont_initialize();
    sensitive << load_layer;

    SC_METHOD(load_instruction);
    dont_initialize();
    sensitive << load;
    
    SC_METHOD(send_instruction);
    dont_initialize();
    sensitive << send;
    

    SC_METHOD(activate_load_queue_vld_handler);
    dont_initialize();
    sensitive << activate_load_queue_vld;

    SC_METHOD(activate_load_queue_end_handler);
    dont_initialize();
    sensitive << activate_load_queue_end;
    
    SC_METHOD(activate_compute_queue_vld_handler);
    dont_initialize();
    sensitive << activate_compute_queue_vld;
    
    SC_METHOD(activate_compute_queue_end_handler);
    dont_initialize();
    sensitive << activate_compute_queue_end;
    
    SC_METHOD(activate_store_queue_vld_handler);
    dont_initialize();
    sensitive << activate_store_queue_vld;
    
    SC_METHOD(activate_store_queue_end_handler);
    dont_initialize();
    sensitive << activate_store_queue_end;


    SC_METHOD(in_trig_handler);
    dont_initialize();
    sensitive << in_trig.pos();

    SC_METHOD(load_queue_rdy_handler);
    dont_initialize();
    sensitive << load_queue_rdy.pos();

    SC_METHOD(compute_queue_rdy_handler);
    dont_initialize();
    sensitive << compute_queue_rdy.pos();

    SC_METHOD(store_queue_rdy_handler);
    dont_initialize();
    sensitive << store_queue_rdy.pos();
    

    SC_METHOD(write_load_data_handler);
    dont_initialize();
    sensitive << write_load_data;

    SC_METHOD(write_compute_data_handler);
    dont_initialize();
    sensitive << write_compute_data;

    SC_METHOD(write_store_data_handler);
    dont_initialize();
    sensitive << write_store_data;

}

void Fetcher::in_trig_handler() {
    load_layer.notify(SC_ZERO_TIME);
}

void Fetcher::load_layer_instructions() {
    if (current_layer == layers.size()) {
        sc_stop(); // Dynamically stop the simulation when ALL layers are finished!
        return;
    } 
    do_fetch_layer = true;
}

void Fetcher::process_axi_read_fsm() {
    if (!ARESETN.read()) {
        ARVALID.write(0);
        RREADY.write(0);
        f_axi_dram_offset = 0;
        axi_state.write(f_idle);
        if (do_fetch_layer) {
            do_fetch_layer = false;
        }
        return;
    }

    switch (axi_state.read()) {
        case f_idle:
            if (do_fetch_layer) {
                do_fetch_layer = false;
                f_axi_num_inst = encoded_splited_instructions[layers[current_layer]].size();
                f_axi_inst_idx = 0;
                if (f_axi_num_inst > 0) {
                    axi_state.write(f_addr);
                } else {
                    current_layer++;
                    load.notify(1, SC_NS);
                }
            }
            break;

        case f_addr:
            ARADDR.write(START_ADDR.read() + f_axi_dram_offset);
            ARLEN.write(3); // 4 beats - 1
            ARVALID.write(1);
            RREADY.write(1);

            if (ARREADY.read() == 1 && ARVALID.read() == 1) {
                ARVALID.write(0);
                f_axi_chunk_count = 0;
                f_axi_part0 = 0;
                f_axi_part1 = 0;
                axi_state.write(f_data);
            }
            break;

        case f_data:
            if (RVALID.read() == 1 && RREADY.read() == 1) {
                uint32_t data_chunk = RDATA.read().to_uint();
                
                // Reconstruct the 128-bit instruction payload
                if (f_axi_chunk_count == 0) f_axi_part0 |= ((uint64_t)data_chunk << 0);
                if (f_axi_chunk_count == 1) f_axi_part0 |= ((uint64_t)data_chunk << 32);
                if (f_axi_chunk_count == 2) f_axi_part1 |= ((uint64_t)data_chunk << 0);
                if (f_axi_chunk_count == 3) f_axi_part1 |= ((uint64_t)data_chunk << 32);
                
                f_axi_chunk_count++;
                f_axi_dram_offset += 4;

                if (f_axi_chunk_count == 4) {
                    RREADY.write(0);
                    
                    auto expected_inst = encoded_splited_instructions[layers[current_layer]][f_axi_inst_idx];
                    InstrType type = std::get<0>(expected_inst);
                    uint64_t expected_part0 = std::get<1>(expected_inst).to_uint64();
                    uint64_t expected_part1 = std::get<2>(expected_inst).to_uint64();

                    parser_inst_part0.write(expected_part0);
                    parser_inst_part1.write(expected_part1);
                    uint64_t parser_part0 = std::get<1>(expected_inst);
                    uint64_t parser_part1 = std::get<2>(expected_inst);

                    trace_expected_part0.write(parser_part0);
                    trace_expected_part1.write(parser_part1);
                    trace_fetched_part0.write(f_axi_part0);
                    trace_fetched_part1.write(f_axi_part1);
                    if (f_axi_part0 != expected_part0 || f_axi_part1 != expected_part1) {
                        std::cout << "[FETCHER ERROR] Mismatch at offset " << std::hex << (f_axi_dram_offset - 16) << "!" << std::endl;
                    }

                    // Push the physically fetched instruction bits to the queue
                    this->instructions.push({type, f_axi_part0, f_axi_part1});
                    // Dispatch this instruction as soon as it is fetched, instead of
                    // waiting for the whole layer to finish fetching (matches the
                    // skeleton's intent: instructions start moving as soon as they
                    // are available, not only once the entire batch is ready).
                    load.notify(1, SC_NS);

                    f_axi_inst_idx++;
                    if (f_axi_inst_idx < f_axi_num_inst) {
                        axi_state.write(f_addr);
                    } else {
                        axi_state.write(f_idle);
                        current_layer++;
                    }
                }
            }
            break;
    }
}

void Fetcher::load_instruction() {
    if (instructions.size() > 0) {
        const auto current = instructions.front();
        instructions.pop();

        current_instruction_type = std::get<0>(current);
        current_instruction_part.clear();
        current_instruction_part.push_back(std::get<1>(current));
        current_instruction_part.push_back(std::get<2>(current));

        send.notify(5, SC_NS);
    }
}

void Fetcher::send_instruction() {
    switch (current_instruction_type)
    {
    case InstrType::LOAD:
        load_queue_vld_state = true;
        activate_load_queue_vld.notify(1, SC_NS);
        break;
    case InstrType::COMPUTE:
        compute_queue_vld_state = true;
        activate_compute_queue_vld.notify(1, SC_NS);
        break;
    case InstrType::STORE:
        store_queue_vld_state = true;
        activate_store_queue_vld.notify(1, SC_NS);
        break;
    default:
        break;
    }
}


void Fetcher::activate_load_queue_vld_handler() {
    this->load_queue_vld.write(this->load_queue_vld_state);
}

void Fetcher::activate_load_queue_end_handler() {
    this->load_queue_end.write(this->load_queue_end_state);
    if (this->load_queue_end_state) {

        this->load_queue_end_state = false;
        activate_load_queue_end.notify(1, SC_NS);
        this->load_queue_vld_state = false;
        activate_load_queue_vld.notify(SC_ZERO_TIME);
        
        load.notify(1, SC_NS);
    }
}

void Fetcher::activate_compute_queue_vld_handler() {
    this->compute_queue_vld.write(this->compute_queue_vld_state);
}

void Fetcher::activate_compute_queue_end_handler() {
    this->compute_queue_end.write(this->compute_queue_end_state);
    if (this->compute_queue_end_state) {

        this->compute_queue_end_state = false;
        activate_compute_queue_end.notify(1, SC_NS);
        this->compute_queue_vld_state = false;
        activate_compute_queue_vld.notify(SC_ZERO_TIME);

        load.notify(1, SC_NS);
    }
}

void Fetcher::activate_store_queue_vld_handler() {
    this->store_queue_vld.write(this->store_queue_vld_state);
}

void Fetcher::activate_store_queue_end_handler() {
    this->store_queue_end.write(this->store_queue_end_state);
    if (this->store_queue_end_state) {

        this->store_queue_end_state = false;
        activate_store_queue_end.notify(1, SC_NS);
        this->store_queue_vld_state = false;
        activate_store_queue_vld.notify(SC_ZERO_TIME);

        load.notify(1, SC_NS);
    }
}


void Fetcher::load_queue_rdy_handler() {
    write_load_data.notify(SC_ZERO_TIME);
}

void Fetcher::compute_queue_rdy_handler() {
    write_compute_data.notify(SC_ZERO_TIME);
}

void Fetcher::store_queue_rdy_handler() {
    write_store_data.notify(SC_ZERO_TIME);
}


void Fetcher::write_load_data_handler() {
    // std::cout << sc_time_stamp() << " " << this->name() << " SEND DATA " << current.encode() << std::endl;
    // std::cout << sc_time_stamp() << " " << this->name() << " SEND Instruction " << current.get_layer() << " " << current.get_pc() << std::endl;
    // load_queue_data.write(std::get<0>(current.encode()));

    // load_queue_vld_state = false;
    // activate_load_queue_vld.notify(1, SC_NS);

    sc_int<64> bits = current_instruction_part.front();
    current_instruction_part.erase(current_instruction_part.begin());

    load_queue_data.write(bits);

    if (current_instruction_part.size() > 0) {
        write_load_data.notify(1, SC_NS);
    } else {
        load_queue_end_state = true;
        activate_load_queue_end.notify(1, SC_NS);
    }
}

void Fetcher::write_compute_data_handler() {
    // std::cout << sc_time_stamp() << " " << this->name() << " SEND DATA " << current.encode() << std::endl;
    // std::cout << sc_time_stamp() << " " << this->name() << " SEND Instruction " << current.get_layer() << " " << current.get_pc() << std::endl;
    // compute_queue_data.write(std::get<0>(current.encode()));

    // compute_queue_vld_state = false;
    // activate_compute_queue_vld.notify(1, SC_NS);

    sc_int<64> bits = current_instruction_part.front();
    current_instruction_part.erase(current_instruction_part.begin());

    compute_queue_data.write(bits);

    if (current_instruction_part.size() > 0) {
        write_compute_data.notify(1, SC_NS);
    } else {
        compute_queue_end_state = true;
        activate_compute_queue_end.notify(1, SC_NS);
    }
}

void Fetcher::write_store_data_handler() {
    // std::cout << sc_time_stamp() << " " << this->name() << " SEND DATA " << current.encode() << std::endl;
    // std::cout << sc_time_stamp() << " " << this->name() << " SEND Instruction " << current.get_layer() << " " << current.get_pc() << std::endl;
    // store_queue_data.write(std::get<0>(current.encode()));

    // store_queue_vld_state = false;
    // activate_store_queue_vld.notify(1, SC_NS);

    sc_int<64> bits = current_instruction_part.front();
    current_instruction_part.erase(current_instruction_part.begin());

    store_queue_data.write(bits);

    if (current_instruction_part.size() > 0) {
        write_store_data.notify(1, SC_NS);
    } else {
        store_queue_end_state = true;
        activate_store_queue_end.notify(1, SC_NS);
    }
}
