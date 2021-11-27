// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "renderer.hpp"

#include <imgui.h>
//#include <imgui_impl_glfw.h>
//#include <imgui_impl_vulkan.h>

namespace vlb {

    Renderer::UniqueWindow Renderer::createWindow(const int& windowWidth, const int& windowHeight)
    {
        assert(windowWidth > 0 && windowHeight > 0);
        this->windowWidth  = static_cast<uint32_t>(windowWidth);
        this->windowHeight = static_cast<uint32_t>(windowHeight);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        auto glfwWindow = glfwCreateWindow(this->windowWidth, this->windowHeight, "Vulkan", nullptr, nullptr);
        std::unique_ptr<GLFWwindow, WindowDestroy> window(glfwWindow);
        return window;
    }

    vk::UniqueSurfaceKHR Renderer::createSurface()
    {
        VkSurfaceKHR tmpSurface;
        VkResult res = glfwCreateWindowSurface(this->instance, this->window.get(), nullptr, &tmpSurface);
        if (res)
        {
            throw std::runtime_error("failed to create window surface");
        }
        vk::ObjectDestroy<vk::Instance, vk::DispatchLoaderDynamic> surfaceDeleter(this->instance);

        auto surface = vk::UniqueSurfaceKHR(tmpSurface, surfaceDeleter);

        if (!isSurfaceSupported(surface))
        {
            throw std::runtime_error("surface is not supported");
        }

        return surface;
    }

    vk::QueueFlagBits Renderer::getQueueFlag()
    {
        return vk::QueueFlagBits::eGraphics;
    }

    bool Renderer::isSurfaceSupported(const vk::UniqueSurfaceKHR& surface)
    {
        assert(surface);
        return this->physicalDevice.getSurfaceSupportKHR(getQueueFamilyIndex(), *surface);
    }

    vk::UniqueSwapchainKHR Renderer::createSwapchain()
    {
        vk::Extent2D extent{};

        vk::SurfaceCapabilitiesKHR capabilities = this->physicalDevice.getSurfaceCapabilitiesKHR(this->surface.get());

        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            extent = capabilities.currentExtent;
        }
        else
        {
            extent.width  = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, this->windowWidth));
            extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, this->windowHeight));
        }

        uint32_t imageCount = capabilities.minImageCount + 1;

        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        {
            imageCount = capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR createInfo{};
        createInfo
            .setSurface(this->surface.get())
            .setMinImageCount(imageCount)
            .setImageFormat(this->surfaceFormat)
            .setImageColorSpace(this->surfaceColorSpace)
            .setImageExtent(extent)
            .setImageArrayLayers(1)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
            .setImageSharingMode(vk::SharingMode::eExclusive)
            .setQueueFamilyIndices(nullptr)
            .setPreTransform(capabilities.currentTransform)
            .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
            .setPresentMode(vk::PresentModeKHR::eFifo)
            .setClipped(VK_TRUE)
            .setOldSwapchain(nullptr);

        vk::ObjectDestroy<vk::Device, vk::DispatchLoaderDynamic> swapchainDeleter(this->device);
        vk::SwapchainKHR swapChain{};
        if (this->device.createSwapchainKHR(&createInfo, nullptr, &swapChain) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create swapchain");
        }

        return vk::UniqueSwapchainKHR(swapChain, swapchainDeleter);
    }

    void Renderer::createSwapchainResourses()
    {
        this->swapChainImages = this->device.getSwapchainImagesKHR(this->swapchain.get());
        uint32_t imageCount = this->swapChainImages.size();

        this->swapchainImageViews.resize(imageCount);
        vk::ObjectDestroy<vk::Device, vk::DispatchLoaderDynamic> imageViewDeleter(this->device);

        vk::ImageViewCreateInfo createInfo{};
        createInfo.viewType     = vk::ImageViewType::e2D;
        createInfo.format       = this->surfaceFormat;
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
        for (auto& uniqueImageView : this->swapchainImageViews)
        {
            createInfo.image        = swapChainImages[i++];

            vk::ImageView imageView{};
            if (this->device.createImageView(&createInfo, nullptr, &imageView) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to create image views for swapchain");
            }

            uniqueImageView = vk::UniqueImageView(imageView, imageViewDeleter);
        }
    }

    std::vector<vk::UniqueCommandBuffer> Renderer::createDrawCommandBuffers()
    {
        return this->device.allocateCommandBuffersUnique(
                vk::CommandBufferAllocateInfo{}
                .setCommandPool(this->commandPool)
                .setLevel(vk::CommandBufferLevel::ePrimary)
                .setCommandBufferCount(this->swapChainImages.size())
                );
    }

    void Renderer::createSyncObjects()
    {
        imageAvailableSemaphores.resize(this->maxFramesInFlight);
        renderFinishedSemaphores.resize(this->maxFramesInFlight);
        inFlightFences.resize(this->maxFramesInFlight);
        this->imagesInFlight.resize(this->swapChainImages.size());

        for (size_t i = 0; i < this->maxFramesInFlight; i++) {
            imageAvailableSemaphores[i] = this->device.createSemaphoreUnique({});
            renderFinishedSemaphores[i] = this->device.createSemaphoreUnique({});
            inFlightFences[i] = this->device.createFence({ vk::FenceCreateFlagBits::eSignaled });
        }
    }

    void Renderer::present(uint32_t imageIndex)
    {
        auto result = this->graphicsQueue.presentKHR(
                vk::PresentInfoKHR{}
                .setWaitSemaphores(this->renderFinishedSemaphores[this->currentFrame].get())
                .setSwapchains(this->swapchain.get())
                .setImageIndices(imageIndex)
                );

        if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to present image");
        }
    }

    void Renderer::render()
    {
        while (!glfwWindowShouldClose(this->window.get()))
        {
            glfwPollEvents();
            draw();

            this->currentFrame = (this->currentFrame + 1) % this->maxFramesInFlight;
        }

        this->device.waitIdle();
    }

    void Renderer::imguiInit()
    {
        const int maxSets = 1000;

        std::vector<vk::DescriptorPoolSize> poolSizes =
        {
            { vk::DescriptorType::eSampler, maxSets },
            { vk::DescriptorType::eCombinedImageSampler, maxSets },
            { vk::DescriptorType::eSampledImage, maxSets },
            { vk::DescriptorType::eStorageImage, maxSets },

            { vk::DescriptorType::eUniformTexelBuffer, maxSets },
            { vk::DescriptorType::eStorageTexelBuffer, maxSets },
            { vk::DescriptorType::eUniformBuffer, maxSets },
            { vk::DescriptorType::eStorageBuffer, maxSets },
            { vk::DescriptorType::eUniformBufferDynamic, maxSets },
            { vk::DescriptorType::eStorageBufferDynamic, maxSets },

            { vk::DescriptorType::eInputAttachment, maxSets }
        };


        this->imguiPool = this->device.createDescriptorPoolUnique(
                vk::DescriptorPoolCreateInfo{}
                .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
                .setMaxSets(maxSets)
                .setPoolSizes(poolSizes)
                );

        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(this->window.get());
    }

    Renderer::Renderer()
    {
        glfwInit();
        Application::pushPresentationExtensions();
        Application::createInstance();
        Application::createDevice();
        Application::createCommandPool();

        this->window  = createWindow(1920, 1080);
        this->surface = createSurface();
        this->swapchain = createSwapchain();
        createSwapchainResourses();
        createDrawCommandBuffers();
        createSyncObjects();

        imguiInit();

        this->graphicsQueue = this->device.getQueue(getQueueFamilyIndex(), 0);
    }

    Renderer::~Renderer()
    {
        for (size_t i = 0; i < this->maxFramesInFlight; ++i)
        {
            this->device.destroy(this->inFlightFences[i]);
        }
    }

}

