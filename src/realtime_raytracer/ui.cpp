// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "ui.hpp"

#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <filesystem>
#include <fstream>

namespace ImGui {
    static auto vector_getter = [](void* vec, int idx, const char** out_text)
    {
        auto& vector = *static_cast<std::vector<std::string>*>(vec);
        if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
        *out_text = vector.at(idx).c_str();
        return true;
    };

    bool Combo(const char* label, int* currIndex, std::vector<std::string>& values, int size)
    {
        if (values.empty()) { return false; }
        return Combo(label, currIndex, vector_getter,
                static_cast<void*>(&values), values.size(), size);
    }
}

namespace glm
{
    void from_json(const nlohmann::json& j, vec3& vec)
    {
        j.at("x").get_to(vec.x);
        j.at("y").get_to(vec.y);
        j.at("z").get_to(vec.z);
    }

    void to_json(nlohmann::json& j, const vec3& vec)
    {
        j = nlohmann::json{{"x", vec.x}, {"y", vec.y}, {"z", vec.z}};
    }
}

namespace vlb {

    void UI::createImguiRenderPass()
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

    void UI::createDepthBuffer()
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

        vk::ImageCreateInfo imageCreateInfo(vk::ImageCreateFlags(),
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
        auto memoryRequirements             = this->device.getImageMemoryRequirements(this->depthBuffer.handle.get());
        auto physicalDeviceMemoryProperties = this->physicalDevice.getMemoryProperties();

        uint32_t typeIndex = -1;
        for (uint32_t i{}; i < VK_MAX_MEMORY_TYPES; ++i)
        {
            if (memoryRequirements.memoryTypeBits & (1 << i))
            {
                if ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryProperties) == memoryProperties)
                {
                    typeIndex = i;
                    break;
                }
            }
        }
        assert(typeIndex != -1);

        this->depthBuffer.memory = this->device.allocateMemoryUnique(vk::MemoryAllocateInfo(memoryRequirements.size, typeIndex));
        this->device.bindImageMemory(this->depthBuffer.handle.get(), this->depthBuffer.memory.get(), 0);

        this->depthBuffer.imageView = this->device.createImageViewUnique(vk::ImageViewCreateInfo(vk::ImageViewCreateFlags(),
                    this->depthBuffer.handle.get(),
                    vk::ImageViewType::e2D,
                    this->depthFormat,
                    {},
                    { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 } ) );
    }

    void UI::createImguiFrameBuffer()
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
            attachments[0] = imageView;
            this->imguiFrameBuffers.push_back(this->device.createFramebufferUnique(framebufferCreateInfo));
        }
    }

    void UI::init(InterfaceInitInfo& info, vk::CommandBuffer& commandBuffer)
    {
        this->device = info.device;
        this->window = info.window;
        this->instance = info.instance;
        this->physicalDevice = info.physicalDevice;
        this->queue = info.queue;
        this->swapchainImageViews = info.swapchainImageViews;
        this->surfaceExtent = info.surfaceExtent;
        this->surfaceFormat = info.surfaceFormat;
        this->depthFormat = info.depthFormat;

        this->pSceneManager = info.pSceneManager;

        createDepthBuffer();
        createImguiRenderPass();
        createImguiFrameBuffer();

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
        ImGui_ImplGlfw_InitForVulkan(this->window, true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = this->instance;
        init_info.PhysicalDevice = this->physicalDevice;
        init_info.Device = this->device;
        init_info.Queue = this->queue;
        init_info.DescriptorPool = imguiPool.get();
        init_info.MinImageCount = this->swapchainImageViews.size();
        init_info.ImageCount = this->swapchainImageViews.size();
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_Init(&init_info, this->imguiPass.get());

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->AddFontFromFileTTF("assets/Roboto-Medium.ttf", 20);
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);

        auto& style = ImGui::GetStyle();
        style.FrameRounding = 4.0f;
        style.WindowBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.GrabRounding = 4.0f;
        style.WindowRounding = 4.0f;
        style.ChildRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;

        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.44f, 0.44f, 0.44f, 0.60f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.57f, 0.57f, 0.57f, 0.70f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.76f, 0.76f, 0.76f, 0.80f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.13f, 0.75f, 0.55f, 0.80f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.13f, 0.75f, 0.75f, 0.80f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.13f, 0.75f, 1.00f, 0.80f);
        colors[ImGuiCol_Button]                 = ImVec4(0.13f, 0.75f, 0.55f, 0.40f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.13f, 0.75f, 0.75f, 0.60f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.13f, 0.75f, 1.00f, 0.80f);
        colors[ImGuiCol_Header]                 = ImVec4(0.13f, 0.75f, 0.55f, 0.40f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.13f, 0.75f, 0.75f, 0.60f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.13f, 0.75f, 1.00f, 0.80f);
        colors[ImGuiCol_Separator]              = ImVec4(0.13f, 0.75f, 0.55f, 0.40f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.13f, 0.75f, 0.75f, 0.60f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.13f, 0.75f, 1.00f, 0.80f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.13f, 0.75f, 0.55f, 0.40f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.13f, 0.75f, 0.75f, 0.60f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.13f, 0.75f, 1.00f, 0.80f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.13f, 0.75f, 0.55f, 0.80f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.13f, 0.75f, 0.75f, 0.80f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.13f, 0.75f, 1.00f, 0.80f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.36f, 0.36f, 0.36f, 0.54f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
        colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

        this->fileDialog = ImGui::FileBrowser{};
        this->fileDialog.SetTitle("Choose model file");
        this->fileDialog.SetTypeFilters({ ".gltf", ".glb", ".*" });

        deserialize();
    }

    void UI::camera()
    {
        ImGui::Text("Camera Settings");

        auto camera = this->pSceneManager->getCamera();

        auto ms = camera->getMovementSpeed();
        ImGui::SliderFloat("Movement speed", &ms, 0.0f, 50.0f);
        camera->setMovementSpeed(ms);

        auto rs = camera->getRotationSpeed();
        ImGui::SliderFloat("Mouse sensitivity", &rs, 0.0f, 1.0f);
        camera->setRotationSpeed(rs);

        if (ImGui::Button("Reset"))
        {
            camera->reset();
        }


        const auto& scene = this->pSceneManager->getScene();
        int cameraIndex = scene->getCameraIndex();
        ImGui::SliderInt("Camera index", &cameraIndex, 0, scene->getCamerasCount() - 1);
        this->pSceneManager->getScene()->setCameraIndex(cameraIndex);
    }

    void UI::sceneManager()
    {
        ImGui::Text("Choose model to render:");

        const auto& sceneNames = this->pSceneManager->getSceneNames();
        int sceneIndex = this->pSceneManager->getSceneIndex();
        if (ImGui::BeginCombo("##combo", sceneNames[sceneIndex].c_str()))
        {
            for (int n = 0; n < sceneNames.size(); ++n)
            {
                bool is_selected = sceneIndex == n;
                if (ImGui::Selectable(sceneNames[n].c_str(), is_selected))
                    sceneIndex = n;
            }
            ImGui::EndCombo();
        }
        this->pSceneManager->setSceneIndex(sceneIndex);

        const auto squareButtonSize = ImVec2{ImGui::GetFrameHeight(), ImGui::GetFrameHeight()};

        {
            ImGui::SameLine();

            if (ImGui::Button("+", squareButtonSize))
            {
                this->fileDialog.Open();
            }

            if (this->fileDialog.HasSelected())
            {
                try
                {
                    auto path = this->fileDialog.GetSelected().string();
                    this->pSceneManager->pushScene(path);
                }
                catch(std::runtime_error& e)
                {
                    std::cerr << e.what() << "\n";
                }
                this->fileDialog.ClearSelected();
            }
        }

        if (this->pSceneManager->getScenesCount() > 1)
        {
            ImGui::SameLine();
            if (ImGui::Button("-", squareButtonSize))
            {
                this->pSceneManager->popScene();
            }
        }
    }

    UI::InteractiveLighting& UI::getLighing()
    {
        return this->lighting;
    }

    void UI::tweakLighting()
    {
        ImGui::Text("Tweak Lighting");

        ImGui::Text("Light position");
        ImGui::SliderFloat("x:", &this->lighting.lightPosition.x, -20.0f, 20.0f);
        ImGui::SliderFloat("y:", &this->lighting.lightPosition.y, -20.0f, 20.0f);
        ImGui::SliderFloat("z:", &this->lighting.lightPosition.z, -20.0f, 20.0f);

        ImGui::SliderFloat("Shadow Bias", &this->lighting.shadowBias, 0.0f, 0.01f);
        ImGui::SliderFloat("Ambience", &this->lighting.ambientLight, 0.0f, 0.2f);
        ImGui::SliderFloat("Diffuse", &this->lighting.Cdiffuse, 0.0f, 1.0f);
        ImGui::SliderFloat("Specular", &this->lighting.Cspecular, 0.0f, 1.0f);
        ImGui::SliderFloat("Glossyness", &this->lighting.Cglossyness, 2.0f, 200.0f);
    }

    void UI::update()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Vulkan Light Bakery");
        {
            ImGui::BeginTabBar("a");
            if (ImGui::BeginTabItem("Scene"))
            {
                sceneManager();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Camera"))
            {
                camera();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Lighting"))
            {
                tweakLighting();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        fileDialog.Display();
        ImGui::EndFrame();
    }

    void UI::draw(uint32_t imageIndex, vk::CommandBuffer& commandBuffer)
    {
        auto& imguiFrameBuffer = this->imguiFrameBuffers[imageIndex];
        std::array<vk::ClearValue, 2> clearValues;
        clearValues[0].color        = vk::ClearColorValue( std::array<float, 4>( { { 0.2f, 0.2f, 0.2f, 0.2f } } ) );
        clearValues[1].depthStencil = vk::ClearDepthStencilValue( 1.0f, 0 );

        auto[width, height, depth] = this->surfaceExtent;
        vk::RenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo
            .setRenderPass(this->imguiPass.get())
            .setFramebuffer(imguiFrameBuffer.get())
            .setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(width, height)))
            .setClearValues(clearValues);

        ImGui::Render();
        commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
        commandBuffer.endRenderPass();
    }

    void UI::deserialize()
    {
        std::ifstream file("vklb.json");

        if (file.is_open())
        {
            nlohmann::json json{};
            file >> json;

            this->pSceneManager->setSceneIndex(json["selectedSceneIndex"].get<int>());
            this->fileDialog.SetPwd(json["assetsBrowsingDir"].get<std::string>());

            for (const auto& jsonScene : json["scenes"])
            {
                Scene_t::CreateInfo ci{};
                ci.name = jsonScene["name"].get<std::string>();
                ci.path = jsonScene["path"].get<std::string>();
                ci.cameraIndex = jsonScene["cameraIndex"].get<int>();

                for (const auto& jsonCamera : jsonScene["cameras"])
                {
                    Scene_t::CreateInfo::CameraInfo cameraCI{};
                    cameraCI.movementSpeed = jsonCamera["movementSpeed"].get<float>();
                    cameraCI.rotationSpeed = jsonCamera["mouse sensitivity"].get<float>();
                    cameraCI.position      = jsonCamera["position"].get<glm::vec3>();
                    cameraCI.yaw           = jsonCamera["yaw"].get<float>();
                    cameraCI.pitch         = jsonCamera["pitch"].get<float>();

                    ci.cameras.push_back(cameraCI);
                }

                try
                {
                    this->pSceneManager->pushScene(ci);
                }
                catch(std::exception& error)
                {
                    std::cerr << error.what() << "\n";
                }
            }
        }
        else
        {
            std::string fileName{VLB_DEFAULT_SCENE_NAME};
            this->pSceneManager->pushScene(fileName);
            this->pSceneManager->setSceneIndex(0);
        }
    }

    void UI::serialize()
    {
        std::ofstream file("vklb.json");

        nlohmann::json json{};
        json["selectedSceneIndex"] = this->pSceneManager->getSceneIndex();
        json["assetsBrowsingDir"] = this->fileDialog.GetPwd();

        json["scenes"] = nlohmann::json::array();
        for (int i{}; i < this->pSceneManager->getScenesCount(); ++i)
        {
            auto& scene = this->pSceneManager->getScene(i);
            auto jsonScene = nlohmann::json::object();

            jsonScene["path"] = scene->path;
            jsonScene["name"] = scene->name;
            jsonScene["cameras"] = nlohmann::json::array();
            jsonScene["cameraIndex"] = scene->getCameraIndex();

            for (int j{}; j < scene->getCamerasCount(); ++j)
            {
                auto camera = scene->getCamera(j);
                auto jsonCamera = nlohmann::json::object();
                jsonCamera["movementSpeed"]     = camera->getMovementSpeed();
                jsonCamera["mouse sensitivity"] = camera->getRotationSpeed();
                jsonCamera["position"]          = camera->getPosition();
                jsonCamera["yaw"]               = camera->getYaw();
                jsonCamera["pitch"]             = camera->getPitch();
                jsonScene["cameras"].push_back(jsonCamera);
            }

            json["scenes"].push_back(jsonScene);
        }
        file << std::setw(4) << json << std::endl;
    }

    void UI::cleanup()
    {
        serialize();
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

