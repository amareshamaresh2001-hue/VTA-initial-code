///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// License Agreement ///////////////////////////////////////////////
// Module: Time-Triggered Schedule Generator for VTA (TTVTA-Simulator)
// file: instruction.h
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
#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <string>
#include <map>
#include <tuple>
#include <systemc.h>
#include "vta_config.h"


enum class InstrType : uint8_t { 
    LOAD = 0, 
    COMPUTE = 1, 
    STORE = 2
};


class Instruction
{
private:
    std::string layer = "DEFAULT";
    int pc = 0;
    std::string name = "DEFAULT";
    std::string moduleName;
    InstrType type;

    int pop_prev = 0;
    int pop_next = 0;
    int push_prev = 0;
    int push_next = 0;

    int l2g_queue = 0;
    int g2l_queue = 0;
    int s2g_queue = 0;
    int g2s_queue = 0;

    std::string dram = "0x00000000";
    std::string sram = "0x0000";

    long long gtb_start_time = 0;
    long long gtb_end_time = 0;

    int num_execution_ticks = 0;
    int dependant_on_pc = 0;

    int y_size = 0;
    int x_size = 0;
    int stride = 0;
    
    int y0_pad = 0;
    int y1_pad = 0;
    int x0_pad = 0;
    int x1_pad = 0;

    int range_0 = 0;
    int range_1 = 0;

    int reset_out = 0;

    int outer_loop_iter = 0;
    int inner_loop_iter = 0;

    long long started_at_gtb = 0;
    long long completed_at_gtb = 0;

    int source1_pipeline = 0;
    int source2_pipeline = 0;
    int destination1_pipeline = 0;
    int destination2_pipeline = 0;
    int current_pipeline = 0;

    int gemm_outer_loop_iter = -1;
    int gemm_outer_loop_wgt = -1;
    int gemm_outer_loop_inp = -1;
    int gemm_outer_loop_acc = -1;

    int gemm_inner_loop_iter = -1;
    int gemm_inner_loop_wgt = -1;
    int gemm_inner_loop_inp = -1;
    int gemm_inner_loop_acc = -1;

    int alu_outer_loop_iter = -1;
    int alu_outer_loop_dst = -1;
    int alu_outer_loop_src = -1;

    int alu_inner_loop_iter = -1;
    int alu_inner_loop_dst = -1;
    int alu_inner_loop_src = -1;

    int cache = 0;

public:

    static std::map<std::string, vta_config::Opcode> instruction_type;
    static std::map<std::string, vta_config::MemoryID> memory_id;
    static std::map<std::string, vta_config::AluOpcode> alu_opcode;
    
    Instruction();
    Instruction(const std::tuple<sc_int<64>, sc_int<64>>& data);
    Instruction(std::string &layer,
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
                int &cache);
                
    std::string get_layer() const;
    int get_pc() const;

    std::string get_name() const;
    void set_name(std::string name);

    std::string get_module_name() const;
    void set_module_name(std::string moduleName);

    InstrType get_type() const;
    void set_pop_prev(int pop_prev);
    void set_pop_next(int pop_next);
    void set_push_prev(int push_prev);
    void set_push_next(int push_next);
    bool get_pop_prev() const;
    bool get_pop_next() const;
    bool get_push_prev() const;
    bool get_push_next() const;
    int get_l2g_queue() const;
    int get_g2l_queue() const;
    int get_s2g_queue() const;
    int get_g2s_queue() const;
    std::string get_dram() const;
    std::string get_sram() const;
    long long get_gtb_start_time() const;
    long long get_gtb_end_time() const;
    int get_num_execution_ticks() const;
    int get_dependant_on_pc() const;
    int get_y_size() const;
    int get_x_size() const;
    int get_stride() const;
    int get_y0_pad() const;
    int get_y1_pad() const;
    int get_x0_pad() const;
    int get_x1_pad() const;
    int get_range_0() const;
    int get_range_1() const;
    bool get_reset_out() const;
    void set_outer_loop_iter(int val);
    void set_inner_loop_iter(int val);
    int get_outer_loop_iter() const;
    int get_inner_loop_iter() const;
    long long get_started_at_gtb() const;
    long long get_completed_at_gtb() const;
    void push_started_at_gtb(long long val);
    void push_completed_at_gtb(long long val);

    int get_source1_pipeline() const;
    int get_source2_pipeline() const;
    int get_destination1_pipeline() const;
    int get_destination2_pipeline() const;
    int get_current_pipeline() const;

    void set_source1_pipeline(int val);
    void set_source2_pipeline(int val);
    void set_destination1_pipeline(int val);
    void set_destination2_pipeline(int val);
    void set_current_pipeline(int val);

    int get_gemm_outer_loop_iter() const;
    int get_gemm_outer_loop_wgt() const;
    int get_gemm_outer_loop_inp() const;
    int get_gemm_outer_loop_acc() const;

    int get_gemm_inner_loop_iter() const;
    int get_gemm_inner_loop_wgt() const;
    int get_gemm_inner_loop_inp() const;
    int get_gemm_inner_loop_acc() const;

    int get_alu_outer_loop_iter() const;
    int get_alu_outer_loop_dst() const;
    int get_alu_outer_loop_src() const;

    int get_alu_inner_loop_iter() const;
    int get_alu_inner_loop_dst() const;
    int get_alu_inner_loop_src() const;
    
    int get_cache() const;

    ~Instruction();
    
    int get_opcode();
    std::tuple<sc_int<64>, sc_int<64>> encode();
};

#endif // INSTRUCTION_H