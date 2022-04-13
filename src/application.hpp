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
                vk::DeviceSize           size;
            };

            struct Image
            {
                vk::UniqueImage        handle;
                vk::UniqueDeviceMemory memory;
                vk::UniqueImageView    imageView;
                // TODO: make fully use of new member
                vk::ImageLayout        imageLayout;
            };

            struct ShaderBindingTable
            {
                std::vector<vk::StridedDeviceAddressRegionKHR> strides;
                Application::Buffer                            buffer;
            };

        protected:

            vk::UniqueInstance    instance;
            vk::PhysicalDevice    physicalDevice;
            vk::UniqueDevice      device;

            vk::DynamicLoader  dl;

            void pushExtentionsForGraphics();
            void initDebugReportCallback();

            struct QueueFamilyIndex
            {
                uint32_t graphics = -1;
                uint32_t transfer = -1;
                uint32_t compute = -1;
            } queueFamilyIndex;
            struct Queue
            {
                vk::Queue graphics;
                vk::Queue transfer;
                vk::Queue compute;
            } queue;
            void setQueueFamilyIndices();
            void getQueues();

            void createInstance();
            void createDevice(size_t physicalDeviceIdx = 0);
            void createCommandPools();
            struct CommandPool
            {
                vk::UniqueCommandPool graphics;
                vk::UniqueCommandPool transfer;
                vk::UniqueCommandPool compute;
            } commandPool;

            vk::PhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
            uint32_t getMemoryType(const vk::MemoryRequirements& memoryRequirements, const vk::MemoryPropertyFlags& memoryProperties);

            vk::CommandBuffer recordGraphicsCommandBuffer(vk::CommandBufferAllocateInfo info = {});
            void flushGraphicsCommandBuffer(vk::CommandBuffer& cmdBuffer);
            vk::CommandBuffer recordTransferCommandBuffer(vk::CommandBufferAllocateInfo info = {});
            void flushTransferCommandBuffer(vk::CommandBuffer& cmdBuffer);
            vk::CommandBuffer recordComputeCommandBuffer(vk::CommandBufferAllocateInfo info = {});
            void flushComputeCommandBuffer(vk::CommandBuffer& cmdBuffer);

            Buffer                 createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryProperty, const void* data = nullptr);
            Image                  createImage(vk::Format imageFormat, vk::Extent3D imageExtent);
            vk::UniqueShaderModule createShaderModule(const std::string& filename);
            ShaderBindingTable     createShaderBindingTable(vk::Pipeline& pipeline);

        private:

            vk::UniqueDebugUtilsMessengerEXT debugMessenger;
            std::vector<const char*> instanceLayers;
            std::vector<const char*> instanceExtensions;
            std::vector<const char*> deviceLayers;
            std::vector<const char*> deviceExtensions;

            void checkValidationLayers(const std::vector<const char*>& layersToCheck);
            void checkExtensions(const std::vector<const char*>& xtensionsToCheck);

        public:

            static uint32_t getMemoryType(const vk::PhysicalDevice& physicalDevice, const vk::MemoryRequirements& memoryRequirements,
                    const vk::MemoryPropertyFlags& memoryProperties);

            static void setImageLayout(
                    vk::CommandBuffer cmdbuffer,
                    vk::Image image,
                    vk::ImageLayout oldImageLayout,
                    vk::ImageLayout newImageLayout,
                    vk::ImageSubresourceRange subresourceRange,
                    vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eAllCommands,
                    vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eAllCommands);

            static vk::CommandBuffer recordCommandBuffer(vk::Device device, vk::CommandPool commandPool, vk::CommandBufferAllocateInfo info = {});
            static void flushCommandBuffer(vk::Device& device, vk::CommandPool& commandPool, vk::CommandBuffer& cmdBuffer, vk::Queue queue);

            static Buffer createBuffer(
                    vk::Device& device,
                    vk::PhysicalDevice& physicalDevice,
                    vk::DeviceSize size,
                    vk::BufferUsageFlags usage,
                    vk::MemoryPropertyFlags memoryProperty,
                    const void* data = nullptr);

            static Image createImage(
                    vk::Device& device,
                    vk::PhysicalDevice& physicalDevice,
                    vk::Format imageFormat,
                    vk::Extent3D imageExtent,
                    vk::CommandPool transferPool,
                    vk::Queue transferQueue);

            static vk::UniqueShaderModule createShaderModule(
                    vk::Device& device,
                    const std::string& filename);

            static ShaderBindingTable createShaderBindingTable(
                    vk::Device& device,
                    vk::PhysicalDevice& physicalDevice,
                    vk::Pipeline& pipeline);

            Application(bool isGraphical = true);
            Application(const Application& other) = delete;
    };

}

#endif // ifndef APPLICATION_HPP

