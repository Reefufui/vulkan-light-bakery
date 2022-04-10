// created in 2022 by Andrey Treefonov https://github.com/Reefufui

#include "env_map_generator.hpp"
#include "structures.h"

#include <stb_image.h>
#include <stb_image_write.h>

namespace vlb {

    EnvMapGenerator::EnvMapGenerator()
    {
    }

    EnvMapGenerator::~EnvMapGenerator()
    {
    }

    void EnvMapGenerator::setScene(Scene&& scene)
    {
        this->scene = scene;
    }

    void EnvMapGenerator::passVulkanResources(VulkanResources& info)
    {
        this->device               = info.device;
        this->physicalDevice       = info.physicalDevice;
        this->queue.graphics       = info.graphicsQueue;
        this->commandPool.graphics = info.graphicsCommandPool;
        this->queue.transfer       = info.transferQueue       ? info.transferQueue       : info.graphicsQueue;
        this->commandPool.transfer = info.transferCommandPool ? info.transferCommandPool : info.graphicsCommandPool;
    }

    void EnvMapGenerator::loadDebugMapFromPNG(const char* filename, unsigned char** texels)
    {
        int width{};
        int height{};
        int texChannels{};

        stbi_uc* texelsPtr{};
        texelsPtr = stbi_load(filename, &width, &height, &texChannels, STBI_rgb_alpha);

        if (!texelsPtr)
        {
            throw std::runtime_error(std::string("Could not load texture: ") + std::string(filename));
        }

        this->envMapExtent.width  = width;
        this->envMapExtent.height = height;
        this->envMapExtent.depth  = 1;

        *texels = new unsigned char[width * height * 4];
        memcpy(*texels, texelsPtr, width * height * 4);
        stbi_image_free(texelsPtr);
    }

    Application::Image& EnvMapGenerator::getImage()
    {
        return this->envMap;
    }

    vk::Extent3D EnvMapGenerator::getImageExtent()
    {
        return this->envMapExtent;
    }

    void EnvMapGenerator::saveImage()
    {
        using enum vk::BufferUsageFlagBits;
        using enum vk::MemoryPropertyFlagBits;
        vk::BufferUsageFlags    usg  = eTransferDst;
        vk::MemoryPropertyFlags mem  = eHostVisible | eHostCoherent;
        vk::DeviceSize          size = static_cast<vk::DeviceSize>(this->envMapExtent.width * this->envMapExtent.height * 4);

        Application::Buffer staging = Application::createBuffer(this->device, this->physicalDevice, size, usg, mem);

        auto cmd = Application::recordCommandBuffer(this->device, this->commandPool.transfer);

        Application::setImageLayout(cmd, this->envMap.handle.get(), vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal,
                { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

        vk::ImageSubresourceLayers layers{};
        layers
            .setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setMipLevel(0)
            .setBaseArrayLayer(0)
            .setLayerCount(1);

        vk::BufferImageCopy region{};
        region
            .setBufferOffset(0)
            .setImageOffset(vk::Offset3D{})
            .setImageExtent(this->envMapExtent)
            .setImageSubresource(layers);

        cmd.copyImageToBuffer(this->envMap.handle.get(), vk::ImageLayout::eTransferSrcOptimal, staging.handle.get(), 1, &region);

        Application::setImageLayout(cmd, this->envMap.handle.get(), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral,
                { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

        Application::flushCommandBuffer(this->device, this->commandPool.transfer, cmd, this->queue.transfer);

        void* dataPtr = this->device.mapMemory(staging.memory.get(), 0, size);
        stbi_write_png("result.png", this->envMapExtent.width, this->envMapExtent.height, 4, (void*)dataPtr, this->envMapExtent.width * 4);
        device.unmapMemory(staging.memory.get());
    }

    Application::Image& EnvMapGenerator::createImage(vk::Format imageFormat, vk::Extent3D imageExtent)
    {
        this->envMapFormat = imageFormat;
        this->envMapExtent = imageExtent;
        this->envMap = Application::createImage(this->device, this->physicalDevice, imageFormat, imageExtent,
                this->commandPool.transfer, this->queue.transfer);
        updateImageDescriptorSet();
        return this->envMap;
    }

    Application::Image& EnvMapGenerator::createImage(vk::Format imageFormat, const char* filename)
    {
        unsigned char* texels = nullptr;
        loadDebugMapFromPNG(filename, &texels);

        this->envMapFormat = imageFormat;
        this->envMap = Application::createImage(this->device, this->physicalDevice, imageFormat, this->envMapExtent,
                this->commandPool.transfer, this->queue.transfer);

        using enum vk::BufferUsageFlagBits;
        using enum vk::MemoryPropertyFlagBits;
        vk::BufferUsageFlags    usg  = eTransferSrc;
        vk::MemoryPropertyFlags mem  = eHostVisible | eHostCoherent;
        vk::DeviceSize          size = static_cast<vk::DeviceSize>(this->envMapExtent.width * this->envMapExtent.height * 4);

        Application::Buffer staging = Application::createBuffer(this->device, this->physicalDevice, size, usg, mem, texels);

        auto cmd = Application::recordCommandBuffer(this->device, this->commandPool.transfer);

        Application::setImageLayout(cmd, this->envMap.handle.get(), vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferDstOptimal,
                { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

        vk::ImageSubresourceLayers layers{};
        layers
            .setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setMipLevel(0)
            .setBaseArrayLayer(0)
            .setLayerCount(1);

        vk::BufferImageCopy region{};
        region
            .setBufferOffset(0)
            .setImageOffset(vk::Offset3D{})
            .setImageExtent(this->envMapExtent)
            .setImageSubresource(layers);

        cmd.copyBufferToImage(staging.handle.get(), this->envMap.handle.get(), vk::ImageLayout::eTransferDstOptimal, 1, &region);

        Application::setImageLayout(cmd, this->envMap.handle.get(), vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral,
               { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

        Application::flushCommandBuffer(this->device, this->commandPool.transfer, cmd, this->queue.transfer);

        free(texels);
        updateImageDescriptorSet();

        return this->envMap;
    }

    void EnvMapGenerator::createRayTracingPipeline()
    {
        // POOL
        std::vector<vk::DescriptorPoolSize> poolSizes = {
            {vk::DescriptorType::eAccelerationStructureKHR, 1},
            {vk::DescriptorType::eStorageImage, 1},
            {vk::DescriptorType::eUniformBuffer, 1},
            {vk::DescriptorType::eStorageBuffer, 2},
            {vk::DescriptorType::eCombinedImageSampler, 1000}
        };

        this->descriptorPool = this->device.createDescriptorPoolUnique(
                vk::DescriptorPoolCreateInfo{}
                .setPoolSizes(poolSizes)
                .setMaxSets(2)
                .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
                    | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
                );

        // LAYOUT
        vk::DescriptorSetLayoutBinding resultImageLayoutBinding{};
        resultImageLayoutBinding
            .setBinding(0)
            .setDescriptorType(vk::DescriptorType::eStorageImage)
            .setDescriptorCount(1)
            .setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR);

        this->imageLayout = this->device.createDescriptorSetLayoutUnique(
                vk::DescriptorSetLayoutCreateInfo{}
                .setBindings(resultImageLayoutBinding)
                );
        auto sceneLayout = this->scene->getDescriptorSetLayout();

        // DESCRIPOR
        this->sceneDS = std::move(this->device.allocateDescriptorSetsUnique(
                    vk::DescriptorSetAllocateInfo{}
                    .setDescriptorPool(this->descriptorPool.get())
                    .setSetLayouts(sceneLayout)
                    ).front());
        this->imageDS = std::move(this->device.allocateDescriptorSetsUnique(
                    vk::DescriptorSetAllocateInfo{}
                    .setDescriptorPool(this->descriptorPool.get())
                    .setSetLayouts(imageLayout.get())
                    ).front());

        // PIPELINE LAYOUT
        const std::vector<vk::PushConstantRange> pushConstants{
            { vk::ShaderStageFlagBits::eRaygenKHR, 0, sizeof(glm::vec3) }
        };
        const std::array<vk::DescriptorSetLayout, 2> layouts { sceneLayout, imageLayout.get()};

        this->pipelineLayout = this->device.createPipelineLayoutUnique(
                vk::PipelineLayoutCreateInfo{}
                .setSetLayouts(layouts)
                .setPushConstantRanges(pushConstants)
                );

        // SHADERS
        enum StageIndices
        {
            eRaygen,
            eMiss,
            eClosestHit,
            eShadow,
            eShaderGroupCount
        };
        this->shaderGroupsCount = static_cast<uint32_t>(StageIndices::eShaderGroupCount);
        std::array<vk::PipelineShaderStageCreateInfo, StageIndices::eShaderGroupCount> shaderStages{};

        std::vector<vk::UniqueShaderModule> shaderModules{};
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups{};

        shaderModules.push_back(Application::createShaderModule(this->device, "shaders/env_map.rgen.spv"));
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

        shaderModules.push_back(Application::createShaderModule(this->device, "shaders/basic.rmiss.spv"));
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

        shaderModules.push_back(Application::createShaderModule(this->device, "shaders/basic.rchit.spv"));
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

        shaderModules.push_back(Application::createShaderModule(this->device, "shaders/shadow.rmiss.spv"));
        shaderStages[StageIndices::eShadow] = vk::PipelineShaderStageCreateInfo{};
        shaderStages[StageIndices::eShadow]
            .setStage(vk::ShaderStageFlagBits::eMissKHR)
            .setModule(shaderModules.back().get())
            .setPName("main");
        shaderGroups.push_back(
                vk::RayTracingShaderGroupCreateInfoKHR{}
                .setType(vk::RayTracingShaderGroupTypeKHR::eGeneral)
                .setGeneralShader(StageIndices::eShadow)
                .setClosestHitShader(VK_SHADER_UNUSED_KHR)
                .setAnyHitShader(VK_SHADER_UNUSED_KHR)
                .setIntersectionShader(VK_SHADER_UNUSED_KHR)
                );

        auto[result, p] = this->device.createRayTracingPipelineKHRUnique(nullptr, nullptr,
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

    void EnvMapGenerator::updateImageDescriptorSet()
    {
        this->device.updateDescriptorSets(
                vk::WriteDescriptorSet{}
                .setDstSet(this->imageDS.get())
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDstBinding(0)
                .setImageInfo(
                    vk::DescriptorImageInfo{}
                    .setImageView(this->envMap.imageView.get())
                    .setImageLayout(vk::ImageLayout::eGeneral)
                    ),
                nullptr);
    }

    void EnvMapGenerator::setupVukanRaytracing()
    {
        createRayTracingPipeline();
        this->scene->updateSceneDescriptorSets(this->sceneDS.get());

        std::vector<std::string> keys = { "rayGen", "miss", "hit", "shadow" };
        this->sbt = Application::createShaderBindingTable(this->device, this->physicalDevice, this->shaderGroupsCount, keys, this->pipeline.get());
    }

    void EnvMapGenerator::getMap(glm::vec3 position)
    {
        auto cmd = Application::recordCommandBuffer(this->device, this->commandPool.graphics);

        cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, this->pipeline.get());

        std::vector<vk::DescriptorSet> descriptorSets(0);
        descriptorSets.push_back(this->sceneDS.get());
        descriptorSets.push_back(this->imageDS.get());

        cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eRayTracingKHR,
                this->pipelineLayout.get(),
                0,
                descriptorSets,
                nullptr
                );

        cmd.pushConstants(
                this->pipelineLayout.get(),
                vk::ShaderStageFlagBits::eRaygenKHR,
                0, sizeof(glm::vec3), &position
                );

        auto[width, height, depth] = this->envMapExtent;
        cmd.traceRaysKHR(
                this->sbt.entries["rayGen"],
                this->sbt.entries["miss"],
                this->sbt.entries["hit"],
                {},
                width, height, depth
                );

        Application::flushCommandBuffer(this->device, this->commandPool.graphics, cmd, this->queue.graphics);
    }
}

