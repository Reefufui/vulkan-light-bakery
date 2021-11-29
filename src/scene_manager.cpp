// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "scene_manager.hpp"

#include <iostream>

namespace vlb {

    SceneManager::SceneManager()
    {
        std::string err;
        std::string warn;

        //NOTE:test
        bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, "assets/models/plants.gltf");

        if (!warn.empty()) {
            printf("Warn: %s\n", warn.c_str());
        }

        if (!err.empty()) {
            printf("Err: %s\n", err.c_str());
        }

        if (!ret) {
            throw std::runtime_error("Failed to parse glTF");
        }
    }

    SceneManager::~SceneManager()
    {
    }

}

