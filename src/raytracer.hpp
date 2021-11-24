// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef RAYTRACER_HPP
#define RAYTRACER_HPP

#include "renderer.hpp"
//TODO: #include "acceleration_structure.hpp"
//TODO: #include "scene.hpp"

namespace vlb {

    //TODO: class implemetation in sepparate file
    struct AccelerationStructure {
        vk::AccelerationStructureKHR handle;
        uint64_t deviceAddress = 0;
        vk::DeviceMemory memory;
        vk::Buffer buffer;
    };

    //TODO: should be part of acceleration_structure.hpp's class
    struct RayTracingScratchBuffer
    {
        uint64_t deviceAddress = 0;
        vk::Buffer handle;
        vk::DeviceMemory memory;
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
            void buildCommandBuffers();
            void draw();

        public:
            Raytracer();
            ~Raytracer();

            AccelerationStructure m_blas;
            AccelerationStructure m_tlas;
            Scene m_scene;
    };

}

#endif // RAYTRACER_HPP

