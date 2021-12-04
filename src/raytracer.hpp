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
            void recordDrawCommandBuffer(uint64_t imageIndex);
            void draw();

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

            vk::UniquePipeline            pipeline;
            vk::UniquePipelineLayout      pipelineLayout;
            vk::UniqueDescriptorSetLayout descriptorSetLayout;
            uint32_t                      shaderGroupsCount;
            vk::UniqueDescriptorPool      descriptorPool;
            vk::UniqueDescriptorSet       descriptorSet;

            std::vector<vk::UniqueCommandBuffer> drawCommandBuffers{};

        public:
            Raytracer();

    };

}

#endif // RAYTRACER_HPP

