// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "raytracer.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace vlb {

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
        size_t vBufferSize = sizeof(Vertex)   * vertices.size();
        size_t iBufferSize = sizeof(uint32_t) * indices.size();

        vk::BufferUsageFlags bufferUsage{ vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
                                        | vk::BufferUsageFlagBits::eShaderDeviceAddress
                                        | vk::BufferUsageFlagBits::eStorageBuffer };
        vk::MemoryPropertyFlags memoryProperty{ vk::MemoryPropertyFlagBits::eHostVisible
                                              | vk::MemoryPropertyFlagBits::eHostCoherent };

        Buffer vBuffer = Application::createBuffer(vBufferSize, bufferUsage, memoryProperty, vertices.data());
        Buffer iBuffer = Application::createBuffer(iBufferSize, bufferUsage, memoryProperty, indices.data());

        /////////////////////////
        // Hello triangle
        /////////////////////////

        vk::AccelerationStructureGeometryTrianglesDataKHR triangleData{};
        triangleData
            .setVertexFormat(vk::Format::eR32G32B32Sfloat)
            .setVertexStride(sizeof(Vertex))
            .setMaxVertex(vertices.size())
            .setIndexType(vk::IndexType::eUint32)
            .setVertexData(vBuffer.deviceAddress)
            .setIndexData(iBuffer.deviceAddress);

        vk::AccelerationStructureGeometryKHR geometry{};
        geometry
            .setGeometryType(vk::GeometryTypeKHR::eTriangles)
            .setGeometry({triangleData})
            .setFlags(vk::GeometryFlagBitsKHR::eOpaque);

        vk::AccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
        buildGeometryInfo
            .setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
            .setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
            .setGeometries(geometry);

        const uint32_t primitiveCount = 1;
        auto buildSizesInfo = this->device.getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice, buildGeometryInfo, primitiveCount);

        bufferUsage = { vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
            | vk::BufferUsageFlagBits::eShaderDeviceAddress };

        this->blas.buffer = Application::createBuffer(buildSizesInfo.accelerationStructureSize, bufferUsage, memoryProperty);

        this->blas.handle = this->device.createAccelerationStructureKHRUnique(
            vk::AccelerationStructureCreateInfoKHR{}
            .setBuffer(this->blas.buffer.handle.get())
            .setSize(buildSizesInfo.accelerationStructureSize)
            .setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
        );

        bufferUsage = { vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress };
        memoryProperty = { vk::MemoryPropertyFlagBits::eDeviceLocal };
        Buffer scratchBuffer = Application::createBuffer(buildSizesInfo.buildScratchSize, bufferUsage, memoryProperty);

        vk::AccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
        accelerationBuildGeometryInfo
            .setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
            .setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
            .setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
            .setDstAccelerationStructure(this->blas.handle.get())
            .setGeometries(geometry)
            .setScratchData(scratchBuffer.deviceAddress);

        vk::AccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
        accelerationStructureBuildRangeInfo
            .setPrimitiveCount(1)
            .setPrimitiveOffset(0)
            .setFirstVertex(0)
            .setTransformOffset(0);

        auto commandBuffer = Application::recordCommandBuffer();
        commandBuffer.buildAccelerationStructuresKHR(accelerationBuildGeometryInfo, &accelerationStructureBuildRangeInfo);
        flushCommandBuffer(commandBuffer, this->graphicsQueue);

        this->blas.buffer.deviceAddress = this->device.getAccelerationStructureAddressKHR({ this->blas.handle.get() });
    }

    void Raytracer::createTLAS()
    {
        VkTransformMatrixKHR transformMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f };

        vk::AccelerationStructureInstanceKHR accelerationStructureInstance{};
        accelerationStructureInstance
            .setTransform(transformMatrix)
            .setInstanceCustomIndex(0)
            .setMask(0xFF)
            .setInstanceShaderBindingTableRecordOffset(0)
            .setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
            .setAccelerationStructureReference(this->blas.buffer.deviceAddress);

        Buffer instancesBuffer = createBuffer(
            sizeof(vk::AccelerationStructureInstanceKHR),
            vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
            | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            &accelerationStructureInstance);

        vk::AccelerationStructureGeometryInstancesDataKHR instancesData{};
        instancesData
            .setArrayOfPointers(false)
            .setData(instancesBuffer.deviceAddress);

        vk::AccelerationStructureGeometryKHR geometry{};
        geometry
            .setGeometryType(vk::GeometryTypeKHR::eInstances)
            .setGeometry({instancesData})
            .setFlags(vk::GeometryFlagBitsKHR::eOpaque);

        vk::AccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
        buildGeometryInfo
            .setType(vk::AccelerationStructureTypeKHR::eTopLevel)
            .setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
            .setGeometries(geometry);

        const uint32_t primitiveCount = 1;
        auto buildSizesInfo = this->device.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice, buildGeometryInfo, primitiveCount);

        vk::BufferUsageFlags bufferUsage = { vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
            | vk::BufferUsageFlagBits::eShaderDeviceAddress };
        vk::MemoryPropertyFlags memoryProperty{ vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent };

        this->tlas.buffer = Application::createBuffer(buildSizesInfo.accelerationStructureSize, bufferUsage, memoryProperty);

        this->tlas.handle = this->device.createAccelerationStructureKHRUnique(
                vk::AccelerationStructureCreateInfoKHR{}
                .setBuffer(this->tlas.buffer.handle.get())
                .setSize(buildSizesInfo.accelerationStructureSize)
                .setType(vk::AccelerationStructureTypeKHR::eTopLevel)
                );

        bufferUsage = { vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress };
        memoryProperty = { vk::MemoryPropertyFlagBits::eDeviceLocal };
        Buffer scratchBuffer = Application::createBuffer(buildSizesInfo.buildScratchSize, bufferUsage, memoryProperty);

        vk::AccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
        accelerationBuildGeometryInfo
            .setType(vk::AccelerationStructureTypeKHR::eTopLevel)
            .setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
            .setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
            .setDstAccelerationStructure(this->tlas.handle.get())
            .setGeometries(geometry)
            .setScratchData(scratchBuffer.deviceAddress);

        vk::AccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
        accelerationStructureBuildRangeInfo
            .setPrimitiveCount(1)
            .setPrimitiveOffset(0)
            .setFirstVertex(0)
            .setTransformOffset(0);

        auto commandBuffer = Application::recordCommandBuffer();
        commandBuffer.buildAccelerationStructuresKHR(accelerationBuildGeometryInfo, &accelerationStructureBuildRangeInfo);
        flushCommandBuffer(commandBuffer, this->graphicsQueue);

        this->tlas.buffer.deviceAddress = this->device.getAccelerationStructureAddressKHR({ this->tlas.handle.get() });
    }

    void Raytracer::createStorageImage()
    {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.format = this->surfaceFormat;
        imageInfo.extent = this->surfaceExtent;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        this->rayGenStorage.handle = this->device.createImageUnique(imageInfo);

        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        vk::MemoryRequirements memReq = this->device.getImageMemoryRequirements(this->rayGenStorage.handle.get());
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
        this->rayGenStorage.memory = this->device.allocateMemoryUnique(allocInfo);
        this->device.bindImageMemory(this->rayGenStorage.handle.get(), this->rayGenStorage.memory.get(), 0);

        vk::ImageViewCreateInfo imageViewInfo{};
        imageViewInfo.viewType = vk::ImageViewType::e2D;
        imageViewInfo.format = this->surfaceFormat;
        imageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;
        imageViewInfo.image = this->rayGenStorage.handle.get();
        this->rayGenStorage.imageView = this->device.createImageViewUnique(imageViewInfo);

        vk::CommandBuffer commandBuffer = recordCommandBuffer();
        Application::setImageLayout(commandBuffer, this->rayGenStorage.handle.get(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
                { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        flushCommandBuffer(commandBuffer, this->graphicsQueue);
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
        this->descriptorSetLayout = device.createDescriptorSetLayoutUnique(
                vk::DescriptorSetLayoutCreateInfo{}
                .setBindings(binding)
                );

        this->pipelineLayout = device.createPipelineLayoutUnique(
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

        auto[result, p] = device.createRayTracingPipelineKHRUnique(nullptr, nullptr,
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
        auto deviceProperties = this->physicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
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
        auto result = this->device.getRayTracingShaderGroupHandlesKHR(this->pipeline.get(), 0, this->shaderGroupsCount, shaderHandles.size(), shaderHandles.data());
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

        this->descriptorPool = this->device.createDescriptorPoolUnique(
                vk::DescriptorPoolCreateInfo{}
                .setPoolSizes(poolSizes)
                .setMaxSets(1)
                .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
                );

        auto descriptorSets = this->device.allocateDescriptorSetsUnique(
                vk::DescriptorSetAllocateInfo{}
                .setDescriptorPool(this->descriptorPool.get())
                .setSetLayouts(descriptorSetLayout.get())
                );
        this->descriptorSet = std::move(descriptorSets.front());

        vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo{};
        descriptorAccelerationStructureInfo
            .setAccelerationStructures(this->tlas.handle.get());

        vk::WriteDescriptorSet accelerationStructureWrite{};
        accelerationStructureWrite
            .setDstSet(this->descriptorSet.get())
            .setDstBinding(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
            .setPNext(&descriptorAccelerationStructureInfo);

        vk::DescriptorImageInfo imageDescriptor{};
        imageDescriptor
            .setImageView(this->rayGenStorage.imageView.get())
            .setImageLayout(vk::ImageLayout::eGeneral);

        vk::WriteDescriptorSet resultImageWrite{};
        resultImageWrite
            .setDstSet(this->descriptorSet.get())
            .setDescriptorType(vk::DescriptorType::eStorageImage)
            .setDstBinding(1)
            .setImageInfo(imageDescriptor);

        this->device.updateDescriptorSets({ accelerationStructureWrite, resultImageWrite }, nullptr);
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

            auto[width, height, depth] = this->surfaceExtent;
            commandBuffer->traceRaysKHR(
                    this->sbt.entries["rayGen"],
                    this->sbt.entries["miss"],
                    this->sbt.entries["hit"],
                    {},
                    width, height, depth
                    );

            Application::setImageLayout(commandBuffer.get(), this->rayGenStorage.handle.get(),
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal, subresourceRange);

            Application::setImageLayout(commandBuffer.get(), swapChainImage,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, subresourceRange);

            commandBuffer->copyImage(
                    this->rayGenStorage.handle.get(),
                    vk::ImageLayout::eTransferSrcOptimal,

                    swapChainImage,
                    vk::ImageLayout::eTransferDstOptimal,

                    vk::ImageCopy{}
                    .setSrcSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
                    .setSrcOffset({ 0, 0, 0 })
                    .setDstSubresource({ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
                    .setDstOffset({ 0, 0, 0 })
                    .setExtent(this->surfaceExtent)
                    );

            Application::setImageLayout(commandBuffer.get(), this->rayGenStorage.handle.get(),
                    vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral, subresourceRange);

            Application::setImageLayout(commandBuffer.get(), swapChainImage,
                    vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR, subresourceRange);

            std::array<vk::ClearValue, 2> clearValues;
            clearValues[0].color        = vk::ClearColorValue( std::array<float, 4>( { { 0.2f, 0.2f, 0.2f, 0.2f } } ) );
            clearValues[1].depthStencil = vk::ClearDepthStencilValue( 1.0f, 0 );

            vk::RenderPassBeginInfo renderPassBeginInfo{};
            renderPassBeginInfo
                .setRenderPass(this->imguiPass.get())
                .setFramebuffer(this->imguiFrameBuffers[i].get())
                .setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(width, height)))
                .setClearValues(clearValues);

            commandBuffer->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::ShowDemoWindow();
            ImGui::Render();
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.get());
            commandBuffer->endRenderPass();

            commandBuffer->end();
        }
    }

    void Raytracer::draw()
    {
        uint64_t timeout = 10000000000000;
        if (device.waitForFences(this->inFlightFences[this->currentFrame], true, timeout) != vk::Result::eSuccess)
        {
            throw std::runtime_error("drawing failed!");
        }

        auto [result, imageIndex] = this->device.acquireNextImageKHR(
                this->swapchain.get(),
                timeout,
                imageAvailableSemaphores[this->currentFrame].get()
                );

        if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to acquire next image!");
        }

        if (this->imagesInFlight[imageIndex] != vk::Fence{})
        {
            if (this->device.waitForFences(this->imagesInFlight[imageIndex], true, timeout) != vk::Result::eSuccess)
            {
                throw std::runtime_error("drawing failed!");
            }
        }
        this->imagesInFlight[imageIndex] = this->inFlightFences[this->currentFrame];

        this->device.resetFences(this->inFlightFences[this->currentFrame]);

        vk::PipelineStageFlags waitStage{vk::PipelineStageFlagBits::eRayTracingShaderKHR};
        vk::SubmitInfo submitInfo{};
        submitInfo
            .setWaitSemaphores(this->imageAvailableSemaphores[this->currentFrame].get())
            .setWaitDstStageMask(waitStage)
            .setCommandBuffers(this->drawCommandBuffers[imageIndex].get())
            .setSignalSemaphores(this->renderFinishedSemaphores[this->currentFrame].get());

        this->graphicsQueue.submit(submitInfo, this->inFlightFences[this->currentFrame]);

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
}

