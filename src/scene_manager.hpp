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
            };

            struct Material
            {
                // TODO(pbr stage)
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
            const std::string& getName();

        private:

            tinygltf::Model model;
            tinygltf::TinyGLTF loader;
            std::string name;

            Application::Buffer vertexBuffer;
            Application::Buffer indexBuffer;
            std::vector<uint32_t> indices;
            std::vector<Vertex> vertices;
            std::vector<Node> nodes;
            std::vector<Node> linearNodes;

            auto loadVertexAttribute(const tinygltf::Primitive& primitive, std::string&& label);
            void loadNode(Node parent, const tinygltf::Node& node, uint32_t nodeIndex);
    };

    class SceneManager
    {
        private:
            vk::PhysicalDevice physicalDevice;
            vk::Device device;
            vk::Queue transferQueue;
            vk::CommandPool transferCommandPool;
            ImGui::FileBrowser* pFileDialog;
            std::vector<std::string>* pSceneNames;
            int* pSelectedSceneIndex;

            std::vector<Scene> scenes{};

        public:
            struct InitInfo
            {
                vk::PhysicalDevice physicalDevice;
                vk::Device device;
                vk::Queue transferQueue;
                vk::CommandPool transferCommandPool;
                vlb::UI* pUI;
            };

            SceneManager();
            ~SceneManager();

            void init(InitInfo& info);
            void update();
            Scene& getScene();

    };

}

#endif // ifndef SCENE_MANAGER_HPP

