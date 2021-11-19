// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef RAYTRACER_HPP
#define RAYTRACER_HPP

#include "application.hpp"

class Raytracer : public Application
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

        UniqueWindow createWindow(const int& a_windowWidth = 480, const int& a_windowHeight = 480);
        vk::UniqueSurfaceKHR createSurface();
        vk::QueueFlagBits getQueueFlag() override;
        bool isSurfaceSupported(const vk::UniqueSurfaceKHR& a_surface);
        vk::UniqueSwapchainKHR createSwapchain();
        void createSwapchainResourses();
        void createBLAS();
        void draw();

    public:

        Raytracer();
        ~Raytracer();
        void render();
};

#endif // ifndef RAYTRACER_HPP
