// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "camera.hpp"

#include <imgui.h>

#include <iostream>

namespace vlb {

    Camera Camera_t::setViewingFrustum(ViewingFrustum frustum)
    {
        this->frustum = frustum;

        return shared_from_this();
    }

    Camera Camera_t::setType(Camera_t::Type type)
    {
        assert(type < Camera_t::Type::eTypeCount);
        this->type = type;

        return shared_from_this();
    }

    Camera Camera_t::setRotationSpeed(float rotationSpeed)
    {
        this->speed.rotation = rotationSpeed;

        return shared_from_this();
    }

    Camera Camera_t::setMovementSpeed(float movementSpeed)
    {
        this->speed.movement = movementSpeed;

        return shared_from_this();
    }

    Camera Camera_t::setPosition(glm::vec3 position)
    {
        this->position = position;

        return shared_from_this();
    }

    Camera Camera_t::setPitch(float pitch)
    {
        this->pitch = pitch;

        return shared_from_this();
    }

    Camera Camera_t::setYaw(float yaw)
    {
        this->yaw = yaw;

        return shared_from_this();
    }

    const float Camera_t::getRotationSpeed()
    {
        return this->speed.rotation;
    }

    const float Camera_t::getMovementSpeed()
    {
        return this->speed.movement;
    }

    const glm::vec3 Camera_t::getPosition()
    {
        return this->position;
    }

    const float Camera_t::getPitch()
    {
        return this->pitch;
    }

    const float Camera_t::getYaw()
    {
        return this->yaw;
    }

    Camera Camera_t::createCameraUBOs(vk::Device device, vk::PhysicalDevice physicalDevice, uint32_t count)
    {
        vk::BufferUsageFlags    usage = vk::BufferUsageFlagBits::eUniformBuffer;
        vk::MemoryPropertyFlags props = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

        this->buffers = std::vector<Application::Buffer>(count);
        for (auto& buffer : buffers)
        {
            buffer = Application::createBuffer(device, physicalDevice, 2 * sizeof(this->matrix), usage, props);
            this->mappedMemory.push_back(device.mapMemory(buffer.memory.get(), 0, 2 * sizeof(this->matrix)));
        }

        createDSLayout(device);
        createDescriptorSets(device);

        return shared_from_this();
    }

    Camera Camera_t::update()
    {
        ImGuiIO& io = ImGui::GetIO();

        if (!io.WantCaptureMouse)
        {
            auto[dx, dy] = ImGui::IsAnyMouseDown() ? io.MouseDelta : ImVec2(0.0f, 0.0f);
            auto newPitch = this->pitch - dy * speed.rotation;
            this->pitch = (newPitch > -90.0f && newPitch < 90.0f) ? newPitch : this->pitch;
            this->yaw   -= dx * speed.rotation;
            this->yaw   = static_cast<float>(static_cast<int>(this->yaw) % 360);
        }

        if (type == Type::eFirstPerson)
        {

            this->forward.x = glm::cos(glm::radians(this->pitch)) * glm::sin(glm::radians(this->yaw));
            this->forward.y = glm::sin(glm::radians(this->pitch));
            this->forward.z = glm::cos(glm::radians(this->pitch)) * glm::cos(glm::radians(this->yaw));
            this->forward = glm::normalize(this->forward);
            auto right = glm::normalize(glm::cross(this->forward, glm::vec3(0.0f, 1.0f, 0.0f)));
            this->up = glm::normalize(glm::cross(right, this->forward));

            float speedModifier = io.KeyShift ? 4.0f : io.KeyCtrl ? 0.25f : 1.0f;
            float length = io.DeltaTime * speed.movement * speedModifier;

            if (!io.WantCaptureKeyboard)
            {
                if (io.KeysDown['W'])
                    this->position += forward * length;
                if (io.KeysDown['S'])
                    this->position -= forward * length;
                if (io.KeysDown['D'])
                    this->position += right * length;
                if (io.KeysDown['A'])
                    this->position -= right * length;
            }
        }

        updateViewMatrix();
        updateProjMatrix();

        return shared_from_this();
    }

    Camera Camera_t::reset()
    {
        this->pitch = 0.0f;
        this->yaw = 0.0f;
        this->position  = glm::vec3();

        return shared_from_this();
    }

    void Camera_t::updateViewMatrix()
    {
        this->matrix.view = glm::lookAt(this->position, this->position + this->forward, this->up);
    }

    void Camera_t::updateProjMatrix()
    {
        this->matrix.projection = glm::perspective(
                glm::radians(this->frustum->yfov),
                this->frustum->aspect,
                this->frustum->znear,
                this->frustum->zfar );

        this->matrix.projection[1][1] *= this->frustum->flipY ? -1.0f : 1.0f;
    }

    void Camera_t::updateUBO(uint32_t imageIndex)
    {
        struct
        {
            glm::mat4 projection;
            glm::mat4 view;
            glm::mat4 projectionInv;
            glm::mat4 viewInv;
        } ubo;

        ubo.projection = this->matrix.projection;
        ubo.view = this->matrix.view;
        ubo.projectionInv = glm::inverse(ubo.projection);
        ubo.viewInv = glm::inverse(ubo.view);

        memcpy(this->mappedMemory[imageIndex], &ubo, sizeof(ubo));
    }

    vk::DescriptorSetLayout& Camera_t::getDescriptorSetLayout()
    {
        return this->descriptor.layout.get();
    }

    vk::DescriptorSet Camera_t::getDescriptorSet(uint32_t imageIndex)
    {
        updateUBO(imageIndex);
        return this->descriptor.sets[imageIndex].get();
    }

    void Camera_t::createDSLayout(vk::Device device)
    {
        vk::DescriptorSetLayoutBinding cameraLayoutBinding{};
        cameraLayoutBinding
            .setBinding(0)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(1)
            .setStageFlags(vk::ShaderStageFlagBits::eVertex); // eRaygenKHR

        this->descriptor.layout = device.createDescriptorSetLayoutUnique(
                vk::DescriptorSetLayoutCreateInfo{}
                .setBindings(cameraLayoutBinding)
                );
    }

    void Camera_t::createDescriptorSets(vk::Device device)
    {
        std::vector<vk::DescriptorPoolSize> poolSizes = {
            {vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(this->buffers.size())},
        };

        this->descriptor.pool = device.createDescriptorPoolUnique(
                vk::DescriptorPoolCreateInfo{}
                .setPoolSizes(poolSizes)
                .setMaxSets(this->buffers.size())
                .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
                );

        std::vector<vk::DescriptorSetLayout> layouts(this->buffers.size(), this->descriptor.layout.get());

        this->descriptor.sets = device.allocateDescriptorSetsUnique(
                vk::DescriptorSetAllocateInfo{}
                .setDescriptorPool(this->descriptor.pool.get())
                .setSetLayouts(layouts)
                );

        for (int i{}; i < this->buffers.size(); ++i)
        {
            vk::DescriptorBufferInfo cameraUBOInfo{};
            cameraUBOInfo
                .setBuffer(this->buffers[i].handle.get())
                .setRange(VK_WHOLE_SIZE);

            vk::WriteDescriptorSet cameraWriteDS{};
            cameraWriteDS
                .setDstSet(this->descriptor.sets[i].get())
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDstBinding(0)
                .setBufferInfo(cameraUBOInfo);

            device.updateDescriptorSets(cameraWriteDS, nullptr);
        }
    }


}

