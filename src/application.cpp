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
            void *)
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

    void Application::pushExtentionsForGraphics()
    {
        uint32_t glfwExtensionsCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
        for (uint32_t i{}; i < glfwExtensionsCount; ++i)
        {
            this->instanceExtensions.push_back(static_cast<const char*>(glfwExtensions[i]));
        }

        this->deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        this->deviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        this->deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        this->deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        this->deviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        this->deviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
        this->deviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
        this->deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    void Application::initDebugReportCallback()
    {
        assert(this->instance.get());

        pfnVkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                this->instance.get().getProcAddr("vkCreateDebugUtilsMessengerEXT"));
        assert(pfnVkCreateDebugUtilsMessengerEXT);

        pfnVkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                this->instance.get().getProcAddr( "vkDestroyDebugUtilsMessengerEXT" ) );
        assert(pfnVkDestroyDebugUtilsMessengerEXT);

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

        this->debugMessenger = this->instance.get().createDebugUtilsMessengerEXTUnique(
                vk::DebugUtilsMessengerCreateInfoEXT({}, severityFlags, messageTypeFlags, &debugMessageFunc));
    }

    void Application::createDevice(size_t physicalDeviceIdx)
    {
        assert(this->instance.get());

        std::vector<vk::PhysicalDevice> devices = this->instance.get().enumeratePhysicalDevices();
        assert(devices.size());
        this->physicalDevice = devices[physicalDeviceIdx];
        this->physicalDeviceMemoryProperties = this->physicalDevice.getMemoryProperties();

        setQueueFamilyIndices();
        float queuePriorities = 1.f;
        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos(3);
        for (auto& info : queueCreateInfos)
        {
            info.setQueueCount(1).setPQueuePriorities(&queuePriorities);
        }
        queueCreateInfos[0]
            .setQueueFamilyIndex(this->queueFamilyIndex.graphics);
        queueCreateInfos[1]
            .setQueueFamilyIndex(this->queueFamilyIndex.transfer);
        queueCreateInfos[2]
            .setQueueFamilyIndex(this->queueFamilyIndex.compute);

        vk::DeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo
            .setQueueCreateInfos(queueCreateInfos)
            .setPEnabledExtensionNames(this->deviceExtensions)
            .setPEnabledLayerNames(this->deviceLayers);

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

        this->physicalDevice.getFeatures2(&c.get<vk::PhysicalDeviceFeatures2>());
        this->device = this->physicalDevice.createDeviceUnique(c.get<vk::DeviceCreateInfo>());

        VULKAN_HPP_DEFAULT_DISPATCHER.init(this->device.get());
    }

    void Application::setQueueFamilyIndices()
    {
        auto queueFamilies = this->physicalDevice.getQueueFamilyProperties();

        for (int i{}; i < queueFamilies.size(); ++i)
        {
            auto& prop = queueFamilies[i];
            if (prop.queueCount > 0)
            {
                if ((prop.queueFlags & vk::QueueFlagBits::eGraphics) && this->queueFamilyIndex.graphics == -1)
                {
                    this->queueFamilyIndex.graphics = i;
                }
                else if ((prop.queueFlags & vk::QueueFlagBits::eTransfer) && this->queueFamilyIndex.transfer == -1)
                {
                    this->queueFamilyIndex.transfer = i;
                }
                else if ((prop.queueFlags & vk::QueueFlagBits::eCompute) && this->queueFamilyIndex.compute == -1)
                {
                    this->queueFamilyIndex.compute = i;
                }
            }
        }

        if (queueFamilyIndex.graphics != -1)
        {
            auto& computeIdx  = this->queueFamilyIndex.compute;
            auto& transferIdx = this->queueFamilyIndex.transfer;
            computeIdx  = computeIdx  == -1 ? this->queueFamilyIndex.graphics : computeIdx;
            transferIdx = transferIdx == -1 ? this->queueFamilyIndex.graphics : transferIdx;
        }
        else
        {
            throw std::runtime_error("could not find graphics queue!");
        }
    }

    void Application::createCommandPools()
    {
        vk::CommandPoolCreateInfo poolInfo{};
        poolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

        poolInfo.setQueueFamilyIndex(this->queueFamilyIndex.graphics);
        this->commandPool.graphics = this->device.get().createCommandPoolUnique(poolInfo, nullptr);
        poolInfo.setQueueFamilyIndex(this->queueFamilyIndex.transfer);
        this->commandPool.transfer = this->device.get().createCommandPoolUnique(poolInfo, nullptr);
        poolInfo.setQueueFamilyIndex(this->queueFamilyIndex.compute);
        this->commandPool.compute  = this->device.get().createCommandPoolUnique(poolInfo, nullptr);
    }

    uint32_t Application::getMemoryType(const vk::MemoryRequirements& memoryRequirements, const vk::MemoryPropertyFlags& memoryProperties)
    {
        return getMemoryType(this->physicalDevice, memoryRequirements, memoryProperties);
    }

    uint32_t Application::getMemoryType(const vk::PhysicalDevice& physicalDevice, const vk::MemoryRequirements& memoryRequirements,
            const vk::MemoryPropertyFlags& memoryProperties)
    {
        auto physicalDeviceMemoryProperties = physicalDevice.getMemoryProperties();
        int32_t result{-1};
        for (uint32_t i{}; i < VK_MAX_MEMORY_TYPES; ++i)
        {
            if (memoryRequirements.memoryTypeBits & (1 << i))
            {
                if ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryProperties) == memoryProperties)
                {
                    result = i;
                    break;
                }
            }
        }
        assert(result != -1);

        return result;
    }

    vk::CommandBuffer Application::recordGraphicsCommandBuffer(vk::CommandBufferAllocateInfo info)
    {
        return recordCommandBuffer(this->device.get(), this->commandPool.graphics.get(), info);
    }

    vk::CommandBuffer Application::recordTransferCommandBuffer(vk::CommandBufferAllocateInfo info)
    {
        return recordCommandBuffer(this->device.get(), this->commandPool.transfer.get(), info);
    }

    vk::CommandBuffer Application::recordComputeCommandBuffer(vk::CommandBufferAllocateInfo info)
    {
        return recordCommandBuffer(this->device.get(), this->commandPool.compute.get(), info);
    }

    vk::CommandBuffer Application::recordCommandBuffer(vk::Device device, vk::CommandPool commandPool, vk::CommandBufferAllocateInfo info)
    {
        auto passed = info == vk::CommandBufferAllocateInfo{};
        info = passed ? vk::CommandBufferAllocateInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1) : info;

        vk::CommandBuffer cmdBuffer = device.allocateCommandBuffers(info).front();
        cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

        return cmdBuffer;
    }

    void Application::flushGraphicsCommandBuffer(vk::CommandBuffer& cmdBuffer)
    {
        flushCommandBuffer(this->device.get(), this->commandPool.graphics.get(), cmdBuffer, this->queue.graphics);
    }

    void Application::flushTransferCommandBuffer(vk::CommandBuffer& cmdBuffer)
    {
        flushCommandBuffer(this->device.get(), this->commandPool.transfer.get(), cmdBuffer, this->queue.transfer);
    }

    void Application::flushComputeCommandBuffer(vk::CommandBuffer& cmdBuffer)
    {
        flushCommandBuffer(this->device.get(), this->commandPool.compute.get(), cmdBuffer, this->queue.compute);
    }

    void Application::flushCommandBuffer(vk::Device& device, vk::CommandPool& commandPool, vk::CommandBuffer& cmdBuffer, vk::Queue queue)
    {
        cmdBuffer.end();
        vk::SubmitInfo submitInfo{};
        submitInfo
            .setCommandBufferCount(1)
            .setPCommandBuffers(&cmdBuffer);

        vk::Fence fence = device.createFence(vk::FenceCreateInfo());
        queue.submit(submitInfo, fence);
        try {
            auto r = device.waitForFences(fence, false, 1000000000);
            if (r != vk::Result::eSuccess) std::cout << "flushing failed!\n";
        }
        catch (std::exception const &e)
        {
            std::cout << e.what() << "\n";
        }

        device.destroy(fence);
        device.freeCommandBuffers(commandPool, 1, &cmdBuffer);
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

        cmdbuffer.pipelineBarrier(srcStageMask, dstStageMask, {}, {}, {}, imageMemoryBarrier);
    }

    vk::UniqueShaderModule Application::createShaderModule(const std::string& filename)
    {
        std::ifstream spvFile(filename, std::ios::ate | std::ios::binary);

        if (!spvFile.is_open())
        {
            throw std::runtime_error("could not locate shader .spv file!");
        }

        auto fileSize = spvFile.tellg();
        std::vector<char> code(fileSize);

        spvFile
            .seekg(0)
            .read(code.data(), fileSize);
        spvFile.close();

        return this->device.get().createShaderModuleUnique(
                vk::ShaderModuleCreateInfo{}
                .setCodeSize(code.size())
                .setPCode(reinterpret_cast<const uint32_t*>(code.data())) );
    }

    Application::Buffer Application::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryProperty, void* data)
    {
        return createBuffer(this->device.get(), this->physicalDevice, size, usage, memoryProperty, data);
    }

    Application::Buffer Application::createBuffer(vk::Device& device, vk::PhysicalDevice& physicalDevice, vk::DeviceSize size,
            vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryProperty, void* data)
    {
        std::array<uint32_t, 3> queueFamilyIndices = {0, 1, 2}; // TODO: this might fail but ok for now
        Buffer buffer{};
        buffer.handle = device.createBufferUnique(
                vk::BufferCreateInfo{}
                .setSize(size)
                .setUsage(usage)
                .setSharingMode(vk::SharingMode::eConcurrent)
                .setQueueFamilyIndices(queueFamilyIndices)
                );

        auto memoryRequirements = device.getBufferMemoryRequirements(buffer.handle.get());
        vk::MemoryAllocateFlagsInfo memoryFlagsInfo{};
        if (usage & vk::BufferUsageFlagBits::eShaderDeviceAddress)
        {
            memoryFlagsInfo.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
        }

        buffer.memory = device.allocateMemoryUnique(
                vk::MemoryAllocateInfo{}
                .setAllocationSize(memoryRequirements.size)
                .setMemoryTypeIndex(getMemoryType(physicalDevice, memoryRequirements, memoryProperty))
                .setPNext(&memoryFlagsInfo)
                );
        device.bindBufferMemory(buffer.handle.get(), buffer.memory.get(), 0);

        if (data)
        {
            if (memoryProperty & vk::MemoryPropertyFlagBits::eHostVisible)
            {
                void* dataPtr = device.mapMemory(buffer.memory.get(), 0, size);
                memcpy(dataPtr, data, static_cast<size_t>(size));
                device.unmapMemory(buffer.memory.get());
            }
            else assert(0);
        }

        if (usage & vk::BufferUsageFlagBits::eShaderDeviceAddress)
        {
            buffer.deviceAddress = device.getBufferAddressKHR({buffer.handle.get()});
        }

        buffer.size = size;

        return buffer;
    }

    void Application::checkValidationLayers(const std::vector<const char*>& layersToCheck)
    {
        auto availableLayers = vk::enumerateInstanceLayerProperties();
        assert(availableLayers.size());

        auto checkValidationLayer = [&availableLayers](const char* wantedLayerName)
        {
            auto compareNames = [&wantedLayerName](const vk::LayerProperties& props)
            {
                return std::string(props.layerName) == std::string(wantedLayerName);
            };

            if (std::find_if(availableLayers.begin(), availableLayers.end(), compareNames) == availableLayers.end())
            {
                throw std::runtime_error("layer not found");
            }
        };

        std::for_each(layersToCheck.begin(), layersToCheck.end(), checkValidationLayer);
    }

    void Application::checkExtensions(const std::vector<const char*>& extensionsToCheck)
    {
        auto availableExtensions = vk::enumerateInstanceExtensionProperties();
        assert(availableExtensions.size());

        auto checkExtension = [&availableExtensions](const char* wantedExtensionName)
        {
            auto compareNames = [&wantedExtensionName](const vk::ExtensionProperties& props)
            {
                return std::string(props.extensionName) == std::string(wantedExtensionName);
            };

            if (std::find_if(availableExtensions.begin(), availableExtensions.end(), compareNames) == availableExtensions.end())
            {
                throw std::runtime_error("extension not found");
            }
        };

        std::for_each(extensionsToCheck.begin(), extensionsToCheck.end(), checkExtension);
    }

    void Application::getQueues()
    {
        this->queue.graphics = this->device.get().getQueue(this->queueFamilyIndex.graphics, 0);
        this->queue.transfer = this->device.get().getQueue(this->queueFamilyIndex.transfer, 0);
        this->queue.compute  = this->device.get().getQueue(this->queueFamilyIndex.compute, 0);
    }

    Application::Application(bool isGraphical)
    {
#ifdef DEBUG
        this->instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
        this->instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        this->instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
        if (isGraphical)
        {
            glfwInit();
            pushExtentionsForGraphics();
        }
        createInstance();
        createDevice();
        createCommandPools();
        getQueues();
    }

    void Application::createInstance()
    {
        // https://github.com/KhronosGroup/Vulkan-Hpp#extensions--per-device-function-pointers
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = this->dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        checkValidationLayers(this->instanceLayers);
        checkExtensions(this->instanceExtensions);
        vk::ApplicationInfo applicationInfo("Vulkan Light Bakery", 1, "Asama", 1, VK_API_VERSION_1_2);
        vk::InstanceCreateInfo instanceCreateInfo(vk::InstanceCreateFlags(), &applicationInfo,
                uint32_t(this->instanceLayers.size()), this->instanceLayers.data(),
                uint32_t(this->instanceExtensions.size()), this->instanceExtensions.data());
        this->instance = vk::createInstanceUnique(instanceCreateInfo);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(this->instance.get());

#ifdef DEBUG
        initDebugReportCallback();
#endif
    }

}

