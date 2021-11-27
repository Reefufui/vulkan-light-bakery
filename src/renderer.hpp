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
            UniqueWindow         window;
            vk::UniqueSurfaceKHR surface;

            UniqueWindow createWindow(const int& windowWidth = 480, const int& windowHeight = 480);
            vk::UniqueSurfaceKHR createSurface();
            vk::QueueFlagBits getQueueFlag() override;
            bool isSurfaceSupported(const vk::UniqueSurfaceKHR& surface);
            vk::UniqueSwapchainKHR createSwapchain();
            void createSwapchainResourses();
            void createSyncObjects();
            virtual void draw() = 0;
            const int maxFramesInFlight = 2;

            void imguiInit();
            vk::UniqueDescriptorPool imguiPool;

        protected:
            vk::Queue            graphicsQueue;
            vk::Format           surfaceFormat{vk::Format::eB8G8R8A8Unorm};
            vk::ColorSpaceKHR    surfaceColorSpace{vk::ColorSpaceKHR::eSrgbNonlinear};
            //TODO: change to vk::Extent
            uint32_t             windowWidth;
            uint32_t             windowHeight;

            std::vector<vk::UniqueSemaphore> imageAvailableSemaphores;
            std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
            std::vector<vk::Fence> inFlightFences;
            std::vector<vk::Fence> imagesInFlight;

            vk::UniqueSwapchainKHR swapchain;
            std::vector<vk::UniqueImageView> swapchainImageViews;
            std::vector<vk::Image> swapChainImages{};
            std::vector<vk::UniqueCommandBuffer> createDrawCommandBuffers();
            void present(uint32_t imageIndex);
            size_t currentFrame = 0;

        public:

            Renderer();
            void render();
            ~Renderer();
    };

}

#endif // ifndef RENDERER_HPP
