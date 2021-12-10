// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "ui.hpp"

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
        this->fileDialog.SetTypeFilters({ ".gltf", ".glb" });
        std::filesystem::path modelsPath{"assets/models"};
        if (std::filesystem::exists(modelsPath))
        {
            this->fileDialog.SetPwd(modelsPath);
        }

        deserialize();
    }

    ImGui::FileBrowser& UI::getFileDialog()
    {
        return this->fileDialog;
    }

    std::vector<std::string>& UI::getSceneNames()
    {
        return this->sceneNames;
    }

    std::vector<std::string>& UI::getScenePaths()
    {
        return this->scenePaths;
    }

    int& UI::getSelectedSceneIndex()
    {
        return this->selectedSceneIndex;
    }

    bool& UI::getFreeSceneFlag()
    {
        return this->freeSceneFlag;
    }

    float& UI::getLightIntensity()
    {
        return this->lightIntensity;
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

        commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Vulkan Light Bakery");
        {
            ImGui::Text("Choose model to render:");
            ImGui::Combo("", &this->selectedSceneIndex, this->sceneNames, 20);
            ImGui::SameLine();
            if (ImGui::Button("+", {26, 26}))
            {
                fileDialog.Open();
            }
            if (this->sceneNames.size() > 1)
            {
                ImGui::SameLine();
                if (ImGui::Button("-", {26, 26}))
                {
                    this->freeSceneFlag = true;
                }
            }

            ImGui::SliderFloat("Light intensity", &(this->lightIntensity), 0.0f, 1.3f);
        }
        ImGui::End();

        fileDialog.Display();
        ImGui::EndFrame();
        ImGui::Render();
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
            this->lightIntensity = json["lightIntensity"].get<float>();
            this->selectedSceneIndex = json["selectedSceneIndex"].get<int>();
            this->scenePaths = json["scenePaths"].get<std::vector<std::string>>();
        }
    }

    void UI::serialize()
    {
        std::ofstream file("vklb.json");

        nlohmann::json json{};
        json["lightIntensity"] = this->lightIntensity;
        json["selectedSceneIndex"] = this->selectedSceneIndex;
        json["scenePaths"] = this->scenePaths;
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

