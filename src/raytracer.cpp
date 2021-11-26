// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "raytracer.hpp"

namespace vlb {

    // TODO: abstract this AS classes
    void Raytracer::createBLAS()
    {
        /////////////////////////
        // Hello triangle NOTE:(we will load glTF model after I set everything up)
        /////////////////////////
        struct Vertex {
            float pos[3];
        };
        std::vector<Vertex> vertices = {
            { {  1.0f,  1.0f, 0.0f } },
            { { -1.0f,  1.0f, 0.0f } },
            { {  0.0f, -1.0f, 0.0f } }
        };
        std::vector<uint32_t> indices = { 0, 1, 2 };
        std::vector<float> transformMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        };
        size_t vBufferSize = sizeof(Vertex)   * vertices.size();
        size_t iBufferSize = sizeof(uint32_t) * indices.size();
        size_t tBufferSize = sizeof(float)    * transformMatrix.size();

        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
        bufferInfo.size  = vBufferSize;
        m_scene.vBuffer = m_device.createBuffer(bufferInfo);
        bufferInfo.size  = iBufferSize;
        m_scene.iBuffer = m_device.createBuffer(bufferInfo);
        bufferInfo.size  = tBufferSize;
        m_scene.tBuffer = m_device.createBuffer(bufferInfo);

        // NOTE: it is ok not to stage the vertex data to the GPU memory for now
        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        vk::MemoryAllocateFlagsInfo allocExt{};
        allocExt.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;

        auto getMemoryForBuffer = [&](vk::Buffer& a_buffer)
        {
            vk::MemoryRequirements memReq = m_device.getBufferMemoryRequirements(a_buffer);
            vk::MemoryAllocateInfo allocInfo{};
            allocInfo.allocationSize  = memReq.size;
            allocInfo.pNext = &allocExt;
            for (uint32_t i = 0; i < this->physicalDeviceMemoryProperties.memoryTypeCount; ++i)
            {
                if ((memReq.memoryTypeBits & (1 << i)) && ((this->physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties)
                            == properties))
                {
                    allocInfo.memoryTypeIndex = i;
                    break;
                }
            }

            return m_device.allocateMemory(allocInfo);
        };

        m_scene.vBufferMemory = getMemoryForBuffer(m_scene.vBuffer);
        m_scene.iBufferMemory = getMemoryForBuffer(m_scene.iBuffer);
        m_scene.tBufferMemory = getMemoryForBuffer(m_scene.tBuffer);

        uint8_t *pData = static_cast<uint8_t *>(m_device.mapMemory(m_scene.vBufferMemory, 0, vBufferSize));
        memcpy(pData, vertices.data(), vBufferSize);
        m_device.unmapMemory(m_scene.vBufferMemory);
        m_device.bindBufferMemory(m_scene.vBuffer, m_scene.vBufferMemory, 0);

        pData = static_cast<uint8_t *>(m_device.mapMemory(m_scene.iBufferMemory, 0, iBufferSize));
        memcpy(pData, indices.data(), iBufferSize);
        m_device.unmapMemory(m_scene.iBufferMemory);
        m_device.bindBufferMemory(m_scene.iBuffer, m_scene.iBufferMemory, 0);

        pData = static_cast<uint8_t *>(m_device.mapMemory(m_scene.tBufferMemory, 0, tBufferSize));
        memcpy(pData, transformMatrix.data(), tBufferSize);
        m_device.unmapMemory(m_scene.tBufferMemory);
        m_device.bindBufferMemory(m_scene.tBuffer, m_scene.tBufferMemory, 0);

        vk::DeviceOrHostAddressConstKHR vBufferDeviceAddress{};
        vk::DeviceOrHostAddressConstKHR iBufferDeviceAddress{};
        vk::DeviceOrHostAddressConstKHR tBufferDeviceAddress{};

        vBufferDeviceAddress.deviceAddress = m_device.getBufferAddressKHR({m_scene.vBuffer});
        iBufferDeviceAddress.deviceAddress = m_device.getBufferAddressKHR({m_scene.iBuffer});
        tBufferDeviceAddress.deviceAddress = m_device.getBufferAddressKHR({m_scene.tBuffer});

        /////////////////////////
        // Hello triangle
        /////////////////////////

        vk::AccelerationStructureGeometryKHR accelerationStructureGeometry{};
        accelerationStructureGeometry.flags        = vk::GeometryFlagBitsKHR::eOpaque;
        accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eTriangles;

        accelerationStructureGeometry.geometry.triangles.sType        = vk::StructureType::eAccelerationStructureGeometryTrianglesDataKHR;
        accelerationStructureGeometry.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
        accelerationStructureGeometry.geometry.triangles.maxVertex    = 3;
        accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(Vertex); // hardcoded for now

        accelerationStructureGeometry.geometry.triangles.indexType    = vk::IndexType::eUint32;

        accelerationStructureGeometry.geometry.triangles.vertexData    = vBufferDeviceAddress;
        accelerationStructureGeometry.geometry.triangles.indexData     = iBufferDeviceAddress;
        accelerationStructureGeometry.geometry.triangles.transformData = tBufferDeviceAddress;

        vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
        buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &accelerationStructureGeometry;

        const uint32_t numTriangles = 1;
        vk::AccelerationStructureBuildSizesInfoKHR buildSizeInfo{};
        m_device.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice,
                &buildInfo,
                &numTriangles,
                &buildSizeInfo);

        // separate fuction needed for this but OKKKK for now
        // creating AS structure buffer
        bufferInfo.size  = buildSizeInfo.accelerationStructureSize;
        bufferInfo.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
            | vk::BufferUsageFlagBits::eShaderDeviceAddress;

        properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        m_blas.buffer = m_device.createBuffer(bufferInfo);
        m_blas.memory = getMemoryForBuffer(m_blas.buffer);

        vk::AccelerationStructureCreateInfoKHR asCreateInfo{};
        asCreateInfo.buffer = m_blas.buffer;
        asCreateInfo.size   = buildSizeInfo.accelerationStructureSize;
        asCreateInfo.type   = vk::AccelerationStructureTypeKHR::eBottomLevel;
        m_blas.handle = m_device.createAccelerationStructureKHR(asCreateInfo);

        // separate fuction needed for this but OKKKK for now
        // creating scratch GPU buffer
        RayTracingScratchBuffer scratchBuffer{};
        bufferInfo.size = buildSizeInfo.buildScratchSize;
        bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        scratchBuffer.handle = m_device.createBuffer(bufferInfo);
        scratchBuffer.memory = getMemoryForBuffer(scratchBuffer.handle);
        m_device.bindBufferMemory(scratchBuffer.handle, scratchBuffer.memory, 0);
        scratchBuffer.deviceAddress = m_device.getBufferAddressKHR(vk::BufferDeviceAddressInfoKHR{scratchBuffer.handle});

        buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
        buildInfo.dstAccelerationStructure = m_blas.handle;
        buildInfo.geometryCount = 1;
        buildInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

        vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
        buildRangeInfo.primitiveCount = numTriangles;
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = { &buildRangeInfo };

        // Build the acceleration structure on the device via a one-time command buffer submission
        vk::CommandBuffer commandBuffer = recordCommandBuffer();
        commandBuffer.buildAccelerationStructuresKHR(1, &buildInfo, buildRangeInfos.data());
        flushCommandBuffer(commandBuffer, m_graphicsQueue); //<- keep having errors... (may be because vulkan.hpp doesnot initialize some stucts)
        m_blas.deviceAddress = m_device.getAccelerationStructureAddressKHR({m_blas.handle});

        m_device.freeMemory(scratchBuffer.memory);
        m_device.destroy(scratchBuffer.handle);
    }

    void Raytracer::createTLAS()
    {
        vk::TransformMatrixKHR transformMatrix = {};

        vk::AccelerationStructureInstanceKHR instance{};
        instance.transform = transformMatrix;
        instance.mask = 0xFF;
        //NOTE: this may fail on futher Vulkan HPP versions (maybe they will add such .hppish flag someday :D)
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = m_blas.deviceAddress;

        struct InstancesBuffer
        {
            vk::Buffer buffer;
            vk::DeviceMemory memory;
            vk::DeviceOrHostAddressConstKHR deviceAddress{};
        } instancesBuffer;

        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
        bufferInfo.size  = sizeof(vk::AccelerationStructureInstanceKHR);
        instancesBuffer.buffer = m_device.createBuffer(bufferInfo);

        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        vk::MemoryAllocateFlagsInfo allocExt{};
        allocExt.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;

        auto getMemoryForBuffer = [&](vk::Buffer& a_buffer)
        {
            vk::MemoryRequirements memReq = m_device.getBufferMemoryRequirements(a_buffer);
            vk::MemoryAllocateInfo allocInfo{};
            allocInfo.allocationSize  = memReq.size;
            allocInfo.pNext = &allocExt;
            for (uint32_t i = 0; i < this->physicalDeviceMemoryProperties.memoryTypeCount; ++i)
            {
                if ((memReq.memoryTypeBits & (1 << i)) && ((this->physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties)
                            == properties))
                {
                    allocInfo.memoryTypeIndex = i;
                    break;
                }
            }

            return m_device.allocateMemory(allocInfo);
        };

        instancesBuffer.memory = getMemoryForBuffer(instancesBuffer.buffer);

        uint8_t *pData = static_cast<uint8_t *>(m_device.mapMemory(instancesBuffer.memory, 0, sizeof(vk::AccelerationStructureInstanceKHR)));
        memcpy(pData, &instance, sizeof(vk::AccelerationStructureInstanceKHR));
        m_device.unmapMemory(instancesBuffer.memory);
        m_device.bindBufferMemory(instancesBuffer.buffer, instancesBuffer.memory, 0);

        instancesBuffer.deviceAddress = m_device.getBufferAddressKHR(vk::BufferDeviceAddressInfoKHR{instancesBuffer.buffer});

        vk::AccelerationStructureGeometryKHR accelerationStructureGeometry{};
        accelerationStructureGeometry.flags        = vk::GeometryFlagBitsKHR::eOpaque;
        accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eInstances;

        accelerationStructureGeometry.geometry.instances.sType           = vk::StructureType::eAccelerationStructureGeometryInstancesDataKHR;
        accelerationStructureGeometry.geometry.instances.arrayOfPointers = false;
        accelerationStructureGeometry.geometry.instances.data            = instancesBuffer.deviceAddress;

        vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
        buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &accelerationStructureGeometry;

        const uint32_t primitive_count = 1;
        vk::AccelerationStructureBuildSizesInfoKHR buildSizeInfo{};
        m_device.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice,
                &buildInfo,
                &primitive_count,
                &buildSizeInfo);

        // separate fuction needed for this but OKKKK for now
        // creating AS structure buffer
        bufferInfo.size  = buildSizeInfo.accelerationStructureSize;
        bufferInfo.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
            | vk::BufferUsageFlagBits::eShaderDeviceAddress;

        properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        m_tlas.buffer = m_device.createBuffer(bufferInfo);
        m_tlas.memory = getMemoryForBuffer(m_tlas.buffer);

        vk::AccelerationStructureCreateInfoKHR asCreateInfo{};
        asCreateInfo.buffer = m_tlas.buffer;
        asCreateInfo.size   = buildSizeInfo.accelerationStructureSize;
        asCreateInfo.type   = vk::AccelerationStructureTypeKHR::eTopLevel;
        m_tlas.handle = m_device.createAccelerationStructureKHR(asCreateInfo);

        // separate fuction needed for this but OKKKK for now
        // creating scratch GPU buffer
        RayTracingScratchBuffer scratchBuffer{};
        bufferInfo.size = buildSizeInfo.buildScratchSize;
        bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        scratchBuffer.handle = m_device.createBuffer(bufferInfo);
        scratchBuffer.memory = getMemoryForBuffer(scratchBuffer.handle);
        m_device.bindBufferMemory(scratchBuffer.handle, scratchBuffer.memory, 0);
        scratchBuffer.deviceAddress = m_device.getBufferAddressKHR(vk::BufferDeviceAddressInfoKHR{scratchBuffer.handle});

        buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
        buildInfo.dstAccelerationStructure = m_tlas.handle;
        buildInfo.geometryCount = 1;
        buildInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

        vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
        buildRangeInfo.primitiveCount = 1;
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = { &buildRangeInfo };

        // Build the acceleration structure on the device via a one-time command buffer submission
        vk::CommandBuffer commandBuffer = recordCommandBuffer();
        //commandBuffer.buildAccelerationStructuresKHR(1, &buildInfo, buildRangeInfos.data());
        flushCommandBuffer(commandBuffer, m_graphicsQueue); // <- keep having errors... (may be because vulkan.hpp doesnot initialize some stucts)
        // TODO: Fine, I'll do it myself
        m_tlas.deviceAddress = m_device.getAccelerationStructureAddressKHR({m_tlas.handle});

        m_device.freeMemory(scratchBuffer.memory);
        m_device.destroy(scratchBuffer.handle);

        m_device.freeMemory(instancesBuffer.memory);
        m_device.destroy(instancesBuffer.buffer);
    }

    void Raytracer::createStorageImage()
    {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.format = this->m_surfaceFormat.format;
        imageInfo.extent.width = this->m_windowWidth;
        imageInfo.extent.height = this->m_windowHeight;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        this->m_rayGenStorage.handle = this->m_device.createImageUnique(imageInfo);

        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        vk::MemoryRequirements memReq = this->m_device.getImageMemoryRequirements(this->m_rayGenStorage.handle.get());
        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.allocationSize  = memReq.size;
        for (uint32_t i = 0; i < this->physicalDeviceMemoryProperties.memoryTypeCount; ++i)
        {
            if ((memReq.memoryTypeBits & (1 << i)) && ((this->physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties)
                        == properties))
            {
                allocInfo.memoryTypeIndex = i;
                break;
            }
        }
        this->m_rayGenStorage.memory = this->m_device.allocateMemoryUnique(allocInfo);
        this->m_device.bindImageMemory(this->m_rayGenStorage.handle.get(), this->m_rayGenStorage.memory.get(), 0);

        vk::ImageViewCreateInfo imageViewInfo{};
        imageViewInfo.viewType = vk::ImageViewType::e2D;
        imageViewInfo.format = this->m_surfaceFormat.format;
        imageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;
        imageViewInfo.image = this->m_rayGenStorage.handle.get();
        this->m_rayGenStorage.imageView = this->m_device.createImageViewUnique(imageViewInfo);

        vk::CommandBuffer commandBuffer = recordCommandBuffer();
        Application::setImageLayout(commandBuffer, this->m_rayGenStorage.handle.get(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
                { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        flushCommandBuffer(commandBuffer, this->m_graphicsQueue);
    }

    void Raytracer::createUniformBuffer()
    {
        //TODO(glTF stage)
    }

    void Raytracer::createRayTracingPipeline()
    {
        vk::DescriptorSetLayoutBinding accelerationStructureLayoutBinding{};
        accelerationStructureLayoutBinding
            .setBinding(0)
            .setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
            .setDescriptorCount(1)
            .setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR);

        vk::DescriptorSetLayoutBinding resultImageLayoutBinding{};
        resultImageLayoutBinding
            .setBinding(1)
            .setDescriptorType(vk::DescriptorType::eStorageImage)
            .setDescriptorCount(1)
            .setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR);

        std::vector<vk::DescriptorSetLayoutBinding> binding{ accelerationStructureLayoutBinding, resultImageLayoutBinding };
        this->descriptorSetLayout = m_device.createDescriptorSetLayoutUnique(
            vk::DescriptorSetLayoutCreateInfo{}
            .setBindings(binding)
        );

        this->pipelineLayout = m_device.createPipelineLayoutUnique(
            vk::PipelineLayoutCreateInfo{}
            .setSetLayouts(descriptorSetLayout.get())
        );

        std::array<vk::PipelineShaderStageCreateInfo, 3> shaderStages{};
        const uint32_t shaderIndexRaygen = 0;
        const uint32_t shaderIndexMiss = 1;
        const uint32_t shaderIndexClosestHit = 2;

        std::vector<vk::UniqueShaderModule> shaderModules{};
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups{};

        shaderModules.push_back(Application::createShaderModule("shaders/basic.rgen.spv"));
        shaderStages[shaderIndexRaygen] =
            vk::PipelineShaderStageCreateInfo{}
            .setStage(vk::ShaderStageFlagBits::eRaygenKHR)
            .setModule(shaderModules.back().get())
            .setPName("main");
        shaderGroups.push_back(
            vk::RayTracingShaderGroupCreateInfoKHR{}
            .setType(vk::RayTracingShaderGroupTypeKHR::eGeneral)
            .setGeneralShader(shaderIndexRaygen)
            .setClosestHitShader(VK_SHADER_UNUSED_KHR)
            .setAnyHitShader(VK_SHADER_UNUSED_KHR)
            .setIntersectionShader(VK_SHADER_UNUSED_KHR)
        );

        shaderModules.push_back(Application::createShaderModule("shaders/basic.rmiss.spv"));
        shaderStages[shaderIndexMiss] =
            vk::PipelineShaderStageCreateInfo{}
            .setStage(vk::ShaderStageFlagBits::eMissKHR)
            .setModule(shaderModules.back().get())
            .setPName("main");
        shaderGroups.push_back(
            vk::RayTracingShaderGroupCreateInfoKHR{}
            .setType(vk::RayTracingShaderGroupTypeKHR::eGeneral)
            .setGeneralShader(shaderIndexMiss)
            .setClosestHitShader(VK_SHADER_UNUSED_KHR)
            .setAnyHitShader(VK_SHADER_UNUSED_KHR)
            .setIntersectionShader(VK_SHADER_UNUSED_KHR)
        );

        shaderModules.push_back(Application::createShaderModule("shaders/basic.rchit.spv"));
        shaderStages[shaderIndexClosestHit] =
            vk::PipelineShaderStageCreateInfo{}
            .setStage(vk::ShaderStageFlagBits::eClosestHitKHR)
            .setModule(shaderModules.back().get())
            .setPName("main");
        shaderGroups.push_back(
            vk::RayTracingShaderGroupCreateInfoKHR{}
            .setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup)
            .setGeneralShader(VK_SHADER_UNUSED_KHR)
            .setClosestHitShader(shaderIndexClosestHit)
            .setAnyHitShader(VK_SHADER_UNUSED_KHR)
            .setIntersectionShader(VK_SHADER_UNUSED_KHR)
        );

        this->shaderGroupsCount = static_cast<uint32_t>(shaderGroups.size());

        auto[result, p] = m_device.createRayTracingPipelineKHRUnique(nullptr, nullptr,
            vk::RayTracingPipelineCreateInfoKHR{}
            .setStages(shaderStages)
            .setGroups(shaderGroups)
            .setMaxPipelineRayRecursionDepth(1)
            .setLayout(this->pipelineLayout.get())
        );

        if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create ray tracing pipeline");
        }
        else
        {
            this->pipeline = std::move(p);
        }
    }

    void Raytracer::createShaderBindingTable()
    {
        auto deviceProperties = this->m_physicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
        auto RTPipelineProperties = deviceProperties.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

        auto alignedSize = [](uint32_t value, uint32_t alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
        };

        const uint32_t handleSize        = RTPipelineProperties.shaderGroupHandleSize;
        const uint32_t handleSizeAligned = alignedSize(handleSize, RTPipelineProperties.shaderGroupHandleAlignment);
        const uint32_t sbtSize           = this->shaderGroupsCount * handleSizeAligned;

        const vk::BufferUsageFlags sbtBufferUsageFlags = vk::BufferUsageFlagBits::eShaderBindingTableKHR
            | vk::BufferUsageFlagBits::eTransferSrc
            | vk::BufferUsageFlagBits::eShaderDeviceAddress;

        const vk::MemoryPropertyFlags sbtMemoryProperty = vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent;

        std::vector<uint8_t> shaderHandles(sbtSize);
        auto result = this->m_device.getRayTracingShaderGroupHandlesKHR(this->pipeline.get(), 0, this->shaderGroupsCount, shaderHandles.size(), shaderHandles.data());
        if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to get ray tracing shader group handles");
        }

        std::vector<std::string> keys = { "rayGen", "miss", "hit" };
        for (auto i{0}; i < keys.size(); ++i)
        {
            sbt.storage.push_back(createBuffer(handleSize, sbtBufferUsageFlags, sbtMemoryProperty, shaderHandles.data() + i * handleSizeAligned));
            sbt.entries[keys[i]] = vk::StridedDeviceAddressRegionKHR{};
            sbt.entries[keys[i]]
                .setDeviceAddress(sbt.storage.back().deviceAddress)
                .setStride(handleSizeAligned)
                .setSize(handleSizeAligned);
        }
    }

    void Raytracer::createDescriptorSets()
    {
        std::vector<vk::DescriptorPoolSize> poolSizes = {
            {vk::DescriptorType::eAccelerationStructureKHR, 1},
            {vk::DescriptorType::eStorageImage, 1}
        };

        this->descriptorPool = this->m_device.createDescriptorPoolUnique(
                vk::DescriptorPoolCreateInfo{}
                .setPoolSizes(poolSizes)
                .setMaxSets(1)
                .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
                );

        auto descriptorSets = this->m_device.allocateDescriptorSetsUnique(
                vk::DescriptorSetAllocateInfo{}
                .setDescriptorPool(this->descriptorPool.get())
                .setSetLayouts(descriptorSetLayout.get())
                );
        this->descriptorSet = std::move(descriptorSets.front());

        vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo{};
        descriptorAccelerationStructureInfo
            .setAccelerationStructures(this->m_tlas.handle);

        vk::WriteDescriptorSet accelerationStructureWrite{};
        accelerationStructureWrite
            .setDstSet(this->descriptorSet.get())
            .setDstBinding(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
            .setPNext(&descriptorAccelerationStructureInfo);

        vk::DescriptorImageInfo imageDescriptor{};
        imageDescriptor
            .setImageView(this->m_rayGenStorage.imageView.get())
            .setImageLayout(vk::ImageLayout::eGeneral);

        vk::WriteDescriptorSet resultImageWrite{};
        resultImageWrite
            .setDstSet(this->descriptorSet.get())
            .setDescriptorType(vk::DescriptorType::eStorageImage)
            .setDstBinding(1)
            .setImageInfo(imageDescriptor);

        this->m_device.updateDescriptorSets({ accelerationStructureWrite, resultImageWrite }, nullptr);
    }

    void Raytracer::recordDrawCommandBuffers()
    {
        this->drawCommandBuffers = createDrawCommandBuffers();
        vk::ImageSubresourceRange subresourceRange{};
        subresourceRange
            .setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setBaseMipLevel(0)
            .setLevelCount(1)
            .setBaseArrayLayer(0)
            .setLayerCount(1);

        for (int32_t i = 0; i < this->drawCommandBuffers.size(); ++i)
        {
            auto& commandBuffer  = this->drawCommandBuffers[i];
            auto& swapChainImage = this->swapChainImages[i];

            commandBuffer->begin(vk::CommandBufferBeginInfo{});

            commandBuffer->bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, this->pipeline.get());

            commandBuffer->bindDescriptorSets(
                    vk::PipelineBindPoint::eRayTracingKHR,
                    this->pipelineLayout.get(),
                    0,
                    this->descriptorSet.get(),
                    nullptr
                    );

            commandBuffer->traceRaysKHR(
                    this->sbt.entries["rayGen"],
                    this->sbt.entries["miss"],
                    this->sbt.entries["hit"],
                    {},
                    this->m_windowWidth, this->m_windowHeight, 1
                    );

            Application::setImageLayout(commandBuffer.get(), this->m_rayGenStorage.handle.get(),
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal, subresourceRange);

            Application::setImageLayout(commandBuffer.get(), swapChainImage,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, subresourceRange);

            commandBuffer->copyImage(
                    this->m_rayGenStorage.handle.get(),
                    vk::ImageLayout::eTransferSrcOptimal,

                    swapChainImage,
                    vk::ImageLayout::eTransferDstOptimal,

                    vk::ImageCopy{}
                    .setSrcSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
                    .setSrcOffset({ 0, 0, 0 })
                    .setDstSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
                    .setDstOffset({ 0, 0, 0 })
                    .setExtent({ this->m_windowWidth, this->m_windowHeight, 1 })
                    );

            Application::setImageLayout(commandBuffer.get(), this->m_rayGenStorage.handle.get(),
                    vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral, subresourceRange);

            Application::setImageLayout(commandBuffer.get(), swapChainImage,
                    vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR, subresourceRange);

            commandBuffer->end();
        }
    }

    void Raytracer::draw()
    {
        uint64_t timeout = 10000000000000;
        if (m_device.waitForFences(this->inFlightFences[this->currentFrame], true, timeout) != vk::Result::eSuccess)
        {
            throw std::runtime_error("drawing failed!");
        }

        auto [result, imageIndex] = this->m_device.acquireNextImageKHR(
            this->m_swapchain.get(),
            timeout,
            imageAvailableSemaphores[this->currentFrame].get()
        );

        if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to acquire next image!");
        }

        if (this->imagesInFlight[imageIndex] != vk::Fence{})
        {
            if (this->m_device.waitForFences(this->imagesInFlight[imageIndex], true, timeout) != vk::Result::eSuccess)
            {
                throw std::runtime_error("drawing failed!");
            }
        }
        this->imagesInFlight[imageIndex] = this->inFlightFences[this->currentFrame];

        this->m_device.resetFences(this->inFlightFences[this->currentFrame]);

        vk::PipelineStageFlags waitStage{vk::PipelineStageFlagBits::eRayTracingShaderKHR};
        vk::SubmitInfo submitInfo{};
        submitInfo
            .setWaitSemaphores(this->imageAvailableSemaphores[this->currentFrame].get())
            .setWaitDstStageMask(waitStage)
            .setCommandBuffers(this->drawCommandBuffers[imageIndex].get())
            .setSignalSemaphores(this->renderFinishedSemaphores[this->currentFrame].get());

        this->m_graphicsQueue.submit(submitInfo, this->inFlightFences[this->currentFrame]);

        Renderer::present(imageIndex);
    }

    Raytracer::Raytracer()
    {
        createBLAS();
        createTLAS();
        createStorageImage();
        createRayTracingPipeline();
        createShaderBindingTable();
        createDescriptorSets();
        recordDrawCommandBuffers();
    }

    // TODO(on abstraction stage): unique handles for vulkan objects to clear up this mess in destructor :)
    Raytracer::~Raytracer()
    {
        m_device.freeMemory(m_blas.memory);
        m_device.destroy(m_blas.buffer);
        m_device.destroy(m_blas.handle);

        m_device.freeMemory(m_tlas.memory);
        m_device.destroy(m_tlas.buffer);
        m_device.destroy(m_tlas.handle);

        m_device.freeMemory(m_scene.vBufferMemory);
        m_device.freeMemory(m_scene.iBufferMemory);
        m_device.freeMemory(m_scene.tBufferMemory);
        m_device.destroy(m_scene.vBuffer);
        m_device.destroy(m_scene.iBuffer);
        m_device.destroy(m_scene.tBuffer);
    }

}

