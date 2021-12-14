// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef CAMERA_HPP
#define CAMERA_HPP

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "application.hpp"
#include "ui.hpp"

namespace vlb {

    class Camera
    {
        public:
            struct ViewingFrustum
            {
                float aspect;
                float yfov;
                float zfar;
                float znear;

                bool  flipY;

                ViewingFrustum()
                    : aspect(1.0f), yfov(45.f), zfar(10000.f), znear(0.001f)
                {
                }

                ViewingFrustum& setAspect(float aspect)
                {
                    this->aspect = aspect;
                    return *this;
                }
            };

            enum class Type
            {
                eArcBall,
                eFirstPerson,
                eTypeCount
            };

            Camera& setViewingFrustum(Camera::ViewingFrustum frustum);
            Camera& setType(Camera::Type type);
            Camera& setRotationSpeed(float rotationSpeed);
            Camera& setMovementSpeed(float movementSpeed);
            Camera& createCameraUBOs(vk::Device device, vk::PhysicalDevice physicalDevice, uint32_t count);

            vk::DescriptorSetLayout& getDescriptorSetLayout();
            Camera& update();
            vk::DescriptorSet getDescriptorSet(uint32_t imageIndex);


            Camera(){};

        private:

            ViewingFrustum frustum;
            Type type;

            std::vector<Application::Buffer> buffers;
            std::vector<void*> mappedMemory;

            struct
            {
                vk::UniqueDescriptorPool             pool;
                vk::UniqueDescriptorSetLayout        layout;
                std::vector<vk::UniqueDescriptorSet> sets;
            } descriptor;

            void createDSLayout(vk::Device device);
            void createDescriptorSets(vk::Device device);
            void updateUBO(uint32_t imageIndex);

            struct
            {
                glm::mat4 projection;
                glm::mat4 view;
            } matrix;

            float pitch = 0.0f;
            float yaw = 0.0f;
            glm::vec3 forward = glm::vec3();
            glm::vec3 up = glm::vec3();
            glm::vec3 position  = glm::vec3();

            struct
            {
                float rotation = 1.0f;
                float movement = 1.0f;
            } speed;

            void updateViewMatrix();
            void updateProjMatrix();
    };
}

#endif // ifndef CAMERA_HPP

