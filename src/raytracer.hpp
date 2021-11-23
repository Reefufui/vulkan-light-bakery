// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef RAYTRACER_HPP
#define RAYTRACER_HPP

#include "renderer.hpp"
#include "acceleration_structure.hpp"

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
            void buildCommandBuffers();
            void draw();

        public:
            Raytracer();
            ~Raytracer();

            AccelerationStructure as{&m_device};
    };

}

#endif // RAYTRACER_HPP

