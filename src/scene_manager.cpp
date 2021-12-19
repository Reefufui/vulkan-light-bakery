// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "scene_manager.hpp"

#include <glm/ext/vector_double3.hpp>
#include <glm/ext/matrix_double4x4.hpp>
#include <glm/ext/quaternion_double.hpp>

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <utility>

namespace vlb {

    glm::mat4 Scene_t::Node_t::localMatrix()
    {
        return glm::translate(glm::mat4(1.0f), this->translation) * glm::mat4(this->rotation) * glm::scale(glm::mat4(1.0f), this->scale) * this->matrix;
    }

    glm::mat4 Scene_t::Node_t::getMatrix()
    {
        glm::mat4 matrix = localMatrix();
        Node p = this->parent;
        while (p)
        {
            matrix = p->localMatrix() * matrix;
            p = p->parent;
        }
        return matrix;
    }

    void Scene_t::Node_t::update()
    {
        if (mesh)
        {
            glm::mat4 matrix = getMatrix();
            memcpy(mesh->uniformBuffer.mapped, &matrix, sizeof(glm::mat4));
        }

        for (auto& child : children)
        {
            child->update();
        }
    }

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

    void Scene_t::loadCameras(vk::Device& device, vk::PhysicalDevice& physicalDevice, uint32_t count)
    {
        // TODO: load gltf cameras as well
        Camera camera{new Camera_t()};
        camera
            ->setType(Camera_t::Type::eFirstPerson)
            ->setRotationSpeed(0.2f)
            ->setMovementSpeed(1.0f)
            ->createCameraUBOs(device, physicalDevice, count);

        this->cameras.push_back(camera);
        this->cameraIndex = 0;
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

    void Scene_t::loadMaterials()
    {
        auto getTexture = [this](tinygltf::Material& material, std::string&& label)
        {
            if (material.values.find(label) != material.values.end())
            {
                auto& value = material.values[label];

                Texture texture      = this->textures[value.TextureIndex()];
                uint8_t textureCoord = value.TextureTexCoord();

                return std::make_tuple(texture, textureCoord);
            }
            else
            {
                return std::make_tuple(Texture(nullptr), uint8_t(-1));
            }
        };

        for (tinygltf::Material &gltfMaterial : this->model.materials)
        {
            Material material{};

            if (gltfMaterial.additionalValues.find("alphaMode") != gltfMaterial.additionalValues.end())
            {
                tinygltf::Parameter& param = gltfMaterial.additionalValues["alphaMode"];

                if (param.string_value == "BLEND")
                {
                    material.alpha.mode = Material::Alpha::Mode::eBlend;
                }
                else if (param.string_value == "MASK")
                {
                    material.alpha.cutOff = 0.5f;
                    material.alpha.mode = Material::Alpha::Mode::eMask;
                }
                else
                {
                    material.alpha.mode = Material::Alpha::Mode::eOpaque;
                }

                if (gltfMaterial.additionalValues.find("alphaCutoff") != gltfMaterial.additionalValues.end())
                {
                    material.alpha.cutOff = static_cast<float>(gltfMaterial.additionalValues["alphaCutoff"].Factor());
                }
            }

            if (gltfMaterial.values.find("baseColorFactor") != gltfMaterial.values.end())
            {
                material.factor.baseColor = glm::make_vec4(gltfMaterial.values["baseColorFactor"].ColorFactor().data());
            }

            if (gltfMaterial.values.find("metallicFactor") != gltfMaterial.values.end())
            {
                material.factor.metallic = static_cast<float>(gltfMaterial.values["metallicFactor"].Factor());
            }

            if (gltfMaterial.values.find("roughnessFactor") != gltfMaterial.values.end())
            {
                material.factor.roughness = static_cast<float>(gltfMaterial.values["roughnessFactor"].Factor());
            }

            if (gltfMaterial.additionalValues.find("emissiveFactor") != gltfMaterial.additionalValues.end())
            {
                material.factor.emissive = glm::vec4(glm::make_vec3(gltfMaterial.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0);
            }

            {
                auto[texture, textureCoord] = getTexture(gltfMaterial, "normalTexture");
                material.texture.normal     = texture;
                material.coordSet.normal = textureCoord;
            }

            {
                auto[texture, textureCoord] = getTexture(gltfMaterial, "occlusionTexture");
                material.texture.occlusion     = texture;
                material.coordSet.occlusion = textureCoord;
            }

            {
                auto[texture, textureCoord] = getTexture(gltfMaterial, "baseColorTexture");
                material.texture.baseColor     = texture;
                material.coordSet.baseColor = textureCoord;
            }

            {
                auto[texture, textureCoord] = getTexture(gltfMaterial, "metallicRoughnessTexture");
                material.texture.metallicRoughness     = texture;
                material.coordSet.metallicRoughness = textureCoord;
            }

            {
                auto[texture, textureCoord] = getTexture(gltfMaterial, "emissiveTexture");
                material.texture.emissive     = texture;
                material.coordSet.emissive = textureCoord;
            }

            if (gltfMaterial.extensions.find("KHR_materials_pbrSpecularGlossiness") != gltfMaterial.extensions.end())
            {
                auto ext = gltfMaterial.extensions.find("KHR_materials_pbrSpecularGlossiness");

                if (ext->second.Has("specularGlossinessTexture"))
                {
                    auto index = ext->second.Get("specularGlossinessTexture").Get("index");
                    material.texture.specularEXT  = this->textures[index.Get<int>()];
                    material.coordSet.specularEXT = ext->second.Get("specularGlossinessTexture").Get("texCoord").Get<int>();
                }

                if (ext->second.Has("diffuseTexture"))
                {
                    auto index = ext->second.Get("diffuseTexture").Get("index");
                    material.texture.diffuseEXT = this->textures[index.Get<int>()];
                    material.coordSet.diffuseEXT = ext->second.Get("diffuseTexture").Get("texCoord").Get<int>();
                }

                if (ext->second.Has("diffuseFactor"))
                {
                    auto factor = ext->second.Get("diffuseFactor");
                    for (uint32_t i = 0; i < factor.ArrayLen(); i++)
                    {
                        auto val = factor.Get(i);
                        material.factor.diffuseEXT[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
                    }
                }
                if (ext->second.Has("specularFactor"))
                {
                    auto factor = ext->second.Get("specularFactor");
                    for (uint32_t i = 0; i < factor.ArrayLen(); i++)
                    {
                        auto val = factor.Get(i);
                        material.factor.specularEXT[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
                    }
                }
            }

            materials.push_back(material);
        }

        materials.push_back(Material());
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
        else
        {
            throw std::runtime_error("Not supported file format");
        }

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

    void Scene_t::setViewingFrustumForCameras(ViewingFrustum frustum)
    {
        for (auto& camera : cameras)
        {
            camera->setViewingFrustum(frustum);
        }
    }

    Camera Scene_t::getCamera(int index)
    {
        return this->cameras[index == -1 ? this->cameraIndex : index];
    }

    void Scene_t::setCameraIndex(int cameraIndex)
    {
        this->cameraIndex = cameraIndex;
    }

    const int Scene_t::getCameraIndex()
    {
        return this->cameraIndex;
    }

    void Scene_t::pushCamera(Camera camera)
    {
        // TODO: change index here
        this->cameras.push_back(camera);
    }

    const size_t Scene_t::getCamerasCount()
    {
        return this->cameras.size();
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

    // graphics queue requred for blitting! ~duh
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

        loadMaterials(); // TODO: call this function somewhere else
    }

    void SceneManager::pushScene(Scene_t::CreateInfo ci)
    {
        Scene scene{new Scene_t(ci.path)};
        scene->createBLASBuffers(this->device, this->physicalDevice, this->queue.transfer, this->commandPool.transfer);
        scene->createObjectDescBuffer(this->device, this->physicalDevice, this->queue.transfer, this->commandPool.transfer);
        scene->setViewingFrustumForCameras(this->frustum);
        scene->setCameraIndex(ci.cameraIndex);

        scene->loadTextures(this->device, this->physicalDevice, this->queue.graphics, this->commandPool.graphics);

        for (auto& cameraInfo : ci.cameras)
        {
            Camera camera{new Camera_t()};
            camera
                ->setType(Camera_t::Type::eFirstPerson)
                ->setRotationSpeed(cameraInfo.rotationSpeed)
                ->setMovementSpeed(cameraInfo.movementSpeed)
                ->setYaw(cameraInfo.yaw)
                ->setPitch(cameraInfo.pitch)
                ->setPosition(cameraInfo.position)
                ->createCameraUBOs(this->device, this->physicalDevice, this->swapchainImagesCount);

            scene->pushCamera(camera);
        }

        scene->path = ci.path;

        this->scenes.push_back(scene);
        this->sceneNames.push_back(ci.name);

        this->sceneChangedFlag = false;
    }

    void SceneManager::pushScene(std::string& fileName)
    {
        Scene scene{new Scene_t(fileName)};
        // should call this functions from other file (im not sure yet)
        scene->createBLASBuffers(this->device, this->physicalDevice, this->queue.transfer, this->commandPool.transfer);
        scene->createObjectDescBuffer(this->device, this->physicalDevice, this->queue.transfer, this->commandPool.transfer);
        scene->setViewingFrustumForCameras(this->frustum);
        //
        scene->loadCameras(this->device, this->physicalDevice, this->swapchainImagesCount);
        scene->loadTextures(this->device, this->physicalDevice, this->queue.graphics, this->commandPool.graphics);

        scene->path = fileName;

        this->scenes.push_back(scene);
        this->sceneNames.push_back(scene->name); // TODO: string views

        this->sceneIndex = getScenesCount() - 1;
        this->sceneChangedFlag = true;
    }

    void SceneManager::popScene()
    {
        this->device.waitIdle();
        this->scenes.erase(this->scenes.begin() + this->sceneIndex);
        this->sceneNames.erase(this->sceneNames.begin() + this->sceneIndex);

        this->sceneIndex = std::max(0, this->sceneIndex - 1);

        this->sceneChangedFlag = true;
    }

    void SceneManager::init(InitInfo& info)
    {
        this->device               = info.device;
        this->physicalDevice       = info.physicalDevice;
        this->queue.transfer       = info.transferQueue;
        this->commandPool.transfer = info.transferCommandPool;
        this->queue.graphics       = info.graphicsQueue;
        this->commandPool.graphics = info.graphicsCommandPool;
        this->swapchainImagesCount = info.swapchainImagesCount;

        this->sceneChangedFlag = false;
        this->sceneIndex = 0;
        this->frustum = std::make_shared<ViewingFrustum_t>();
    }

    Scene& SceneManager::getScene(int index)
    {
        return this->scenes[index == -1 ? this->sceneIndex : index];
    }

    const int SceneManager::getSceneIndex()
    {
        return this->sceneIndex;
    }

    std::vector<std::string>& SceneManager::getSceneNames()
    {
        return this->sceneNames;
    }

    const size_t SceneManager::getScenesCount()
    {
        return this->scenes.size();
    }

    Camera SceneManager::getCamera()
    {
        return getScene()->getCamera();
    }

    SceneManager& SceneManager::setSceneIndex(int sceneIndex)
    {
        this->sceneIndex = sceneIndex;

        static int oldSceneIndex = 0;
        if (this->sceneIndex != oldSceneIndex)
        {
            oldSceneIndex = this->sceneIndex;
            this->sceneChangedFlag = true;
        }

        return *this;
    }

    const bool SceneManager::sceneChanged()
    {
        bool sceneChanged = false;
        std::swap(sceneChanged, this->sceneChangedFlag);
        return sceneChanged;
    }

    SceneManager::SceneManager()
    {
    }

    SceneManager::~SceneManager()
    {
    }

}

