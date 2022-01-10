// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef SCENE_MANAGER_HPP
#define SCENE_MANAGER_HPP

#define VLB_DEFAULT_SCENE_NAME "default_blender_cube.gltf"

#include <tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "application.hpp"
#include "camera.hpp"

namespace vlb {

    struct Scene_t;
    typedef std::shared_ptr<Scene_t> Scene;
    class Scene_t : public std::enable_shared_from_this<Scene_t>
    {
        private:

            struct Sampler
            {
                vk::Filter magFilter = vk::Filter::eLinear;
                vk::Filter minFilter = vk::Filter::eLinear;
                vk::SamplerAddressMode addressModeU = vk::SamplerAddressMode::eRepeat;
                vk::SamplerAddressMode addressModeV = vk::SamplerAddressMode::eRepeat;
                vk::SamplerAddressMode addressModeW = vk::SamplerAddressMode::eRepeat;
            };

            struct Texture_t;
            typedef std::shared_ptr<Texture_t> Texture;
            struct Texture_t
            {
                Application::Image      image;
                uint32_t                mipLevels;
                vk::DescriptorImageInfo descriptor;
                vk::UniqueSampler       sampler;
            };

            struct AccelerationStructure_t;
            typedef std::shared_ptr<AccelerationStructure_t> AccelerationStructure;
            struct AccelerationStructure_t
            {
                vk::UniqueAccelerationStructureKHR handle;
                Application::Buffer                buffer;
                vk::DeviceAddress                  address;
            };

            struct Primitive_t;
            typedef std::shared_ptr<Primitive_t> Primitive;
            struct Primitive_t
            {
                AccelerationStructure blas;
                uint64_t              materialIndex;
                uint32_t              indexCount;
                uint32_t              vertexCount;
                Application::Buffer   vertexBuffer;
                Application::Buffer   indexBuffer;

                auto getGeometry();
            };

            struct Mesh_t;
            typedef std::shared_ptr<Mesh_t> Mesh;
            struct Mesh_t
            {
                std::vector<Primitive> primitives;
            };

            struct Node_t;
            typedef std::shared_ptr<Node_t> Node;
            struct Node_t {
                Node parent;
                uint32_t index;
                std::vector<Node> children;
                Mesh mesh;
                glm::mat4 matrix;
                VkTransformMatrixKHR getMatrix();
            };

        public:

            struct VulkanResources
            {
                vk::PhysicalDevice physicalDevice;
                vk::Device device;
                vk::Queue transferQueue;
                vk::CommandPool transferCommandPool;
                vk::Queue graphicsQueue;
                vk::CommandPool graphicsCommandPool;
                vk::Queue computeQueue;
                vk::CommandPool computeCommandPool;
                uint32_t swapchainImagesCount;
            };

            struct CreateInfo
            {
                struct CameraInfo
                {
                    float rotationSpeed;
                    float movementSpeed;
                    glm::vec3 position;
                    float yaw;
                    float pitch;
                };

                std::vector<CameraInfo> cameras;
                int cameraIndex;
                std::string name;
                std::string path;
            };

            Scene_t() = delete;
            Scene_t(std::string& filename);

            Scene passVulkanResources(VulkanResources& info);

            // Labels TODO: MAKE PRIVATE
            std::string name;
            std::string path;

            // load*(*); functions must be called in the same order as declared below
            Scene loadSamplers();
            Scene loadTextures();
            Scene loadMaterials();
            Scene loadNodes();
            Scene buildAccelerationStructures();
            Scene createDescriptorSetLayout();
            Scene loadCameras();

            // Camera management
            Scene setCameraIndex(int cameraIndex);
            Scene setViewingFrustumForCameras(ViewingFrustum frustum);
            Scene pushCamera(Camera camera);
            Camera       getCamera(int index = -1);
            const int    getCameraIndex();
            const size_t getCamerasCount();
            vk::DescriptorSetLayout getDescriptorSetLayout();

            // TODO MAKE PRIVATE
            AccelerationStructure tlas;
            Application::Buffer   instanceInfoBuffer;
            Application::Buffer   materialBuffer;
            size_t materialsCount;
            std::vector<Texture>  textures;

        private:

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
            uint32_t       swapchainImagesCount;

            tinygltf::Model    model;
            tinygltf::TinyGLTF loader;

            vk::UniqueDescriptorSetLayout descriptorSetLayout;

            std::vector<Sampler>  samplers;
            std::vector<Node>     nodes;
            std::vector<Node>     linearNodes;
            std::vector<Camera>   cameras;

            int cameraIndex;

            void loadNode(const Node parent, const tinygltf::Node& node, const uint32_t nodeIndex);
            glm::mat4 loadMatrix(const tinygltf::Node& gltfNode);
            auto fetchVertices(const tinygltf::Primitive& primitive);
            auto fetchIndices(const tinygltf::Primitive& primitive);
            auto loadVertexAttribute(const tinygltf::Primitive& primitive, std::string&& label);
            template <class T> Application::Buffer toBuffer(T data, size_t size = -1);
            AccelerationStructure buildAS(const vk::AccelerationStructureGeometryKHR& geometry, const vk::AccelerationStructureBuildRangeInfoKHR& range);
    };

    class SceneManager
    {
        private:

            ViewingFrustum frustum;

            bool sceneShouldBeFreed;
            bool sceneChangedFlag;
            int  sceneIndex;
            std::vector<Scene      > scenes;
            std::vector<std::string> sceneNames;

            Scene_t::VulkanResources initInfo;

        public:

            void passVulkanResources(Scene_t::VulkanResources& info);

            Scene&                    getScene(int index = -1);
            const int                 getSceneIndex();
            const size_t              getScenesCount();
            std::vector<std::string>& getSceneNames();
            Camera                    getCamera();
            ViewingFrustum            getViewingFrustum();

            const bool                sceneChanged();

            SceneManager& setSceneIndex(int sceneIndex);
            SceneManager& setViewingFrustum(ViewingFrustum frustum);

            void pushScene(std::string& fileName);  // hot push on run-time
            void pushScene(Scene_t::CreateInfo ci); // load scene using deserialized ci
            void popScene();

            ~SceneManager();
    };
}

#endif // ifndef SCENE_MANAGER_HPP

