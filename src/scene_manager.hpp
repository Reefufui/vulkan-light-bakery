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

    class Scene
    {
        public:

            struct Vertex
            {
                glm::vec3 position;
                glm::vec3 normal;
            };

            struct Material
            {
                // TODO(pbr stage)
            };

            struct Primitive {
                uint32_t indexOffset;
                uint32_t indexCount;
                uint32_t vertexCount;
                Material &material;
                bool hasIndices;
            };

            struct Mesh {
                std::vector<Primitive*> primitives;
                struct UniformBuffer {
                    Application::Buffer buffer;
                    VkDescriptorBufferInfo descriptor;
                    VkDescriptorSet descriptorSet;
                    void *mapped;
                } uniformBuffer;
            };

            struct Node {
                Node *parent;
                uint32_t index;
                std::vector<Node*> children;
                Mesh *mesh;
                glm::vec3 translation{};
                glm::vec3 scale{ 1.0f };
                glm::quat rotation{};
            };

            Scene() = delete;
            Scene(std::string& filename);
            const std::string& getName();

        private:

            tinygltf::Model model;
            tinygltf::TinyGLTF loader;
            std::string name;

            void loadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model,
                    std::vector<uint32_t>& iBuffer, std::vector<Vertex>& vBuffer);

    };

    class SceneManager
    {
        private:
            vk::PhysicalDevice physicalDevice;
            vk::Device device;
            vk::Queue transferQueue;
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
                vlb::UI* pUI;
            };

            SceneManager();
            ~SceneManager();

            void init(InitInfo& info);
            void update();

    };

}

#endif // ifndef SCENE_MANAGER_HPP

