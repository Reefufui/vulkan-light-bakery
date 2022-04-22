// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef UI_HPP
#define UI_HPP

#include "application.hpp"
#include "scene_manager.hpp"
#include "camera.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <imfilebrowser.h>

namespace vlb {

    class UI
    {
        private:

            void loadFonts();
            void createImguiRenderPass();
            void createImguiFrameBuffer();
            void createDepthBuffer();
            vk::UniqueDescriptorPool imguiPool;
            Application::Image depthBuffer;
            vk::UniqueRenderPass imguiPass;
            ImGui::FileBrowser fileDialog;
            std::vector<vk::UniqueFramebuffer> imguiFrameBuffers;

            GLFWwindow*   window;
            vk::Instance  instance;
            vk::PhysicalDevice physicalDevice;
            vk::Device device;
            vk::Queue queue;
            std::vector<vk::ImageView> swapchainImageViews;
            vk::Extent3D surfaceExtent;
            vk::Format surfaceFormat;
            vk::Format depthFormat;

            vlb::SceneManager* pSceneManager;
            void sceneManager();
            void camera();
            void tweakLighting();

        public:
            struct InteractiveLighting
            {
                glm::vec3 lightPosition = glm::vec3(1.0f, 10.0f, 1.0f);
                float shadowBias = 0.005f;
                float ambientLight = 0.01f;
                float Cdiffuse = 0.5f;
                float Cspecular = 0.5f;
                float Cglossyness = 16.0f;
            };
            InteractiveLighting& getLighing();

        private:

            InteractiveLighting lighting;

        public:

            struct InterfaceInitInfo
            {
                GLFWwindow*   window;
                vk::Instance  instance;
                vk::PhysicalDevice physicalDevice;
                vk::Device device;
                vk::Queue queue;
                std::vector<vk::ImageView> swapchainImageViews;
                vk::Extent3D surfaceExtent;
                vk::Format surfaceFormat;
                vk::Format depthFormat;

                vlb::SceneManager* pSceneManager;
            };

            void init(InterfaceInitInfo& info, vk::CommandBuffer& commandBuffer);
            void update();
            void draw(uint32_t imageIndex, vk::CommandBuffer& commandBuffer);
            void serialize();
            void deserialize();
            void cleanup();
    };

}

#endif // ifndef UI_HPP

