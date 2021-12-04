// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "scene_manager.hpp"

#include <glm/ext/vector_double3.hpp>
#include <glm/ext/matrix_double4x4.hpp>
#include <glm/ext/quaternion_double.hpp>

#include <iostream>
#include <filesystem>

namespace vlb {

    auto Scene_t::loadVertexAttribute(const tinygltf::Primitive& primitive, std::string&& label)
    {
        int byteStride{};
        const float* buffer{};
        int componentType{};
        if (primitive.attributes.find(label) != primitive.attributes.end()) {
            const tinygltf::Accessor &accessor = this->model.accessors[primitive.attributes.find(label)->second];
            const tinygltf::BufferView &bufferView = this->model.bufferViews[accessor.bufferView];
            buffer = reinterpret_cast<const float *>(&(this->model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]));
            assert(accessor.ByteStride(bufferView));
            byteStride = accessor.ByteStride(bufferView) / sizeof(float);
            componentType = accessor.componentType;
        }
        return std::make_tuple(buffer, byteStride, componentType);
    }

    void Scene_t::loadNode(Node parent, const tinygltf::Node& node, uint32_t nodeIndex)
    {
        Node newNode{new Node_t()};
        newNode->parent = parent;
        newNode->index = nodeIndex;

        newNode->translation = node.translation.size() != 3 ? glm::dvec3(0.0)
            : glm::make_vec3(node.translation.data());

        newNode->rotation = node.rotation.size() != 3 ? glm::dmat4(glm::dquat{})
            : glm::dmat4(glm::make_quat(node.rotation.data()));

        newNode->scale = node.scale.size() != 3 ? glm::dvec3(1.0f)
            : glm::make_vec3(node.scale.data());

        newNode->matrix = node.matrix.size() != 16 ? glm::dmat4(1.0f)
            : glm::make_mat4x4(node.matrix.data());

        for (const auto& childIndex : node.children)
        {
            const auto& child = this->model.nodes[childIndex];
            loadNode(newNode, child, childIndex);
        }

        if (node.mesh > -1)
        {
            const tinygltf::Mesh& mesh = this->model.meshes[node.mesh];
            Mesh newMesh{new Mesh_t()};
            for (const auto& primitive: mesh.primitives)
            {
                assert(primitive.attributes.find("POSITION") != primitive.attributes.end());
                uint32_t vertexStart = static_cast<uint32_t>(this->vertices.size());
                uint32_t vertexCount = static_cast<uint32_t>(this->model.accessors[primitive.attributes.find("POSITION")->second].count);

                auto[posBuffer, posByteStride, posComponentType] = loadVertexAttribute(primitive, "POSITION");
                auto[normalBuffer, normalByteStride, normalComponentType] = loadVertexAttribute(primitive, "NORMAL");

                for (uint32_t v{}; v < vertexCount; ++v)
                {
                    Vertex vertex{};
                    vertex.position = glm::vec4(glm::make_vec3(&posBuffer[v * posByteStride]), 1.0f);
                    vertex.normal = normalBuffer ? glm::normalize(glm::vec3(glm::make_vec3(&normalBuffer[v * normalByteStride]))) : glm::vec3(0.0f);
                    this->vertices.push_back(vertex);
                }

                uint32_t indexStart = static_cast<uint32_t>(this->indices.size());
                uint32_t indexCount = 0;
                if (primitive.indices > -1)
                {
                    const auto &accessor = this->model.accessors[primitive.indices];
                    const auto &bufferView = this->model.bufferViews[accessor.bufferView];
                    const auto &buffer = this->model.buffers[bufferView.buffer];

                    indexCount = static_cast<uint32_t>(accessor.count);
                    const void *ptr = &(buffer.data[accessor.byteOffset + bufferView.byteOffset]);

                    auto fillIndices = [this, &ptr, vertexStart, indexCount]<typename T>(const T *buff)
                    {
                        buff = static_cast<const T*>(ptr);
                        for (uint32_t i{}; i < indexCount; ++i)
                        {
                            this->indices.push_back(buff[i] + vertexStart);
                        }
                    };

                    if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT)
                    {
                        const uint32_t *buff{};
                        fillIndices(buff);
                    }
                    else if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT)
                    {
                        const uint16_t *buff{};
                        fillIndices(buff);
                    }
                    else if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE)
                    {
                        const uint8_t *buff{};
                        fillIndices(buff);
                    }
                    else
                    {
                        throw std::runtime_error("Index component type not supported!");
                    }
                }

                Primitive newPrimitive{new Primitive_t(indexStart, indexCount, vertexCount)};
                newMesh->primitives.push_back(newPrimitive);
            }

            newNode->mesh = newMesh;
        }

        if (parent)
        {
            parent->children.push_back(newNode);
        }
        else
        {
            nodes.push_back(newNode);
        }
        linearNodes.push_back(newNode);
    }

    Scene_t::Scene_t(std::string& filename)
    {
        std::string err;
        std::string warn;

        std::filesystem::path filePath = filename;

        bool loaded{false};
        if (filePath.extension() == ".gltf")
        {
            loaded = loader.LoadASCIIFromFile(&this->model, &err, &warn, filename);
        }
        else if (filePath.extension() == ".glb")
        {
            loaded = loader.LoadBinaryFromFile(&this->model, &err, &warn, filename);
        }
        else assert(0);

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
            const auto& scene = this->model.scenes[0];
            for (const auto& nodeIndex : scene.nodes)
            {
                const auto& node = this->model.nodes[nodeIndex];
                loadNode(nullptr, node, nodeIndex);
            }

            //TODO: loadTextureSamplers(this->model);
            //TODO: loadMaterials(this->model);
            //TODO: loadAnimations(this->model);
            //TODO: loadSkins(this->model);

            for (auto& node : linearNodes)
            {
                // TODO Assign skins
                // TODO Set Initial pose
            }
        }
        else
        {
            throw std::runtime_error("Failed to parse glTF");
        }
    }

    void Scene_t::createBLASBuffers(vk::Device& device, vk::PhysicalDevice& physicalDevice, vk::Queue& transferQueue, vk::CommandPool& copyCommandPool)
    {
        size_t vertexBufferSize = this->vertices.size() * sizeof(Vertex);
        size_t indexBufferSize = this->indices.size() * sizeof(uint32_t);

        auto copyCmdBuffer = Application::recordCommandBuffer(device, copyCommandPool);

        vk::BufferUsageFlags usage             = vk::BufferUsageFlagBits::eTransferSrc;
        vk::MemoryPropertyFlags memoryProperty = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

        Application::Buffer stagingVertexBuffer = Application::createBuffer(device, physicalDevice, vertexBufferSize, usage, memoryProperty, this->vertices.data());
        Application::Buffer stagingIndexBuffer  = Application::createBuffer(device, physicalDevice, indexBufferSize, usage, memoryProperty, this->indices.data());

        usage = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
            | vk::BufferUsageFlagBits::eShaderDeviceAddress
            | vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eStorageBuffer;
        memoryProperty = vk::MemoryPropertyFlagBits::eDeviceLocal;

        this->vertexBuffer = Application::createBuffer(device, physicalDevice, vertexBufferSize, usage, memoryProperty);
        this->indexBuffer = Application::createBuffer(device, physicalDevice, indexBufferSize, usage, memoryProperty);

        vk::BufferCopy copyRegion{};
        copyRegion.setSize(vertexBufferSize);
        copyCmdBuffer.copyBuffer(stagingVertexBuffer.handle.get(), this->vertexBuffer.handle.get(), 1, &copyRegion);
        copyRegion.setSize(indexBufferSize);
        copyCmdBuffer.copyBuffer(stagingIndexBuffer.handle.get(), this->indexBuffer.handle.get(), 1, &copyRegion);

        Application::flushCommandBuffer(device, copyCommandPool, copyCmdBuffer, transferQueue);
    }

    const std::string& Scene_t::getName()
    {
        return this->name;
    }

    void SceneManager::init(InitInfo& info)
    {
        this->device = info.device;
        this->physicalDevice = info.physicalDevice;
        this->transferQueue = info.transferQueue;
        this->transferCommandPool = info.transferCommandPool;
        this->pFileDialog = &(info.pUI->getFileDialog());
        this->pSceneNames = &(info.pUI->getSceneNames());
        this->pSelectedSceneIndex = &(info.pUI->getSelectedSceneIndex());

        /*
        std::string fileName{"default_blender_cube.gltf"};
        Scene scene{new Scene_t(fileName)};
        scene->createBLASBuffers(info.device, info.physicalDevice, info.transferQueue, info.transferCommandPool);
        scenes.push_back(scene);
        (*this->pSceneNames).push_back(scene->getName());
        *this->pSelectedSceneIndex = 0;
        */
    }

    void SceneManager::update()
    {
        if (this->pFileDialog->HasSelected())
        {
            std::string fileName{this->pFileDialog->GetSelected().string()};
            Scene scene{new Scene_t(fileName)};
            scene->createBLASBuffers(this->device, this->physicalDevice, this->transferQueue, this->transferCommandPool);
            //TODO: scene->loadTextures(this->device, this->transferQueue, this->transferCommandPool);
            scenes.push_back(scene);
            (*this->pSceneNames).push_back(scene->getName());
            *this->pSelectedSceneIndex = static_cast<int>((*this->pSceneNames).size()) - 1;

            this->pFileDialog->ClearSelected();
        }
    }

    Scene& SceneManager::getScene()
    {
        return scenes[*pSelectedSceneIndex];
    }

    SceneManager::SceneManager()
    {
    }

    SceneManager::~SceneManager()
    {
    }

}

