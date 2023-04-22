// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef BVH_OBSERVER_HPP
#define BVH_OBSERVER_HPP

#include "renderer.hpp"

namespace vlb {

    class BVHObserver : public Renderer
    {
        private:
            void createDescriptorSets();
            void createGraphicsPipeline();
            void createRenderPass();
            void createFrameBuffer();
            void recordDrawCommandBuffer(uint64_t imageIndex);
            void draw();

            vk::UniqueDescriptorPool descriptorPool;

            struct DSLayout
            {
                vk::UniqueDescriptorSetLayout resultImage;
            };
            struct DS
            {
                vk::UniqueDescriptorSet scene;
            };

            DS descriptorSet;
            DSLayout descriptorSetLayout;

            vk::UniqueRenderPass     renderPass;
            vk::UniquePipeline       pipeline;
            vk::UniquePipelineLayout pipelineLayout;

            Application::Image depthBuffer;

            std::vector<vk::UniqueCommandBuffer> drawCommandBuffers{};
            std::vector<vk::UniqueFramebuffer> frameBuffers;

        public:
            BVHObserver();
    };

}

#endif // BVH_OBSERVER_HPP

