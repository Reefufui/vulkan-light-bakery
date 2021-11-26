// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef RAYTRACER_HPP
#define RAYTRACER_HPP

#include "renderer.hpp"
//TODO: #include "acceleration_structure.hpp" - maybe ??
//TODO: #include "scene.hpp"

#include <unordered_map>

namespace vlb {

    //TODO: class implemetation in sepparate file
    struct AccelerationStructure
    {
        vk::AccelerationStructureKHR handle;
        uint64_t deviceAddress = 0;
        vk::DeviceMemory memory;
        vk::Buffer buffer;
    };

    //TODO: should be part of acceleration_structure.hpp's class
    struct RayTracingScratchBuffer
    {
        vk::Buffer handle;
        vk::DeviceMemory memory;
        uint64_t deviceAddress = 0;
    };

    //TODO: should be part of scene.hpp's class
    struct Scene
    {
        vk::DeviceMemory vBufferMemory;
        vk::DeviceMemory iBufferMemory;
        vk::DeviceMemory tBufferMemory;
        vk::Buffer vBuffer;
        vk::Buffer iBuffer;
        vk::Buffer tBuffer;
    };

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

            struct ShaderBindingTable
            {
                std::unordered_map<std::string, vk::StridedDeviceAddressRegionKHR> entries;
                std::vector<Buffer> storage;
            };

            AccelerationStructure m_blas;
            AccelerationStructure m_tlas;
            Scene m_scene;
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
            ~Raytracer();

    };

}

#endif // RAYTRACER_HPP

