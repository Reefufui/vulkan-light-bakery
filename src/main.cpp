#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <cassert>

#define DEBUG

#include "messenger.hpp"

class Application
{
    protected:

        vk::Instance       m_instance;
        vk::PhysicalDevice m_physicalDevice;
        vk::Device         m_device;
        vk::CommandPool    m_commandPool;

        void pushPresentationExtensions()
        {
            uint32_t glfwExtensionsCount = 0;
            const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
            for (uint32_t i{}; i < glfwExtensionsCount; ++i)
            {
                m_instanceExtensions.push_back(static_cast<const char*>(glfwExtensions[i]));
            }

            m_deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }

        void initDebugReportCallback()
        {
            assert(m_instance);

            pfnVkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                    m_instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));

            if (!pfnVkCreateDebugUtilsMessengerEXT)
            {
                std::cerr << "GetInstanceProcAddr: Unable to find pfnVkCreateDebugUtilsMessengerEXT function." << std::endl;
                exit(1);
            }

            pfnVkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    m_instance.getProcAddr( "vkDestroyDebugUtilsMessengerEXT" ) );
            if (!pfnVkDestroyDebugUtilsMessengerEXT)
            {
                std::cerr << "GetInstanceProcAddr: Unable to find pfnVkDestroyDebugUtilsMessengerEXT function." << std::endl;
                exit(1);
            }

            vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                    | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
            vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                    | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                    | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

            m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(
                    vk::DebugUtilsMessengerCreateInfoEXT({}, severityFlags, messageTypeFlags, &debugMessageFunc));
        }

        void createInstance()
        {
            checkValidationLayers(m_instanceLayers);
            checkExtensions(m_instanceExtensions);

            vk::ApplicationInfo applicationInfo("Vulkan", 1, "Asama", 1, VK_API_VERSION_1_1);
            vk::InstanceCreateInfo instanceCreateInfo(vk::InstanceCreateFlags(), &applicationInfo,
                    uint32_t(m_instanceLayers.size()), m_instanceLayers.data(),
                    uint32_t(m_instanceExtensions.size()), m_instanceExtensions.data());
            m_instance = vk::createInstance(instanceCreateInfo);

#ifdef DEBUG
            initDebugReportCallback();
#endif
        }

        virtual vk::QueueFlagBits getQueueFlag() = 0;

        void createDevices()
        {
            findPhysicalDevice();
            createLogicalDevice();
        }

        void findPhysicalDevice()
        {
            assert(m_instance);

            uint32_t deviceCount{};
            if (m_instance.enumeratePhysicalDevices(&deviceCount, nullptr) != vk::Result::eSuccess || !deviceCount)
            {
                throw std::runtime_error("no devices found");
            }

            std::vector<vk::PhysicalDevice> devices(deviceCount);
            if (m_instance.enumeratePhysicalDevices(&deviceCount, devices.data()) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to fetch devices");
            }

            vk::PhysicalDeviceProperties props{};
            vk::PhysicalDeviceFeatures   features{};

            for (auto& device : devices)
            {
                device.getProperties(&props);
                device.getFeatures(&features);
            }

            if (!m_physicalDevice)
            {
                m_physicalDevice = devices[0];
            }
        }

        uint32_t getQueueFamilyIndex()
        {
            uint32_t queueFamilyCount;
            m_physicalDevice.getQueueFamilyProperties(&queueFamilyCount, nullptr);

            std::vector<vk::QueueFamilyProperties> queueFamilies(queueFamilyCount);
            m_physicalDevice.getQueueFamilyProperties(&queueFamilyCount, queueFamilies.data());

            auto queueFlag{ getQueueFlag() };
            uint32_t index = 0;
            for (auto& prop : queueFamilies)
            {
                if (prop.queueCount > 0 && (prop.queueFlags & queueFlag))
                {
                    break;
                }
                ++index;
            }

            if (index == queueFamilies.size())
            {
                throw std::runtime_error("could not find a queue family that supports operations");
            }

            return index;
        }

        void createLogicalDevice()
        {
            vk::DeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.queueFamilyIndex = getQueueFamilyIndex();
            queueCreateInfo.queueCount       = 1;
            float queuePriorities            = 1.f;
            queueCreateInfo.pQueuePriorities = &queuePriorities;

            vk::PhysicalDeviceFeatures deviceFeatures{};
            vk::DeviceCreateInfo deviceCreateInfo{};
            deviceCreateInfo.enabledLayerCount = uint32_t(m_deviceLayers.size());
            deviceCreateInfo.ppEnabledLayerNames  = m_deviceLayers.data();
            deviceCreateInfo.pQueueCreateInfos    = &queueCreateInfo;
            deviceCreateInfo.queueCreateInfoCount = 1;
            deviceCreateInfo.pEnabledFeatures     = &deviceFeatures;

            deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(m_deviceExtensions.size());
            deviceCreateInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

            m_device = m_physicalDevice.createDevice(deviceCreateInfo, nullptr);
        }

        void createCommandPool()
        {
            vk::CommandPoolCreateInfo poolInfo{};
            poolInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
            poolInfo.queueFamilyIndex = getQueueFamilyIndex();

            m_commandPool = m_device.createCommandPool(poolInfo, nullptr);
        }

    private:

        vk::DebugUtilsMessengerEXT m_debugMessenger;
        std::vector<const char*> m_instanceLayers;
        std::vector<const char*> m_instanceExtensions;
        std::vector<const char*> m_deviceLayers;
        std::vector<const char*> m_deviceExtensions;

        void checkValidationLayers(const std::vector<const char*>& a_layersToCheck)
        {
            uint32_t layerCount{};
            if (vk::enumerateInstanceLayerProperties(&layerCount, nullptr) != vk::Result::eSuccess)
            {
                throw std::runtime_error("0 layers found");
            }
            std::vector<vk::LayerProperties> availableLayers(layerCount);

            if (vk::enumerateInstanceLayerProperties(&layerCount, availableLayers.data()) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to fetch layers");
            }

            auto checkValidationLayer = [&availableLayers](const char* a_wantedLayerName)
            {
                auto compareNames = [&a_wantedLayerName](const vk::LayerProperties& a_props)
                {
                    return std::string(a_props.layerName) == std::string(a_wantedLayerName);
                };

                if (std::find_if(availableLayers.begin(), availableLayers.end(), compareNames) == availableLayers.end())
                {
                    throw std::runtime_error("layer not found");
                }
            };

            std::for_each(a_layersToCheck.begin(), a_layersToCheck.end(), checkValidationLayer);
        }

        void checkExtensions(const std::vector<const char*>& a_extensionsToCheck)
        {
            uint32_t extensionCount;
            if (vk::enumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr) != vk::Result::eSuccess)
            {
                throw std::runtime_error("0 extensions found");
            }

            std::vector<vk::ExtensionProperties> availableExtensions(extensionCount);
            if (vk::enumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to fetch extensions");
            }

            auto checkExtension = [&availableExtensions](const char* a_wantedExtensionName)
            {
                auto compareNames = [&a_wantedExtensionName](const vk::ExtensionProperties& a_props)
                {
                    return std::string(a_props.extensionName) == std::string(a_wantedExtensionName);
                };

                if (std::find_if(availableExtensions.begin(), availableExtensions.end(), compareNames) == availableExtensions.end())
                {
                    throw std::runtime_error("extension not found");
                }
            };

            std::for_each(a_extensionsToCheck.begin(), a_extensionsToCheck.end(), checkExtension);
        }

    public:

        Application()
        {
            static int instance{1};
            assert(instance-- && "[Application::Application()]: single instance allowed");

#ifdef DEBUG
            m_instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
            m_instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
        }

        Application(const Application& other) = delete;

        ~Application()
        {
            m_device.destroyCommandPool(m_commandPool);
            m_device.destroy();
            m_instance.destroyDebugUtilsMessengerEXT(m_debugMessenger);
            m_instance.destroy();
        }
};

class Renderer : public Application
{
    private:
        struct WindowDestroy
        {
            void operator()(GLFWwindow* ptr)
            {
                glfwDestroyWindow(ptr);
            }
        };

    protected:

        typedef std::unique_ptr<GLFWwindow, WindowDestroy> UniqueWindow;

        UniqueWindow         m_window;
        uint32_t             m_windowWidth;
        uint32_t             m_windowHeight;

        vk::UniqueSurfaceKHR             m_surface;
        vk::UniqueSwapchainKHR           m_swapchain;
        std::vector<vk::Image>           m_swapchainImages;
        std::vector<vk::UniqueImageView> m_swapchainImageViews;
        vk::SurfaceFormatKHR             m_surfaceFormat{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };

        vk::Queue m_graphicsQueue;
        vk::Queue m_presentQueue;

    private:

        UniqueWindow createWindow(const int& a_windowWidth = 480, const int& a_windowHeight = 480)
        {
            assert(a_windowWidth > 0 && a_windowHeight > 0);
            m_windowWidth  = static_cast<uint32_t>(a_windowWidth);
            m_windowHeight = static_cast<uint32_t>(a_windowHeight);

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

            auto glfwWindow = glfwCreateWindow(m_windowWidth, m_windowHeight, "Vulkan", nullptr, nullptr);
            std::unique_ptr<GLFWwindow, WindowDestroy> window(glfwWindow);
            return window;
        }

        vk::UniqueSurfaceKHR createSurface()
        {
            VkSurfaceKHR tmpSurface;
            VkResult res = glfwCreateWindowSurface(m_instance, m_window.get(), nullptr, &tmpSurface);
            if (res)
            {
                throw std::runtime_error("failed to create window surface");
            }
            vk::ObjectDestroy<vk::Instance, vk::DispatchLoaderStatic> surfaceDeleter(m_instance);

            auto surface = vk::UniqueSurfaceKHR(tmpSurface, surfaceDeleter);

            if (!isSurfaceSupported(surface))
            {
                throw std::runtime_error("surface is not supported");
            }

            return surface;
        }

        vk::QueueFlagBits getQueueFlag() override
        {
            return vk::QueueFlagBits::eGraphics;
        }

        bool isSurfaceSupported(const vk::UniqueSurfaceKHR& a_surface)
        {
            assert(a_surface);
            return m_physicalDevice.getSurfaceSupportKHR(getQueueFamilyIndex(), *a_surface);
        }

        vk::UniqueSwapchainKHR createSwapchain()
        {
            vk::SurfaceCapabilitiesKHR capabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface.get());

            uint32_t formatCount;
            if (m_physicalDevice.getSurfaceFormatsKHR(m_surface.get(), &formatCount, nullptr) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to fetch surface formats");
            }
            assert(formatCount);

            std::vector<vk::SurfaceFormatKHR> availableFormats(formatCount);

            if (m_physicalDevice.getSurfaceFormatsKHR(m_surface.get(), &formatCount, availableFormats.data()) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to fetch surface formats");
            }

            uint32_t presentModeCount;
            if (m_physicalDevice.getSurfacePresentModesKHR(m_surface.get(), &presentModeCount, nullptr) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to fetch present modes");
            }
            assert(presentModeCount);

            std::vector<vk::PresentModeKHR> availablePresentModes(presentModeCount);

            if (m_physicalDevice.getSurfacePresentModesKHR(m_surface.get(), &presentModeCount, availablePresentModes.data())
                    != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to fetch present modes");
            }

            auto compareFormats = [&](const vk::SurfaceFormatKHR& a_surfaceFormat)
            {
                return a_surfaceFormat == m_surfaceFormat;
            };

            if (std::find_if(availableFormats.begin(), availableFormats.end(), compareFormats) == availableFormats.end())
            {
                throw std::runtime_error("surface format not found");
            }

            vk::PresentModeKHR presentMode{};

            auto comparePresentModes = [&](const vk::PresentModeKHR& a_presentMode)
            {
                if (a_presentMode == vk::PresentModeKHR::eMailbox)
                {
                    presentMode = vk::PresentModeKHR::eMailbox;
                    return true;
                }
                else if (a_presentMode == vk::PresentModeKHR::eImmediate)
                {
                    presentMode = vk::PresentModeKHR::eImmediate;
                    return true;
                }

                return false;
            };

            if (std::find_if(availablePresentModes.begin(), availablePresentModes.end(), comparePresentModes)
                    == availablePresentModes.end())
            {
                presentMode = vk::PresentModeKHR::eFifo;
            }

            vk::Extent2D extent{};

            if (capabilities.currentExtent.width != UINT32_MAX)
            {
                extent = capabilities.currentExtent;
            }
            else
            {
                extent.width  = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, m_windowWidth));
                extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, m_windowHeight));
            }

            uint32_t imageCount = capabilities.minImageCount + 1;

            if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
            {
                imageCount = capabilities.maxImageCount;
            }

            vk::SwapchainCreateInfoKHR createInfo{};
            createInfo.surface          = m_surface.get();
            createInfo.minImageCount    = imageCount;
            createInfo.imageFormat      = m_surfaceFormat.format;
            createInfo.imageColorSpace  = m_surfaceFormat.colorSpace;
            createInfo.imageExtent      = extent;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;
            createInfo.imageSharingMode = vk::SharingMode::eExclusive;
            createInfo.preTransform     = capabilities.currentTransform;
            createInfo.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
            createInfo.presentMode      = presentMode;
            createInfo.clipped          = VK_TRUE;
            createInfo.oldSwapchain     = nullptr;

            vk::ObjectDestroy<vk::Device, vk::DispatchLoaderStatic> swapchainDeleter(m_device);
            vk::SwapchainKHR swapChain{};
            if (m_device.createSwapchainKHR(&createInfo, nullptr, &swapChain) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to create swapchain");
            }

            return vk::UniqueSwapchainKHR(swapChain, swapchainDeleter);
        }

        void createSwapchainResourses()
        {
            uint32_t imageCount{};

            if (m_device.getSwapchainImagesKHR(m_swapchain.get(), &imageCount, nullptr) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to fetch swapchain images");
            }

            m_swapchainImages.resize(imageCount);

            if (m_device.getSwapchainImagesKHR(m_swapchain.get(), &imageCount, m_swapchainImages.data()) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to fetch swapchain images");
            }

            m_swapchainImageViews.resize(imageCount);
            vk::ObjectDestroy<vk::Device, vk::DispatchLoaderStatic> imageViewDeleter(m_device);

            vk::ImageViewCreateInfo createInfo{};
            createInfo.viewType     = vk::ImageViewType::e2D;
            createInfo.format       = m_surfaceFormat.format;
            createInfo.components.r = vk::ComponentSwizzle::eIdentity;
            createInfo.components.g = vk::ComponentSwizzle::eIdentity;
            createInfo.components.b = vk::ComponentSwizzle::eIdentity;
            createInfo.components.a = vk::ComponentSwizzle::eIdentity;
            createInfo.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
            createInfo.subresourceRange.baseMipLevel   = 0;
            createInfo.subresourceRange.levelCount     = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount     = 1;

            size_t i{};
            for (auto& uniqueImageView : m_swapchainImageViews)
            {
                createInfo.image        = m_swapchainImages[i++];

                vk::ImageView imageView{};
                if (m_device.createImageView(&createInfo, nullptr, &imageView) != vk::Result::eSuccess)
                {
                    throw std::runtime_error("failed to create image views for swapchain");
                }

                uniqueImageView = vk::UniqueImageView(imageView, imageViewDeleter);
            }
        }

    public:

        Renderer()
        {
            glfwInit();
            pushPresentationExtensions();
            createInstance();
            createDevices();
            createCommandPool();

            m_window  = createWindow();
            m_surface = createSurface();
            m_graphicsQueue = m_device.getQueue(getQueueFamilyIndex(), 0);
            m_presentQueue  = m_device.getQueue(getQueueFamilyIndex(), 0);

            m_swapchain = createSwapchain();
            createSwapchainResourses();
        }

        ~Renderer()
        {
            glfwTerminate();
        }
};

int main(int, char**)
{
    try
    {
        Renderer app{};
    }
    catch(const vk::SystemError& error)
    {
        return EXIT_FAILURE;
    }
    catch(std::exception& error)
    {
        std::cerr << "std::exception: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "unknown error\n";
        return EXIT_FAILURE;
    }

    std::cout << "exiting...\n";
    return EXIT_SUCCESS;
}

