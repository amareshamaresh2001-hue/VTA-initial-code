///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// License Agreement ///////////////////////////////////////////////
// Module: Time-Triggered Schedule Generator for VTA (TTVTA-Simulator)
// file: config.h
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
#ifndef CONFIG_H
#define CONFIG_H

#include <fstream>
#include <string>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <magic_enum.hpp>
#include "utilities.h"
#include "type.h"


std::string module_type(std::string& instName);

struct ProcessorSwitchConnection
{
    int processor_id;
    int switch_id;
};

struct SwitchSwitchConnection
{
    int first_switch;
    int second_switch;
};

struct Topology
{
    std::vector<std::string> switches;
    std::vector<std::string> processors;
    std::vector<ProcessorSwitchConnection> processors_switches_connections;
    std::vector<SwitchSwitchConnection> switches_switches_connections;

    Topology() {

    }

    Topology(nlohmann::json topology_data) {
        switches = topology_data["switches"].get_to(switches);
        processors = topology_data["processors"].get_to(processors);
        for (auto connection_data :  topology_data["processors_switches_connections"]) {
            processors_switches_connections.push_back({connection_data["processor"], connection_data["switch"]});
        }
        for (auto connection_data :  topology_data["switches_switches_connections"]) {
            switches_switches_connections.push_back({connection_data["first_switch"], connection_data["second_switch"]});
        }
    }

    std::vector<std::string> find_path(std::string source, std::string dest) {
        std::vector<std::string> path;
        path.push_back(source);
        path.push_back("switch_1"); // switch
        path.push_back(dest);
        return path;
    }

    std::vector<std::string> switch_ports(std::string switch_name) {
        return processors;
    }

};

struct PlatformConfig {

private:
    PlatformConfig() {
        std::ifstream file(concatStringsModern(SOURCE_DIR, "/config/config.json"));
        if (file.is_open()) {
            nlohmann::json config;
            file >> config;

            model_name = config["model_name"];
            en_ddr_lock = config["en_ddr_lock"];
            en_cache = config["en_cache"];
            en_prefetch = config["en_prefetch"];
            max_cache_mem_size = config["max_cache_mem_size"];
            nr_load_pipeline = config["nr_load_pipeline"];
            nr_compute_pipeline = config["nr_compute_pipeline"];
            nr_store_pipeline = config["nr_store_pipeline"];
            load_cycles = config["load_cycles"];
            load_weigts_cycles = config["load_weigts_cycles"];
            store_cycles = config["store_cycles"];
            granulatity = magic_enum::enum_cast<Granularity>(std::string(config["granulatity"])).value();
        }
    }

    // Delete copy constructor and assignment operator
    PlatformConfig(const PlatformConfig&) = delete;
    PlatformConfig& operator=(const PlatformConfig&) = delete;

public:
    std::string model_name;
    bool en_ddr_lock;
    bool en_cache;
    bool en_prefetch;
    long long max_cache_mem_size;
    int nr_load_pipeline;
    int nr_compute_pipeline;
    int nr_store_pipeline;
    int load_cycles;
    int load_weigts_cycles;
    int store_cycles;
    Granularity granulatity;

    void apply_from_json(const nlohmann::json& patch);
    nlohmann::json to_json() const;

    static PlatformConfig& getInstance() {
        static PlatformConfig instance;  // Created only once (thread-safe in C++11+)
        return instance;
    }
};

#endif // CONFIG_H

/*
    "topology": {
        "switches": ["switch_1"],
        "processors": ["processor_1", "processor_2", "processor_3", "processor_4"],
        "processors_switches_connections": [
            {
                "processor": 0,
                "switch": 0
            },
            {
                "processor": 0,
                "switch": 1
            },
            {
                "processor": 0,
                "switch": 2
            },
            {
                "processor": 0,
                "switch": 3
            }
        ],
        "switches_switches_connections": []
    }

    */