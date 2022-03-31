// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include <iostream>
#include <string>

#include <stb_image.h>
#include <stb_image_write.h>

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
            unsigned char*      testPixels;

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

            void loadDebugMapFromPNG(const char* filename)
            {
                int width{};
                int height{};
                int texChannels{};

                stbi_uc* pixels{};
                pixels = stbi_load(filename, &width, &height, &texChannels, STBI_rgb_alpha);

                if (!pixels)
                {
                    throw std::runtime_error(std::string("Could not load texture: ") + std::string(filename));
                }

                this->envMapExtent.width  = width;
                this->envMapExtent.height = height;

                this->testPixels = new unsigned char[width * height * 4];
                memcpy(testPixels, pixels, width * height * 4);
                stbi_image_free(pixels);

                this->envMap = Application::createImage(this->device, this->physicalDevice, this->envMapFormat, this->envMapExtent,
                        this->commandPool.transfer, this->queue.transfer);

                stbi_write_png("result.png", this->envMapExtent.width, this->envMapExtent.height, 4, (void*)testPixels, this->envMapExtent.width * 4);
            }

            int getTestMap(const char* filename)
            {
                loadDebugMapFromPNG("test_env_map.jpg");

                return 0;
            }

            Application::Image& createAndGetImage(vk::Format imageFormat, vk::Extent3D imageExtent)
            {
                this->envMapFormat = imageFormat;
                this->envMapExtent = imageExtent;
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
        private:
            struct PushConstant
            {
                uint32_t width = 1000;
                uint32_t height = 1000;
            } pushConstants;

            EnvMapGenerator envMapGenerator;

            float                  probeSpacingDistance;
            std::vector<glm::vec3> probePositions;
            Application::Buffer    SHCoeffs;

            vk::UniqueDescriptorPool      descriptorPool;
            vk::UniqueDescriptorSet       descriptorSet;
            vk::UniqueDescriptorSetLayout descriptorSetLayout;
            vk::UniquePipeline            pipeline;
            vk::UniquePipelineCache       pipelineCache;
            vk::UniquePipelineLayout      pipelineLayout;

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

                float init[16 * 3] = {0};
                this->SHCoeffs = createBuffer(16 * 3 * sizeof(float), usg, mem, init);

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
                Image& image = this->envMapGenerator.createAndGetImage(vk::Format::eR32G32B32A32Sfloat, {pushConstants.height, pushConstants.width, 1});
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
                vk::PushConstantRange pcRange{};
                pcRange
                    .setStageFlags(vk::ShaderStageFlagBits::eCompute)
                    .setOffset(0)
                    .setSize(sizeof(this->pushConstants));

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

            void dipatchBakingKernel()
            {
                auto cmd = this->recordComputeCommandBuffer();
                cmd.bindPipeline(vk::PipelineBindPoint::eCompute, this->pipeline.get());
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                        this->pipelineLayout.get(),
                        0,
                        { this->descriptorSet.get() },
                        {});
                cmd.pushConstants(
                        this->pipelineLayout.get(),
                        vk::ShaderStageFlagBits::eCompute,
                        0, sizeof(this->pushConstants), &(this->pushConstants)
                        );
                cmd.dispatch((uint32_t)ceil(this->pushConstants.width / float(WORKGROUP_SIZE)), (uint32_t)ceil(this->pushConstants.height / float(WORKGROUP_SIZE)), 1);
                this->flushComputeCommandBuffer(cmd);
            }

            void bake()
            {
                for (glm::vec3 pos : probePositions)
                {
                    auto envMap = envMapGenerator.getMap(pos);
                    //auto envMap = envMapGenerator.getTestMap("test_env_map.jpg");

                    dipatchBakingKernel();
                }
            }
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

