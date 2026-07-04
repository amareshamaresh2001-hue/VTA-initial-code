#include "parser.h"

Parser::Parser(char *model_path)
{
    this->model_path = model_path;

    load_instructions_from_csv();
    switch (PlatformConfig::getInstance().granulatity)
    {
    case Granularity::layer:
        load_layers_instruction();
        break;
    case Granularity::block:
        laod_blocks_instructions();
        break;
    default:
        break;
    }
    build_encoded_splited_instructions();
}

Parser::~Parser() {}

int Parser::get_splits_count() const
{
    return this->keys.size();
}

void Parser::load_instructions_from_csv() {
    std::vector<std::string> content;
    std::string line, word;
    std::fstream file(model_path, std::ios::in);

    if (file.is_open())
    {
        while (getline(file, line))
        {
            line = concatStringsModern(line, " ");
            std::stringstream str(line);
            while (getline(str, word, ','))
            {
                content.push_back(word);
            }

            std::string layer = content[vta_config::LAYER_POS];
            int pc = std::stoi(content[vta_config::PC_POS]);
            std::string name = content[vta_config::INSTRUCTION_POS];
            std::string moduleName = content[vta_config::MODULE_POS];

            int pop_prev = to_int(content[vta_config::POP_PREV_POS]);
            int pop_next = to_int(content[vta_config::POP_NEXT_POS]);
            int push_prev = to_int(content[vta_config::PUSH_PREV_POS]);
            int push_next = to_int(content[vta_config::PUSH_NEXT_POS]);

            int l2g_queue = to_int(content[vta_config::L2G_QUEUE_POS]);
            int g2l_queue = to_int(content[vta_config::G2L_QUEUE_POS]);
            int s2g_queue = to_int(content[vta_config::S2G_QUEUE_POS]);
            int g2s_queue = to_int(content[vta_config::G2S_QUEUE_POS]);

            std::string dram = content[vta_config::DRAM_ADDRESS_POS] != "" ? content[vta_config::DRAM_ADDRESS_POS] : "0x00000000";
            std::string sram = content[vta_config::SRAM_ADDRESS_POS] != "" ? content[vta_config::SRAM_ADDRESS_POS] : "0x0000";

            long long gtb_start_time = to_int(content[vta_config::GTB_START_TIME_POS]);
            long long gtb_end_time = to_int(content[vta_config::GTB_END_TIME_POS]);

            int num_execution_ticks = to_int(content[vta_config::TOTAL_TIME_POS]);
            int dependant_on_pc = to_int(content[vta_config::DEPENDENT_ON_PC_POS]);

            int y_size = to_int(content[vta_config::Y_SIZE_POS]);
            int x_size = to_int(content[vta_config::X_SIZE_POS]);
            int stride = to_int(content[vta_config::STRIDE_POS]);

            int y0_pad = to_int(content[vta_config::Y0_PAD_POS]);
            int y1_pad = to_int(content[vta_config::Y1_PAD_POS]);
            int x0_pad = to_int(content[vta_config::X0_PAD_POS]);
            int x1_pad = to_int(content[vta_config::X1_PAD_POS]);

            int range_0 = to_int(content[vta_config::RANGE_0_POS]);
            int range_1 = to_int(content[vta_config::RANGE_1_POS]);

            int reset_out = to_int(content[vta_config::RESET_OUT_POS]);

            int source1_pipeline = 0;
            int source2_pipeline = 0;
            int destination1_pipeline = 0;
            int destination2_pipeline = 0;

            int gemm_outer_loop_iter = to_int(content[vta_config::GEMM_OUTER_ITER_POS]);
            int gemm_outer_loop_wgt = to_int(content[vta_config::GEMM_OUTER_WGT_POS]);
            int gemm_outer_loop_inp = to_int(content[vta_config::GEMM_OUTER_INP_POS]);
            int gemm_outer_loop_acc = to_int(content[vta_config::GEMM_OUTER_ACC_POS]);

            int gemm_inner_loop_iter = to_int(content[vta_config::GEMM_INNER_ITER_POS]);
            int gemm_inner_loop_wgt = to_int(content[vta_config::GEMM_INNER_WGT_POS]);
            int gemm_inner_loop_inp = to_int(content[vta_config::GEMM_INNER_INP_POS]);
            int gemm_inner_loop_acc = to_int(content[vta_config::GEMM_INNER_ACC_POS]);

            int alu_outer_loop_iter = to_int(content[vta_config::ALU_OUTER_ITER_POS]);
            int alu_outer_loop_dst = to_int(content[vta_config::ALU_OUTER_DST_POS]);
            int alu_outer_loop_src = to_int(content[vta_config::ALU_OUTER_SRC_POS]);

            int alu_inner_loop_iter = to_int(content[vta_config::ALU_INNER_ITER_POS]);
            int alu_inner_loop_dst = to_int(content[vta_config::ALU_INNER_DST_POS]);
            int alu_inner_loop_src = to_int(content[vta_config::ALU_INNER_SRC_POS]);

            int cache = vta_config::CACHE_POS < content.size() ? to_int(content[vta_config::CACHE_POS]) : 0;

            Instruction instruction = Instruction(
                layer,
                pc,
                name,
                moduleName,
                pop_prev,
                pop_next,
                push_prev,
                push_next,
                l2g_queue,
                g2l_queue,
                s2g_queue,
                g2s_queue,
                gtb_start_time,
                gtb_end_time,
                num_execution_ticks,
                dependant_on_pc,
                dram,
                sram,
                y_size,
                x_size,
                stride,
                y0_pad,
                y1_pad,
                x0_pad,
                x1_pad,
                source1_pipeline,
                source2_pipeline,
                destination1_pipeline,
                destination2_pipeline,
                range_0,
                range_1,
                reset_out,
                gemm_outer_loop_iter,
                gemm_outer_loop_wgt,
                gemm_outer_loop_inp,
                gemm_outer_loop_acc,
                gemm_inner_loop_iter,
                gemm_inner_loop_wgt,
                gemm_inner_loop_inp,
                gemm_inner_loop_acc,
                alu_outer_loop_iter,
                alu_outer_loop_dst,
                alu_outer_loop_src,
                alu_inner_loop_iter,
                alu_inner_loop_dst,
                alu_inner_loop_src,
                cache
            );

            all_instructions.push_back(instruction);
            content.clear();
        }
    }
}

void Parser::load_layers_instruction() {
    for (auto instruction: all_instructions) {
        std::string layer = instruction.get_layer();
        splited_instructions[layer].push_back(instruction);
    }
    for (int i = 0; i < static_cast<int>(this->splited_instructions.size()); i++) {
        keys.push_back(concatStringsModern("layer_", std::to_string(i)));
    }
}

void Parser::laod_blocks_instructions() {
    std::vector<std::string> layers;
    std::map<std::string, std::vector<Instruction>> layers_instructions;

    for (auto instruction: all_instructions) {
        std::string layer = instruction.get_layer();
        layers_instructions[layer].push_back(instruction);
    }
    for (int i = 0; i < static_cast<int>(layers_instructions.size()); i++) {
        layers.push_back(concatStringsModern("layer_", std::to_string(i)));
    }
    
    for (int layer_idx = 0; layer_idx < static_cast<int>(layers.size()); layer_idx++) {
        std::string current_layer = layers[layer_idx];
        int start_block_idx = this->keys.size();
        int block_idx = 0;
        bool block_strated = false;
        std::string block_name;
        bool add_prefix_instructions = true;
        bool add_postfix_instructions = false;
        std::vector<Instruction> prefix_instructions;
        std::vector<Instruction> postfix_instructions;
        for (auto instruction : layers_instructions[current_layer]) {
            std::string module_name = instruction.get_module_name();
            std::string name = instruction.get_name();
            if (module_name == "LOAD")  {
                if (name == "LOAD INP") {
                    if (!block_strated) {
                        postfix_instructions.clear();
                        add_prefix_instructions = false;
                        add_postfix_instructions = false;
                        block_strated = true;
                        block_name = concatStringsModern("block_", std::to_string(layer_idx), "_", std::to_string(block_idx));
                        this->keys.push_back(block_name);
                        block_idx++;
                    }
                }
                if (block_strated) {
                    this->splited_instructions[block_name].push_back(instruction);
                }
                if(add_prefix_instructions) {
                    prefix_instructions.push_back(instruction);
                }
                if(add_postfix_instructions) {
                    postfix_instructions.push_back(instruction);
                }
            }
            else if (module_name == "COMPUTE") {
                if (block_strated) {
                    this->splited_instructions[block_name].push_back(instruction);
                }
                if(add_prefix_instructions) {
                    prefix_instructions.push_back(instruction);
                }
                if(add_postfix_instructions) {
                    postfix_instructions.push_back(instruction);
                }
            }
            else if (module_name == "STORE") {
                if (name == "STORE") {
                    add_postfix_instructions = true;
                    block_strated = false;
                    this->splited_instructions[block_name].push_back(instruction);
                } else {
                    if (block_strated) {
                        this->splited_instructions[block_name].push_back(instruction);
                    }
                    if(add_prefix_instructions) {
                        prefix_instructions.push_back(instruction);
                    }
                    if(add_postfix_instructions) {
                        postfix_instructions.push_back(instruction);
                    }
                }
            }
        }
        for (int i = start_block_idx; i < static_cast<int>(this->keys.size()); i++) {
            for (int j = 0; j < static_cast<int>(prefix_instructions.size()); j++) {
                auto instruction = prefix_instructions[j];
                this->splited_instructions[this->keys[i]].insert(this->splited_instructions[this->keys[i]].begin() + j, instruction);
            }
            for (auto instruction : postfix_instructions) {
                this->splited_instructions[this->keys[i]].push_back(instruction);
            }
        }
        prefix_instructions.clear();
        postfix_instructions.clear();
    }
}

void Parser::build_encoded_splited_instructions()
{
    encoded_splited_instructions.clear();
    for (const auto& key : keys) {
        auto& encoded_vector = encoded_splited_instructions[key];
        const auto& split_instructions = splited_instructions[key];
        encoded_vector.reserve(split_instructions.size());
        for (auto instruction : split_instructions) {
            const auto encoded = instruction.encode();
            encoded_vector.push_back({instruction.get_type(), std::get<0>(encoded), std::get<1>(encoded)});
        }
    }
}

void Parser::check_memory_cache()
{
    for (auto key : this->keys) {
        for (auto instruction : this->splited_instructions[key]) {
            std::string inst_name = instruction.get_name();
            if (inst_name == "LOAD INP" || inst_name == "LOAD WGT" || inst_name == "STORE")
            {
                std::string dram = instruction.get_dram();
                int y = instruction.get_y_size();
                int x = instruction.get_x_size();
                int stride = instruction.get_stride();
                int word_size = 0;
                if (inst_name == "LOAD INP")
                {
                    word_size = 16;
                }
                else if (inst_name == "LOAD WGT")
                {
                    word_size = 256;
                }
                else if (inst_name == "STORE")
                {
                    word_size = 16;
                }

                if (unique_ddr_address_idx.find(dram) == unique_ddr_address_idx.end())
                {
                    unique_ddr_address.push_back(MemoryAddress(dram, x, y, stride, word_size));
                    unique_ddr_address_idx[dram] = unique_ddr_address.size() - 1;
                }
                unique_ddr_address[unique_ddr_address_idx[dram]].hit(key);
            }
        }
    }

    std::stable_sort(unique_ddr_address.begin(), unique_ddr_address.end(), [](const MemoryAddress &a, const MemoryAddress &b) { return a.nr_of_hits > b.nr_of_hits; });

    int tmp_cache_size = 0;
    float cache_size_after_unique = 0;
    float ddr_size_after_unique = 0;
    for (const auto &p : unique_ddr_address)
    {
        if (tmp_cache_size + p.get_size() <= PlatformConfig::getInstance().max_cache_mem_size)
        {
            to_be_cached_address.push_back(p);
            tmp_cache_size = tmp_cache_size + p.get_size();
        }
        ddr_size_after_unique = ddr_size_after_unique + p.get_size();
    }
    std::cout << "-------- TO BE Cached Addresses --------" << std::endl;
    for (const auto &p : to_be_cached_address)
    {
        cache_size_after_unique = cache_size_after_unique + p.get_size();
    }

    std::cout << "-->>DDR Size: " << ddr_size_after_unique / 1000 << " - " << " Cache Size " << cache_size_after_unique / 1000 << " KBytes" << "  " << to_be_cached_address.size() << '\n';
    std::cout << "-->>Cache/DDR ratio: " << cache_size_after_unique / ddr_size_after_unique * 100 << "%" << '\n';
}

void Parser::sort_instructions_to_pipelines()
{
    int nr_load_pipeline = PlatformConfig::getInstance().nr_load_pipeline;
    int nr_compute_pipeline = PlatformConfig::getInstance().nr_compute_pipeline;
    int nr_store_pipeline = PlatformConfig::getInstance().nr_store_pipeline;
    for (auto key : keys) {
        load_instructions[key] = new std::vector<Instruction>[nr_load_pipeline];
        compute_instructions[key] = new std::vector<Instruction>[nr_compute_pipeline];
        store_instructions[key] = new std::vector<Instruction>[nr_store_pipeline];

        int current_store_pl_counter = 1;
        int current_compute_pl_counter = 1;
        int current_load_pl_counter = 1;

        std::queue<int> load_to_compute_queue;
        std::queue<int> compute_to_load_queue;
        std::queue<int> store_to_compute_queue;
        std::queue<int> compute_to_store_queue;
        for (auto &instruction : splited_instructions[key]) {
            std::string module_name = instruction.get_module_name();
            std::string name = instruction.get_name();
            if (module_name == "STORE")
            {
                if (name == "STORE")
                {
                    if (instruction.get_pop_prev())
                    {
                        instruction.set_current_pipeline(current_store_pl_counter);
                        current_store_pl_counter++;
                        if (current_store_pl_counter > nr_store_pipeline)
                        {
                            current_store_pl_counter = 1;
                        }
                        instruction.set_source1_pipeline(compute_to_store_queue.front());
                        compute_to_store_queue.pop();
                    }

                    if (instruction.get_push_prev())
                    {
                        int destination1_pipeline = static_cast<int>(store_to_compute_queue.size()) + 1;
                        instruction.set_destination1_pipeline(destination1_pipeline);
                        store_to_compute_queue.push(instruction.get_destination1_pipeline());
                    }
                }
                if (name == "NOP-STORE-STAGE")
                {
                    if (instruction.get_pop_prev())
                    {
                        instruction.set_source1_pipeline(compute_to_store_queue.front());
                        compute_to_store_queue.pop();
                    }

                    if (instruction.get_push_prev())
                    {
                        instruction.set_current_pipeline(current_store_pl_counter);
                        current_store_pl_counter++;
                        if (current_store_pl_counter > nr_store_pipeline)
                        {
                            current_store_pl_counter = 1;
                        }
                        int destination1_pipeline = static_cast<int>(store_to_compute_queue.size()) + 1;
                        instruction.set_destination1_pipeline(destination1_pipeline);
                        store_to_compute_queue.push(instruction.get_destination1_pipeline());
                    }
                }
            }

            else if (module_name == "LOAD")
            {
                if (instruction.get_pop_next())
                {
                    instruction.set_source1_pipeline(compute_to_load_queue.front());
                    compute_to_load_queue.pop();
                }

                if (instruction.get_push_next())
                {
                    instruction.set_current_pipeline(current_load_pl_counter);
                    current_load_pl_counter++;
                    if (current_load_pl_counter > nr_load_pipeline)
                    {
                        current_load_pl_counter = 1;
                    }
                    int destination1_pipeline = static_cast<int>(load_to_compute_queue.size()) + 1;
                    instruction.set_destination1_pipeline(destination1_pipeline);
                    load_to_compute_queue.push(instruction.get_destination1_pipeline());
                }
                else
                {
                    instruction.set_current_pipeline(current_load_pl_counter);
                }
            }

            else if (module_name == "COMPUTE")
            {
                if (instruction.get_pop_prev())
                {
                    instruction.set_source1_pipeline(load_to_compute_queue.front());
                    load_to_compute_queue.pop();
                }

                if (instruction.get_pop_next())
                {
                    instruction.set_source2_pipeline(store_to_compute_queue.front());
                    store_to_compute_queue.pop();
                }

                if (instruction.get_push_prev())
                {
                    instruction.set_current_pipeline(current_compute_pl_counter);
                    current_compute_pl_counter++;
                    if (current_compute_pl_counter > nr_compute_pipeline)
                    {
                        current_compute_pl_counter = 1;
                    }
                    int destination1_pipeline = static_cast<int>(compute_to_load_queue.size()) + 1;
                    instruction.set_destination1_pipeline(destination1_pipeline);
                    compute_to_load_queue.push(instruction.get_destination1_pipeline());
                }
                else if (instruction.get_push_next())
                {
                    instruction.set_current_pipeline(current_compute_pl_counter);
                    current_compute_pl_counter++;
                    if (current_compute_pl_counter > nr_compute_pipeline)
                    {
                        current_compute_pl_counter = 1;
                    }
                    int destination2_pipeline = static_cast<int>(compute_to_store_queue.size()) + 1;
                    instruction.set_destination2_pipeline(destination2_pipeline);
                    compute_to_store_queue.push(instruction.get_destination2_pipeline());
                }
                else
                {
                    instruction.set_current_pipeline(current_compute_pl_counter);
                }
            }
            int pipeline = instruction.get_current_pipeline() - 1;
            if (module_type(name) == "LOAD")
            {
                load_instructions[key][pipeline].push_back(instruction);
            }
            else if (module_type(name) == "COMPUTE")
            {
                compute_instructions[key][pipeline].push_back(instruction);
            }
            else if (module_type(name) == "STORE")
            {
                store_instructions[key][pipeline].push_back(instruction);
            }
            else
            {
                std::cout << "ERROR!";
            }

            if (name == "FINISH")
            {
                current_store_pl_counter = 1;
                current_compute_pl_counter = 1;
                current_load_pl_counter = 1;
            }
        }
    }

    for (auto key : keys) {
        std::ofstream direct_to_pipeleine("/home/abdulmajeed/Documents/Projects/STT_VTA/data/direct_to_pipeline/" + key + ".txt");
        for (auto instruction : splited_instructions[key]) {
            direct_to_pipeleine << instruction.get_current_pipeline() << ", "
                                << instruction.get_source1_pipeline() << ", "
                                << instruction.get_source2_pipeline() << ", "
                                << instruction.get_destination1_pipeline() << ", "
                                << instruction.get_destination2_pipeline() << ", " << std::endl;
        }
        direct_to_pipeleine.close();
    }
}
