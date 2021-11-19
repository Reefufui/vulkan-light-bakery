// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "raytracer.hpp"

Raytracer::UniqueWindow Raytracer::createWindow(const int& a_windowWidth, const int& a_windowHeight)
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

vk::UniqueSurfaceKHR Raytracer::createSurface()
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

vk::QueueFlagBits Raytracer::getQueueFlag()
{
    return vk::QueueFlagBits::eGraphics;
}

bool Raytracer::isSurfaceSupported(const vk::UniqueSurfaceKHR& a_surface)
{
    assert(a_surface);
    return m_physicalDevice.getSurfaceSupportKHR(getQueueFamilyIndex(), *a_surface);
}

vk::UniqueSwapchainKHR Raytracer::createSwapchain()
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

void Raytracer::createSwapchainResourses()
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

void Raytracer::createBLAS()
{
    // NOTE: hello triangle
    const float vertices[9] = {
        0.25f, 0.25f, 0.0f,
        0.75f, 0.25f, 0.0f,
        0.50f, 0.75f, 0.0f
    };

    const uint32_t indices[3] = { 0, 1, 2 };

}

void Raytracer::draw()
{
}

void Raytracer::render()
{
    m_swapchain = createSwapchain();
    createSwapchainResourses();

    // TODO: Loading of scenes using glTF here
    createBLAS();

    while (!glfwWindowShouldClose(m_window.get()))
    {
        glfwPollEvents();
        draw();
    }

    m_device.waitIdle();
}

Raytracer::Raytracer()
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
}

Raytracer::~Raytracer()
{
    glfwTerminate();
}

