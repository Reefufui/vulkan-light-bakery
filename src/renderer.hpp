// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "application.hpp"
#include "scene_manager.hpp"
#include "ui.hpp"
#include "camera.hpp"

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

            typedef std::unique_ptr<GLFWwindow, WindowDestroy> UniqueWindow;
            UniqueWindow         window;
            vk::UniqueSurfaceKHR surface;

            UniqueWindow createWindow(const int& windowWidth = 480, const int& windowHeight = 480);
            vk::UniqueSurfaceKHR createSurface();
            vk::UniqueSwapchainKHR createSwapchain();
            void createSwapchainResourses();
            void createSyncObjects();
            virtual void draw() = 0;
            const int maxFramesInFlight = 2;
            void initUI();
            void initSceneManager();
            void initCamera();

        protected:
            vk::Format           surfaceFormat{vk::Format::eB8G8R8A8Unorm};
            vk::Format           depthFormat{vk::Format::eD16Unorm};
            vk::ColorSpaceKHR    surfaceColorSpace{vk::ColorSpaceKHR::eSrgbNonlinear};
            vk::Extent3D         surfaceExtent;

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

            SceneManager sceneManager;
            UI ui;
            Camera camera;

        public:

            Renderer();
            void render();
            ~Renderer();
    };

}

#endif // ifndef RENDERER_HPP

