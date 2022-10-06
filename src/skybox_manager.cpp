// created in 2022 by Andrey Treefonov https://github.com/Reefufui

#include "skybox_manager.hpp"

#include <stb_image.h>
#include <filesystem>
#include <cmath>

namespace vlb {

    Skybox_t::Skybox_t(std::string& filename)
    {
        std::filesystem::path filePath{filename};
        this->path = filename;
        this->name = filePath.stem();

        stbi_uc* texelsPtr{};
        texelsPtr = stbi_load(filename.c_str(), &this->width, &this->height, &this->texChannels, STBI_rgb_alpha);

        if (!texelsPtr)
        {
            throw std::runtime_error(std::string("Could not load skybox texture: ") + std::string(filename));
        }

        size_t size = this->width * this->height * 4;
        this->texels = new unsigned char[size];
        memcpy(texels, texelsPtr, size);
        stbi_image_free(texelsPtr);
    }

    Skybox_t::~Skybox_t()
    {
    }

    Skybox Skybox_t::passVulkanContext(VulkanContext& context)
    {
        this->device               = context.device;
        this->physicalDevice       = context.physicalDevice;
        this->queue.graphics       = context.graphicsQueue;
        this->commandPool.graphics = context.graphicsCommandPool;
        this->queue.transfer       = context.transferQueue       ? context.transferQueue       : context.graphicsQueue;
        this->commandPool.transfer = context.transferCommandPool ? context.transferCommandPool : context.graphicsCommandPool;
        this->queue.compute        = context.computeQueue        ? context.computeQueue        : context.graphicsQueue;
        this->commandPool.compute  = context.computeCommandPool  ? context.computeCommandPool  : context.graphicsCommandPool;

        return shared_from_this();
    }

    Skybox Skybox_t::createTexture()
    {
        size_t size = this->width * this->height * 4;

        vk::BufferUsageFlags usage             = vk::BufferUsageFlagBits::eTransferSrc;
        vk::MemoryPropertyFlags memoryProperty = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        Application::Buffer staging = Application::createBuffer(this->device, this->physicalDevice, size, usage, memoryProperty, texels);

        vk::Extent3D extent{static_cast<uint32_t>(this->width), static_cast<uint32_t>(this->height), 1};

        this->texture = Application::bufferToImage(this->device, this->physicalDevice, this->commandPool.graphics, this->queue.graphics,
                staging, Application::Sampler{}, extent, 1);

        return shared_from_this();
    }

    Skybox Skybox_t::createSHBuffer()
    {
        using enum vk::BufferUsageFlagBits;
        using enum vk::MemoryPropertyFlagBits;
        vk::BufferUsageFlags    usg{ eStorageBuffer };
        vk::MemoryPropertyFlags mem{ eHostVisible | eHostCoherent };

        float init[16 * 3] = {0.f};
        this->SHCoeffs = Application::createBuffer(this->device, this->physicalDevice, 16 * 3 * sizeof(float), usg, mem, init);

        return shared_from_this();
    }

    Skybox Skybox_t::updateDescriptorSets(vk::DescriptorSet targetDS)
    {
        vk::WriteDescriptorSet writeImage{};
        writeImage
            .setDstSet(targetDS)
            .setDstBinding(0)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setImageInfo(this->texture.descriptor);

        this->device.updateDescriptorSets(writeImage, nullptr);

        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo
            .setBuffer(SHCoeffs.handle.get())
            .setOffset(0)
            .setRange(SHCoeffs.size);

        vk::WriteDescriptorSet writeBuffer{};
        writeBuffer
            .setDstSet(targetDS)
            .setDstBinding(1)
            .setDescriptorType(vk::DescriptorType::eStorageBuffer)
            .setBufferInfo(bufferInfo);

        this->device.updateDescriptorSets(writeBuffer, nullptr);

        return shared_from_this();
    }

    Skybox Skybox_t::computeSH(vk::Pipeline computePipeline, vk::PipelineLayout layout, vk::DescriptorSet ds)
    {
        auto cmd = Application::recordCommandBuffer(this->device, this->commandPool.compute);
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                layout,
                0,
                { ds },
                {});
        struct PushConstants
        {
            uint32_t width;
            uint32_t height;
        } pushConstants = { static_cast<uint32_t>(this->width), static_cast<uint32_t>(this->height) };
        cmd.pushConstants(
                layout,
                vk::ShaderStageFlagBits::eCompute,
                0, sizeof(PushConstants), &pushConstants
                );
        cmd.dispatch((uint32_t)ceil(this->width / float(WORKGROUP_SIZE)), (uint32_t)ceil(this->height / float(WORKGROUP_SIZE)), 1);
        Application::flushCommandBuffer(this->device, this->commandPool.compute, cmd, this->queue.compute);

        return shared_from_this();
    }

    std::string Skybox_t::getName()
    {
        return this->name;
    }

    void SkyboxManager::createDescriptorSetLayout()
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings{
            vk::DescriptorSetLayoutBinding{}
            .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eMissKHR),

                vk::DescriptorSetLayoutBinding{}
            .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eMissKHR)
        };

        this->descriptorSetLayout = context.device.createDescriptorSetLayoutUnique(
                vk::DescriptorSetLayoutCreateInfo{}
                .setBindings(bindings)
                );
    }

    vk::DescriptorSetLayout SkyboxManager::getDescriptorSetLayout()
    {
        return this->descriptorSetLayout.get();
    }

    vk::DescriptorSet SkyboxManager::getDescriptorSet()
    {
        return this->descriptorSet.get();
    }

    void SkyboxManager::createDescriptorSets()
    {
        std::vector<vk::DescriptorPoolSize> poolSizes = {
            {vk::DescriptorType::eStorageBuffer, 1},
            {vk::DescriptorType::eCombinedImageSampler, 1}
        };

        this->descriptorPool = context.device.createDescriptorPoolUnique(
                vk::DescriptorPoolCreateInfo{}
                .setPoolSizes(poolSizes)
                .setMaxSets(1)
                .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
                    | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
                );

        createDescriptorSetLayout();

        this->descriptorSet = std::move(context.device.allocateDescriptorSetsUnique(
                    vk::DescriptorSetAllocateInfo{}
                    .setDescriptorPool(this->descriptorPool.get())
                    .setSetLayouts(this->descriptorSetLayout.get())
                    ).front());
    }

    void SkyboxManager::createSHComputePipeline()
    {
        vk::PushConstantRange pcRange{};
        pcRange
            .setStageFlags(vk::ShaderStageFlagBits::eCompute)
            .setOffset(0)
            .setSize(sizeof(uint32_t) * 2);

        this->pipelineLayout = context.device.createPipelineLayoutUnique(
                vk::PipelineLayoutCreateInfo{}
                .setSetLayouts(this->descriptorSetLayout.get())
                .setPushConstantRanges(pcRange)
                );

        vk::UniqueShaderModule shaderModule = Application::createShaderModule(context.device, "shaders/skybox_sh.comp.spv");

        vk::PipelineShaderStageCreateInfo shaderStageCreateInfo{};
        shaderStageCreateInfo
            .setStage(vk::ShaderStageFlagBits::eCompute)
            .setModule(shaderModule.get())
            .setPName("main");

        this->pipelineCache = context.device.createPipelineCacheUnique(vk::PipelineCacheCreateInfo());

        auto[result, p] = context.device.createComputePipelineUnique(
                pipelineCache.get(),
                vk::ComputePipelineCreateInfo{}
                .setStage(shaderStageCreateInfo)
                .setLayout(this->pipelineLayout.get()));

        if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create compute pipeline");
        }
        else
        {
            this->pipeline = std::move(p);
        }
    }

    void SkyboxManager::passVulkanContext(Skybox_t::VulkanContext& context)
    {
        this->context = context;
        this->skyboxChangedFlag = false;
        this->skyboxIndex = 0;

        createDescriptorSets();
        createSHComputePipeline();
    }

    Skybox& SkyboxManager::getSkybox(int index)
    {
        return this->skyboxes[index == -1 ? this->skyboxIndex : index];
    }

    const int SkyboxManager::getSkyboxIndex()
    {
        return this->skyboxIndex;
    }

    const size_t SkyboxManager::getSkyboxCount()
    {
        return this->skyboxes.size();
    }

    std::vector<std::string>& SkyboxManager::getSkyboxNames()
    {
        return this->skyboxNames;
    }

    const bool SkyboxManager::skyboxChanged()
    {
        bool skyboxChanged = false;
        std::swap(skyboxChanged, this->skyboxChangedFlag);
        return skyboxChanged;
    }

    SkyboxManager& SkyboxManager::setSkyboxIndex(int skyboxIndex)
    {
        this->skyboxIndex = skyboxIndex;

        static int oldSkyboxIndex = 0;
        if (this->skyboxIndex != oldSkyboxIndex)
        {
            oldSkyboxIndex = this->skyboxIndex;
            this->skyboxChangedFlag = true;
        }

        return *this;
    }

    void SkyboxManager::pushSkybox(std::string& fileName)
    {
        Skybox skybox{new Skybox_t(fileName)};
        skybox->passVulkanContext(this->context);
        skybox->createTexture();
        skybox->createSHBuffer();
        skybox->updateDescriptorSets(this->descriptorSet.get());
        skybox->computeSH(this->pipeline.get(), this->pipelineLayout.get(), this->descriptorSet.get());

        this->skyboxes.push_back(skybox);
        this->skyboxNames.push_back(skybox->getName());

        this->skyboxIndex = getSkyboxCount() - 1;
        this->skyboxChangedFlag = true;
    }

    void SkyboxManager::pushSkybox(Skybox_t::CreateInfo ci) // NOTE: not tested yet (no serialization)
    {
        Skybox skybox{new Skybox_t(ci.path)};
        skybox->passVulkanContext(this->context);
        skybox->createTexture();
        skybox->createSHBuffer();
        skybox->updateDescriptorSets(this->descriptorSet.get());
        skybox->computeSH(this->pipeline.get(), this->pipelineLayout.get(), this->descriptorSet.get());

        this->skyboxes.push_back(skybox);
        this->skyboxNames.push_back(ci.name);

        this->skyboxChangedFlag = false;
    }

    void SkyboxManager::popSkybox()
    {
        this->context.device.waitIdle();
        this->skyboxes.erase(this->skyboxes.begin() + this->skyboxIndex);
        this->skyboxNames.erase(this->skyboxNames.begin() + this->skyboxIndex);

        this->skyboxIndex = std::max(0, this->skyboxIndex - 1);
        this->skyboxChangedFlag = true;
    }

    SkyboxManager::~SkyboxManager()
    {
        while (this->skyboxes.size())
        {
            popSkybox();
        }
    }
}

