// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef RAYTRACER_HPP
#define RAYTRACER_HPP

#include "renderer.hpp"

#include <unordered_map>

namespace vlb {

    class Raytracer : public Renderer
    {
        private:
            void createBLAS();
            void createTLAS();
            void createStorageImage();
            void createUniformBuffer();
            void createRayTracingPipeline();
            void createShaderBindingTable();
            void createDescriptorSets();
            void recordDrawCommandBuffers();
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
            Image m_rayGenStorage;
            ShaderBindingTable sbt;

            //NOTE: ok for now (trying out not to use m_... prefix | using self->... instead)
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

