// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "scene_manager.hpp"

#include <glm/ext/vector_double3.hpp>
#include <glm/ext/matrix_double4x4.hpp>
#include <glm/ext/quaternion_double.hpp>

#include <iostream>
#include <filesystem>
#include <algorithm>

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
                auto[uv0Buffer, uv0ByteStride, uv0ComponentType] = loadVertexAttribute(primitive, "TEXCOORD_0");
                auto[uv1Buffer, uv1ByteStride, uv1ComponentType] = loadVertexAttribute(primitive, "TEXCOORD_1");

                for (uint32_t v{}; v < vertexCount; ++v)
                {
                    Vertex vertex{};
                    vertex.position = glm::vec4(glm::make_vec3(&posBuffer[v * posByteStride]), 1.0f);
                    vertex.position.y = - vertex.position.y;
                    vertex.normal = normalBuffer ? glm::normalize(glm::vec3(glm::make_vec3(&normalBuffer[v * normalByteStride]))) : glm::vec3(0.0f);
                    vertex.uv0 = uv0Buffer ? glm::make_vec2(&uv0Buffer[v * uv0ByteStride]) : glm::vec2(0.0f);
                    vertex.uv1 = uv1Buffer ? glm::make_vec2(&uv1Buffer[v * uv1ByteStride]) : glm::vec2(0.0f);
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

    void Scene_t::loadSamplers()
    {
        for (auto& gltfSampler : this->model.samplers)
        {
            auto toVkWrapMode = [](int32_t gltfWrapMode) -> vk::SamplerAddressMode
            {
                if (gltfWrapMode == 33071)
                {
                    return vk::SamplerAddressMode::eClampToEdge;
                }
                else if (gltfWrapMode == 33648)
                {
                    return vk::SamplerAddressMode::eMirroredRepeat;
                }
                else
                {
                    return vk::SamplerAddressMode::eRepeat;
                }
            };

            auto toVkFilterMode = [](int32_t gltfFilterMode) -> vk::Filter
            {
                if (gltfFilterMode == 9728 || gltfFilterMode == 9984 || gltfFilterMode == 9985)
                {
                    return vk::Filter::eNearest;
                }
                else
                {
                    return vk::Filter::eLinear;
                }
            };

            Sampler sampler{};
            sampler.minFilter = toVkFilterMode(gltfSampler.minFilter);
            sampler.magFilter = toVkFilterMode(gltfSampler.magFilter);
            sampler.addressModeU = toVkWrapMode(gltfSampler.wrapS);
            sampler.addressModeV = toVkWrapMode(gltfSampler.wrapT);
            sampler.addressModeW = sampler.addressModeV;

            this->samplers.push_back(sampler);
        }
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
            printf("Warn: %s", warn.c_str());
        }

        if (!err.empty())
        {
            printf("Err: %s", err.c_str());
        }

        if (loaded)
        {
            this->name = filePath.stem();
            const auto& scene = this->model.scenes[0];
            for (const auto& nodeIndex : scene.nodes)
            {
                const auto& node = this->model.nodes[nodeIndex];
                loadNode(nullptr, node, nodeIndex);
            }

            loadSamplers();
            //TODO: loadMaterials();
            //TODO: loadAnimations();
            //TODO: loadSkins();

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

    void Scene_t::createObjectDescBuffer(vk::Device& device, vk::PhysicalDevice& physicalDevice, vk::Queue& transferQueue, vk::CommandPool& copyCommandPool)
    {
        struct ObjDesc
        {
            vk::DeviceAddress vertexBuffer;
            vk::DeviceAddress indexBuffer;
        } objDesc;

        objDesc.vertexBuffer = device.getBufferAddressKHR(this->vertexBuffer.handle.get());
        objDesc.indexBuffer  = device.getBufferAddressKHR(this->indexBuffer.handle.get());

        auto copyCmdBuffer = Application::recordCommandBuffer(device, copyCommandPool);

        vk::BufferUsageFlags usage             = vk::BufferUsageFlagBits::eTransferSrc;
        vk::MemoryPropertyFlags memoryProperty = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

        auto size = sizeof(ObjDesc);

        Application::Buffer staging = Application::createBuffer(device, physicalDevice, size, usage, memoryProperty, &objDesc);

        usage = vk::BufferUsageFlagBits::eShaderDeviceAddress
            | vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eStorageBuffer;
        memoryProperty = vk::MemoryPropertyFlagBits::eDeviceLocal;

        this->objDescBuffer = Application::createBuffer(device, physicalDevice, size, usage, memoryProperty);

        vk::BufferCopy copyRegion{};
        copyRegion.setSize(size);
        copyCmdBuffer.copyBuffer(staging.handle.get(), this->objDescBuffer.handle.get(), 1, &copyRegion);

        Application::flushCommandBuffer(device, copyCommandPool, copyCmdBuffer, transferQueue);
    }

    // graphics queue requred for blitting!
    void Scene_t::loadTextures(vk::Device& device, vk::PhysicalDevice& physicalDevice, vk::Queue& graphicsQueue, vk::CommandPool& graphicsCommandPool)
    {
        for (const auto& gltfTexture : this->model.textures)
        {
            Texture texture{new Texture_t()};

            const tinygltf::Image& gltfImage = this->model.images[gltfTexture.source];
            if (gltfImage.component == 3)
            {
                throw std::runtime_error("RGB (not RGBA) textures not allowed!");
            }

            vk::BufferUsageFlags usage             = vk::BufferUsageFlagBits::eTransferSrc;
            vk::MemoryPropertyFlags memoryProperty = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
            Application::Buffer staging = Application::createBuffer(device, physicalDevice, gltfImage.image.size(), usage, memoryProperty, gltfImage.image.data());

            std::array<uint32_t, 3> queueFamilyIndices = {0, 1, 2}; // TODO: this might fail but ok for now
            vk::Extent3D extent{static_cast<uint32_t>(gltfImage.width), static_cast<uint32_t>(gltfImage.height), 1};
            uint32_t mipLevels{static_cast<uint32_t>(floor(log2(std::min(gltfImage.width, gltfImage.height))) + 1.0)};

            texture->image.handle = device.createImageUnique(
                    vk::ImageCreateInfo{}
                    .setImageType(vk::ImageType::e2D)
                    .setFormat(vk::Format::eB8G8R8A8Unorm)
                    .setArrayLayers(1)
                    .setMipLevels(mipLevels)
                    .setSamples(vk::SampleCountFlagBits::e1)
                    .setTiling(vk::ImageTiling::eOptimal)
                    .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc)
                    .setSharingMode(vk::SharingMode::eConcurrent)
                    .setQueueFamilyIndices(queueFamilyIndices)
                    .setInitialLayout(vk::ImageLayout::eUndefined)
                    .setExtent(extent)
                    );

            auto memoryRequirements = device.getImageMemoryRequirements(texture->image.handle.get());
            memoryProperty = vk::MemoryPropertyFlagBits::eDeviceLocal;

            texture->image.memory = device.allocateMemoryUnique(
                    vk::MemoryAllocateInfo{}
                    .setAllocationSize(memoryRequirements.size)
                    .setMemoryTypeIndex(Application::getMemoryType(physicalDevice, memoryRequirements, memoryProperty))
                    );

            device.bindImageMemory(texture->image.handle.get(), texture->image.memory.get(), 0);

            auto blittingCmdBuffer = Application::recordCommandBuffer(device, graphicsCommandPool);

            Application::setImageLayout(blittingCmdBuffer, texture->image.handle.get(), vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                    { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

            blittingCmdBuffer.copyBufferToImage(
                    staging.handle.get(),
                    texture->image.handle.get(),
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::BufferImageCopy{}
                    .setImageSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
                    .setImageExtent(extent)
                    );

            Application::setImageLayout(blittingCmdBuffer, texture->image.handle.get(), vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                    { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

            for (uint32_t lvl = 1; lvl < mipLevels; ++lvl)
            {
                auto getMipLevelOffset = [extent](uint32_t lvl)
                {
                    return vk::Offset3D{static_cast<int32_t>(extent.width >> lvl), static_cast<int32_t>(extent.height >> lvl), 1};
                };

                vk::ImageBlit imageBlit{};
                imageBlit
                    .setSrcSubresource({ vk::ImageAspectFlagBits::eColor, lvl - 1, 0, 1 })
                    .setDstSubresource({ vk::ImageAspectFlagBits::eColor, lvl,     0, 1 })
                    .setSrcOffsets({ vk::Offset3D{}, getMipLevelOffset(lvl - 1) })
                    .setDstOffsets({ vk::Offset3D{}, getMipLevelOffset(lvl    ) });

                Application::setImageLayout(blittingCmdBuffer, texture->image.handle.get(), vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                        { vk::ImageAspectFlagBits::eColor, lvl, 1, 0, 1 });

                blittingCmdBuffer.blitImage(texture->image.handle.get(), vk::ImageLayout::eTransferSrcOptimal, texture->image.handle.get(), vk::ImageLayout::eTransferDstOptimal, 1, &imageBlit, vk::Filter::eLinear);

                Application::setImageLayout(blittingCmdBuffer, texture->image.handle.get(), vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                        { vk::ImageAspectFlagBits::eColor, lvl, 1, 0, 1 });
            }

            Application::setImageLayout(blittingCmdBuffer, texture->image.handle.get(), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                    { vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1 });

            Application::flushCommandBuffer(device, graphicsCommandPool, blittingCmdBuffer, graphicsQueue);

            Sampler textureSampler = gltfTexture.sampler == -1 ? Sampler{} : this->samplers[gltfTexture.sampler];

            texture->sampler = device.createSamplerUnique(
                    vk::SamplerCreateInfo{}
                    .setMagFilter(textureSampler.magFilter)
                    .setMinFilter(textureSampler.minFilter)
                    .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                    .setAddressModeU(textureSampler.addressModeU)
                    .setAddressModeV(textureSampler.addressModeV)
                    .setAddressModeW(textureSampler.addressModeW)
                    .setCompareOp(vk::CompareOp::eNever)
                    .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
                    .setMaxAnisotropy(1.0)
                    .setAnisotropyEnable(VK_FALSE)
                    .setMaxLod(static_cast<float>(mipLevels))
                    .setMaxAnisotropy(8.0f)
                    .setAnisotropyEnable(VK_TRUE));

            texture->image.imageView = device.createImageViewUnique(
                     vk::ImageViewCreateInfo{}
                    .setImage(texture->image.handle.get())
                    .setViewType(vk::ImageViewType::e2D)
                    .setFormat(vk::Format::eB8G8R8A8Unorm)
                    .setComponents({ vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA })
                    .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1 })
                    );

            texture->image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal; // TODO: change layout on go as member

            texture->descriptor
                .setSampler(texture->sampler.get())
                .setImageView(texture->image.imageView.get())
                .setImageLayout(texture->image.imageLayout);

            this->textures.push_back(texture);
        }
    }

    void SceneManager::init(InitInfo& info)
    {
        this->device = info.device;
        this->physicalDevice = info.physicalDevice;
        this->transferQueue = info.transferQueue;
        this->transferCommandPool = info.transferCommandPool;
        this->graphicsQueue = info.graphicsQueue;
        this->graphicsCommandPool = info.graphicsCommandPool;
        this->pFileDialog = &(info.pUI->getFileDialog());
        this->pSceneNames = &(info.pUI->getSceneNames());
        this->pSelectedSceneIndex = &(info.pUI->getSelectedSceneIndex());
        this->pFreeScene = &(info.pUI->getFreeSceneFlag());

        std::string fileName{"default_blender_cube.gltf"};
        Scene scene{new Scene_t(fileName)};
        scene->createBLASBuffers(info.device, info.physicalDevice, info.transferQueue, info.transferCommandPool);
        scene->createObjectDescBuffer(info.device, info.physicalDevice, info.transferQueue, info.transferCommandPool);
        scenes.push_back(scene);
        (*this->pSceneNames).push_back(scene->name);
        *this->pSelectedSceneIndex = 0;
        this->sceneChangedFlag = false;
    }

    void SceneManager::update()
    {
        auto& sceneIndex = *this->pSelectedSceneIndex;
        auto& sceneNames = *this->pSceneNames;

        this->sceneChangedFlag = false;

        // "-"
        if (*this->pFreeScene)
        {
            this->device.waitIdle();
            scenes.erase(scenes.begin() + sceneIndex);
            sceneNames.erase(sceneNames.begin() + sceneIndex);
            sceneIndex = std::max(0, sceneIndex - 1);

            *this->pFreeScene = false;
            this->sceneChangedFlag = true;
        }

        // "+"
        if (this->pFileDialog->HasSelected())
        {
            std::string fileName{this->pFileDialog->GetSelected().string()};
            try
            {
                Scene scene{new Scene_t(fileName)};
                scene->createBLASBuffers(this->device, this->physicalDevice, this->transferQueue, this->transferCommandPool);
                scene->createObjectDescBuffer(this->device, this->physicalDevice, this->transferQueue, this->transferCommandPool);
                scene->loadTextures(this->device, this->physicalDevice, this->graphicsQueue, this->graphicsCommandPool);
                scenes.push_back(scene);

                sceneNames.push_back(scene->name);
                sceneIndex = static_cast<int>(sceneNames.size()) - 1;

                this->sceneChangedFlag = true;
            }
            catch(std::runtime_error& e)
            {
                std::cerr << e.what() << "\n";
            }

            this->pFileDialog->ClearSelected();
        }

        // "drop-down menu"
        static int oldSceneIndex = 0;
        if (oldSceneIndex != sceneIndex)
        {
            this->sceneChangedFlag = true;
            oldSceneIndex = sceneIndex;
        }
    }

    Scene& SceneManager::getScene()
    {
        return scenes[*pSelectedSceneIndex];
    }

    bool SceneManager::sceneChanged()
    {
        return this->sceneChangedFlag;
    }

    SceneManager::SceneManager()
    {
    }

    SceneManager::~SceneManager()
    {
    }

}

