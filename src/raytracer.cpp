// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "raytracer.hpp"
#include "structures.h"

#include <limits>

namespace vlb {

    void Raytracer::createStorageImage()
    {
        std::array<uint32_t, 3> queueFamilyIndices = {0, 1, 2}; // TODO: this might fail but ok for now
        vk::ImageCreateInfo imageInfo{};
        imageInfo
            .setImageType(vk::ImageType::e2D)
            .setFormat(this->surfaceFormat)
            .setExtent(this->surfaceExtent)
            .setMipLevels(1)
            .setArrayLayers(1)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(vk::ImageTiling::eOptimal)
            .setUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setSharingMode(vk::SharingMode::eConcurrent)
            .setQueueFamilyIndices(queueFamilyIndices);

        this->rayGenStorage.handle = this->device.get().createImageUnique(imageInfo);

        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        vk::MemoryRequirements memReq = this->device.get().getImageMemoryRequirements(this->rayGenStorage.handle.get());
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
        this->rayGenStorage.memory = this->device.get().allocateMemoryUnique(allocInfo);
        this->device.get().bindImageMemory(this->rayGenStorage.handle.get(), this->rayGenStorage.memory.get(), 0);

        vk::ImageViewCreateInfo imageViewInfo{};
        imageViewInfo.viewType = vk::ImageViewType::e2D;
        imageViewInfo.format = this->surfaceFormat;
        imageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;
        imageViewInfo.image = this->rayGenStorage.handle.get();
        this->rayGenStorage.imageView = this->device.get().createImageViewUnique(imageViewInfo);

        vk::CommandBuffer commandBuffer = recordTransferCommandBuffer();
        Application::setImageLayout(commandBuffer, this->rayGenStorage.handle.get(), vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
                { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        flushTransferCommandBuffer(commandBuffer);
    }

    void Raytracer::createResultImageDSLayout()
    {
        vk::DescriptorSetLayoutBinding resultImageLayoutBinding{};
        resultImageLayoutBinding
            .setBinding(0)
            .setDescriptorType(vk::DescriptorType::eStorageImage)
            .setDescriptorCount(1)
            .setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR);

        this->descriptorSetLayout.resultImage = this->device.get().createDescriptorSetLayoutUnique(
                vk::DescriptorSetLayoutCreateInfo{}
                .setBindings(resultImageLayoutBinding)
                );
    }

    void Raytracer::createRayTracingPipeline()
    {
        std::vector<vk::DescriptorSetLayout> layouts;
        layouts.push_back(this->sceneManager.getScene()->getDescriptorSetLayout());
        layouts.push_back(this->descriptorSetLayout.resultImage.get());
        layouts.push_back(this->sceneManager.getCamera()->getDescriptorSetLayout());

        std::vector<vk::PushConstantRange> pushConstants{
            { vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR, 0, sizeof(shader::PushConstant) }
        };

        this->pipelineLayout = this->device.get().createPipelineLayoutUnique(
                vk::PipelineLayoutCreateInfo{}
                .setSetLayouts(layouts)
                .setPushConstantRanges(pushConstants)
                );

        enum StageIndices
        {
            eRaygen,
            eMiss,
            eClosestHit,
            eShaderGroupCount
        };
        std::array<vk::PipelineShaderStageCreateInfo, StageIndices::eShaderGroupCount> shaderStages{};

        std::vector<vk::UniqueShaderModule> shaderModules{};
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups{};

        shaderModules.push_back(Application::createShaderModule("shaders/basic.rgen.spv"));
        shaderStages[StageIndices::eRaygen] = vk::PipelineShaderStageCreateInfo{};
        shaderStages[StageIndices::eRaygen]
            .setStage(vk::ShaderStageFlagBits::eRaygenKHR)
            .setModule(shaderModules.back().get())
            .setPName("main");
        shaderGroups.push_back(
                vk::RayTracingShaderGroupCreateInfoKHR{}
                .setType(vk::RayTracingShaderGroupTypeKHR::eGeneral)
                .setGeneralShader(StageIndices::eRaygen)
                .setClosestHitShader(VK_SHADER_UNUSED_KHR)
                .setAnyHitShader(VK_SHADER_UNUSED_KHR)
                .setIntersectionShader(VK_SHADER_UNUSED_KHR)
                );

        shaderModules.push_back(Application::createShaderModule("shaders/basic.rmiss.spv"));
        shaderStages[StageIndices::eMiss] = vk::PipelineShaderStageCreateInfo{};
        shaderStages[StageIndices::eMiss]
            .setStage(vk::ShaderStageFlagBits::eMissKHR)
            .setModule(shaderModules.back().get())
            .setPName("main");
        shaderGroups.push_back(
                vk::RayTracingShaderGroupCreateInfoKHR{}
                .setType(vk::RayTracingShaderGroupTypeKHR::eGeneral)
                .setGeneralShader(StageIndices::eMiss)
                .setClosestHitShader(VK_SHADER_UNUSED_KHR)
                .setAnyHitShader(VK_SHADER_UNUSED_KHR)
                .setIntersectionShader(VK_SHADER_UNUSED_KHR)
                );

        shaderModules.push_back(Application::createShaderModule("shaders/basic.rchit.spv"));
        shaderStages[StageIndices::eClosestHit] = vk::PipelineShaderStageCreateInfo{};
        shaderStages[StageIndices::eClosestHit]
            .setStage(vk::ShaderStageFlagBits::eClosestHitKHR)
            .setModule(shaderModules.back().get())
            .setPName("main");
        shaderGroups.push_back(
                vk::RayTracingShaderGroupCreateInfoKHR{}
                .setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup)
                .setGeneralShader(VK_SHADER_UNUSED_KHR)
                .setClosestHitShader(StageIndices::eClosestHit)
                .setAnyHitShader(VK_SHADER_UNUSED_KHR)
                .setIntersectionShader(VK_SHADER_UNUSED_KHR)
                );

        this->shaderGroupsCount = static_cast<uint32_t>(StageIndices::eShaderGroupCount);

        auto[result, p] = this->device.get().createRayTracingPipelineKHRUnique(nullptr, nullptr,
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
        auto result = this->device.get().getRayTracingShaderGroupHandlesKHR(this->pipeline.get(), 0, this->shaderGroupsCount, shaderHandles.size(), shaderHandles.data());
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

    void Raytracer::updateSceneDescriptorSets()
    {
        const auto& scene = this->sceneManager.getScene();

        vk::WriteDescriptorSetAccelerationStructureKHR tlasDescriptorInfo{};
        tlasDescriptorInfo
            .setAccelerationStructures(scene->tlas->handle.get());

        vk::WriteDescriptorSet tlasWriteDS{};
        tlasWriteDS
            .setDstSet(this->descriptorSet.scene.get())
            .setDstBinding(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
            .setPNext(&tlasDescriptorInfo);

        vk::DescriptorBufferInfo objDescDescriptorInfo{};
        objDescDescriptorInfo
            .setBuffer(scene->instanceInfoBuffer.handle.get())
            .setRange(VK_WHOLE_SIZE);

        vk::WriteDescriptorSet objDescWriteDS{};
        objDescWriteDS
            .setDstSet(this->descriptorSet.scene.get())
            .setDescriptorType(vk::DescriptorType::eStorageBuffer)
            .setDstBinding(1)
            .setBufferInfo(objDescDescriptorInfo);

        vk::DescriptorBufferInfo materialsDescriptorInfo{};
        materialsDescriptorInfo
            .setBuffer(scene->materialBuffer.handle.get())
            .setRange(VK_WHOLE_SIZE);

        vk::WriteDescriptorSet materialsWriteDS{};
        materialsWriteDS
            .setDstSet(this->descriptorSet.scene.get())
            .setDescriptorType(vk::DescriptorType::eStorageBuffer)
            .setDstBinding(2)
            .setBufferInfo(materialsDescriptorInfo);

        std::vector<vk::DescriptorImageInfo> texturesDescriptorInfo{};
        for (const auto& texture : scene->textures)
        {
            texturesDescriptorInfo.push_back(texture->descriptor);
        }

        vk::WriteDescriptorSet texturesWriteDS{};
        texturesWriteDS
            .setDstSet(this->descriptorSet.scene.get())
            .setDstBinding(3)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setImageInfo(texturesDescriptorInfo);

        this->device.get().updateDescriptorSets(tlasWriteDS, nullptr);
        this->device.get().updateDescriptorSets(objDescWriteDS, nullptr);
        this->device.get().updateDescriptorSets(materialsWriteDS, nullptr);
        if (texturesDescriptorInfo.size())
        {
            this->device.get().updateDescriptorSets(texturesWriteDS, nullptr);
        }
    }

    void Raytracer::updateResultImageDescriptorSets()
    {
        vk::DescriptorImageInfo imageDescriptorInfo{};
        imageDescriptorInfo
            .setImageView(this->rayGenStorage.imageView.get())
            .setImageLayout(vk::ImageLayout::eGeneral);

        vk::WriteDescriptorSet resultImageWrite{};
        resultImageWrite
            .setDstSet(this->descriptorSet.resultImage.get())
            .setDescriptorType(vk::DescriptorType::eStorageImage)
            .setDstBinding(0)
            .setImageInfo(imageDescriptorInfo);

        this->device.get().updateDescriptorSets(resultImageWrite, nullptr);
    }

    void Raytracer::createDescriptorSets()
    {
        std::vector<vk::DescriptorPoolSize> poolSizes = {
            {vk::DescriptorType::eAccelerationStructureKHR, 1},
            {vk::DescriptorType::eStorageImage, 1},
            {vk::DescriptorType::eUniformBuffer, 1},
            {vk::DescriptorType::eStorageBuffer, 2},
            {vk::DescriptorType::eCombinedImageSampler, 1000}
        };

        this->descriptorPool = this->device.get().createDescriptorPoolUnique(
                vk::DescriptorPoolCreateInfo{}
                .setPoolSizes(poolSizes)
                .setMaxSets(2)
                .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
                    | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
                );

        {
            auto sceneLayout = this->sceneManager.getScene()->getDescriptorSetLayout();
            auto sceneDS = this->device.get().allocateDescriptorSetsUnique(
                    vk::DescriptorSetAllocateInfo{}
                    .setDescriptorPool(this->descriptorPool.get())
                    .setSetLayouts(sceneLayout)
                    );

            this->descriptorSet.scene = std::move(sceneDS.front());
        }

        {
            auto resultImageDS = this->device.get().allocateDescriptorSetsUnique(
                    vk::DescriptorSetAllocateInfo{}
                    .setDescriptorPool(this->descriptorPool.get())
                    .setSetLayouts(descriptorSetLayout.resultImage.get())
                    );

            this->descriptorSet.resultImage = std::move(resultImageDS.front());
        }

        updateSceneDescriptorSets();
        updateResultImageDescriptorSets();
    }

    void Raytracer::recordDrawCommandBuffer(uint64_t imageIndex)
    {
        if (this->sceneManager.sceneChanged())
        {
            handleSceneChange();
        }

        auto& commandBuffer    = this->drawCommandBuffers[imageIndex];
        auto& swapChainImage   = this->swapChainImages[imageIndex];

        commandBuffer->begin(vk::CommandBufferBeginInfo{}
                .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

        commandBuffer->bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, this->pipeline.get());

        std::vector<vk::DescriptorSet> descriptorSets(0);
        descriptorSets.push_back(this->descriptorSet.scene.get());
        descriptorSets.push_back(this->descriptorSet.resultImage.get());
        descriptorSets.push_back(this->sceneManager.getCamera()->update()->getDescriptorSet(imageIndex));

        commandBuffer->bindDescriptorSets(
                vk::PipelineBindPoint::eRayTracingKHR,
                this->pipelineLayout.get(),
                0,
                descriptorSets,
                nullptr
                );

        static int frameNumber = 0;
        shader::PushConstant pc { this->ui.getLightIntensity(), static_cast<int>(frameNumber++) };

        commandBuffer->pushConstants(
                this->pipelineLayout.get(),
                vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR,
                0, sizeof(pc), &pc
                );

        auto[width, height, depth] = this->surfaceExtent;
        commandBuffer->traceRaysKHR(
                this->sbt.entries["rayGen"],
                this->sbt.entries["miss"],
                this->sbt.entries["hit"],
                {},
                width, height, depth
                );

        vk::ImageSubresourceRange subresourceRange{};
        subresourceRange
            .setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setBaseMipLevel(0)
            .setLevelCount(1)
            .setBaseArrayLayer(0)
            .setLayerCount(1);

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

        ui.draw(imageIndex, commandBuffer.get());

        commandBuffer->end();
    }

    void Raytracer::draw()
    {
        auto timeout = std::numeric_limits<uint64_t>::max();
        this->device.get().waitForFences(this->inFlightFences[this->currentFrame], true, timeout);
        this->device.get().resetFences(this->inFlightFences[this->currentFrame]);

        auto [result, imageIndex] = this->device.get().acquireNextImageKHR(
                this->swapchain.get(),
                timeout,
                imageAvailableSemaphores[this->currentFrame].get() );

        if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to acquire next image!");
        }

        if (this->imagesInFlight[imageIndex] != vk::Fence{})
        {
            this->device.get().waitForFences(this->imagesInFlight[imageIndex], true, timeout);
        }
        this->imagesInFlight[imageIndex] = this->inFlightFences[this->currentFrame];

        recordDrawCommandBuffer(imageIndex);

        vk::PipelineStageFlags waitStage{vk::PipelineStageFlagBits::eRayTracingShaderKHR};
        vk::SubmitInfo submitInfo{};
        submitInfo
            .setWaitSemaphores(this->imageAvailableSemaphores[this->currentFrame].get())
            .setWaitDstStageMask(waitStage)
            .setCommandBuffers(this->drawCommandBuffers[imageIndex].get())
            .setSignalSemaphores(this->renderFinishedSemaphores[this->currentFrame].get());

        this->queue.graphics.submit(submitInfo, this->inFlightFences[this->currentFrame]);

        Renderer::present(imageIndex);
    }

    void Raytracer::handleSceneChange()
    {
        this->device.get().waitIdle();
        createRayTracingPipeline();
        createShaderBindingTable();
        this->descriptorSet.scene.reset();
        this->descriptorSet.resultImage.reset();
        this->descriptorPool.reset();
        createDescriptorSets();
    }

    Raytracer::Raytracer()
    {
        createStorageImage();
        createResultImageDSLayout();

        createRayTracingPipeline();
        createShaderBindingTable();
        createDescriptorSets();

        this->drawCommandBuffers = Renderer::createDrawCommandBuffers();
    }
}

