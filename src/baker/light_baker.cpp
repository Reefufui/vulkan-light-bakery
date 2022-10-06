// created in 2022 by Andrey Treefonov https://github.com/Reefufui

#include "light_baker.hpp"
#include "tqdm/tqdm.h"

#include <fstream>
#include <glm/gtx/string_cast.hpp> // Debug
#include <nlohmann/json.hpp>

namespace glm
{
    void to_json(nlohmann::json& j, const vec3& vec)
    {
        j = nlohmann::json{{"x", vec.x}, {"y", vec.y}, {"z", vec.z}};
    }
}

namespace vlb {

    LightBaker::LightBaker(std::string& assetName)
    {
        auto vulkanContext = EnvMapGenerator::VulkanResources
        {
            this->physicalDevice,
                this->device.get(),
                this->queue.transfer,
                this->commandPool.transfer.get(),
                this->queue.graphics,
                this->commandPool.graphics.get(),
        };

        this->envMapGenerator.passVulkanResources(vulkanContext);

        if (assetName.find(".gltf") != std::string::npos || assetName.find(".glb") != std::string::npos)
        {
            this->gltfFileName  = assetName;
            this->imageInput    = false;
            this->probesCount3D = glm::vec3(7, 7, 7); // TODO set this somewhere else

            {
                auto sceneManagerVulkanContext = Scene_t::VulkanResources
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

                SceneManager sceneManager{};
                sceneManager.passVulkanResources(sceneManagerVulkanContext);
                sceneManager.pushScene(assetName);
                sceneManager.setSceneIndex(0);
                auto scene = sceneManager.getScene();
                this->probePositions = probePositionsFromBoudingBox(scene->getBounds());
                this->envMapGenerator.setScene(std::move(scene));
            }

            this->envMapGenerator.setupVukanRaytracing();

            this->envMapGenerator.setEnvShpereRadius(500u);
            this->envMapGenerator.createImage();
        }
        else
        {
            this->imageInput = true;
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

        this->gridStep = (bounds[1] - bounds[0]) / (probesCount3D - 1.f);

        for (int dim = 0; dim < 3; ++dim)
        {
            auto positionsCopy = positions;
            for (auto position : positionsCopy)
            {
                for (int i = 0; i < this->probesCount3D[dim] - 1; ++i)
                {
                    position[dim] += gridStep[dim];
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

        tqdm bar;
        bar.set_theme_circle();

        unsigned lmax = 16u;
        this->coeffs = std::vector<uint8_t>(this->probePositions.size() * lmax * sizeof(glm::vec3));
        void* ptr = static_cast<void*>(this->coeffs.data());

        for (int i = 0; i < this->probePositions.size(); ++i)
        {
            auto& pos = this->probePositions[i];

            this->envMapGenerator.getMap(pos);

            dispatchBakingKernel();

            std::string name = std::to_string(i) + ".png";
            this->envMapGenerator.saveImage(name);

            /*
            modifyPipelineForDebug();
            dispatchBakingKernel();

            name = "output.png";
            this->envMapGenerator.saveImage(name);
            */

            void* dataPtr = this->device.get().mapMemory(SHCoeffs.memory.get(), 0, lmax * sizeof(glm::vec3));
            {
                memcpy(ptr, dataPtr, lmax * sizeof(glm::vec3));
                ptr += lmax * sizeof(glm::vec3);
            }
            device.get().unmapMemory(SHCoeffs.memory.get());

            bar.progress(i, this->probePositions.size());
        }

        bar.finish();
    }

    std::string base64_encode(uint8_t const *bytes_to_encode, unsigned int in_len)
    {
        std::string ret;
        int i = 0;
        int j = 0;
        uint8_t char_array_3[3];
        uint8_t char_array_4[4];

        const char* base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

        while (in_len--) {
            char_array_3[i++] = *(bytes_to_encode++);
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] =
                    ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] =
                    ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for (i = 0; (i < 4); i++) ret += base64_chars[char_array_4[i]];
                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 3; j++) char_array_3[j] = '\0';

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] =
                ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] =
                ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

            for (j = 0; (j < i + 1); j++) ret += base64_chars[char_array_4[j]];

            while ((i++ < 3)) ret += '=';
        }

        return ret;
    }

    void LightBaker::serialize()
    {
        std::ifstream i(this->gltfFileName);
        nlohmann::json json{};
        i >> json;

        std::string header = "data:application/octet-stream;base64,";
        std::string encodedData = base64_encode(this->coeffs.data(), static_cast<unsigned>(this->coeffs.size()));

        nlohmann::json buffer{};
        buffer["byteLength"] = this->coeffs.size();
        buffer["uri"] = header + encodedData;
        json["buffers"].push_back(buffer);

        nlohmann::json bufferView{};
        bufferView["buffer"] = json["buffers"].size() - 1;
        bufferView["byteLength"] = this->coeffs.size();
        bufferView["byteOffset"] = 0;
        json["bufferViews"].push_back(bufferView);

        json["light"]["gridStep"] = this->gridStep;
        json["light"]["lmax"] = 16u;
        json["light"]["bufferView"] = json["bufferViews"].size() - 1;

        std::ofstream o("baked_" + this->gltfFileName);
        o << std::setw(4) << json << std::endl;
    }

}

