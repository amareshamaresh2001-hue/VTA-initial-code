///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// License Agreement ///////////////////////////////////////////////
// Module: Time-Triggered Schedule Generator for VTA (TTVTA-Simulator)
// file: parser.h
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
#ifndef PARSER_H
#define PARSER_H

#include "instruction.h"
#include "type.h"
#include "config.h"
#include "utilities.h"

#include <fstream>
#include <map>
#include <string>
#include <vector>

class Parser
{
public:
    char *model_path;

    std::vector<Instruction> all_instructions;
    std::vector<std::string> keys;
    std::map<std::string, std::vector<Instruction>> splited_instructions;
    std::map<std::string, std::vector<std::tuple<InstrType, sc_int<64>, sc_int<64>>>> encoded_splited_instructions;

    std::map<std::string, int> unique_ddr_address_idx;
    std::vector<MemoryAddress> unique_ddr_address;
    std::vector<MemoryAddress> to_be_cached_address;

    std::map<std::string, std::vector<Instruction> *> load_instructions;
    std::map<std::string, std::vector<Instruction> *> compute_instructions;
    std::map<std::string, std::vector<Instruction> *> store_instructions;

public:
    Parser(char *model_path);
    ~Parser();

    void load_instructions_from_csv();
    void load_layers_instruction();
    void laod_blocks_instructions();
    void build_encoded_splited_instructions();
    int get_splits_count() const;
    void check_memory_cache();
    void sort_instructions_to_pipelines();
};

#endif
