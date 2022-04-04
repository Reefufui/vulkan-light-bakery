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

        public:
            EnvMapGenerator();
            ~EnvMapGenerator();

            void                setScene(Scene&& scene);
            void                passVulkanResources(VulkanResources& info);
            int                 getMap(glm::vec3 position);
            void                loadDebugMapFromPNG(const char* filename, unsigned char** texels);
            Application::Image& createImage(vk::Format imageFormat, vk::Extent3D imageExtent);
            Application::Image& createImage(vk::Format imageFormat, const char* filename);
            Application::Image& getImage();
            vk::Extent3D        getImageExtent();
            void                saveImage();
    };
}

#endif // ENV_MAP_GENERATOR

