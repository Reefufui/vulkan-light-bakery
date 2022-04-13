// created in 2022 by Andrey Treefonov https://github.com/Reefufui

#ifndef ENV_MAP_GENERATOR
#define ENV_MAP_GENERATOR

#include "scene_manager.hpp"

namespace vlb {

    class EnvMapGenerator
    {
        public:
            struct VulkanResources
            {
                vk::PhysicalDevice physicalDevice;
                vk::Device device;
                vk::Queue transferQueue;
                vk::CommandPool transferCommandPool;
                vk::Queue graphicsQueue;
                vk::CommandPool graphicsCommandPool;
            };

        private:
            vk::PhysicalDevice physicalDevice;
            vk::Device         device;
            struct
            {
                vk::Queue transfer;
                vk::Queue graphics;
            } queue;
            struct
            {
                vk::CommandPool transfer;
                vk::CommandPool graphics;
            } commandPool;

            Scene               scene;
            Application::Image  envMap;
            vk::Format          envMapFormat;
            vk::Extent3D        envMapExtent;

            vk::UniqueDescriptorPool      descriptorPool;
            vk::UniqueDescriptorSet       sceneDS;
            vk::UniqueDescriptorSetLayout imageLayout;
            vk::UniqueDescriptorSet       imageDS;
            vk::UniquePipeline            pipeline;
            vk::UniquePipelineLayout      pipelineLayout;

            Application::ShaderBindingTable sbt;

            void createRayTracingPipeline();
            void updateImageDescriptorSet();

        public:
            EnvMapGenerator();
            ~EnvMapGenerator();

            void                setScene(Scene&& scene);
            void                passVulkanResources(VulkanResources& info);
            void                setupVukanRaytracing();
            void                getMap(glm::vec3 position);
            void                loadDebugMapFromPNG(const char* filename, unsigned char** texels);
            Application::Image& createImage(vk::Format imageFormat, vk::Extent3D imageExtent);
            Application::Image& createImage(vk::Format imageFormat, const char* filename);
            Application::Image& getImage();
            vk::Extent3D        getImageExtent();
            void                saveImage(const std::string& imageName);
    };
}

#endif // ENV_MAP_GENERATOR

