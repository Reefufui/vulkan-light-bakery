// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef RAYTRACER_HPP
#define RAYTRACER_HPP

#include "renderer.hpp"

namespace vlb {

    class Raytracer : public Renderer
    {
        private:
            void createRayTracingPipeline();
            void createShaderBindingTable();
            void createResultImageDSLayout();
            void createDescriptorSets();
            void updateResultImageDescriptorSets();
            void recordDrawCommandBuffer(uint64_t imageIndex);
            void draw();
            void handleSceneChange();

            Application::Image              rayGenStorage;
            Application::ShaderBindingTable sbt;
            uint32_t                        shaderGroupsCount;

            //TODO: impl. better descriptor sets managment (maybe auto-generated)
            struct DSLayout
            {
                vk::UniqueDescriptorSetLayout resultImage;
            };

            struct DS
            {
                vk::UniqueDescriptorSet scene;
                vk::UniqueDescriptorSet resultImage;
            };

            struct Constants
            {
                glm::vec3               gridStep;
                UI::InteractiveLighting lighting;
            } constants;

            vk::UniqueDescriptorPool descriptorPool;
            DS descriptorSet;
            DSLayout descriptorSetLayout;

            vk::UniquePipeline       pipeline;
            vk::UniquePipelineLayout pipelineLayout;

            std::vector<vk::UniqueCommandBuffer> drawCommandBuffers{};

        public:
            Raytracer();

    };

}

#endif // RAYTRACER_HPP

