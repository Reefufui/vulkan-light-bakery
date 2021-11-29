// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_STORAGE_SHARED_EXPORT
#define VULKAN_HPP_STORAGE_SHARED
#define VULKAN_HPP_NO_NODISCARD_WARNINGS
#include <vulkan/vulkan.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <cassert>

#define DEBUG

namespace vlb {

    class Application
    {
        public:

            struct Buffer
            {
                vk::UniqueBuffer         handle;
                vk::UniqueDeviceMemory   memory;
                vk::DeviceAddress        deviceAddress;
            };

            struct Image
            {
                vk::UniqueImage        handle;
                vk::UniqueDeviceMemory memory;
                vk::UniqueImageView    imageView;
            };

        protected:

            vk::UniqueInstance    instance;
            vk::PhysicalDevice    physicalDevice;
            vk::UniqueDevice      device;
            vk::UniqueCommandPool commandPool;

            vk::DynamicLoader  dl;

            void pushExtentionsForGraphics();
            void initDebugReportCallback();

            //TODO: mulitple queue families
            vk::QueueFlagBits getQueueFlag() {return vk::QueueFlagBits::eGraphics;};
            uint32_t getQueueFamilyIndex();

            void createInstance();
            void createDevice(size_t physicalDeviceIdx = 0);

            vk::PhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
            uint32_t getMemoryType(const vk::MemoryRequirements& memoryRequiriments, const vk::MemoryPropertyFlags memoryProperties);

            void setImageLayout(
                    vk::CommandBuffer cmdbuffer,
                    vk::Image image,
                    vk::ImageLayout oldImageLayout,
                    vk::ImageLayout newImageLayout,
                    vk::ImageSubresourceRange subresourceRange,
                    vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eAllCommands,
                    vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eAllCommands);

            void createCommandPool();
            vk::CommandBuffer recordCommandBuffer(vk::CommandBufferAllocateInfo info = {});
            void flushCommandBuffer(vk::CommandBuffer& cmdBuffer, vk::Queue queue);

            vk::UniqueShaderModule createShaderModule(const std::string& filename);

            Buffer createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryProperty, void* data = nullptr);

        private:

            vk::UniqueDebugUtilsMessengerEXT debugMessenger;
            std::vector<const char*> instanceLayers;
            std::vector<const char*> instanceExtensions;
            std::vector<const char*> deviceLayers;
            std::vector<const char*> deviceExtensions;

            void checkValidationLayers(const std::vector<const char*>& layersToCheck);
            void checkExtensions(const std::vector<const char*>& xtensionsToCheck);

        public:

            Application(bool isGraphical = true);
            Application(const Application& other) = delete;
    };

}

#endif // ifndef APPLICATION_HPP

