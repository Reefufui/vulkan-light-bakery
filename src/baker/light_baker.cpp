// created in 2022 by Andrey Treefonov https://github.com/Reefufui

#include "light_baker.hpp"
#include "tqdm/tqdm.h"

#include <fstream>
#include <glm/gtx/string_cast.hpp> // Debug
#include <nlohmann/json.hpp>

namespace vlb {

    LightBaker::LightBaker(std::string& assetName)
    {
        if (assetName.find(".gltf") != std::string::npos || assetName.find(".glb") != std::string::npos)
        {
            this->gltfFileName = assetName;

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

            this->envMapGenerator.passVulkanResources(res2);

            this->probesCount3D = glm::vec3(5, 5, 5); // TODO set this somewhere else

            {
                SceneManager sceneManager{};
                sceneManager.passVulkanResources(res);
                sceneManager.pushScene(assetName);
                sceneManager.setSceneIndex(0);
                auto scene = sceneManager.getScene();
                std::cout << "here\n";
                this->probePositions = probePositionsFromBoudingBox(scene->getBounds());
                this->envMapGenerator.setScene(std::move(scene));
            }

            this->envMapGenerator.setupVukanRaytracing();

            this->envMapGenerator.setEnvShpereRadius(100u);
            this->envMapGenerator.createImage();
        }
        else
        {
            this->probePositions.push_back(glm::vec3(0.0f));
            this->envMapGenerator.createImage(assetName.c_str());
        }
    }

    LightBaker::~LightBaker()
    {
    }

    std::vector<glm::vec3> LightBaker::probePositionsFromBoudingBox(std::array<glm::vec3, 2> bounds)
    {
        std::vector<glm::vec3> positions(0);
        positions.push_back(bounds[0]);

        const glm::vec3 step = (bounds[1] - bounds[0]) / (probesCount3D - 1.f);

        for (int dim = 0; dim < 3; ++dim)
        {
            auto positionsCopy = positions;
            for (auto position : positionsCopy)
            {
                for (int i = 0; i < this->probesCount3D[dim] - 1; ++i)
                {
                    position[dim] += step[dim];
                    positions.push_back(position);
                }
            }
        }

        return positions;
    }

    void LightBaker::createBakingPipeline()
    {
        using enum vk::BufferUsageFlagBits;
        using enum vk::MemoryPropertyFlagBits;
        vk::BufferUsageFlags    usg{ eStorageBuffer };
        vk::MemoryPropertyFlags mem{ eHostVisible | eHostCoherent };

        float init[16 * 3] = {0.f};
        /*
           float init[16 * 3] = {
           -1.028, 0.779, -0.275, 0.601, -0.256,
           1.891, -1.658, -0.370, -0.772,
           -0.591, -0.713, 0.191, 1.206, -0.587,
           -0.051, 1.543, -0.818, 1.482,
           -1.119, 0.559, 0.433, -0.680, -1.815,
           -0.915, 1.345, 1.572, -0.622};
           */

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
        Image& image = this->envMapGenerator.getImage();
        vk::Extent3D extent = this->envMapGenerator.getImageExtent();
        pushConstants.width  = extent.width;
        pushConstants.height = extent.height;
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

    void LightBaker::modifyPipelineForDebug()
    {
        this->pipeline.reset();
        this->pipelineCache.reset();

        vk::UniqueShaderModule shaderModule = Application::createShaderModule("shaders/sh_sum.comp.spv");
        vk::PipelineShaderStageCreateInfo shaderStageCreateInfo{};
        shaderStageCreateInfo
            .setStage(vk::ShaderStageFlagBits::eCompute)
            .setModule(shaderModule.get())
            .setPName("main");

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

    void LightBaker::dispatchBakingKernel()
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

    void LightBaker::bake()
    {
        createBakingPipeline();

        nlohmann::json json{};
        std::ifstream i(this->gltfFileName);
        //std::ofstream o(this->gltfFileName);


        tqdm bar;
        bar.set_theme_circle();

        for (int i = 0; i < this->probePositions.size(); ++i)
        {
            auto& pos = this->probePositions[i];

            this->envMapGenerator.getMap(pos);
            dispatchBakingKernel();
            void* dataPtr = this->device.get().mapMemory(SHCoeffs.memory.get(), 0, 16 * 3 * sizeof(float));
            float* coeffs = static_cast<float*>(dataPtr);
            device.get().unmapMemory(SHCoeffs.memory.get());

            std::string name = std::to_string(i) + ".png";
            //this->envMapGenerator.saveImage(name);

            bar.progress(i, this->probePositions.size());
        }

        bar.finish();
    }
}

