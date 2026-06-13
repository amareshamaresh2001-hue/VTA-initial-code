#include "compute_module.h"
#include <cstring>
#include <algorithm>

constexpr int VTA_BLOCK_IN  = 16;//the number of input-channel elements in one tile.
constexpr int VTA_BLOCK_OUT = 16;//the number of output-channel elements in one tile.

constexpr int UOP_BUFF_DEPTH = (1 << vta_config::UOP_DST_WIDTH);// maximum number of micro-op entries in the micro-op SRAM.
constexpr int ACC_BUFF_DEPTH = (1 << vta_config::UOP_DST_WIDTH);//maximum number of accumulator tiles in the accumulator SRAM.
constexpr int INP_BUFF_DEPTH = (1 << vta_config::UOP_SRC_WIDTH);// maximum number of input activation tiles in the input SRAM.
constexpr int WGT_BUFF_DEPTH = (1 << vta_config::UOP_WGT_WIDTH);//maximum number of weight tiles in the weight SRAM.

constexpr uint32_t DST_MASK = (1u << vta_config::UOP_DST_WIDTH) - 1u;// Extracts the DST field = destination accumulator tile index.
constexpr uint32_t SRC_MASK = (1u << vta_config::UOP_SRC_WIDTH) - 1u;//extracts the SRC field = source input tile index in inp_mem OR  extracts the SRC field = second operand acc tile index in acc_mem.
constexpr uint32_t WGT_MASK = (1u << vta_config::UOP_WGT_WIDTH) - 1u;// extracts the WGT field = weight tile index in wgt_mem.


static uint32_t uop_mem[UOP_BUFF_DEPTH] = {};//micro-op SRAM.
static int32_t acc_mem[ACC_BUFF_DEPTH][VTA_BLOCK_OUT] = {};//accumulator SRAM.

int8_t inp_mem[INP_BUFF_DEPTH][VTA_BLOCK_IN] = {};// input activation SRAM.  Shared with LoadModule.
int8_t wgt_mem[WGT_BUFF_DEPTH][VTA_BLOCK_OUT * VTA_BLOCK_IN] = {};// weight SRAM.  Shared with LoadModule.
int8_t out_mem[ACC_BUFF_DEPTH][VTA_BLOCK_OUT] = {};//output SRAM.  Shared with StoreModule.


uint32_t dram_uops[UOP_BUFF_DEPTH] = {};//simulated DRAM holding the micro-op program.
int32_t dram_biases[ACC_BUFF_DEPTH][VTA_BLOCK_OUT] = {};// simulated DRAM holding the bias tensor.

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
    // Extract the name of the current instruction to determine which operation to perform.
    std::string name = current->get_name();

    if (name == "FINISH") {
        // FINISH instruction: Indicates the end of a layer's execution.
        // No computation is needed here; the finalize_instruction() method handles the 
        // printing and cleanup for this specific instruction.
    }
    else if (name == "LOAD UOP") {
        // LOAD UOP: This instruction loads the micro-op (UOP) program from DRAM into the on-chip SRAM.
        // Micro-ops are the low-level instructions that tell GEMM/ALU which tiles to process.
        try {
            // Get the SRAM and DRAM base addresses from the instruction fields (stored as hex strings).
            std::string sram_str = current->get_sram();
            std::string dram_str = current->get_dram();
            
            // Convert the hex strings to integers. If the string is empty, default to 0.
            uint32_t sram_base = sram_str.empty() ? 0 : std::stoul(sram_str, nullptr, 16);
            uint32_t dram_base = dram_str.empty() ? 0 : std::stoul(dram_str, nullptr, 16);
            
            // Get the number of micro-ops to copy.
            uint32_t x_size = current->get_x_size();
            
            // Bounds check to prevent segmentation faults (0xC0000005).
            // We must ensure that the read from dram_uops and the write to uop_mem are both fully within bounds.
            if (x_size > 0 && sram_base + x_size <= UOP_BUFF_DEPTH && dram_base + x_size <= UOP_BUFF_DEPTH) {
                // Copy the micro-ops from the simulated DRAM array to the local uop_mem SRAM.
                std::memcpy(&uop_mem[sram_base], &dram_uops[dram_base], x_size * sizeof(uint32_t));
            }
        } catch (...) {
            // Catch block to prevent crashes if std::stoul fails (e.g., due to malformed CSV fields).
        }
    }
    else if (name == "LOAD ACC") {
        // LOAD ACC: This instruction pre-loads the accumulator SRAM with bias values from DRAM.
        // This is necessary because in VTA, biases are added to the accumulator before the GEMM starts.
        try {
            // Get the base addresses for SRAM and DRAM.
            std::string sram_str = current->get_sram();
            std::string dram_str = current->get_dram();
            uint32_t sram_idx = sram_str.empty() ? 0 : std::stoul(sram_str, nullptr, 16);
            uint32_t dram_idx = dram_str.empty() ? 0 : std::stoul(dram_str, nullptr, 16);
            
            // Emulate the 2D DMA transfer (load_pad_2d in vta.cc).
            // It loops over the 'y' dimension (height) of the tensor transfer.
            for (int y = 0; y < current->get_y_size(); y++) {
                // Bounds check to prevent segmentation faults (0xC0000005).
                if (sram_idx < ACC_BUFF_DEPTH && dram_idx < ACC_BUFF_DEPTH && current->get_x_size() > 0) {
                    // Copy one row (x_size elements) from DRAM to SRAM.
                    // Each element is an entire output tile (VTA_BLOCK_OUT * int32_t).
                    std::memcpy(&acc_mem[sram_idx][0],
                                &dram_biases[dram_idx][0],
                                current->get_x_size() * sizeof(int32_t) * VTA_BLOCK_OUT);
                }
                // Advance the pointers to the next row based on size and stride.
                sram_idx += current->get_x_size();
                dram_idx += current->get_stride();
            }
        } catch (...) {
             // Catch block to prevent crashes on malformed input.
        }
    }
    else if (name == "GEMM") {
        // GEMM: General Matrix Multiply. 
        // This is the core tensor operation: it multiplies input activations by weights and adds them to the accumulator.
        
        // These variables track the base index offset for the destination (acc), source (inp), and weights.
        // They are updated after every outer and inner loop iteration to move through the large tensor.
        int dst_offset_out = 0, src_offset_out = 0, wgt_offset_out = 0;
        
        // --- OUTER LOOP ---
        // Iterates 'iter_out' times. This loop handles the largest blocking of the tensor computation.
        for (int it_out = 0; it_out < current->get_outer_loop_iter(); it_out++) {
            
            // Initialize the inner loop offsets with the current outer loop offsets.
            int dst_offset_in = dst_offset_out;
            int src_offset_in = src_offset_out;
            int wgt_offset_in = wgt_offset_out;
            
            // --- INNER LOOP ---
            // Iterates 'iter_in' times. This loop handles the inner blocking.
            for (int it_in = 0; it_in < current->get_inner_loop_iter(); it_in++) {
                
                // --- MICRO-OP LOOP ---
                // Iterates through a slice of the micro-op program memory defined by range_0 and range_1.
                // Each micro-op tells the hardware exactly which 16x16 tiles to multiply.
                for (int upc = current->get_range_0(); upc < current->get_range_1(); upc++) {
                    
                    // Bounds check: Ensure the program counter 'upc' doesn't exceed the micro-op memory size.
                    if (upc >= UOP_BUFF_DEPTH || upc < 0) continue; 
                    
                    // Fetch the 32-bit micro-op instruction from SRAM.
                    uint32_t uop = uop_mem[upc];
                    
                    // Decode the 32-bit micro-op word into its three components:
                    // 1. Destination tile index (for acc_mem): bits [10:0].
                    int dst_idx = (int)((uop >> 0) & DST_MASK) + dst_offset_in;
                    // 2. Source tile index (for inp_mem): bits [21:11].
                    int src_idx = (int)((uop >> vta_config::UOP_DST_WIDTH) & SRC_MASK) + src_offset_in;
                    // 3. Weight tile index (for wgt_mem): bits [31:22].
                    int wgt_idx = (int)((uop >> (vta_config::UOP_DST_WIDTH + vta_config::UOP_SRC_WIDTH)) & WGT_MASK) + wgt_offset_in;
                    
                    // Strict bounds checking to prevent segmentation faults if the decoded indices are corrupted.
                    if (dst_idx >= ACC_BUFF_DEPTH || dst_idx < 0) continue; 
                    if (src_idx >= INP_BUFF_DEPTH || src_idx < 0) continue; 
                    if (wgt_idx >= WGT_BUFF_DEPTH || wgt_idx < 0) continue; 

                    // --- OUTPUT CHANNEL LOOP ---
                    // Iterates over the 16 output channels in the destination tile.
                    for (int oc = 0; oc < VTA_BLOCK_OUT; oc++) {
                        // Read the current value from the 32-bit accumulator SRAM.
                        int32_t accum = acc_mem[dst_idx][oc];
                        int32_t tmp = 0; // Temporary sum for the dot product.
                        
                        // --- INPUT CHANNEL LOOP (Dot Product) ---
                        // Iterates over the 16 input channels to compute the dot product for this output channel.
                        for (int ic = 0; ic < VTA_BLOCK_IN; ic++) {
                            // Read the 8-bit weight. The weight array is flattened, so we compute the 1D index: (oc * 16) + ic.
                            int8_t w = wgt_mem[wgt_idx][oc * VTA_BLOCK_IN + ic];
                            // Read the 8-bit input activation.
                            int8_t x = inp_mem[src_idx][ic];
                            
                            // Multiply the 8-bit values (cast to 32-bit to prevent overflow) and add to the temporary sum.
                            tmp += (int32_t)(x * w);
                        }
                        
                        // Add the computed dot product to the existing accumulator value.
                        accum += tmp;
                        
                        // Write the result back to the accumulator SRAM.
                        // If 'reset_out' is true, we discard the old accumulator value and start fresh.
                        acc_mem[dst_idx][oc] = current->get_reset_out() ? 0 : accum;
                        
                        // Re-quantize the 32-bit accumulator value down to an 8-bit output by keeping only the lowest 8 bits (& 0xFF).
                        // Write this 8-bit result to the output SRAM (which the Store module will later send to DRAM).
                        out_mem[dst_idx][oc] = (int8_t)(accum & 0xFF);
                    }
                }
                
                // Update the inner loop offsets by adding the inner step factors defined in the instruction.
                dst_offset_in += current->get_gemm_inner_loop_acc();
                src_offset_in += current->get_gemm_inner_loop_inp();
                wgt_offset_in += current->get_gemm_inner_loop_wgt();
            }
            // Update the outer loop offsets by adding the outer step factors defined in the instruction.
            dst_offset_out += current->get_gemm_outer_loop_acc();
            src_offset_out += current->get_gemm_outer_loop_inp();
            wgt_offset_out += current->get_gemm_outer_loop_wgt();
        }
    }
    else if (name != "NOP-COMPUTE-STAGE") { 
        // ALU: Arithmetic Logic Unit operations (MAX, MIN, ADD, SHR).
        // This handles element-wise operations on the accumulator tiles.
        // It skips NOP (No Operation), which just passes time.

        // Determine if this is an "immediate" operation (e.g., adding a constant value to a tensor).
        bool use_imm = (name.find("imm") != std::string::npos);
        // The simulator doesn't extract the immediate value from the CSV currently, so we default to 0.
        // This perfectly models the standard ReLU activation function: MAX(x, 0).
        int32_t imm = 0; 

        // Similar to GEMM, these track the base index offsets for the two operand tiles.
        int dst_offset_out = 0, src_offset_out = 0;

        // --- OUTER LOOP ---
        for (int it_out = 0; it_out < current->get_outer_loop_iter(); it_out++) {
            int dst_offset_in = dst_offset_out;
            int src_offset_in = src_offset_out;

            // --- INNER LOOP ---
            for (int it_in = 0; it_in < current->get_inner_loop_iter(); it_in++) {

                // --- MICRO-OP LOOP ---
                for (int upc = current->get_range_0(); upc < current->get_range_1(); upc++) {
                    // Bounds check for the micro-op program counter.
                    if (upc >= UOP_BUFF_DEPTH || upc < 0) continue; 

                    uint32_t uop = uop_mem[upc];

                    // Decode the micro-op. ALU operations only use two indices: destination and source.
                    // (They do not use weights).
                    int dst_idx = (int)((uop >> 0) & DST_MASK) + dst_offset_in;
                    int src_idx = (int)((uop >> vta_config::UOP_DST_WIDTH) & SRC_MASK) + src_offset_in;

                    // Strict bounds checking.
                    if (dst_idx >= ACC_BUFF_DEPTH || dst_idx < 0) continue; 
                    // Only check the source index if we are NOT using an immediate value.
                    if (!use_imm && (src_idx >= ACC_BUFF_DEPTH || src_idx < 0)) continue; 

                    // --- ELEMENT-WISE LOOP ---
                    // Iterates over the 16 elements in the 1D accumulator tile.
                    for (int oc = 0; oc < VTA_BLOCK_OUT; oc++) {

                        // Operand 0 is always the value currently in the destination accumulator.
                        int32_t src_0 = acc_mem[dst_idx][oc];

                        // Operand 1 is either the immediate value (0) OR the value from the source accumulator tile.
                        int32_t src_1 = use_imm ? imm : acc_mem[src_idx][oc];
                        int32_t result;

                        // Perform the operation based on the instruction name.
                        if      (name.find("max") != std::string::npos) result = std::max(src_0, src_1); // e.g., ReLU
                        else if (name.find("min") != std::string::npos) result = std::min(src_0, src_1); // Clamping
                        else if (name.find("add") != std::string::npos) result = src_0 + src_1;          // Residual connections
                        else                                            result = src_0 >> src_1;         // Shift Right (quantization)

                        // Write the computed result back to the 32-bit accumulator.
                        acc_mem[dst_idx][oc] = result;

                        // Truncate the 32-bit result to an 8-bit integer and write it to the output SRAM for the Store module.
                        out_mem[dst_idx][oc] = (int8_t)(result & 0xFF);
                    }
                }
                // Update the inner loop offsets. 
                // NOTE: We use the `get_gemm_*` getters here as a workaround for a known CSV decoder bug 
                // where the ALU iteration counts were incorrectly saved in the GEMM fields.
                dst_offset_in += current->get_gemm_inner_loop_acc();
                src_offset_in += current->get_gemm_inner_loop_inp();
            }
            // Update the outer loop offsets.
            dst_offset_out += current->get_gemm_outer_loop_acc();
            src_offset_out += current->get_gemm_outer_loop_inp();
        }
    }

    // After all computation is complete (or instantly for NOP), we notify the SystemC scheduler 
    // that this module has finished its work and consumed simulated time.
    // The latency() function calculates how many nanoseconds this operation took based on the hardware model.
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
