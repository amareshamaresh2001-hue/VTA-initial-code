#include <iostream>
#include <filesystem>

#ifndef FILES_MANAGER_H
#define FILES_MANAGER_H

namespace fs = std::filesystem;


#include "utilities.h"

class FilesManager {

    public:
        char *model_name;
        char *model_path;
        char *results_dir;
        // char *instructions_path;
        char *vta_schedules_path;
        // char *direct_to_pipeline_path;
        char *dependencies_dir;
        char *schedules_dir;

    FilesManager(Granularity granularity, char *model_name) {
        this->model_name = model_name;
        std::string result_folder_name;
        switch (granularity)
        {
        case Granularity::layer:
            result_folder_name = "layer_wise";
            break;
        case Granularity::block:
            result_folder_name = "block_wise";
            break;
        default:
            break;
        }
        this->model_path = concatStringsModern(SOURCE_DIR, "/data/models/", model_name, ".csv");
        this->results_dir = concatStringsModern(SOURCE_DIR, "/data/", model_name, "/", result_folder_name);
        // this->instructions_path = concatStringsModern(results_dir, "/instructions/");
        this->vta_schedules_path = concatStringsModern(results_dir, "/vta_schedules/");
        // this->direct_to_pipeline_path = concatStringsModern(vta_schedules_path, "direct_to_pipeline/");
        this->dependencies_dir = concatStringsModern(results_dir, "/dependencies");
        this->schedules_dir = concatStringsModern(results_dir, "/schedules");


        fs::create_directories(concatStringsModern(SOURCE_DIR, "/data/", model_name));
        fs::create_directories(this->results_dir);
        fs::create_directories(this->vta_schedules_path);
        fs::create_directories(this->dependencies_dir);
        fs::create_directories(this->schedules_dir);

        // system(concatStringsModern("dir ", concatStringsModern(SOURCE_DIR, "/data/", model_name)));
        // // system(concatStringsModern("rm -r ", results_dir));
        // system(concatStringsModern("dir ", results_dir));
        // // system(concatStringsModern("mkdir ", instructions_path));
        // system(concatStringsModern("dir ", vta_schedules_path));
        // // system(concatStringsModern("mkdir ", direct_to_pipeline_path));
        // system(concatStringsModern("dir ", dependencies_dir));
        // system(concatStringsModern("dir ", schedules_dir));
    }

};

#endif
