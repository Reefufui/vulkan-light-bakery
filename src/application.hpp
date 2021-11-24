// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_STORAGE_SHARED_EXPORT
#define VULKAN_HPP_STORAGE_SHARED
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
        protected:

            vk::Instance       m_instance;
            vk::PhysicalDevice m_physicalDevice;
            vk::Device         m_device;
            vk::CommandPool    m_commandPool;

            vk::DynamicLoader  m_dl;

            void pushPresentationExtensions();
            void initDebugReportCallback();

            virtual vk::QueueFlagBits getQueueFlag() = 0;
            uint32_t getQueueFamilyIndex();

            void createInstance();
            void createDevice(size_t a_physicalDeviceIdx = 0);

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
            vk::CommandBuffer recordCommandBuffer(vk::CommandBufferAllocateInfo a_info = {});
            void flushCommandBuffer(vk::CommandBuffer& a_cmdBuffer, vk::Queue a_queue);

            vk::UniqueShaderModule createShaderModule(const std::string& filename);

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

            Buffer createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryProperty, void* data = nullptr);

        private:

            vk::DebugUtilsMessengerEXT m_debugMessenger;
            std::vector<const char*> m_instanceLayers;
            std::vector<const char*> m_instanceExtensions;
            std::vector<const char*> m_deviceLayers;
            std::vector<const char*> m_deviceExtensions;

            void checkValidationLayers(const std::vector<const char*>& a_layersToCheck);
            void checkExtensions(const std::vector<const char*>& a_extensionsToCheck);

        public:

            Application();
            Application(const Application& other) = delete;
            ~Application();
    };

}

#endif // ifndef APPLICATION_HPP

