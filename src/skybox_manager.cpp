// created in 2022 by Andrey Treefonov https://github.com/Reefufui

#include "skybox_manager.hpp"

#include <stb_image.h>
#include <filesystem>

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

    Skybox Skybox_t::computeSH()
    {
        // TODO

        return shared_from_this();
    }

    std::string Skybox_t::getName()
    {
        return this->name;
    }

    Skybox Skybox_t::createDescriptorSetLayout()
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings{
            vk::DescriptorSetLayoutBinding{}
            .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eMissKHR),
        };

        this->descriptorSetLayout = this->device.createDescriptorSetLayoutUnique(
                vk::DescriptorSetLayoutCreateInfo{}
                .setBindings(bindings)
                );

        return shared_from_this();
    }

    vk::DescriptorSetLayout Skybox_t::getDescriptorSetLayout()
    {
        return this->descriptorSetLayout.get();
    }

    Skybox Skybox_t::updateSkyboxDescriptorSets(vk::DescriptorSet targetDS)
    {
        vk::WriteDescriptorSet texturesWriteDS{};
        texturesWriteDS
            .setDstSet(targetDS)
            .setDstBinding(0)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setImageInfo(texture.descriptor);

        this->device.updateDescriptorSets(texturesWriteDS, nullptr);

        return shared_from_this();
    }

    void SkyboxManager::passVulkanContext(Skybox_t::VulkanContext& context)
    {
        this->context = context;
        this->skyboxChangedFlag = false;
        this->skyboxIndex = 0;
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
        skybox->computeSH();
        skybox->createDescriptorSetLayout();

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
        skybox->createDescriptorSetLayout();

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

