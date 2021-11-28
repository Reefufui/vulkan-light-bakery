// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "renderer.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

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

    void Renderer::createImguiRenderPass()
    {
        std::array<vk::AttachmentDescription, 2> attachmentDescriptions;
        attachmentDescriptions[0] = vk::AttachmentDescription( vk::AttachmentDescriptionFlags(),
                this->surfaceFormat,
                vk::SampleCountFlagBits::e1,
                vk::AttachmentLoadOp::eLoad,
                vk::AttachmentStoreOp::eStore,
                vk::AttachmentLoadOp::eDontCare,
                vk::AttachmentStoreOp::eDontCare,
                vk::ImageLayout::ePresentSrcKHR,
                vk::ImageLayout::ePresentSrcKHR );
        attachmentDescriptions[1] = vk::AttachmentDescription( vk::AttachmentDescriptionFlags(),
                this->depthFormat,
                vk::SampleCountFlagBits::e1,
                vk::AttachmentLoadOp::eClear,
                vk::AttachmentStoreOp::eDontCare,
                vk::AttachmentLoadOp::eDontCare,
                vk::AttachmentStoreOp::eDontCare,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eDepthStencilAttachmentOptimal );

        vk::AttachmentReference colorReference( 0, vk::ImageLayout::eColorAttachmentOptimal );
        vk::AttachmentReference depthReference( 1, vk::ImageLayout::eDepthStencilAttachmentOptimal );

        vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(),
                vk::PipelineBindPoint::eGraphics, {}, colorReference, {}, &depthReference );

        this->imguiPass = this->device.createRenderPassUnique(
                vk::RenderPassCreateInfo( vk::RenderPassCreateFlags(), attachmentDescriptions, subpass ) );
    }

    void Renderer::createDepthBuffer()
    {
        vk::FormatProperties formatProperties = this->physicalDevice.getFormatProperties(this->depthFormat);

        vk::ImageTiling tiling;
        if (formatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        {
            tiling = vk::ImageTiling::eLinear;
        }
        else if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        {
            tiling = vk::ImageTiling::eOptimal;
        }
        else
        {
            throw std::runtime_error("DepthStencilAttachment is not supported for D16Unorm depth format.");
        }

        vk::ImageCreateInfo imageCreateInfo( vk::ImageCreateFlags(),
                vk::ImageType::e2D,
                depthFormat,
                this->surfaceExtent,
                1,
                1,
                vk::SampleCountFlagBits::e1,
                tiling,
                vk::ImageUsageFlagBits::eDepthStencilAttachment);

        this->depthBuffer.handle = this->device.createImageUnique(imageCreateInfo);

        vk::MemoryPropertyFlags memoryProperties   = vk::MemoryPropertyFlagBits::eDeviceLocal;
        vk::MemoryRequirements  memoryRequirements = device.getImageMemoryRequirements(this->depthBuffer.handle.get());
        auto typeIndex = Application::getMemoryType(memoryRequirements, memoryProperties);
        assert(typeIndex != uint32_t( ~0 ));

        this->depthBuffer.memory = this->device.allocateMemoryUnique(vk::MemoryAllocateInfo(memoryRequirements.size, typeIndex));
        this->device.bindImageMemory(this->depthBuffer.handle.get(), this->depthBuffer.memory.get(), 0);

        this->depthBuffer.imageView = this->device.createImageViewUnique(vk::ImageViewCreateInfo(vk::ImageViewCreateFlags(),
                        this->depthBuffer.handle.get(),
                        vk::ImageViewType::e2D,
                        this->depthFormat,
                        {},
                        { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 } ) );
    }

    void Renderer::createImguiFrameBuffer()
    {
        std::array<vk::ImageView, 2> attachments;
        attachments[1] = this->depthBuffer.imageView.get();

        vk::FramebufferCreateInfo framebufferCreateInfo{};
        framebufferCreateInfo
            .setRenderPass(this->imguiPass.get())
            .setAttachments(attachments)
            .setWidth(this->surfaceExtent.width)
            .setHeight(this->surfaceExtent.height)
            .setLayers(this->surfaceExtent.depth);

        this->imguiFrameBuffers.reserve(this->swapchainImageViews.size());
        for (auto const & imageView : this->swapchainImageViews)
        {
            attachments[0] = imageView.get();
            this->imguiFrameBuffers.push_back(this->device.createFramebufferUnique(framebufferCreateInfo));
        }
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
        ImGui_ImplGlfw_InitForVulkan(this->window.get(), true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = this->instance;
        init_info.PhysicalDevice = this->physicalDevice;
        init_info.Device = this->device;
        init_info.Queue = this->graphicsQueue;
        init_info.DescriptorPool = imguiPool.get();
        init_info.MinImageCount = this->swapchainImageViews.size();
        init_info.ImageCount = this->swapchainImageViews.size();
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_Init(&init_info, this->imguiPass.get());

        auto commandBuffer = Application::recordCommandBuffer();
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
        flushCommandBuffer(commandBuffer, this->graphicsQueue);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void Renderer::render()
    {
        while (!glfwWindowShouldClose(this->window.get()))
        {
            glfwPollEvents();
            //TODO: handle window resize here
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::ShowDemoWindow();
            draw();
            this->currentFrame = (this->currentFrame + 1) % this->maxFramesInFlight;
        }

        this->device.waitIdle();
    }

    Renderer::Renderer()
    {
        glfwInit();
        Application::pushPresentationExtensions();
        Application::createInstance();
        Application::createDevice();
        Application::createCommandPool();

        this->window  = createWindow(2000, 1000);
        this->surface = createSurface();
        this->swapchain = createSwapchain();
        createSwapchainResourses();
        createDrawCommandBuffers();
        createSyncObjects();

        this->graphicsQueue = this->device.getQueue(getQueueFamilyIndex(), 0);

        createDepthBuffer();
        createImguiRenderPass();
        createImguiFrameBuffer();
        imguiInit();
    }

    Renderer::~Renderer()
    {
        for (size_t i = 0; i < this->maxFramesInFlight; ++i)
        {
            this->device.destroy(this->inFlightFences[i]);
        }
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

}

