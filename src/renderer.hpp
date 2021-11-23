// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "application.hpp"

namespace vlb {

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

        private:

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

            UniqueWindow createWindow(const int& a_windowWidth = 480, const int& a_windowHeight = 480);
            vk::UniqueSurfaceKHR createSurface();
            vk::QueueFlagBits getQueueFlag() override;
            bool isSurfaceSupported(const vk::UniqueSurfaceKHR& a_surface);
            vk::UniqueSwapchainKHR createSwapchain();
            void createSwapchainResourses();
            virtual void draw() = 0;

        public:

            Renderer();
            ~Renderer();
            void render();
    };

}

#endif // ifndef RENDERER_HPP
