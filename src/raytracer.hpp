// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef RAYTRACER_HPP
#define RAYTRACER_HPP

#include "renderer.hpp"

namespace vlb {

    class Raytracer : public Renderer
    {
        private:
            void createBLAS();
            void createTLAS();
            void createStorageImage();
            void createRayTracingPipeline();
            void createShaderBindingTable();
            void createDescriptorSets();
            void updateResultImageDescriptorSets();
            void updateCameraDescriptorSets();
            void updateSceneDescriptorSets();
            void recordDrawCommandBuffer(uint64_t imageIndex);
            void draw();
            void handleSceneChange();

            struct AccelerationStructure
            {
                vk::UniqueAccelerationStructureKHR handle;
                Application::Buffer buffer;
            };

            struct ShaderBindingTable
            {
                std::unordered_map<std::string, vk::StridedDeviceAddressRegionKHR> entries;
                std::vector<Buffer> storage;
            };

            AccelerationStructure blas;
            AccelerationStructure tlas;
            Image rayGenStorage;
            ShaderBindingTable sbt;

            struct PushConstant
            {
                float lightIntensity;
            } pc;

            //TODO: impl. better descriptor sets managment (maybe auto-generated)
            struct DSLayout
            {
                vk::UniqueDescriptorSetLayout scene;
                vk::UniqueDescriptorSetLayout resultImage;
            };

            struct DS
            {
                vk::UniqueDescriptorSet scene;
                vk::UniqueDescriptorSet resultImage;
            };

            vk::UniqueDescriptorPool descriptorPool;
            DS descriptorSet;
            DSLayout descriptorSetLayout;

            vk::UniquePipeline            pipeline;
            vk::UniquePipelineLayout      pipelineLayout;
            uint32_t                      shaderGroupsCount;

            std::vector<vk::UniqueCommandBuffer> drawCommandBuffers{};

        public:
            Raytracer();

    };

}

#endif // RAYTRACER_HPP

