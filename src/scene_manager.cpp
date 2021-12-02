// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "scene_manager.hpp"

#include <iostream>
#include <filesystem>

namespace vlb {

    void Scene::loadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model,
            std::vector<uint32_t>& iBuffer, std::vector<Vertex>& vBuffer)
    {
        std::cout << "creating node!\n";
    }

    Scene::Scene(std::string& filename)
    {
        std::string err;
        std::string warn;

        std::filesystem::path filePath = filename;

        bool loaded{false};
        if (filePath.extension() == ".gltf")
        {
            loaded = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
        }
        else if (filePath.extension() == ".glb")
        {
            loaded = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
        }
        else
        {
            assert(0);
        }

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
            this->name = filePath.stem();
            std::cout << "Parsed " << filePath.stem() << "!\n";
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

    const std::string& Scene::getName()
    {
        return this->name;
    }

    void SceneManager::init(InitInfo& info)
    {
        this->device = info.device;
        this->physicalDevice = info.physicalDevice;
        this->transferQueue = info.transferQueue;
        this->pFileDialog = &(info.pUI->getFileDialog());
        this->pSceneNames = &(info.pUI->getSceneNames());
        this->pSelectedSceneIndex = &(info.pUI->getSelectedSceneIndex());
    }

    void SceneManager::update()
    {
        if (this->pFileDialog->HasSelected())
        {
            std::string fileName{this->pFileDialog->GetSelected().string()};
            scenes.push_back(Scene(fileName));
            (*this->pSceneNames).push_back(scenes.back().getName());
            this->pFileDialog->ClearSelected();
            *this->pSelectedSceneIndex = static_cast<int>((*this->pSceneNames).size()) - 1;
        }
    }

    SceneManager::SceneManager()
    {
    }

    SceneManager::~SceneManager()
    {
    }

}

