// created in 2022 by Andrey Treefonov https://github.com/Reefufui

#ifndef LIGHT_BAKER_HPP
#define LIGHT_BAKER_HPP

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

            float                  probeSpacingDistance;
            std::vector<glm::vec3> probePositions;
            Application::Buffer    SHCoeffs;

            vk::UniqueDescriptorPool      descriptorPool;
            vk::UniqueDescriptorSet       descriptorSet;
            vk::UniqueDescriptorSetLayout descriptorSetLayout;
            vk::UniquePipeline            pipeline;
            vk::UniquePipelineCache       pipelineCache;
            vk::UniquePipelineLayout      pipelineLayout;

        public:

            LightBaker(std::string& sceneName);
            ~LightBaker();

            std::vector<glm::vec3> probePositionsFromBoudingBox(std::array<glm::vec3, 8> boundingBox);
            void createBakingPipeline();
            void modifyPipelineForDebug();
            void dispatchBakingKernel();
            void bake();
    };
}

#endif // LIGHT_BAKER_HPP

