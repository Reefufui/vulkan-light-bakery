// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef UI_HPP
#define UI_HPP

#include "application.hpp"

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

            int selectedSceneIndex = 0;
            bool freeSceneFlag = false;
            std::vector<std::string> sceneNames{};
            std::vector<std::string> scenePaths{};
            float lightIntensity = 1.0f;
            float rotationSpeed;

        public:

            ImGui::FileBrowser& getFileDialog();
            std::vector<std::string>& getSceneNames();
            std::vector<std::string>& getScenePaths();
            int& getSelectedSceneIndex();
            bool& getFreeSceneFlag();
            float& getLightIntensity();

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
            };

            void init(InterfaceInitInfo& info, vk::CommandBuffer& commandBuffer);
            void draw(uint32_t imageIndex, vk::CommandBuffer& commandBuffer);
            void serialize();
            void deserialize();
            void cleanup();
    };

}

#endif // ifndef UI_HPP

