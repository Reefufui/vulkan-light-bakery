// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "ui.hpp"

#include <iostream>
#include <filesystem>

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

        this->fileDialog = ImGui::FileBrowser{};
        this->fileDialog.SetTitle("Choose model file");
        this->fileDialog.SetTypeFilters({ ".gltf", ".glb" });
        std::filesystem::path modelsPath{"assets/models"};
        if (std::filesystem::exists(modelsPath))
        {
            this->fileDialog.SetPwd(modelsPath);
        }
    }

    ImGui::FileBrowser& UI::getFileDialog()
    {
        return this->fileDialog;
    }

    std::vector<std::string>& UI::getSceneNames()
    {
        return this->sceneNames;
    }

    int& UI::getSelectedSceneIndex()
    {
        return this->selectedSceneIndex;
    }

    bool& UI::getFreeSceneFlag()
    {
        return this->freeSceneFlag;
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
            ImGui::SameLine();
            if (ImGui::Button("-", {26, 26}))
            {
                this->freeSceneFlag = true;
            }
        }
        ImGui::End();

        fileDialog.Display();
        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
        commandBuffer.endRenderPass();
    }

    void UI::cleanup()
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

