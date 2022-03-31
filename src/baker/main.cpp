// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include <iostream>
#include <string>

#include "application.hpp"
#include "scene_manager.hpp"

#define WORKGROUP_SIZE 16

namespace vlb
{
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
            EnvMapGenerator()
            {
            }

            ~EnvMapGenerator()
            {
            }

            void setScene(Scene&& scene)
            {
                this->scene = scene;
            }

            void passVulkanResources(VulkanResources& info)
            {
                this->device               = info.device;
                this->physicalDevice       = info.physicalDevice;
                this->queue.graphics       = info.graphicsQueue;
                this->commandPool.graphics = info.graphicsCommandPool;
                this->queue.transfer       = info.transferQueue       ? info.transferQueue       : info.graphicsQueue;
                this->commandPool.transfer = info.transferCommandPool ? info.transferCommandPool : info.graphicsCommandPool;
            }

            Application::Image& createAndGetImage(vk::Format imageFormat, vk::Extent3D imageExtent)
            {
                this->envMap = Application::createImage(this->device, this->physicalDevice, imageFormat, imageExtent,
                        this->commandPool.transfer, this->queue.transfer);
                return this->envMap;
            }

            int getMap(glm::vec3 position)
            {
                // TODO
                //
                return 0;
            }
    };

    class LightBaker : public Application
    {
        public:
            LightBaker(std::string& sceneName)
            {
                auto res = Scene_t::VulkanResources
                {
                    this->physicalDevice,
                        this->device.get(),
                        this->queue.transfer,
                        this->commandPool.transfer.get(),
                        this->queue.graphics,
                        this->commandPool.graphics.get(),
                        this->queue.compute,
                        this->commandPool.compute.get(),
                        static_cast<uint32_t>(1)
                };

                auto res2 = EnvMapGenerator::VulkanResources
                {
                    this->physicalDevice,
                        this->device.get(),
                        this->queue.transfer,
                        this->commandPool.transfer.get(),
                        this->queue.graphics,
                        this->commandPool.graphics.get(),
                };

                SceneManager sceneManager{};
                sceneManager.passVulkanResources(res);
                sceneManager.pushScene(sceneName);
                sceneManager.setSceneIndex(0);
                auto scene = sceneManager.getScene();

                this->probePositions = probePositionsFromBoudingBox(scene->getBoundingBox());
                this->envMapGenerator.setScene(std::move(sceneManager.getScene()));
                this->envMapGenerator.passVulkanResources(res2);

                createBakingPipeline();
            }

            ~LightBaker()
            {
            }

            std::vector<glm::vec3> probePositionsFromBoudingBox(std::array<glm::vec3, 8> boundingBox) // TODO
            {
                std::vector<glm::vec3> positions(0);
                positions.push_back(glm::vec3(0.0f));
                return positions;
            }

            void createBakingPipeline()
            {
                using enum vk::BufferUsageFlagBits;
                using enum vk::MemoryPropertyFlagBits;
                vk::BufferUsageFlags    usg{ eStorageBuffer };
                vk::MemoryPropertyFlags mem{ eHostVisible | eHostCoherent };

                double init[9 * 3] = {0};
                Buffer SHCoeffs = createBuffer(9 * 3 * sizeof(double), usg, mem, init);

                // POOL
                std::vector<vk::DescriptorPoolSize> poolSizes = {
                    {vk::DescriptorType::eStorageBuffer, 1},
                    {vk::DescriptorType::eStorageImage, 1}
                };

                this->descriptorPool = this->device.get().createDescriptorPoolUnique(
                        vk::DescriptorPoolCreateInfo{}
                        .setPoolSizes(poolSizes)
                        .setMaxSets(1)
                        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
                            | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
                        );

                // LAYOUT
                std::vector<vk::DescriptorSetLayoutBinding> dsLayoutBinding(0);
                dsLayoutBinding.push_back(vk::DescriptorSetLayoutBinding{}
                        .setBinding(0)
                        .setDescriptorType(vk::DescriptorType::eStorageImage)
                        .setDescriptorCount(1)
                        .setStageFlags(vk::ShaderStageFlagBits::eCompute)
                        );
                dsLayoutBinding.push_back(vk::DescriptorSetLayoutBinding{}
                        .setBinding(1)
                        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                        .setDescriptorCount(1)
                        .setStageFlags(vk::ShaderStageFlagBits::eCompute)
                        );

                this->descriptorSetLayout = this->device.get().createDescriptorSetLayoutUnique(
                        vk::DescriptorSetLayoutCreateInfo{}
                        .setBindings(dsLayoutBinding)
                        );

                // DESCRIPOR
                this->descriptorSet = std::move(this->device.get().allocateDescriptorSetsUnique(
                        vk::DescriptorSetAllocateInfo{}
                        .setDescriptorPool(this->descriptorPool.get())
                        .setSetLayouts(this->descriptorSetLayout.get())
                        ).front());

                // WRITE
                Image& image = this->envMapGenerator.createAndGetImage(vk::Format::eR32G32B32A32Sfloat, {envMapHeight, envMapWidth, 1});
                vk::DescriptorImageInfo imageInfo{};
                imageInfo
                    .setImageView(image.imageView.get())
                    .setImageLayout(vk::ImageLayout::eGeneral);

                vk::WriteDescriptorSet writeImage{};
                writeImage
                    .setDstSet(this->descriptorSet.get())
                    .setDstBinding(0)
                    .setDescriptorType(vk::DescriptorType::eStorageImage)
                    .setImageInfo(imageInfo);

                this->device.get().updateDescriptorSets(writeImage, nullptr);

                vk::DescriptorBufferInfo bufferInfo{};
                bufferInfo
                    .setBuffer(SHCoeffs.handle.get())
                    .setOffset(0)
                    .setRange(SHCoeffs.size);

                vk::WriteDescriptorSet writeBuffer{};
                writeBuffer
                    .setDstSet(this->descriptorSet.get())
                    .setDstBinding(1)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setBufferInfo(bufferInfo);

                this->device.get().updateDescriptorSets(writeBuffer, nullptr);

                // PIPELINE LAYOUT
                struct PushConstant
                {
                    uint32_t width;
                    uint32_t height;
                };

                vk::PushConstantRange pcRange{};
                pcRange
                    .setStageFlags(vk::ShaderStageFlagBits::eCompute)
                    .setOffset(0)
                    .setSize(sizeof(PushConstant));

                this->pipelineLayout = this->device.get().createPipelineLayoutUnique(
                        vk::PipelineLayoutCreateInfo{}
                        .setSetLayouts(this->descriptorSetLayout.get())
                        .setPushConstantRanges(pcRange)
                        );

                // SHADER MODULE
                vk::UniqueShaderModule shaderModule = Application::createShaderModule("shaders/sh.comp.spv");

                vk::PipelineShaderStageCreateInfo shaderStageCreateInfo{};
                shaderStageCreateInfo
                    .setStage(vk::ShaderStageFlagBits::eCompute)
                    .setModule(shaderModule.get())
                    .setPName("main");

                // PIPELINE
                this->pipelineCache = this->device.get().createPipelineCacheUnique(vk::PipelineCacheCreateInfo());

                auto[result, p] = this->device.get().createComputePipelineUnique(
                        pipelineCache.get(),
                        vk::ComputePipelineCreateInfo{}
                        .setStage(shaderStageCreateInfo)
                        .setLayout(this->pipelineLayout.get()));

                if (result != vk::Result::eSuccess)
                {
                    throw std::runtime_error("failed to create compute pipeline");
                }
                else
                {
                    this->pipeline = std::move(p);
                }
            }

            void updateMapDescriptorSet()
            {
            }

            void recordBakingCmd()
            {
                auto cmd = this->recordComputeCommandBuffer();
                cmd.bindPipeline(vk::PipelineBindPoint::eCompute, this->pipeline.get());
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                        this->pipelineLayout.get(),
                        0,
                        { this->descriptorSet.get() },
                        {});
                cmd.dispatch((uint32_t)ceil(this->envMapWidth / float(WORKGROUP_SIZE)), (uint32_t)ceil(this->envMapHeight / float(WORKGROUP_SIZE)), 1);
                this->flushComputeCommandBuffer(cmd);
            }

            void bake()
            {
                for (glm::vec3 pos : probePositions)
                {
                    auto envMap = envMapGenerator.getMap(pos);
                    updateMapDescriptorSet();

                    //recordBakingCmd();
                }
            }

        private:
            EnvMapGenerator envMapGenerator;

            float                  probeSpacingDistance;
            uint32_t               envMapWidth = 1000;
            uint32_t               envMapHeight = 1000;
            std::vector<glm::vec3> probePositions;

            vk::UniqueDescriptorPool      descriptorPool;
            vk::UniqueDescriptorSet       descriptorSet;
            vk::UniqueDescriptorSetLayout descriptorSetLayout;
            vk::UniquePipeline            pipeline;
            vk::UniquePipelineCache       pipelineCache;
            vk::UniquePipelineLayout      pipelineLayout;
    };
}

int main(int argc, char** argv)
{
    try
    {
        std::string sceneFileName{};
        if (argc < 2)
        {
            throw std::runtime_error("Select scene to bake.");
        }
        else
        {
            sceneFileName = argv[1];
        }

        vlb::LightBaker baker{sceneFileName};
        baker.bake();
    }
    catch(const vk::SystemError& error)
    {
        return EXIT_FAILURE;
    }
    catch(std::exception& error)
    {
        std::cerr << "std::exception: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "unknown error\n";
        return EXIT_FAILURE;
    }

    std::cout << "exiting...\n";
    return EXIT_SUCCESS;
}

