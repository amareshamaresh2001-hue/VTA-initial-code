#include <systemc.h>
#include <chrono>
#include <iostream>
#include <stdexcept>

#include "config.h"
#include "files_manager.h"
#include "parser.h"
#include "vta.h"

int sc_main(int, char*[]) {
    auto& config = PlatformConfig::getInstance();
    FilesManager files_manager(config.granulatity, const_cast<char*>(config.model_name.c_str()));
    Parser parser(files_manager.model_path);

    VTA vta("vta", parser.keys, parser.encoded_splited_instructions);
    sc_start();
    return 0;
}
