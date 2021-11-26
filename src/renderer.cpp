// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "renderer.hpp"

namespace vlb {

    Renderer::UniqueWindow Renderer::createWindow(const int& a_windowWidth, const int& a_windowHeight)
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

    vk::UniqueSurfaceKHR Renderer::createSurface()
    {
        VkSurfaceKHR tmpSurface;
        VkResult res = glfwCreateWindowSurface(m_instance, m_window.get(), nullptr, &tmpSurface);
        if (res)
        {
            throw std::runtime_error("failed to create window surface");
        }
        vk::ObjectDestroy<vk::Instance, vk::DispatchLoaderDynamic> surfaceDeleter(m_instance);

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

    bool Renderer::isSurfaceSupported(const vk::UniqueSurfaceKHR& a_surface)
    {
        assert(a_surface);
        return m_physicalDevice.getSurfaceSupportKHR(getQueueFamilyIndex(), *a_surface);
    }

    vk::UniqueSwapchainKHR Renderer::createSwapchain()
    {
        vk::Extent2D extent{};

        vk::SurfaceCapabilitiesKHR capabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(this->m_surface.get());

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
        createInfo
            .setSurface(this->m_surface.get())
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

        vk::ObjectDestroy<vk::Device, vk::DispatchLoaderDynamic> swapchainDeleter(m_device);
        vk::SwapchainKHR swapChain{};
        if (m_device.createSwapchainKHR(&createInfo, nullptr, &swapChain) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create swapchain");
        }

        return vk::UniqueSwapchainKHR(swapChain, swapchainDeleter);
    }

    void Renderer::createSwapchainResourses()
    {
        this->swapChainImages = this->m_device.getSwapchainImagesKHR(this->m_swapchain.get());
        uint32_t imageCount = this->swapChainImages.size();

        m_swapchainImageViews.resize(imageCount);
        vk::ObjectDestroy<vk::Device, vk::DispatchLoaderDynamic> imageViewDeleter(m_device);

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
        for (auto& uniqueImageView : m_swapchainImageViews)
        {
            createInfo.image        = swapChainImages[i++];

            vk::ImageView imageView{};
            if (m_device.createImageView(&createInfo, nullptr, &imageView) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to create image views for swapchain");
            }

            uniqueImageView = vk::UniqueImageView(imageView, imageViewDeleter);
        }
    }

    std::vector<vk::UniqueCommandBuffer> Renderer::createDrawCommandBuffers()
    {
        return this->m_device.allocateCommandBuffersUnique(
                vk::CommandBufferAllocateInfo{}
                .setCommandPool(this->m_commandPool)
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
            imageAvailableSemaphores[i] = this->m_device.createSemaphoreUnique({});
            renderFinishedSemaphores[i] = this->m_device.createSemaphoreUnique({});
            inFlightFences[i] = this->m_device.createFence({ vk::FenceCreateFlagBits::eSignaled });
        }
    }

    void Renderer::present(uint32_t imageIndex)
    {
        auto result = this->m_graphicsQueue.presentKHR(
                vk::PresentInfoKHR{}
                .setWaitSemaphores(this->renderFinishedSemaphores[this->currentFrame].get())
                .setSwapchains(this->m_swapchain.get())
                .setImageIndices(imageIndex)
                );

        if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to present image");
        }
    }

    void Renderer::render()
    {
        while (!glfwWindowShouldClose(m_window.get()))
        {
            glfwPollEvents();
            draw();

            this->currentFrame = (this->currentFrame + 1) % this->maxFramesInFlight;
        }

        m_device.waitIdle();
    }

    Renderer::Renderer()
    {
        glfwInit();
        Application::pushPresentationExtensions();
        Application::createInstance();
        Application::createDevice();
        Application::createCommandPool();

        m_window  = createWindow(1920, 1080);
        m_surface = createSurface();
        m_swapchain = createSwapchain();
        createSwapchainResourses();
        createDrawCommandBuffers();
        createSyncObjects();

        m_graphicsQueue = m_device.getQueue(getQueueFamilyIndex(), 0);
    }

    Renderer::~Renderer()
    {
        for (size_t i = 0; i < this->maxFramesInFlight; ++i)
        {
            this->m_device.destroy(this->inFlightFences[i]);
        }
    }

}

