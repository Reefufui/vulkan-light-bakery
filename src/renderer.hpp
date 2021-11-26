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
                    glfwTerminate();
                }
            };

            typedef std::unique_ptr<GLFWwindow, WindowDestroy> UniqueWindow;

            UniqueWindow m_window;

            vk::UniqueSurfaceKHR                 m_surface;
            vk::UniqueSwapchainKHR               m_swapchain;
            std::vector<vk::UniqueImageView>     m_swapchainImageViews;

            UniqueWindow createWindow(const int& a_windowWidth = 480, const int& a_windowHeight = 480);
            vk::UniqueSurfaceKHR createSurface();
            vk::QueueFlagBits getQueueFlag() override;
            bool isSurfaceSupported(const vk::UniqueSurfaceKHR& a_surface);
            vk::UniqueSwapchainKHR createSwapchain();
            void createSwapchainResourses();
            void createDrawCommandBuffers();
            void createSyncObjects();
            virtual void draw() = 0;

        protected:
            vk::Queue            m_graphicsQueue;
            vk::SurfaceFormatKHR m_surfaceFormat{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
            //TODO: change to vk::Extent
            uint32_t             m_windowWidth;
            uint32_t             m_windowHeight;

            std::vector<vk::UniqueCommandBuffer> drawCommandBuffers{};
            std::vector<vk::Image>               swapChainImages{};

            const int maxFramesInFlight = 2;
            std::vector<vk::UniqueSemaphore> imageAvailableSemaphores;
            std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
            std::vector<vk::UniqueFence> inFlightFences;
            std::vector<vk::UniqueFence> imagesInFlight;

            size_t currentFrame = 0;

        public:

            Renderer();
            void render();
    };

}

#endif // ifndef RENDERER_HPP
