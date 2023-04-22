// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "bvh_observer.hpp"
#include "structures.h"
#include <vulkan/vulkan_structs.hpp>

namespace vlb {

    void BVHObserver::createGraphicsPipeline()
    {
        std::vector<vk::UniqueShaderModule> shaderModules{};
        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages{};

        shaderModules.push_back(Application::createShaderModule("shaders/bvh_observer.vert.spv"));
        shaderStages.push_back(vk::PipelineShaderStageCreateInfo{}
            .setStage(vk::ShaderStageFlagBits::eVertex)
            .setModule(shaderModules.back().get())
            .setPName("main"));

        shaderModules.push_back(Application::createShaderModule("shaders/bvh_observer.frag.spv"));
        shaderStages.push_back(vk::PipelineShaderStageCreateInfo{}
            .setStage(vk::ShaderStageFlagBits::eFragment)
            .setModule(shaderModules.back().get())
            .setPName("main"));

        
        std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        vk::PipelineDynamicStateCreateInfo dynamicState({}, dynamicStates);

        vk::VertexInputBindingDescription bindingDescription{};
        bindingDescription
            .setBinding(0)
            .setStride(sizeof(shader::Vertex))
            .setInputRate(vk::VertexInputRate::eVertex);

        std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions = {};

        // position attribute
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32A32Sfloat;
        attributeDescriptions[0].offset = offsetof(shader::Vertex, position);

        // normal attribute
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(shader::Vertex, normal);

        // uv0 attribute
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[2].offset = offsetof(shader::Vertex, uv0);

        // uv1 attribute
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[3].offset = offsetof(shader::Vertex, uv1);

        vk::PipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput
            .setVertexBindingDescriptionCount(1)
            .setPVertexBindingDescriptions(&bindingDescription)
            .setVertexAttributeDescriptionCount(static_cast<uint32_t>(attributeDescriptions.size()))
            .setPVertexAttributeDescriptions(attributeDescriptions.data());

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

        auto[width, height, depth] = this->surfaceExtent;
        vk::Extent3D extent(width, height, depth);
        vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
        vk::Rect2D scissor({0, 0}, {width, height});
        vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);
        vk::PipelineRasterizationStateCreateInfo rasterizerInfo(
                /* flags */ {}, 
                /* depthClampEnable */ false, 
                /* rasterizerDiscardEnable */ false, 
                /* polygonMode */ vk::PolygonMode::eLine, // eFill
                /* cullMode */ vk::CullModeFlagBits::eNone, // eBack
                /* frontFace */ vk::FrontFace::eCounterClockwise, 
                /* depthBiasEnable */ false, 
                /* depthBiasConstantFactor */ 0.0f, 
                /* depthBiasClamp */ 0.0f, 
                /* depthBiasSlopeFactor */ 0.0f, 
                /* lineWidth */ 1.0f // TODO: enable wide lines feature
                );

        vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.setColorWriteMask(
                vk::ColorComponentFlagBits::eR
                | vk::ColorComponentFlagBits::eG
                | vk::ColorComponentFlagBits::eB
                | vk::ColorComponentFlagBits::eA);
        colorBlendAttachment.setBlendEnable(false);

        vk::PipelineColorBlendStateCreateInfo colorBlending;
        colorBlending.setAttachmentCount(1)
            .setPAttachments(&colorBlendAttachment)
            .setLogicOpEnable(VK_FALSE)
            .setLogicOp(vk::LogicOp::eCopy)
            .setBlendConstants({0.0f, 0.0f, 0.0f, 0.0f});

        std::vector<vk::DescriptorSetLayout> layouts;
        layouts.push_back(this->sceneManager.getCamera()->getDescriptorSetLayout());
        layouts.push_back(this->skyboxManager.getDescriptorSetLayout());

        this->pipelineLayout = this->device.get().createPipelineLayoutUnique(
                vk::PipelineLayoutCreateInfo{}
                .setSetLayouts(layouts)
                );

        this->depthBuffer = Renderer::createDepthImage();

        vk::PipelineDepthStencilStateCreateInfo depthStencil {
            {},                                 // flags
                true,                           // depth test enable
                true,                           // depth write enable
                vk::CompareOp::eLess,           // depth compare op
                false,                          // depth bounds test enable
                false,                          // stencil test enable
                vk::StencilOpState{},           // front stencil operation state
                vk::StencilOpState{},           // back stencil operation state
                0.0f,                           // minimum depth bounds
                1.0f                            // maximum depth bounds
        };

        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo
            .setStages(shaderStages)
            .setPVertexInputState(&vertexInput)
            .setPInputAssemblyState(&inputAssembly)
            .setPViewportState(&viewportState)
            .setPRasterizationState(&rasterizerInfo)
            .setPDepthStencilState(&depthStencil)
            .setPDynamicState(&dynamicState)
            .setPColorBlendState(&colorBlending)
            .setLayout(this->pipelineLayout.get())
            .setRenderPass(this->renderPass.get())
            .setSubpass(0);

        auto[result, p] = this->device.get().createGraphicsPipelineUnique(nullptr, pipelineInfo);

        if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create ray tracing pipeline");
        }
        else
        {
            this->pipeline = std::move(p);
        }

        std::cout << "[createGraphicsPipeline]: ok\n";
    }

    void BVHObserver::createRenderPass()
    {
        std::array<vk::AttachmentDescription, 2> attachmentDescriptions;
        attachmentDescriptions[0] = vk::AttachmentDescription( vk::AttachmentDescriptionFlags(),
                this->surfaceFormat,
                vk::SampleCountFlagBits::e1,
                vk::AttachmentLoadOp::eClear,
                vk::AttachmentStoreOp::eStore,
                vk::AttachmentLoadOp::eDontCare,
                vk::AttachmentStoreOp::eDontCare,
                vk::ImageLayout::eUndefined,
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

        std::array<vk::SubpassDescription, 1> subpasses;
        subpasses[0].setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
        subpasses[0].setColorAttachmentCount(1);
        subpasses[0].setPColorAttachments(&colorReference);
        subpasses[0].setPDepthStencilAttachment(&depthReference);
        // TODO: dependency

        this->renderPass = this->device.get().createRenderPassUnique(
                vk::RenderPassCreateInfo( vk::RenderPassCreateFlags(), attachmentDescriptions, subpasses ) );

        std::cout << "[createRenderPass]: ok\n";
    }

    void BVHObserver::createFrameBuffer()
    {
        std::array<vk::ImageView, 2> attachments;
        attachments[1] = this->depthBuffer.imageView.get();

        vk::FramebufferCreateInfo framebufferCreateInfo{};
        framebufferCreateInfo
            .setRenderPass(this->renderPass.get())
            .setAttachments(attachments)
            .setWidth(this->surfaceExtent.width)
            .setHeight(this->surfaceExtent.height)
            .setLayers(this->surfaceExtent.depth);

        this->frameBuffers.reserve(this->swapchainImageViews.size());
        for (auto const & imageView : this->swapchainImageViews)
        {
            attachments[0] = imageView.get();
            this->frameBuffers.push_back(this->device.get().createFramebufferUnique(framebufferCreateInfo));
        }

        std::cout << "[createFrameBuffer]: ok\n";
    }

    void BVHObserver::createDescriptorSets()
    {
        /*
        // TODO: move scene related to SceneManager
        std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eAccelerationStructureKHR, 1},
        {vk::DescriptorType::eStorageImage, 1},
        {vk::DescriptorType::eUniformBuffer, 1},
        {vk::DescriptorType::eStorageBuffer, 2},
        {vk::DescriptorType::eCombinedImageSampler, 1000}
        };

        this->descriptorPool = this->device.get().createDescriptorPoolUnique(
        vk::DescriptorPoolCreateInfo{}
        .setPoolSizes(poolSizes)
        .setMaxSets(4)
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
        | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
        );

        auto& scene = this->sceneManager.getScene();
        auto sceneLayout = scene->getDescriptorSetLayout();

        this->descriptorSet.scene = std::move(this->device.get().allocateDescriptorSetsUnique(
        vk::DescriptorSetAllocateInfo{}
        .setDescriptorPool(this->descriptorPool.get())
        .setSetLayouts(sceneLayout)
        ).front());

        scene->updateSceneDescriptorSets(this->descriptorSet.scene.get());
        */
    }

    void BVHObserver::recordDrawCommandBuffer(uint64_t imageIndex)
    {
        auto& commandBuffer    = this->drawCommandBuffers[imageIndex];
        auto& swapChainImage   = this->swapChainImages[imageIndex];

        commandBuffer->begin(vk::CommandBufferBeginInfo{}
                .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

        std::array<vk::ClearValue, 2> clearValues = {};
        clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{0.5f, 0.5f, 0.5f, 1.0f});
        clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

        auto[width, height, depth] = this->surfaceExtent;

        vk::RenderPassBeginInfo renderPassInfo;
        renderPassInfo
            .setRenderPass(this->renderPass.get())
            .setFramebuffer(this->frameBuffers[imageIndex].get())
            .setRenderArea({{0, 0}, {width, height}})
            .setClearValues(clearValues);

        commandBuffer->beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);
        {
            commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, this->pipeline.get());

            vk::Viewport viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
            commandBuffer->setViewport(0, 1, &viewport);

            vk::Rect2D scissor({0, 0}, {width, height});
            commandBuffer->setScissor(0, 1, &scissor);

            std::vector<vk::DescriptorSet> descriptorSets(0);
            descriptorSets.push_back(this->sceneManager.getCamera()->update()->getDescriptorSet(imageIndex));
            descriptorSets.push_back(this->skyboxManager.getDescriptorSet());

            commandBuffer->bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    this->pipelineLayout.get(),
                    0,
                    descriptorSets,
                    nullptr
                    );

            const auto& scene = this->sceneManager.getScene();
            const auto& vertexBuffer = scene->getVertexBuffer();
            const auto& indexBuffer = scene->getIndexBuffer();
            vk::DeviceSize offset = 0;
            commandBuffer->bindVertexBuffers(0, 1, &vertexBuffer, &offset);
            commandBuffer->bindIndexBuffer(indexBuffer, offset, vk::IndexType::eUint32);

            uint32_t indicesCount = static_cast<uint32_t>(scene->getTotalIndicesCount());
            commandBuffer->drawIndexed(indicesCount, 1, 0, 0, 0);
        }
        commandBuffer->endRenderPass();

        this->ui.draw(imageIndex, commandBuffer.get());

        commandBuffer->end();
    }

    void BVHObserver::draw()
    {
        auto timeout = std::numeric_limits<uint64_t>::max();
        this->device.get().waitForFences(this->inFlightFences[this->currentFrame], true, timeout);
        this->device.get().resetFences(this->inFlightFences[this->currentFrame]);

        auto [result, imageIndex] = this->device.get().acquireNextImageKHR(
                this->swapchain.get(),
                timeout,
                imageAvailableSemaphores[this->currentFrame].get() );

        if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to acquire next image!");
        }

        if (this->imagesInFlight[imageIndex] != vk::Fence{})
        {
            this->device.get().waitForFences(this->imagesInFlight[imageIndex], true, timeout);
        }
        this->imagesInFlight[imageIndex] = this->inFlightFences[this->currentFrame];

        recordDrawCommandBuffer(imageIndex);

        vk::PipelineStageFlags waitStage{vk::PipelineStageFlagBits::eRayTracingShaderKHR};
        vk::SubmitInfo submitInfo{};
        submitInfo
            .setWaitSemaphores(this->imageAvailableSemaphores[this->currentFrame].get())
            .setWaitDstStageMask(waitStage)
            .setCommandBuffers(this->drawCommandBuffers[imageIndex].get())
            .setSignalSemaphores(this->renderFinishedSemaphores[this->currentFrame].get());

        this->queue.graphics.submit(submitInfo, this->inFlightFences[this->currentFrame]);

        Renderer::present(imageIndex);
    }

    BVHObserver::BVHObserver()
    {
        createRenderPass();
        createGraphicsPipeline();
        createFrameBuffer();

        this->drawCommandBuffers = Renderer::createDrawCommandBuffers();
    }
}

