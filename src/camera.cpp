// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "camera.hpp"

#include <imgui.h>

#include <iostream>

namespace vlb {

    Camera& Camera::setViewingFrustum(Camera::ViewingFrustum frustum)
    {
        assert(frustum.znear < frustum.zfar);
        assert(frustum.yfov > 0);
        assert(frustum.aspect > 0);

        this->frustum = frustum;

        return *this;
    }

    Camera& Camera::setType(Camera::Type type)
    {
        assert(type < Camera::Type::eTypeCount);
        this->type = type;

        return *this;
    }

    Camera& Camera::setRotationSpeed(float rotationSpeed)
    {
        this->speed.rotation = rotationSpeed;

        return *this;
    }

    Camera& Camera::setMovementSpeed(float movementSpeed)
    {
        this->speed.movement = movementSpeed;

        return *this;
    }

    const float Camera::getRotationSpeed()
    {
        return this->speed.rotation;
    }

    const float Camera::getMovementSpeed()
    {
        return this->speed.movement;
    }

    Camera& Camera::createCameraUBOs(vk::Device device, vk::PhysicalDevice physicalDevice, uint32_t count)
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

        return *this;
    }

    Camera& Camera::update()
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

            this->forward.x = -glm::cos(glm::radians(this->pitch)) * glm::sin(glm::radians(this->yaw));
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
                    this->position -= forward * length;
                if (io.KeysDown['S'])
                    this->position += forward * length;
                if (io.KeysDown['D'])
                    this->position += right * length;
                if (io.KeysDown['A'])
                    this->position -= right * length;
            }
        }

        if (io.KeysDown['E'])
        {
            std::cout << "p(" << position.x << "; " << position.y << "; " << position.z << ")\n";
            std::cout << "f(" << forward.x << "; " << forward.y << "; " << forward.z << ")\n";
            std::cout << "u(" << up.x << "; " << up.y << "; " << up.z << ")\n";
        }
        if (io.KeysDown['Q'])
        {
            std::cout << "p(" << pitch << ")\n";
            std::cout << "y(" << yaw << ")\n";
        }

        updateViewMatrix();

        return *this;
    }

    Camera& Camera::reset()
    {
        this->pitch = 0.0f;
        this->yaw = 0.0f;
        this->position  = glm::vec3();

        return *this;
    }

    void Camera::updateViewMatrix()
    {
        this->matrix.view = glm::lookAt(this->position, this->position + this->forward, this->up);
    }

    void Camera::updateProjMatrix()
    {
        this->matrix.projection = glm::perspective(
                glm::radians(this->frustum.yfov),
                this->frustum.aspect,
                this->frustum.znear,
                this->frustum.zfar );

        this->matrix.projection[1][1] *= this->frustum.flipY ? -1.0f : 1.0f;
    }

    void Camera::updateUBO(uint32_t imageIndex)
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

        ubo.projectionInv = glm::mat4(1.0f); // TODO

        memcpy(this->mappedMemory[imageIndex], &ubo, sizeof(ubo));
    }

    vk::DescriptorSetLayout& Camera::getDescriptorSetLayout()
    {
        return this->descriptor.layout.get();
    }

    vk::DescriptorSet Camera::getDescriptorSet(uint32_t imageIndex)
    {
        updateUBO(imageIndex);
        return this->descriptor.sets[imageIndex].get();
    }

    void Camera::createDSLayout(vk::Device device)
    {
        vk::DescriptorSetLayoutBinding cameraLayoutBinding{};
        cameraLayoutBinding
            .setBinding(0)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(1)
            .setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR);

        this->descriptor.layout = device.createDescriptorSetLayoutUnique(
                vk::DescriptorSetLayoutCreateInfo{}
                .setBindings(cameraLayoutBinding)
                );
    }

    void Camera::createDescriptorSets(vk::Device device)
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

