///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// License Agreement ///////////////////////////////////////////////
// Module: Time-Triggered Schedule Generator for VTA (TTVTA-Simulator)
// file: instruction.cpp
// Developer: Yosab Bebawy
// Date: 01.05.2023
// Contact data: yosab.bebawy@uni-siegen.de
// distribution Rights: only reserved for Yosab Bebawy (@Bebawy)
// Copyrights © reserved for Mr. Bebawy
// License: This program is NOT free software.
//          - You can NOT redistribute it and/or modify it without the agreement of Mr. Bebawy.
//          - This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
//            without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
////////////////////////////////// End of License Agreement ///////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
#include "utilities.h"
#include "instruction.h"
#include "vta_config.h"

// Encoding
std::map<std::string, vta_config::Opcode> Instruction::instruction_type = {

    {"LOAD ACC", vta_config::Opcode::LOAD},
    {"LOAD UOP", vta_config::Opcode::LOAD},
    {"LOAD INP", vta_config::Opcode::LOAD},
    {"LOAD WGT", vta_config::Opcode::LOAD},
    {"NOP-MEMORY-STAGE", vta_config::Opcode::LOAD},
   
    {"GEMM", vta_config::Opcode::GEMM},
    {"NOP-COMPUTE-STAGE", vta_config::Opcode::GEMM},
    {"ALU - add", vta_config::Opcode::ALU},
    {"ALU - add imm",vta_config::Opcode::ALU},
    {"ALU - max imm", vta_config::Opcode::ALU},
    {"ALU - min imm", vta_config::Opcode::ALU},
    {"ALU - shr", vta_config::Opcode::ALU},

    {"FINISH", vta_config::Opcode::FINISH},

    {"NOP-STORE-STAGE", vta_config::Opcode::STORE},
    {"STORE", vta_config::Opcode::STORE}
};

std::map<std::string, vta_config::MemoryID> Instruction::memory_id = {

    {"LOAD ACC", vta_config::MemoryID::ACC},
    {"LOAD UOP", vta_config::MemoryID::UOP},
    {"LOAD INP", vta_config::MemoryID::INP},
    {"LOAD WGT", vta_config::MemoryID::WGT},
    {"NOP-MEMORY-STAGE", vta_config::MemoryID::UOP},

    {"NOP-STORE-STAGE", vta_config::MemoryID::UOP},
    {"STORE", vta_config::MemoryID::OUT}
};

std::map<std::string, vta_config::AluOpcode> Instruction::alu_opcode = {

    {"ALU - add", vta_config::AluOpcode::ADD},
    {"ALU - add imm",vta_config::AluOpcode::ADD},
    {"ALU - max imm", vta_config::AluOpcode::MAX},
    {"ALU - min imm", vta_config::AluOpcode::MIN},
    {"ALU - mul", vta_config::AluOpcode::MUL},
    {"ALU - shr", vta_config::AluOpcode::SHR}
};

// Decoding
std::map<vta_config::Opcode, std::string> opcode_2_type = {
    {vta_config::Opcode::LOAD, "LOAD"},

    {vta_config::Opcode::STORE, "STORE"},
   
    {vta_config::Opcode::GEMM, "GEMM"},

    {vta_config::Opcode::FINISH, "FINISH"},
    
    {vta_config::Opcode::ALU, "ALU"}
};

std::map<vta_config::MemoryID, std::string> memory_id_2_name = {

    {vta_config::MemoryID::ACC, "ACC"},
    {vta_config::MemoryID::UOP, "UOP"},
    {vta_config::MemoryID::INP, "INP"},
    {vta_config::MemoryID::WGT, "WGT"},
    {vta_config::MemoryID::OUT, "STORE"}
};

std::map<vta_config::AluOpcode, std::string> alu_opcode_2_name = {

    {vta_config::AluOpcode::ADD, "ALU - add"},
    {vta_config::AluOpcode::ADD, "ALU - add imm"},
    {vta_config::AluOpcode::MAX, "ALU - max imm"},
    {vta_config::AluOpcode::MIN, "ALU - min imm"},
    {vta_config::AluOpcode::MUL, "ALU - mul"},
    {vta_config::AluOpcode::SHR, "ALU - shr"}
};


Instruction::Instruction() {}

Instruction::Instruction(const std::tuple<sc_int<64>, sc_int<64>>& data) { 
    
    this->pc = 0;
    
    sc_int<64> part_1 = std::get<0>(data);
    sc_int<64> part_2 = std::get<1>(data);

    int left = 63;

    // opcode
    auto opcode = vta_config::Opcode(part_1.range(left, left - vta_config::OPCODE_WIDTH + 1).to_uint());
    left -= vta_config::OPCODE_WIDTH;
    auto type = opcode_2_type[opcode];

    // dependencies;
    this->pop_prev = part_1[left--];
    this->pop_next = part_1[left--];
    this->push_prev = part_1[left--];
    this->push_next = part_1[left--];

    if (opcode == vta_config::Opcode::LOAD || opcode == vta_config::Opcode::STORE) {

        auto memory_id = vta_config::MemoryID(part_1.range(left, left - vta_config::MEMORY_TYPE_WIDTH + 1).to_uint());
        left -= vta_config::MEMORY_TYPE_WIDTH;
        this->name = concatStringsModern(type, " ", memory_id_2_name[memory_id]);

        auto sram_address_hex = part_1.range(left, left - vta_config::SRAM_BASE_WIDTH + 1).to_uint();
        left -= vta_config::SRAM_BASE_WIDTH;
        this->sram = int_to_hex(sram_address_hex); 
        
        auto dram_address_hex = part_1.range(left, left - vta_config::DRAM_BASE_WIDTH + 1).to_uint();
        left -= vta_config::DRAM_BASE_WIDTH;
        this->dram =int_to_hex(dram_address_hex);

        left = 63;

        this->y_size = part_2.range(left, left - vta_config::Y_SIZE_WIDTH + 1).to_uint();
        left -= vta_config::Y_SIZE_WIDTH;

        this->x_size = part_2.range(left, left - vta_config::X_SIZE_WIDTH + 1).to_uint();
        left -= vta_config::X_SIZE_WIDTH;

        // this->pc = part_2.range(left, left - vta_config::X_STRIDE_WIDTH + 1).to_uint();
        this->stride = part_2.range(left, left - vta_config::X_STRIDE_WIDTH + 1).to_uint();
        left -= vta_config::X_STRIDE_WIDTH;

        this->y0_pad = part_2.range(left, left - vta_config::Y_PAD_0_WIDTH + 1).to_uint();
        left -= vta_config::Y_PAD_0_WIDTH;

        this->y1_pad = part_2.range(left, left - vta_config::Y_PAD_1_WIDTH + 1).to_uint();
        left -= vta_config::Y_PAD_1_WIDTH;

        this->x0_pad = part_2.range(left, left - vta_config::X_PAD_0_WIDTH + 1).to_uint();
        left -= vta_config::X_PAD_0_WIDTH;

        this->x1_pad = part_2.range(left, left - vta_config::X_PAD_1_WIDTH + 1).to_uint();
        left -= vta_config::X_PAD_1_WIDTH;

    } 
    else if (opcode == vta_config::Opcode::GEMM || opcode == vta_config::Opcode::FINISH) {

        this->name = type;

        this->reset_out = part_1[left--];

        this->range_0 = part_1.range(left, left - vta_config::UOP_BGN_WIDTH + 1).to_uint();
        left -= vta_config::UOP_BGN_WIDTH;

        this->range_1 = part_1.range(left, left - vta_config::UOP_END_WIDTH + 1).to_uint();
        left -= vta_config::UOP_END_WIDTH;

        this->gemm_outer_loop_iter = part_1.range(left, left - vta_config::ITER_OUT_WIDTH + 1).to_uint();
        left -= vta_config::ITER_OUT_WIDTH;

        this->gemm_inner_loop_iter = part_1.range(left, left - vta_config::ITER_IN_WIDTH + 1).to_uint();
        left -= vta_config::ITER_IN_WIDTH;
            
        left = 63;

        // this->pc = part_2.range(left, left - vta_config::DST_FAC_OUT_WIDTH + 1).to_uint();
        this->gemm_outer_loop_acc = part_2.range(left, left - vta_config::DST_FAC_OUT_WIDTH + 1).to_uint();
        left -= vta_config::DST_FAC_OUT_WIDTH;

        this->gemm_inner_loop_acc = part_2.range(left, left - vta_config::DST_FAC_IN_WIDTH + 1).to_uint();
        left -= vta_config::DST_FAC_IN_WIDTH;

        this->gemm_outer_loop_inp = part_2.range(left, left - vta_config::GSRC_FAC_OUT_WIDTH + 1).to_uint();
        left -= vta_config::GSRC_FAC_OUT_WIDTH;

        this->gemm_inner_loop_inp = part_2.range(left, left - vta_config::GSRC_FAC_IN_WIDTH + 1).to_uint();
        left -= vta_config::GSRC_FAC_IN_WIDTH;

        this->gemm_outer_loop_wgt = part_2.range(left, left - vta_config::WGT_FAC_OUT_WIDTH + 1).to_uint();
        left -= vta_config::WGT_FAC_OUT_WIDTH;

        this->gemm_inner_loop_wgt = part_2.range(left, left - vta_config::WGT_FAC_IN_WIDTH + 1).to_uint();
        left -= vta_config::WGT_FAC_IN_WIDTH;

    }
    else if (opcode == vta_config::Opcode::ALU) {

        this->reset_out = part_1[left--];

        this->range_0 = part_1.range(left, left - vta_config::UOP_BGN_WIDTH + 1).to_uint();
        left -= vta_config::UOP_BGN_WIDTH;

        this->range_1 = part_1.range(left, left - vta_config::UOP_END_WIDTH + 1).to_uint();
        left -= vta_config::UOP_END_WIDTH;

        this->gemm_outer_loop_iter = part_1.range(left, left - vta_config::ITER_OUT_WIDTH + 1).to_uint();
        left -= vta_config::ITER_OUT_WIDTH;

        this->gemm_inner_loop_iter = part_1.range(left, left - vta_config::ITER_IN_WIDTH + 1).to_uint();
        left -= vta_config::ITER_IN_WIDTH;
            
        left = 63;

        // this->pc = part_2.range(left, left - vta_config::DST_FAC_OUT_WIDTH + 1).to_uint();
        this->alu_outer_loop_dst = part_2.range(left, left - vta_config::DST_FAC_OUT_WIDTH + 1).to_uint();
        left -= vta_config::DST_FAC_OUT_WIDTH;

        this->alu_inner_loop_dst = part_2.range(left, left - vta_config::DST_FAC_IN_WIDTH + 1).to_uint();
        left -= vta_config::DST_FAC_IN_WIDTH;

        this->alu_outer_loop_src = part_2.range(left, left - vta_config::ASRC_FAC_OUT_WIDTH + 1).to_uint();
        left -= vta_config::ASRC_FAC_OUT_WIDTH;

        this->alu_inner_loop_src = part_2.range(left, left - vta_config::ASRC_FAC_IN_WIDTH + 1).to_uint();
        left -= vta_config::ASRC_FAC_IN_WIDTH;
        
        auto alu_opcode = vta_config::AluOpcode(part_2.range(left, left - vta_config::ALU_OPCODE_WIDTH + 1).to_uint());
        left -= vta_config::ALU_OPCODE_WIDTH;
        this->name = alu_opcode_2_name[alu_opcode];        
    }
}

Instruction::Instruction(std::string &layer,
                int &pc,
                std::string &name,
                std::string moduleName,
                int &pop_prev,
                int &pop_next,
                int &push_prev,
                int &push_next,
                int l2g_queue,
                int g2l_queue,
                int s2g_queue,
                int g2s_queue,
                long long &gtb_start_time,
                long long &gtb_end_time,
                int &num_execution_ticks,
                int &dependant_on_pc,
                std::string &dram,
                std::string &sram,
                int &y_size,
                int &x_size,
                int &stride,
                int &y0_pad,
                int &y1_pad,
                int &x0_pad,
                int &x1_pad,
                int &source1_pipeline,
                int &source2_pipeline,
                int &destination1_pipeline,
                int &destination2_pipeline,
                int range_0,
                int range_1,
                int reset_out,
                int gemm_outer_loop_iter,
                int gemm_outer_loop_wgt,
                int gemm_outer_loop_inp,
                int gemm_outer_loop_acc,
                int gemm_inner_loop_iter,
                int gemm_inner_loop_wgt,
                int gemm_inner_loop_inp,
                int gemm_inner_loop_acc,
                int alu_outer_loop_iter,
                int alu_outer_loop_dst,
                int alu_outer_loop_src,
                int alu_inner_loop_iter,
                int alu_inner_loop_dst,
                int alu_inner_loop_src,
                int &cache)
{
    this->layer = layer;
    this->pc = pc;
    this->name = name;
    this->moduleName = moduleName;
    this->pop_prev = pop_prev;
    this->pop_next = pop_next;
    this->push_prev = push_prev;
    this->push_next = push_next;
    this->l2g_queue = l2g_queue;
    this->g2l_queue = g2l_queue;
    this->s2g_queue = s2g_queue;
    this->g2s_queue = g2s_queue;
    this->gtb_start_time = gtb_start_time;
    this->gtb_end_time = gtb_end_time;
    this->num_execution_ticks = num_execution_ticks;
    this->dependant_on_pc = dependant_on_pc;
    this->dram = dram;
    this->sram = sram;
    this->y_size = y_size;
    this->x_size = x_size;
    this->stride = stride;
    this->y0_pad = y0_pad;
    this->y1_pad = y1_pad;
    this->x0_pad = x0_pad;
    this->x1_pad = x1_pad;
    this->source1_pipeline = source1_pipeline;
    this->source2_pipeline = source2_pipeline;
    this->destination1_pipeline = destination1_pipeline;
    this->destination2_pipeline = destination2_pipeline;
    this->range_0 = range_0;
    this->range_1 = range_1;
    this->reset_out = reset_out;
    this->gemm_outer_loop_iter = gemm_outer_loop_iter;
    this->gemm_outer_loop_wgt = gemm_outer_loop_wgt;
    this->gemm_outer_loop_inp = gemm_outer_loop_inp;
    this->gemm_outer_loop_acc = gemm_outer_loop_acc;
    this->gemm_inner_loop_iter = gemm_inner_loop_iter;
    this->gemm_inner_loop_wgt = gemm_inner_loop_wgt;
    this->gemm_inner_loop_inp = gemm_inner_loop_inp;
    this->gemm_inner_loop_acc = gemm_inner_loop_acc;
    this->alu_outer_loop_iter = alu_outer_loop_iter;
    this->alu_outer_loop_dst = alu_outer_loop_dst;
    this->alu_outer_loop_src = alu_outer_loop_src;
    this->alu_inner_loop_iter = alu_inner_loop_iter;
    this->alu_inner_loop_dst = alu_inner_loop_dst;
    this->alu_inner_loop_src = alu_inner_loop_src;
    this->cache = cache;

    if (moduleName == "LOAD") {
        this->type = InstrType::LOAD;
    } else if (moduleName == "COMPUTE") {
        this->type = InstrType::COMPUTE;
    } else {
        this->type = InstrType::STORE;
    }
}

std::string Instruction::get_layer() const { return layer; }

int Instruction::get_pc() const { return pc; }


std::string Instruction::get_name() const { return name; }

void Instruction::set_name(std::string name) { this->name = name; }


std::string Instruction::get_module_name() const { return moduleName; }

void Instruction::set_module_name(std::string moduleName) { this->moduleName = moduleName; }


InstrType Instruction::get_type() const { return type; }

void Instruction::set_pop_prev(int pop_prev) { this->pop_prev = pop_prev; }

void Instruction::set_pop_next(int pop_next) { this->pop_next = pop_next; }

void Instruction::set_push_prev(int push_prev) { this->push_prev = push_prev; }

void Instruction::set_push_next(int push_next) { this->push_next = push_next; }

bool Instruction::get_pop_prev() const { 
    return pop_prev; 
}

bool Instruction::get_pop_next() const { return pop_next; }

bool Instruction::get_push_prev() const { return push_prev; }

bool Instruction::get_push_next() const { return push_next; }

int Instruction::get_l2g_queue() const { return l2g_queue; }

int Instruction::get_g2l_queue() const { return g2l_queue; }

int Instruction::get_s2g_queue() const { return s2g_queue; }

int Instruction::get_g2s_queue() const { return g2s_queue; }

std::string Instruction::get_dram() const { return dram; }

std::string Instruction::get_sram() const { return sram; }

long long Instruction::get_gtb_start_time() const { return gtb_start_time; }

long long Instruction::get_gtb_end_time() const { return gtb_end_time; }

int Instruction::get_num_execution_ticks() const { return num_execution_ticks; }

int Instruction::get_dependant_on_pc() const { return dependant_on_pc; }

int Instruction::get_y_size() const { return y_size; }

int Instruction::get_x_size() const { return x_size; }

int Instruction::get_stride() const { return stride; }

int Instruction::get_y0_pad() const { return y0_pad; }

int Instruction::get_y1_pad() const { return y1_pad; }

int Instruction::get_x0_pad() const { return x0_pad; }

int Instruction::get_x1_pad() const { return x1_pad; }

int Instruction::get_range_0() const { return range_0; }

int Instruction::get_range_1() const { return range_1; }

bool Instruction::get_reset_out() const { return reset_out; }

void Instruction::set_outer_loop_iter(int val) { this->outer_loop_iter = val; }

void Instruction::set_inner_loop_iter(int val) { this->inner_loop_iter = val; }

int Instruction::get_outer_loop_iter() const { 
    if (name == "LOAD UOP" || name == "LOAD ACC")
    {
        
    }
    else if (name == "FINISH" || name == "NOP-COMPUTE-STAGE")
    {
        
    }
    else if (name == "GEMM")
    {
        return gemm_outer_loop_iter;
    }
    else
    {
        return alu_outer_loop_iter;
    }

    return outer_loop_iter; 
}

int Instruction::get_inner_loop_iter() const { 
    if (name == "LOAD UOP" || name == "LOAD ACC")
    {
        
    }
    else if (name == "FINISH" || name == "NOP-COMPUTE-STAGE")
    {
        
    }
    else if (name == "GEMM")
    {
        return gemm_inner_loop_iter;
    }
    else
    {
        return alu_inner_loop_iter;
    }

    return inner_loop_iter; 
}

long long Instruction::get_started_at_gtb() const { return started_at_gtb; }

long long Instruction::get_completed_at_gtb() const { return completed_at_gtb; }

void Instruction::push_started_at_gtb(long long val) { this->started_at_gtb = val; }

void Instruction::push_completed_at_gtb(long long val) { this->completed_at_gtb = val; }

int Instruction::get_source1_pipeline() const { return source1_pipeline; }

int Instruction::get_source2_pipeline() const { return source2_pipeline; }

int Instruction::get_destination1_pipeline() const { return destination1_pipeline; }

int Instruction::get_destination2_pipeline() const { return destination2_pipeline; }

int Instruction::get_current_pipeline() const { return current_pipeline; }

void Instruction::set_source1_pipeline(int val) { this->source1_pipeline = val; }

void Instruction::set_source2_pipeline(int val) { this->source2_pipeline = val; }

void Instruction::set_destination1_pipeline(int val) { this->destination1_pipeline = val; }

void Instruction::set_destination2_pipeline(int val) { this->destination2_pipeline = val; }

void Instruction::set_current_pipeline(int val) { this->current_pipeline = val; }

int Instruction::get_gemm_outer_loop_iter() const { return gemm_outer_loop_iter; }

int Instruction::get_gemm_outer_loop_wgt() const { return gemm_outer_loop_wgt; }

int Instruction::get_gemm_outer_loop_inp() const { return gemm_outer_loop_inp; }

int Instruction::get_gemm_outer_loop_acc() const { return gemm_outer_loop_acc; }

int Instruction::get_gemm_inner_loop_iter() const { return gemm_inner_loop_iter; }

int Instruction::get_gemm_inner_loop_wgt() const { return gemm_inner_loop_wgt; }

int Instruction::get_gemm_inner_loop_inp() const { return gemm_inner_loop_inp; }

int Instruction::get_gemm_inner_loop_acc() const { return gemm_inner_loop_acc; }

int Instruction::get_alu_outer_loop_iter() const { return alu_outer_loop_iter; }

int Instruction::get_alu_outer_loop_dst() const { return alu_outer_loop_dst; }

int Instruction::get_alu_outer_loop_src() const { return alu_outer_loop_src; }

int Instruction::get_alu_inner_loop_iter() const { return alu_inner_loop_iter; }

int Instruction::get_alu_inner_loop_dst() const { return alu_inner_loop_dst; }

int Instruction::get_alu_inner_loop_src() const { return alu_inner_loop_src; }

int Instruction::get_cache() const { return cache; }

Instruction::~Instruction() {}

std::tuple<sc_int<64>, sc_int<64>> Instruction::encode() {
    sc_int<64> part_1 = 0;
    sc_int<64> part_2 = 0;

    int left = 63;

    // opcode
    auto opcode = Instruction::instruction_type[this->name];
    part_1.range(left, left - vta_config::OPCODE_WIDTH + 1) = static_cast<uint8_t>(opcode);
    left -= vta_config::OPCODE_WIDTH;

    // dependencies;
    part_1[left--] = this->pop_prev;
    part_1[left--] = this->pop_next;
    part_1[left--] = this->push_prev;
    part_1[left--] = this->push_next;

    if (opcode == vta_config::Opcode::LOAD || opcode == vta_config::Opcode::STORE) {

        auto memory_id = Instruction::memory_id[this->name];
        part_1.range(left, left - vta_config::MEMORY_TYPE_WIDTH + 1) = static_cast<uint8_t>(memory_id);
        left -= vta_config::MEMORY_TYPE_WIDTH;

        auto sram_address = std::stoi(this->get_sram(), nullptr, 16);
        part_1.range(left, left - vta_config::SRAM_BASE_WIDTH + 1) = sram_address;
        left -= vta_config::SRAM_BASE_WIDTH;

        auto dram_address = std::stoi(this->get_dram(), nullptr, 16);
        part_1.range(left, left - vta_config::DRAM_BASE_WIDTH + 1) = dram_address;
        left -= vta_config::DRAM_BASE_WIDTH;

        left = 63;

        part_2.range(left, left - vta_config::Y_SIZE_WIDTH + 1) = this->get_y_size();
        left -= vta_config::Y_SIZE_WIDTH;

        part_2.range(left, left - vta_config::X_SIZE_WIDTH + 1) = this->get_x_size();
        left -= vta_config::X_SIZE_WIDTH;
        
        part_2.range(left, left - vta_config::X_STRIDE_WIDTH + 1) = this->get_stride(); //////////////////
        left -= vta_config::X_STRIDE_WIDTH;

        part_2.range(left, left - vta_config::Y_PAD_0_WIDTH + 1) = this->get_y0_pad();
        left -= vta_config::Y_PAD_0_WIDTH;

        part_2.range(left, left - vta_config::Y_PAD_1_WIDTH + 1) = this->get_y1_pad();
        left -= vta_config::Y_PAD_1_WIDTH;

        part_2.range(left, left - vta_config::X_PAD_0_WIDTH + 1) = this->get_x0_pad();
        left -= vta_config::X_PAD_0_WIDTH;

        part_2.range(left, left - vta_config::X_PAD_1_WIDTH + 1) = this->get_x1_pad();
        left -= vta_config::X_PAD_1_WIDTH;

    } else if (opcode == vta_config::Opcode::GEMM) {

        part_1[left--] = this->get_reset_out();

        part_1.range(left, left - vta_config::UOP_BGN_WIDTH + 1) = this->get_range_0();
        left -= vta_config::UOP_BGN_WIDTH;

        part_1.range(left, left - vta_config::UOP_END_WIDTH + 1) = this->get_range_1();
        left -= vta_config::UOP_END_WIDTH;

        part_1.range(left, left - vta_config::ITER_OUT_WIDTH + 1) = this->get_gemm_outer_loop_iter();
        left -= vta_config::ITER_OUT_WIDTH;

        part_1.range(left, left - vta_config::ITER_IN_WIDTH + 1) = this->get_gemm_inner_loop_iter();
        left -= vta_config::ITER_IN_WIDTH;
            
        left = 63;

        part_2.range(left, left - vta_config::DST_FAC_OUT_WIDTH + 1) = this->get_gemm_outer_loop_acc(); /////////// 
        left -= vta_config::DST_FAC_OUT_WIDTH;

        part_2.range(left, left - vta_config::DST_FAC_IN_WIDTH + 1) = this->get_gemm_inner_loop_acc();
        left -= vta_config::DST_FAC_IN_WIDTH;

        part_2.range(left, left - vta_config::GSRC_FAC_OUT_WIDTH + 1) = this->get_gemm_outer_loop_inp();
        left -= vta_config::GSRC_FAC_OUT_WIDTH;

        part_2.range(left, left - vta_config::GSRC_FAC_IN_WIDTH + 1) = this->get_gemm_inner_loop_inp();
        left -= vta_config::GSRC_FAC_IN_WIDTH;

        part_2.range(left, left - vta_config::WGT_FAC_OUT_WIDTH + 1) = this->get_gemm_outer_loop_wgt();
        left -= vta_config::WGT_FAC_OUT_WIDTH;

        part_2.range(left, left - vta_config::WGT_FAC_IN_WIDTH + 1) = this->get_gemm_inner_loop_wgt();
        left -= vta_config::WGT_FAC_IN_WIDTH;

    } else if (opcode == vta_config::Opcode::ALU) {

        part_1[left--] = this->get_reset_out();

        part_1.range(left, left - vta_config::UOP_BGN_WIDTH + 1) = this->get_range_0();
        left -= vta_config::UOP_BGN_WIDTH;

        part_1.range(left, left - vta_config::UOP_END_WIDTH + 1) = this->get_range_1();
        left -= vta_config::UOP_END_WIDTH;

        part_1.range(left, left - vta_config::ITER_OUT_WIDTH + 1) = this->get_alu_outer_loop_iter();
        left -= vta_config::ITER_OUT_WIDTH;

        part_1.range(left, left - vta_config::ITER_IN_WIDTH + 1) = this->get_alu_inner_loop_iter();
        left -= vta_config::ITER_IN_WIDTH;

        left = 63;

        part_2.range(left, left - vta_config::DST_FAC_OUT_WIDTH + 1) = this->get_alu_outer_loop_dst();  /////////// 
        left -= vta_config::DST_FAC_OUT_WIDTH;

        part_2.range(left, left - vta_config::DST_FAC_IN_WIDTH + 1) = this->get_alu_inner_loop_dst();
        left -= vta_config::DST_FAC_IN_WIDTH;

        part_2.range(left, left - vta_config::ASRC_FAC_OUT_WIDTH + 1) = this->get_alu_outer_loop_src();
        left -= vta_config::ASRC_FAC_OUT_WIDTH;

        part_2.range(left, left - vta_config::ASRC_FAC_IN_WIDTH + 1) = this->get_alu_inner_loop_src();
        left -= vta_config::ASRC_FAC_IN_WIDTH;
        
        auto alu_opcode = Instruction::alu_opcode[this->name];
        part_2.range(left, left - vta_config::ALU_OPCODE_WIDTH + 1) = static_cast<uint8_t>(alu_opcode);
        left -= vta_config::ALU_OPCODE_WIDTH;
        
    }

    return {part_1, part_2};
}
