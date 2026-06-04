cmake_minimum_required(VERSION 3.15)
project(MySystemCProject VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_DIR "${CMAKE_SOURCE_DIR}")
add_compile_definitions(SOURCE_DIR="${SOURCE_DIR}")

find_package(SystemCLanguage CONFIG REQUIRED) 
find_package(cppzmq CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(nlohmann_json_schema_validator CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)

add_executable(my_systemc 
    main.cpp
    src/utilities/utilities.cpp
    src/config/config.cpp
    src/config/config_json.cpp
    src/instruction/instruction.cpp
    src/instruction/parser.cpp
    src/type/type.cpp
    src/communication/communication.cpp
    src/simulation/fetcher.cpp
    src/simulation/module.cpp
    src/simulation/load_module.cpp
    src/simulation/compute_module.cpp
    src/simulation/store_module.cpp
    src/simulation/queue.cpp
    src/simulation/vta.cpp
    src/simulation/arm.cpp
)

# Include your headers
target_include_directories(my_systemc PRIVATE include)
target_include_directories(my_systemc PRIVATE include/config)
target_include_directories(my_systemc PRIVATE include/utilities)
target_include_directories(my_systemc PRIVATE include/type)
target_include_directories(my_systemc PRIVATE include/communication)
target_include_directories(my_systemc PRIVATE include/simulation)
target_include_directories(my_systemc PRIVATE include/instruction)

target_link_libraries(my_systemc 
    PRIVATE 
        SystemC::systemc 
        libzmq
        cppzmq-static
        nlohmann_json::nlohmann_json
        nlohmann_json_schema_validator::validator
        magic_enum::magic_enum
)