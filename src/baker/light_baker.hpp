// created in 2022 by Andrey Treefonov https://github.com/Reefufui

#ifndef LIGHT_BAKER_HPP
#define LIGHT_BAKER_HPP

#include <memory>

#include "scene_manager.hpp"
#include "application.hpp"
#include "env_map_generator.hpp"

#define WORKGROUP_SIZE 16

namespace vlb {

    class LightBaker : public Application
    {
        private:
            struct PushConstant
            {
                uint32_t width;
                uint32_t height;
            } pushConstants;

            EnvMapGenerator envMapGenerator;

            std::string            gltfFileName;
            bool                   imageInput;
            std::vector<glm::vec3> probePositions;
            glm::vec3              probesCount3D;
            glm::vec3              gridStep;
            Application::Buffer    SHCoeffs;

            std::vector<uint8_t> coeffs;

            vk::UniqueDescriptorPool      descriptorPool;
            vk::UniqueDescriptorSet       descriptorSet;
            vk::UniqueDescriptorSetLayout descriptorSetLayout;
            vk::UniquePipeline            pipeline;
            vk::UniquePipelineCache       pipelineCache;
            vk::UniquePipelineLayout      pipelineLayout;

        public:

            LightBaker(std::string& sceneName);
            ~LightBaker();

            std::vector<glm::vec3> probePositionsFromBoudingBox(std::array<glm::vec3, 2> boundingBox);
            void createBakingPipeline();
            void modifyPipelineForDebug();
            void dispatchBakingKernel();
            void bake();
            void serialize();
    };
}

#endif // LIGHT_BAKER_HPP

