// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "application.hpp"

#include <fstream>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace vlb {

    PFN_vkCreateDebugUtilsMessengerEXT  pfnVkCreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT;

    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
            VkInstance                                instance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            const VkAllocationCallbacks*              pAllocator,
            VkDebugUtilsMessengerEXT*                 pMessenger)
    {
        return pfnVkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
    }

    VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
            VkInstance                   instance,
            VkDebugUtilsMessengerEXT     messenger,
            VkAllocationCallbacks const* pAllocator)
    {
        return pfnVkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(
            VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
            VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
            void * /*pUserData*/ )
    {
        std::ostringstream message{};

        message << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) << ": ";
        message << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) << ":\n";
        message << "\tmessageIDName   = <" << pCallbackData->pMessageIdName  << ">\n";
        message << "\tmessageIdNumber = "  << pCallbackData->messageIdNumber << "\n";
        message << "\tmessage         = <" << pCallbackData->pMessage        << ">\n";

        if (pCallbackData->queueLabelCount > 0)
        {
            message << "\tQueue Labels:\n";
            for (uint8_t i{}; i < pCallbackData->queueLabelCount; i++)
            {
                message << "\t\tlabelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
            }
        }
        if (pCallbackData->cmdBufLabelCount > 0)
        {
            message << "\tCommandBuffer Labels:\n";
            for (uint8_t i{}; i < pCallbackData->cmdBufLabelCount; i++)
            {
                message << "\t\tlabelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
            }
        }
        if (pCallbackData->objectCount > 0)
        {
            message << "\tObjects:\n";
            for ( uint8_t i = 0; i < pCallbackData->objectCount; i++ )
            {
                message << "\t\tObject " << i << "\n";
                message << "\t\t\tobjectType   = ";
                message << vk::to_string(static_cast<vk::ObjectType>(pCallbackData->pObjects[i].objectType)) << "\n";
                message << "\t\t\tobjectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
                if (pCallbackData->pObjects[i].pObjectName)
                {
                    message << "\t\t\t" << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
                }
            }
        }

        std::cout << message.str() << '\n';

        return false;
    }

    void Application::pushPresentationExtensions()
    {
        uint32_t glfwExtensionsCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
        for (uint32_t i{}; i < glfwExtensionsCount; ++i)
        {
            m_instanceExtensions.push_back(static_cast<const char*>(glfwExtensions[i]));
        }

        m_deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        m_deviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        m_deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        m_deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        m_deviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        m_deviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
        m_deviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
        m_deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    void Application::initDebugReportCallback()
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

    void Application::createDevice(size_t a_physicalDeviceIdx)
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

        assert(a_physicalDeviceIdx < deviceCount);
        m_physicalDevice = devices[a_physicalDeviceIdx];
        this->physicalDeviceMemoryProperties = m_physicalDevice.getMemoryProperties();

        vk::DeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.queueFamilyIndex = getQueueFamilyIndex();
        queueCreateInfo.queueCount       = 1;
        float queuePriorities            = 1.f;
        queueCreateInfo.pQueuePriorities = &queuePriorities;

        vk::DeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.pQueueCreateInfos    = &queueCreateInfo;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(m_deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

        auto c = vk::StructureChain<
            vk::DeviceCreateInfo,
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceBufferDeviceAddressFeatures,
            vk::PhysicalDeviceRayTracingPipelineFeaturesKHR,
            vk::PhysicalDeviceAccelerationStructureFeaturesKHR>{
                deviceCreateInfo,
                vk::PhysicalDeviceFeatures2(),
                vk::PhysicalDeviceBufferDeviceAddressFeatures(),
                vk::PhysicalDeviceRayTracingPipelineFeaturesKHR(),
                vk::PhysicalDeviceAccelerationStructureFeaturesKHR()
            };

        m_physicalDevice.getFeatures2(&c.get<vk::PhysicalDeviceFeatures2>());
        m_device = m_physicalDevice.createDevice(c.get<vk::DeviceCreateInfo>());

        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);
    }

    uint32_t Application::getQueueFamilyIndex()
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

    void Application::createCommandPool()
    {
        vk::CommandPoolCreateInfo poolInfo{};
        poolInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        poolInfo.queueFamilyIndex = getQueueFamilyIndex();

        m_commandPool = m_device.createCommandPool(poolInfo, nullptr);
    }

    uint32_t Application::getMemoryType(const vk::MemoryRequirements& memoryRequiriments, const vk::MemoryPropertyFlags memoryProperties)
    {
        int32_t result{-1};
        for (uint32_t i{}; i < VK_MAX_MEMORY_TYPES; ++i) {
            if (memoryRequiriments.memoryTypeBits & (1 << i)) {
                if ((this->physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryProperties) == memoryProperties) {
                    result = i;
                    break;
                }
            }
        }
        if (result == -1) {
            throw std::runtime_error("failed to get memory type index");
        }
        return result;
    }

    vk::CommandBuffer Application::recordCommandBuffer(vk::CommandBufferAllocateInfo a_info)
    {
        auto info = a_info == vk::CommandBufferAllocateInfo{};
        a_info = info ? vk::CommandBufferAllocateInfo(m_commandPool, vk::CommandBufferLevel::ePrimary, 1) : a_info;

        vk::CommandBuffer cmdBuffer = m_device.allocateCommandBuffers(a_info).front();
        cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

        return cmdBuffer;
    }

    void Application::flushCommandBuffer(vk::CommandBuffer& a_cmdBuffer, vk::Queue a_queue)
    {
        a_cmdBuffer.end();
        vk::SubmitInfo submitInfo{};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &a_cmdBuffer;

        vk::Fence fence = m_device.createFence(vk::FenceCreateInfo());
        a_queue.submit(submitInfo, fence);
        try {
            auto r = m_device.waitForFences(fence, false, 1000000000);
            if (r != vk::Result::eSuccess) std::cout << "flushing failed!\n";
        }
        catch (std::exception const &e)
        {
            std::cout << e.what() << "\n";
            std::cout << "dont close the window THAT fast\n";
        }
        m_device.destroy(fence);
        m_device.freeCommandBuffers(m_commandPool, 1, &a_cmdBuffer);
    }

    void Application::setImageLayout(
            vk::CommandBuffer cmdbuffer,
            vk::Image image,
            vk::ImageLayout oldImageLayout,
            vk::ImageLayout newImageLayout,
            vk::ImageSubresourceRange subresourceRange,
            vk::PipelineStageFlags srcStageMask,
            vk::PipelineStageFlags dstStageMask)
    {
        vk::ImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(image)
            .setOldLayout(oldImageLayout)
            .setNewLayout(newImageLayout)
            .setSubresourceRange(subresourceRange);

        // Source layouts (old)
        switch (oldImageLayout) {
            case vk::ImageLayout::eUndefined:
                imageMemoryBarrier.srcAccessMask = {};
                break;
            case vk::ImageLayout::ePreinitialized:
                imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
                break;
            case vk::ImageLayout::eColorAttachmentOptimal:
                imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
                break;
            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                break;
            case vk::ImageLayout::eTransferSrcOptimal:
                imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
                break;
            case vk::ImageLayout::eTransferDstOptimal:
                imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                break;
            case vk::ImageLayout::eShaderReadOnlyOptimal:
                imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
                break;
            default:
                break;
        }

        // Target layouts (new)
        switch (newImageLayout) {
            case vk::ImageLayout::eTransferDstOptimal:
                imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
                break;
            case vk::ImageLayout::eTransferSrcOptimal:
                imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
                break;
            case vk::ImageLayout::eColorAttachmentOptimal:
                imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
                break;
            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                break;
            case vk::ImageLayout::eShaderReadOnlyOptimal:
                if (imageMemoryBarrier.srcAccessMask == vk::AccessFlags{}) {
                    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite;
                }
                imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
                break;
            default:
                break;
        }

        cmdbuffer.pipelineBarrier(
                srcStageMask,      // srcStageMask
                dstStageMask,      // dstStageMask
                {},                // dependencyFlags
                {},                // memoryBarriers
                {},                // bufferMemoryBarriers
                imageMemoryBarrier // imageMemoryBarriers
                );
    }

    vk::UniqueShaderModule Application::createShaderModule(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> code(fileSize);

        file.seekg(0);
        file.read(code.data(), fileSize);

        file.close();

        vk::UniqueShaderModule shaderModule = m_device.createShaderModuleUnique({
                {}, code.size(), reinterpret_cast<const uint32_t*>(code.data()) });

        return shaderModule;
    }

    Application::Buffer Application::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryProperty, void* data)
    {
        Buffer buffer{};
        buffer.handle = this->m_device.createBufferUnique(
            vk::BufferCreateInfo{}
            .setSize(size)
            .setUsage(usage)
            .setQueueFamilyIndexCount(0)
        );

        auto memoryRequirements = this->m_device.getBufferMemoryRequirements(buffer.handle.get());
        vk::MemoryAllocateFlagsInfo memoryFlagsInfo{};
        if (usage & vk::BufferUsageFlagBits::eShaderDeviceAddress)
        {
            memoryFlagsInfo.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
        }

        buffer.memory = this->m_device.allocateMemoryUnique(
            vk::MemoryAllocateInfo{}
            .setAllocationSize(memoryRequirements.size)
            .setMemoryTypeIndex(Application::getMemoryType(memoryRequirements, memoryProperty))
            .setPNext(&memoryFlagsInfo)
        );
        this->m_device.bindBufferMemory(buffer.handle.get(), buffer.memory.get(), 0);

        if (data)
        {
            //TODO: flushing small command buffer if device local memory requested
            void* dataPtr = this->m_device.mapMemory(buffer.memory.get(), 0, size);
            memcpy(dataPtr, data, static_cast<size_t>(size));
            this->m_device.unmapMemory(buffer.memory.get());
        }

        buffer.deviceAddress = this->m_device.getBufferAddressKHR({buffer.handle.get()});

        return buffer;
    }

    void Application::checkValidationLayers(const std::vector<const char*>& a_layersToCheck)
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

    void Application::checkExtensions(const std::vector<const char*>& a_extensionsToCheck)
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

    Application::Application()
    {
        static int instance{1};
        assert(instance-- && "[Application::Application()]: single instance allowed");

#ifdef DEBUG
        m_instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
        m_instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    }

    Application::~Application()
    {
        m_device.destroyCommandPool(m_commandPool);
        m_device.destroy();
        m_instance.destroyDebugUtilsMessengerEXT(m_debugMessenger);
        m_instance.destroy();
    }

    void Application::createInstance()
    {
        // https://github.com/KhronosGroup/Vulkan-Hpp#extensions--per-device-function-pointers
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = m_dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        checkValidationLayers(m_instanceLayers);
        checkExtensions(m_instanceExtensions);
        vk::ApplicationInfo applicationInfo("Vulkan Light Bakery", 1, "Asama", 1, VK_API_VERSION_1_2);
        vk::InstanceCreateInfo instanceCreateInfo(vk::InstanceCreateFlags(), &applicationInfo,
                uint32_t(m_instanceLayers.size()), m_instanceLayers.data(),
                uint32_t(m_instanceExtensions.size()), m_instanceExtensions.data());
        m_instance = vk::createInstance(instanceCreateInfo);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);

#ifdef DEBUG
        initDebugReportCallback();
#endif
    }

}

