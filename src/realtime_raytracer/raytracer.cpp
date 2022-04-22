// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "raytracer.hpp"
#include "structures.h"

#include <limits>

namespace vlb {

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

        const std::vector<vk::PushConstantRange> constants{
            { vk::ShaderStageFlagBits::eClosestHitKHR, 0, sizeof(Raytracer::Constants) }
        };

        this->pipelineLayout = this->device.get().createPipelineLayoutUnique(
                vk::PipelineLayoutCreateInfo{}
                .setSetLayouts(layouts)
                .setPushConstantRanges(constants)
                );

        enum StageIndices
        {
            eRaygen,
            eMiss,
            eShadow,
            eSH,
            eClosestHit,
            eShaderGroupCount
        };
        std::array<vk::PipelineShaderStageCreateInfo, StageIndices::eShaderGroupCount> shaderStages{};

        std::vector<vk::UniqueShaderModule> shaderModules{};
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups{};

        vk::RayTracingShaderGroupCreateInfoKHR groupTemplate{};
        groupTemplate
            .setType(vk::RayTracingShaderGroupTypeKHR::eGeneral)
            .setClosestHitShader(VK_SHADER_UNUSED_KHR)
            .setAnyHitShader(VK_SHADER_UNUSED_KHR)
            .setIntersectionShader(VK_SHADER_UNUSED_KHR);

        shaderModules.push_back(Application::createShaderModule("shaders/main.rgen.spv"));
        shaderStages[StageIndices::eRaygen] = vk::PipelineShaderStageCreateInfo{};
        shaderStages[StageIndices::eRaygen]
            .setStage(vk::ShaderStageFlagBits::eRaygenKHR)
            .setModule(shaderModules.back().get())
            .setPName("main");
        shaderGroups.push_back(groupTemplate.setGeneralShader(StageIndices::eRaygen));

        shaderModules.push_back(Application::createShaderModule("shaders/main.rmiss.spv"));
        shaderStages[StageIndices::eMiss] = vk::PipelineShaderStageCreateInfo{};
        shaderStages[StageIndices::eMiss]
            .setStage(vk::ShaderStageFlagBits::eMissKHR)
            .setModule(shaderModules.back().get())
            .setPName("main");
        shaderGroups.push_back(groupTemplate.setGeneralShader(StageIndices::eMiss));

        shaderModules.push_back(Application::createShaderModule("shaders/shadow.rmiss.spv"));
        shaderStages[StageIndices::eShadow] = vk::PipelineShaderStageCreateInfo{};
        shaderStages[StageIndices::eShadow]
            .setStage(vk::ShaderStageFlagBits::eMissKHR)
            .setModule(shaderModules.back().get())
            .setPName("main");
        shaderGroups.push_back(groupTemplate.setGeneralShader(StageIndices::eShadow));

        shaderModules.push_back(Application::createShaderModule("shaders/sh.rmiss.spv"));
        shaderStages[StageIndices::eSH] = vk::PipelineShaderStageCreateInfo{};
        shaderStages[StageIndices::eSH]
            .setStage(vk::ShaderStageFlagBits::eMissKHR)
            .setModule(shaderModules.back().get())
            .setPName("main");
        shaderGroups.push_back(groupTemplate.setGeneralShader(StageIndices::eSH));

        shaderModules.push_back(Application::createShaderModule("shaders/main.rchit.spv"));
        shaderStages[StageIndices::eClosestHit] = vk::PipelineShaderStageCreateInfo{};
        shaderStages[StageIndices::eClosestHit]
            .setStage(vk::ShaderStageFlagBits::eClosestHitKHR)
            .setModule(shaderModules.back().get())
            .setPName("main");
        shaderGroups.push_back(
                groupTemplate
                .setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup)
                .setGeneralShader(VK_SHADER_UNUSED_KHR)
                .setClosestHitShader(StageIndices::eClosestHit)
                );

        this->shaderGroupsCount = static_cast<uint32_t>(StageIndices::eShaderGroupCount);
        auto[result, p] = this->device.get().createRayTracingPipelineKHRUnique(nullptr, nullptr,
                vk::RayTracingPipelineCreateInfoKHR{}
                .setStages(shaderStages)
                .setGroups(shaderGroups)
                .setMaxPipelineRayRecursionDepth(2)
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

        auto sceneLayout = this->sceneManager.getScene()->getDescriptorSetLayout();
        this->descriptorSet.scene = std::move(this->device.get().allocateDescriptorSetsUnique(
                    vk::DescriptorSetAllocateInfo{}
                    .setDescriptorPool(this->descriptorPool.get())
                    .setSetLayouts(sceneLayout)
                    ).front());

        this->descriptorSet.resultImage = std::move(this->device.get().allocateDescriptorSetsUnique(
                    vk::DescriptorSetAllocateInfo{}
                    .setDescriptorPool(this->descriptorPool.get())
                    .setSetLayouts(descriptorSetLayout.resultImage.get())
                    ).front());

        this->sceneManager.getScene()->updateSceneDescriptorSets(this->descriptorSet.scene.get());
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

        this->constants.lighting = this->ui.getLighing();

        commandBuffer->pushConstants(
                this->pipelineLayout.get(),
                vk::ShaderStageFlagBits::eClosestHitKHR,
                0, sizeof(Raytracer::Constants), &this->constants
                );

        auto[width, height, depth] = this->surfaceExtent;
        commandBuffer->traceRaysKHR(
                this->sbt.strides[0],
                this->sbt.strides[1],
                this->sbt.strides[2],
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

    void Raytracer::createShaderBindingTable()
    {
        this->sbt = Application::createShaderBindingTable(this->pipeline.get(), 3u, 1u);
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
        this->rayGenStorage = createImage(this->surfaceFormat, this->surfaceExtent);
        createResultImageDSLayout();

        createRayTracingPipeline();
        createShaderBindingTable();
        createDescriptorSets();

        this->drawCommandBuffers = Renderer::createDrawCommandBuffers();
    }
}

