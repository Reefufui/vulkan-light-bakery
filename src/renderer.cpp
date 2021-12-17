// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "renderer.hpp"

namespace vlb {

    Renderer::UniqueWindow Renderer::createWindow(const int& windowWidth, const int& windowHeight)
    {
        assert(windowWidth > 0 && windowHeight > 0);
        this->surfaceExtent
            .setWidth(static_cast<uint32_t>(windowWidth))
            .setHeight(static_cast<uint32_t>(windowHeight))
            .setDepth(1);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        auto glfwWindow = glfwCreateWindow(windowWidth, windowHeight, "Vulkan", nullptr, nullptr);
        std::unique_ptr<GLFWwindow, WindowDestroy> window(glfwWindow);
        return window;
    }

    vk::UniqueSurfaceKHR Renderer::createSurface()
    {
        VkSurfaceKHR tmpSurface;
        VkResult res = glfwCreateWindowSurface(this->instance.get(), this->window.get(), nullptr, &tmpSurface);
        if (res)
        {
            throw std::runtime_error("failed to create window surface");
        }
        vk::ObjectDestroy<vk::Instance, vk::DispatchLoaderDynamic> surfaceDeleter(this->instance.get());

        auto surface = vk::UniqueSurfaceKHR(tmpSurface, surfaceDeleter);

        if (!this->physicalDevice.getSurfaceSupportKHR(this->queueFamilyIndex.graphics, *surface))
        {
            throw std::runtime_error("surface is not supported");
        }

        return surface;
    }

    vk::UniqueSwapchainKHR Renderer::createSwapchain()
    {
        vk::Extent2D extent{this->surfaceExtent.width, this->surfaceExtent.height};

        vk::SurfaceCapabilitiesKHR capabilities = this->physicalDevice.getSurfaceCapabilitiesKHR(this->surface.get());

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

        vk::ObjectDestroy<vk::Device, vk::DispatchLoaderDynamic> swapchainDeleter(this->device.get());
        vk::SwapchainKHR swapChain{};
        if (this->device.get().createSwapchainKHR(&createInfo, nullptr, &swapChain) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create swapchain");
        }

        return vk::UniqueSwapchainKHR(swapChain, swapchainDeleter);
    }

    void Renderer::createSwapchainResourses()
    {
        this->swapChainImages = this->device.get().getSwapchainImagesKHR(this->swapchain.get());
        uint32_t imageCount = this->swapChainImages.size();

        this->swapchainImageViews.resize(imageCount);
        vk::ObjectDestroy<vk::Device, vk::DispatchLoaderDynamic> imageViewDeleter(this->device.get());

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
            if (this->device.get().createImageView(&createInfo, nullptr, &imageView) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to create image views for swapchain");
            }

            uniqueImageView = vk::UniqueImageView(imageView, imageViewDeleter);
        }
    }

    std::vector<vk::UniqueCommandBuffer> Renderer::createDrawCommandBuffers()
    {
        return this->device.get().allocateCommandBuffersUnique(
                vk::CommandBufferAllocateInfo{}
                .setCommandPool(this->commandPool.graphics.get())
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
            imageAvailableSemaphores[i] = this->device.get().createSemaphoreUnique({});
            renderFinishedSemaphores[i] = this->device.get().createSemaphoreUnique({});
            inFlightFences[i] = this->device.get().createFence({ vk::FenceCreateFlagBits::eSignaled });
        }
    }

    void Renderer::present(uint32_t imageIndex)
    {
        auto result = this->queue.graphics.presentKHR(
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

    void Renderer::initUI()
    {
        auto commandBuffer = Application::recordGraphicsCommandBuffer();
        std::vector<vk::ImageView> swapchainImageViews2{};
        for (auto& sciv : this->swapchainImageViews)
        {
            swapchainImageViews2.push_back(sciv.get());
        }

        auto uiInitInfo = UI::InterfaceInitInfo
        {
            this->window.get(),
                this->instance.get(),
                this->physicalDevice,
                this->device.get(),
                this->queue.transfer,
                swapchainImageViews2,
                this->surfaceExtent,
                this->surfaceFormat,
                this->depthFormat,

                &this->sceneManager
        };
        this->ui.init(uiInitInfo, commandBuffer);
        flushGraphicsCommandBuffer(commandBuffer);
    }

    void Renderer::initSceneManager()
    {
        auto sceneManagerInitInfo = SceneManager::InitInfo
        {
            this->physicalDevice,
                this->device.get(),
                this->queue.transfer,
                this->commandPool.transfer.get(),
                this->queue.graphics,
                this->commandPool.graphics.get(),
                this->ui.getScenePaths(),
                static_cast<uint32_t>(this->swapchainImageViews.size())
        };
        this->sceneManager.init(sceneManagerInitInfo);
    }

    void Renderer::render()
    {
        while (!glfwWindowShouldClose(this->window.get()))
        {
            glfwPollEvents();
            //TODO: handle window resize here
            this->ui.update();
            draw();
            this->currentFrame = (this->currentFrame + 1) % this->maxFramesInFlight;
        }

        this->device.get().waitIdle();
        ui.cleanup();
        glfwTerminate();
    }

    Renderer::Renderer()
    {
        this->window  = createWindow(2000, 1000);
        this->surface = createSurface();
        this->swapchain = createSwapchain();
        createSwapchainResourses();
        createDrawCommandBuffers();
        createSyncObjects();

        initUI();
        initSceneManager();
    }

    Renderer::~Renderer()
    {
        for (size_t i = 0; i < this->maxFramesInFlight; ++i)
        {
            this->device.get().destroy(this->inFlightFences[i]);
        }
    }

}

