// created in 2022 by Andrey Treefonov https://github.com/Reefufui

#ifndef SKYBOX_MANAGER_HPP
#define SKYBOX_MANAGER_HPP

#define VLB_DEFAULT_SKYBOX_NAME "default_skybox.jpg"
#define WORKGROUP_SIZE 16

#include "application.hpp"

namespace vlb {

    struct Skybox_t;
    typedef std::shared_ptr<Skybox_t> Skybox;
    class Skybox_t : public std::enable_shared_from_this<Skybox_t>
    {
        public:
            enum class Type
            {
                eCubemap,
                ePanorama,
                eTypeCount
            };

        private:

            std::string name;
            std::string path;
            int height;
            int width;
            int texChannels;
            Type        type; // NOTE: not implemented yet (only panorama type)

            unsigned char* texels;
            Application::Texture texture;
            Application::Buffer SHCoeffs;
            vk::UniqueDescriptorSetLayout descriptorSetLayout;

            // Vulkan resourses
            vk::PhysicalDevice physicalDevice;
            vk::Device         device;
            struct
            {
                vk::Queue transfer;
                vk::Queue compute;
                vk::Queue graphics;
            } queue;
            struct
            {
                vk::CommandPool transfer;
                vk::CommandPool compute;
                vk::CommandPool graphics;
            } commandPool;

        public:

            struct VulkanContext
            {
                vk::PhysicalDevice physicalDevice;
                vk::Device device;
                vk::Queue transferQueue;
                vk::CommandPool transferCommandPool;
                vk::Queue graphicsQueue;
                vk::CommandPool graphicsCommandPool;
                vk::Queue computeQueue;
                vk::CommandPool computeCommandPool;
            };

            struct CreateInfo
            {
                std::string name;
                std::string path;
                Type        type;
            };

            Skybox_t() = delete;
            Skybox_t(std::string& filename);
            ~Skybox_t();

            Skybox passVulkanContext(VulkanContext& context);
            Skybox createTexture();
            Skybox createSHBuffer();
            Skybox updateDescriptorSets(vk::DescriptorSet targetDS);
            Skybox computeSH(vk::Pipeline computePipeline, vk::PipelineLayout layout, vk::DescriptorSet ds);

            std::string getName();
    };

    class SkyboxManager
    {
        private:

            bool skyboxShouldBeFreed;
            bool skyboxChangedFlag;
            int  skyboxIndex;
            std::vector<Skybox     > skyboxes;
            std::vector<std::string> skyboxNames;

            Skybox_t::VulkanContext context;

            // Computing SH
            vk::UniqueDescriptorPool      descriptorPool;
            vk::UniqueDescriptorSet       descriptorSet;
            vk::UniqueDescriptorSetLayout descriptorSetLayout;
            vk::UniquePipeline            pipeline;
            vk::UniquePipelineCache       pipelineCache;
            vk::UniquePipelineLayout      pipelineLayout;

            void createSHComputePipeline();
            void createDescriptorSetLayout();
            void createDescriptorSets();

        public:

            void passVulkanContext(Skybox_t::VulkanContext& context);

            Skybox&                   getSkybox(int index = -1);
            const int                 getSkyboxIndex();
            const size_t              getSkyboxCount();
            std::vector<std::string>& getSkyboxNames();

            vk::DescriptorSetLayout getDescriptorSetLayout();
            vk::DescriptorSet getDescriptorSet();
            const bool                skyboxChanged();

            SkyboxManager& setSkyboxIndex(int skyboxIndex);

            void pushSkybox(std::string& fileName);  // hot push on run-time
            void pushSkybox(Skybox_t::CreateInfo ci); // load skybox using deserialized ci
            void popSkybox();

            SkyboxManager() = default;
            ~SkyboxManager();
    };

}

#endif // ifndef SKYBOX_MANAGER_HPP

