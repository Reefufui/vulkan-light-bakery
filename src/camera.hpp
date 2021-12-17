// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef CAMERA_HPP
#define CAMERA_HPP

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "application.hpp"

namespace vlb {

    struct ViewingFrustum_t;
    typedef std::shared_ptr<ViewingFrustum_t> ViewingFrustum;
    struct ViewingFrustum_t
    {
        float aspect;
        float yfov;
        float zfar;
        float znear;

        bool  flipY;

        ViewingFrustum_t()
            : aspect(1.0f), yfov(60.f), zfar(10000.f), znear(0.001f)
        {
        }

        ViewingFrustum_t& setAspect(float aspect)
        {
            this->aspect = aspect;
            return *this;
        }
    };

    class Camera
    {
        public:
            enum class Type
            {
                eArcBall,
                eFirstPerson,
                eTypeCount
            };

            Camera& setViewingFrustum(ViewingFrustum frustum);
            Camera& setType(Camera::Type type);
            Camera& setRotationSpeed(float rotationSpeed);
            Camera& setMovementSpeed(float movementSpeed);
            Camera& createCameraUBOs(vk::Device device, vk::PhysicalDevice physicalDevice, uint32_t count);

            const float getRotationSpeed();
            const float getMovementSpeed();

            vk::DescriptorSetLayout& getDescriptorSetLayout();
            Camera& update();
            Camera& reset();
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
                float rotation = 0.25f;
                float movement = 1.0f;
            } speed;

            void updateViewMatrix();
            void updateProjMatrix();
    };
}

#endif // ifndef CAMERA_HPP

