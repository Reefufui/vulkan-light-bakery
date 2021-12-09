// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef SCENE_MANAGER_HPP
#define SCENE_MANAGER_HPP

#include <tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "application.hpp"
#include "ui.hpp"

namespace vlb {

    struct Scene_t;
    typedef std::shared_ptr<Scene_t> Scene;
    class Scene_t
    {
        public:

            struct Vertex
            {
                glm::vec4 position;
                glm::vec3 normal;
                glm::vec2 uv0;
                glm::vec2 uv1;
            };

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
                Application::Image image;
                uint32_t mipLevels;
                vk::DescriptorImageInfo descriptor;
                vk::UniqueSampler sampler;
            };

            // TODO: redesign?
            struct Material
            {
                struct Alpha
                {
                    enum class Mode
                    {
                        eOpaque,
                        eMask,
                        eBlend,
                        eAlphaModeCount
                    } mode{};

                    float cutOff{1.0f};
                } alpha;

                struct Factors
                {
                    glm::vec4 baseColor = glm::vec4(1.0f);
                    float     metallic{1.0f};
                    float     roughness{1.0f};
                    glm::vec4 emissive = glm::vec4(1.0f);

                    glm::vec4 diffuseEXT  = glm::vec4(1.0f);
                    glm::vec3 specularEXT = glm::vec3(0.0f);
                } factor;

                struct Textures
                {
                    Texture normal;
                    Texture occlusion;

                    Texture baseColor;
                    Texture metallicRoughness;
                    Texture emissive;

                    Texture diffuseEXT;
                    Texture specularEXT;
                } texture;

                struct TexCoordSets
                {
                    uint8_t normal{};
                    uint8_t occlusion{};

                    uint8_t baseColor{};
                    uint8_t metallicRoughness{};
                    uint8_t emissive{};

                    uint8_t diffuseEXT{};
                    uint8_t specularEXT{};
                } coordSet;
            };

            struct Primitive_t;
            typedef std::shared_ptr<Primitive_t> Primitive;
            struct Primitive_t {
                uint32_t indexOffset;
                uint32_t indexCount;
                uint32_t vertexCount;
            };

            struct Mesh_t;
            typedef std::shared_ptr<Mesh_t> Mesh;
            struct Mesh_t {
                std::vector<Primitive> primitives;
                struct UniformBuffer {
                    Application::Buffer buffer;
                    vk::DescriptorBufferInfo descriptor;
                    vk::DescriptorSet descriptorSet;
                    void* mapped;
                } uniformBuffer;
            };

            struct Node_t;
            typedef std::shared_ptr<Node_t> Node;
            struct Node_t {
                Node parent;
                uint32_t index;
                std::vector<Node> children;
                Mesh mesh;
                glm::vec3 translation;
                glm::vec3 scale;
                glm::mat4 rotation;
                glm::mat4 matrix;
            };

            Scene_t() = delete;
            Scene_t(std::string& filename);
            void createBLASBuffers(vk::Device& device, vk::PhysicalDevice& physicalDevice, vk::Queue& transferQueue, vk::CommandPool& copyCommandPool);
            void createObjectDescBuffer(vk::Device& device, vk::PhysicalDevice& physicalDevice, vk::Queue& transferQueue, vk::CommandPool& copyCommandPool);
            void loadTextures(vk::Device& device, vk::PhysicalDevice& physicalDevice, vk::Queue& transferQueue, vk::CommandPool& copyCommandPool);

            Application::Buffer objDescBuffer;
            Application::Buffer vertexBuffer;
            Application::Buffer indexBuffer;
            std::vector<uint32_t> indices;
            std::vector<Vertex> vertices;
            std::string name;

        private:

            tinygltf::Model model;
            tinygltf::TinyGLTF loader;

            std::vector<Node> nodes;
            std::vector<Node> linearNodes;
            std::vector<Sampler> samplers;
            std::vector<Texture> textures;
            std::vector<Material> materials;

            auto loadVertexAttribute(const tinygltf::Primitive& primitive, std::string&& label);
            void loadNode(Node parent, const tinygltf::Node& node, uint32_t nodeIndex);
            void loadSamplers();
            void loadMaterials();
    };

    class SceneManager
    {
        private:
            vk::PhysicalDevice physicalDevice;
            vk::Device device;
            vk::Queue transferQueue;
            vk::CommandPool transferCommandPool;
            vk::Queue graphicsQueue;
            vk::CommandPool graphicsCommandPool;
            ImGui::FileBrowser* pFileDialog;
            std::vector<std::string>* pSceneNames;
            int* pSelectedSceneIndex;
            bool* pFreeScene;
            bool sceneChangedFlag;

            std::vector<Scene> scenes{};

        public:
            struct InitInfo
            {
                vk::PhysicalDevice physicalDevice;
                vk::Device device;
                vk::Queue transferQueue;
                vk::CommandPool transferCommandPool;
                vk::Queue graphicsQueue;
                vk::CommandPool graphicsCommandPool;
                vlb::UI* pUI;
            };

            SceneManager();
            ~SceneManager();

            void init(InitInfo& info);
            void update();
            Scene& getScene();
            bool sceneChanged();

    };

}

#endif // ifndef SCENE_MANAGER_HPP

