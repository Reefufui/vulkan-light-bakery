// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "scene_manager.hpp"

#include <iostream>

namespace vlb {

    void Scene::loadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model,
            std::vector<uint32_t>& iBuffer, std::vector<Vertex>& vBuffer)
    {
        std::cout << "creating node!\n";
    }

    Scene::Scene(std::string& pathToScene)
    {
        std::string err;
        std::string warn;

        //TODO: deal with binary versions of file format as well (.glb)
        bool loaded = loader.LoadASCIIFromFile(&model, &err, &warn, pathToScene);

        std::vector<uint32_t> indexBuffer;
        std::vector<Vertex> vertexBuffer;

        if (!warn.empty())
        {
            printf("Warn: %s\n", warn.c_str());
        }

        if (!err.empty())
        {
            printf("Err: %s\n", err.c_str());
        }

        if (loaded)
        {
            std::cout << "Parsed " << pathToScene << "!\n";
            const auto& scene = model.scenes[0];
            for (const auto& nodeIndex : scene.nodes)
            {
                const auto& node = model.nodes[nodeIndex];
                loadNode(nullptr, node, nodeIndex, model, indexBuffer, vertexBuffer);
            }

        }
        else
        {
            throw std::runtime_error("Failed to parse glTF");
        }
    }

    void SceneManager::init(InitInfo& info)
    {
        this->device = info.device;
        this->physicalDevice = info.physicalDevice;
        this->transferQueue = info.transferQueue;
        this->pFileDialog = &(info.pUI->getFileDialog());
    }

    void SceneManager::update()
    {
        if (this->pFileDialog->HasSelected())
        {
            std::string pathToScene{};
            std::cout << "Selected filename" << (pathToScene = this->pFileDialog->GetSelected().string()) << "\n";
            this->pFileDialog->ClearSelected();
            scenes.push_back(Scene(pathToScene));
        }
    }

    SceneManager::SceneManager()
    {
    }

    SceneManager::~SceneManager()
    {
    }

}

