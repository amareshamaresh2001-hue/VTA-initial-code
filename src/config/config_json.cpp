#include "config.h"

namespace {

std::uint64_t read_seed(const nlohmann::json& seed_json) {
    if (seed_json.is_number_unsigned()) {
        return seed_json.get<std::uint64_t>();
    }
    if (seed_json.is_number_integer()) {
        return static_cast<std::uint64_t>(seed_json.get<long long>());
    }
    return 0ULL;
}

} // namespace

void PlatformConfig::apply_from_json(const nlohmann::json& patch) {
    if (patch.contains("model_name")) model_name = patch["model_name"];
    if (patch.contains("en_ddr_lock")) en_ddr_lock = patch["en_ddr_lock"];
    if (patch.contains("en_cache")) en_cache = patch["en_cache"];
    if (patch.contains("en_prefetch")) en_prefetch = patch["en_prefetch"];
    if (patch.contains("max_cache_mem_size")) max_cache_mem_size = patch["max_cache_mem_size"];
    if (patch.contains("nr_load_pipeline")) nr_load_pipeline = patch["nr_load_pipeline"];
    if (patch.contains("nr_compute_pipeline")) nr_compute_pipeline = patch["nr_compute_pipeline"];
    if (patch.contains("nr_store_pipeline")) nr_store_pipeline = patch["nr_store_pipeline"];
    if (patch.contains("load_cycles")) load_cycles = patch["load_cycles"];
    if (patch.contains("load_weigts_cycles")) load_weigts_cycles = patch["load_weigts_cycles"];
    if (patch.contains("store_cycles")) store_cycles = patch["store_cycles"];
    if (patch.contains("granulatity")) {
        granulatity = magic_enum::enum_cast<Granularity>(std::string(patch["granulatity"])).value();
    }
}

nlohmann::json PlatformConfig::to_json() const {
    nlohmann::json config;
    config["model_name"] = model_name;
    config["en_ddr_lock"] = en_ddr_lock;
    config["en_cache"] = en_cache;
    config["en_prefetch"] = en_prefetch;
    config["max_cache_mem_size"] = max_cache_mem_size;
    config["nr_load_pipeline"] = nr_load_pipeline;
    config["nr_compute_pipeline"] = nr_compute_pipeline;
    config["nr_store_pipeline"] = nr_store_pipeline;
    config["load_cycles"] = load_cycles;
    config["load_weigts_cycles"] = load_weigts_cycles;
    config["store_cycles"] = store_cycles;
    config["granulatity"] = std::string(magic_enum::enum_name(granulatity));
    return config;
}
