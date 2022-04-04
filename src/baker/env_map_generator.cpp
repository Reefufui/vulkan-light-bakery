// created in 2022 by Andrey Treefonov https://github.com/Reefufui

#include "env_map_generator.hpp"
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

        return this->envMap;
    }

    int EnvMapGenerator::getMap(glm::vec3 position)
    {
        // TODO
        //
        return 0;
    }

}

