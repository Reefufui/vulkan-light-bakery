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
        void createCommandPool();

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

#endif // ifndef APPLICATION_HPP

