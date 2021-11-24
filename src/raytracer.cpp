// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "raytracer.hpp"

namespace vlb {

    // TODO: abstract this AS classes
    void Raytracer::createBLAS()
    {
        /////////////////////////
        // Hello triangle NOTE:(we will load glTF model after I set everything up)
        /////////////////////////
        struct Vertex {
            float pos[3];
        };
        std::vector<Vertex> vertices = {
            { {  1.0f,  1.0f, 0.0f } },
            { { -1.0f,  1.0f, 0.0f } },
            { {  0.0f, -1.0f, 0.0f } }
        };
        std::vector<uint32_t> indices = { 0, 1, 2 };
        std::vector<float> transformMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        };
        size_t vBufferSize = sizeof(Vertex)   * vertices.size();
        size_t iBufferSize = sizeof(uint32_t) * indices.size();
        size_t tBufferSize = sizeof(float)    * transformMatrix.size();

        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
        bufferInfo.size  = vBufferSize;
        m_scene.vBuffer = m_device.createBuffer(bufferInfo);
        bufferInfo.size  = iBufferSize;
        m_scene.iBuffer = m_device.createBuffer(bufferInfo);
        bufferInfo.size  = tBufferSize;
        m_scene.tBuffer = m_device.createBuffer(bufferInfo);

        // NOTE: it is ok not to stage the vertex data to the GPU memory for now
        vk::PhysicalDeviceMemoryProperties memoryProperties = m_physicalDevice.getMemoryProperties();
        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        vk::MemoryAllocateFlagsInfo allocExt{};
        allocExt.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;

        auto getMemoryForBuffer = [&](vk::Buffer& a_buffer)
        {
            vk::MemoryRequirements memReq = m_device.getBufferMemoryRequirements(a_buffer);
            vk::MemoryAllocateInfo allocInfo{};
            allocInfo.allocationSize  = memReq.size;
            allocInfo.pNext = &allocExt;
            for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
            {
                if ((memReq.memoryTypeBits & (1 << i)) && ((memoryProperties.memoryTypes[i].propertyFlags & properties)
                            == properties))
                {
                    allocInfo.memoryTypeIndex = i;
                    break;
                }
            }

            return m_device.allocateMemory(allocInfo);
        };

        m_scene.vBufferMemory = getMemoryForBuffer(m_scene.vBuffer);
        m_scene.iBufferMemory = getMemoryForBuffer(m_scene.iBuffer);
        m_scene.tBufferMemory = getMemoryForBuffer(m_scene.tBuffer);

        uint8_t *pData = static_cast<uint8_t *>(m_device.mapMemory(m_scene.vBufferMemory, 0, vBufferSize));
        memcpy(pData, vertices.data(), vBufferSize);
        m_device.unmapMemory(m_scene.vBufferMemory);
        m_device.bindBufferMemory(m_scene.vBuffer, m_scene.vBufferMemory, 0);

        pData = static_cast<uint8_t *>(m_device.mapMemory(m_scene.iBufferMemory, 0, iBufferSize));
        memcpy(pData, indices.data(), iBufferSize);
        m_device.unmapMemory(m_scene.iBufferMemory);
        m_device.bindBufferMemory(m_scene.iBuffer, m_scene.iBufferMemory, 0);

        pData = static_cast<uint8_t *>(m_device.mapMemory(m_scene.tBufferMemory, 0, tBufferSize));
        memcpy(pData, transformMatrix.data(), tBufferSize);
        m_device.unmapMemory(m_scene.tBufferMemory);
        m_device.bindBufferMemory(m_scene.tBuffer, m_scene.tBufferMemory, 0);

        vk::DeviceOrHostAddressConstKHR vBufferDeviceAddress{};
        vk::DeviceOrHostAddressConstKHR iBufferDeviceAddress{};
        vk::DeviceOrHostAddressConstKHR tBufferDeviceAddress{};

        vBufferDeviceAddress.deviceAddress = m_device.getBufferAddressKHR({m_scene.vBuffer});
        iBufferDeviceAddress.deviceAddress = m_device.getBufferAddressKHR({m_scene.iBuffer});
        tBufferDeviceAddress.deviceAddress = m_device.getBufferAddressKHR({m_scene.tBuffer});

        /////////////////////////
        // Hello triangle
        /////////////////////////

        vk::AccelerationStructureGeometryKHR accelerationStructureGeometry{};
        accelerationStructureGeometry.flags        = vk::GeometryFlagBitsKHR::eOpaque;
        accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eTriangles;

        accelerationStructureGeometry.geometry.triangles.sType        = vk::StructureType::eAccelerationStructureGeometryTrianglesDataKHR;
        accelerationStructureGeometry.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
        accelerationStructureGeometry.geometry.triangles.maxVertex    = 3;
        accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(Vertex); // hardcoded for now

        accelerationStructureGeometry.geometry.triangles.indexType    = vk::IndexType::eUint32;

        accelerationStructureGeometry.geometry.triangles.vertexData    = vBufferDeviceAddress;
        accelerationStructureGeometry.geometry.triangles.indexData     = iBufferDeviceAddress;
        accelerationStructureGeometry.geometry.triangles.transformData = tBufferDeviceAddress;

        vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
        buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &accelerationStructureGeometry;

        const uint32_t numTriangles = 1;
        vk::AccelerationStructureBuildSizesInfoKHR buildSizeInfo{};
        m_device.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice,
                &buildInfo,
                &numTriangles,
                &buildSizeInfo);

        // separate fuction needed for this but OKKKK for now
        // creating AS structure buffer
        bufferInfo.size  = buildSizeInfo.accelerationStructureSize;
        bufferInfo.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
            | vk::BufferUsageFlagBits::eShaderDeviceAddress;

        properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        m_blas.buffer = m_device.createBuffer(bufferInfo);
        m_blas.memory = getMemoryForBuffer(m_blas.buffer);

        vk::AccelerationStructureCreateInfoKHR asCreateInfo{};
        asCreateInfo.buffer = m_blas.buffer;
        asCreateInfo.size   = buildSizeInfo.accelerationStructureSize;
        asCreateInfo.type   = vk::AccelerationStructureTypeKHR::eBottomLevel;
        m_blas.handle = m_device.createAccelerationStructureKHR(asCreateInfo);

        // separate fuction needed for this but OKKKK for now
        // creating scratch GPU buffer
        RayTracingScratchBuffer scratchBuffer{};
        bufferInfo.size = buildSizeInfo.buildScratchSize;
        bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        scratchBuffer.handle = m_device.createBuffer(bufferInfo);
        scratchBuffer.memory = getMemoryForBuffer(scratchBuffer.handle);
        m_device.bindBufferMemory(scratchBuffer.handle, scratchBuffer.memory, 0);
        scratchBuffer.deviceAddress = m_device.getBufferAddressKHR(vk::BufferDeviceAddressInfoKHR{scratchBuffer.handle});

        buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
        buildInfo.dstAccelerationStructure = m_blas.handle;
        buildInfo.geometryCount = 1;
        buildInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

        vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
        buildRangeInfo.primitiveCount = numTriangles;
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = { &buildRangeInfo };

        // Build the acceleration structure on the device via a one-time command buffer submission
        vk::CommandBuffer commandBuffer = recordCommandBuffer();
        //commandBuffer.buildAccelerationStructuresKHR(1, &buildInfo, buildRangeInfos.data());
        flushCommandBuffer(commandBuffer, m_graphicsQueue); //<- keep having errors... (may be because vulkan.hpp doesnot initialize some stucts)
        m_blas.deviceAddress = m_device.getAccelerationStructureAddressKHR({m_blas.handle});

        m_device.freeMemory(scratchBuffer.memory);
        m_device.destroy(scratchBuffer.handle);
    }

    void Raytracer::createTLAS()
    {
        vk::TransformMatrixKHR transformMatrix = {};

        vk::AccelerationStructureInstanceKHR instance{};
        instance.transform = transformMatrix;
        instance.mask = 0xFF;
        //NOTE: this may fail on futher Vulkan HPP versions (maybe they will add such .hppish flag someday :D)
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = m_blas.deviceAddress;

        struct InstancesBuffer
        {
            vk::Buffer buffer;
            vk::DeviceMemory memory;
            vk::DeviceOrHostAddressConstKHR deviceAddress{};
        } instancesBuffer;

        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
        bufferInfo.size  = sizeof(vk::AccelerationStructureInstanceKHR);
        instancesBuffer.buffer = m_device.createBuffer(bufferInfo);

        vk::PhysicalDeviceMemoryProperties memoryProperties = m_physicalDevice.getMemoryProperties();
        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        vk::MemoryAllocateFlagsInfo allocExt{};
        allocExt.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;

        auto getMemoryForBuffer = [&](vk::Buffer& a_buffer)
        {
            vk::MemoryRequirements memReq = m_device.getBufferMemoryRequirements(a_buffer);
            vk::MemoryAllocateInfo allocInfo{};
            allocInfo.allocationSize  = memReq.size;
            allocInfo.pNext = &allocExt;
            for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
            {
                if ((memReq.memoryTypeBits & (1 << i)) && ((memoryProperties.memoryTypes[i].propertyFlags & properties)
                            == properties))
                {
                    allocInfo.memoryTypeIndex = i;
                    break;
                }
            }

            return m_device.allocateMemory(allocInfo);
        };

        instancesBuffer.memory = getMemoryForBuffer(instancesBuffer.buffer);

        uint8_t *pData = static_cast<uint8_t *>(m_device.mapMemory(instancesBuffer.memory, 0, sizeof(vk::AccelerationStructureInstanceKHR)));
        memcpy(pData, &instance, sizeof(vk::AccelerationStructureInstanceKHR));
        m_device.unmapMemory(instancesBuffer.memory);
        m_device.bindBufferMemory(instancesBuffer.buffer, instancesBuffer.memory, 0);

        instancesBuffer.deviceAddress = m_device.getBufferAddressKHR(vk::BufferDeviceAddressInfoKHR{instancesBuffer.buffer});

        vk::AccelerationStructureGeometryKHR accelerationStructureGeometry{};
        accelerationStructureGeometry.flags        = vk::GeometryFlagBitsKHR::eOpaque;
        accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eInstances;

        accelerationStructureGeometry.geometry.instances.sType           = vk::StructureType::eAccelerationStructureGeometryInstancesDataKHR;
        accelerationStructureGeometry.geometry.instances.arrayOfPointers = false;
        accelerationStructureGeometry.geometry.instances.data            = instancesBuffer.deviceAddress;

        vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
        buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &accelerationStructureGeometry;

        const uint32_t primitive_count = 1;
        vk::AccelerationStructureBuildSizesInfoKHR buildSizeInfo{};
        m_device.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice,
                &buildInfo,
                &primitive_count,
                &buildSizeInfo);

        // separate fuction needed for this but OKKKK for now
        // creating AS structure buffer
        bufferInfo.size  = buildSizeInfo.accelerationStructureSize;
        bufferInfo.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
            | vk::BufferUsageFlagBits::eShaderDeviceAddress;

        properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        m_tlas.buffer = m_device.createBuffer(bufferInfo);
        m_tlas.memory = getMemoryForBuffer(m_tlas.buffer);

        vk::AccelerationStructureCreateInfoKHR asCreateInfo{};
        asCreateInfo.buffer = m_tlas.buffer;
        asCreateInfo.size   = buildSizeInfo.accelerationStructureSize;
        asCreateInfo.type   = vk::AccelerationStructureTypeKHR::eTopLevel;
        m_tlas.handle = m_device.createAccelerationStructureKHR(asCreateInfo);

        // separate fuction needed for this but OKKKK for now
        // creating scratch GPU buffer
        RayTracingScratchBuffer scratchBuffer{};
        bufferInfo.size = buildSizeInfo.buildScratchSize;
        bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        scratchBuffer.handle = m_device.createBuffer(bufferInfo);
        scratchBuffer.memory = getMemoryForBuffer(scratchBuffer.handle);
        m_device.bindBufferMemory(scratchBuffer.handle, scratchBuffer.memory, 0);
        scratchBuffer.deviceAddress = m_device.getBufferAddressKHR(vk::BufferDeviceAddressInfoKHR{scratchBuffer.handle});

        buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
        buildInfo.dstAccelerationStructure = m_tlas.handle;
        buildInfo.geometryCount = 1;
        buildInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

        vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
        buildRangeInfo.primitiveCount = 1;
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = { &buildRangeInfo };

        // Build the acceleration structure on the device via a one-time command buffer submission
        vk::CommandBuffer commandBuffer = recordCommandBuffer();
        //commandBuffer.buildAccelerationStructuresKHR(1, &buildInfo, buildRangeInfos.data());
        flushCommandBuffer(commandBuffer, m_graphicsQueue); // <- keep having errors... (may be because vulkan.hpp doesnot initialize some stucts)
        // TODO: Fine, I'll do it myself
        m_tlas.deviceAddress = m_device.getAccelerationStructureAddressKHR({m_tlas.handle});

        m_device.freeMemory(scratchBuffer.memory);
        m_device.destroy(scratchBuffer.handle);

        m_device.freeMemory(instancesBuffer.memory);
        m_device.destroy(instancesBuffer.buffer);
    }

    // TODO: move this function somewhere else
    // copied from https://github.com/nishidate-yuki/vulkan_raytracing_from_scratch/blob/master/code/vkutils.hpp
    void setImageLayout(
            vk::CommandBuffer cmdbuffer,
            vk::Image image,
            vk::ImageLayout oldImageLayout,
            vk::ImageLayout newImageLayout,
            vk::ImageSubresourceRange subresourceRange,
            vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eAllCommands,
            vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eAllCommands)
    {
        vk::ImageMemoryBarrier imageMemoryBarrier{};
        // NOTE: i like how this guy fills up such structs - looks sick!
        imageMemoryBarrier
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(image)
            .setOldLayout(oldImageLayout)
            .setNewLayout(newImageLayout)
            .setSubresourceRange(subresourceRange);

        // Source layouts (old)
        switch (oldImageLayout) {
            case vk::ImageLayout::eUndefined:
                imageMemoryBarrier.srcAccessMask = {};
                break;
            case vk::ImageLayout::ePreinitialized:
                imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
                break;
            case vk::ImageLayout::eColorAttachmentOptimal:
                imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
                break;
            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                break;
            case vk::ImageLayout::eTransferSrcOptimal:
                imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
                break;
            case vk::ImageLayout::eTransferDstOptimal:
                imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                break;
            case vk::ImageLayout::eShaderReadOnlyOptimal:
                imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
                break;
            default:
                break;
        }

        // Target layouts (new)
        switch (newImageLayout) {
            case vk::ImageLayout::eTransferDstOptimal:
                imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
                break;
            case vk::ImageLayout::eTransferSrcOptimal:
                imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
                break;
            case vk::ImageLayout::eColorAttachmentOptimal:
                imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
                break;
            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                break;
            case vk::ImageLayout::eShaderReadOnlyOptimal:
                if (imageMemoryBarrier.srcAccessMask == vk::AccessFlags{}) {
                    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite;
                }
                imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
                break;
            default:
                break;
        }

        cmdbuffer.pipelineBarrier(srcStageMask, dstStageMask, {}, {}, {}, imageMemoryBarrier);
    }

    void Raytracer::createStorageImage()
    {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.format = m_surfaceFormat.format;
        imageInfo.extent.width = m_windowWidth;
        imageInfo.extent.height = m_windowHeight;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        m_rayGenStorage.image = m_device.createImage(imageInfo);

        // TODO: make this Application class's member
        vk::PhysicalDeviceMemoryProperties memoryProperties = m_physicalDevice.getMemoryProperties();

        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        vk::MemoryRequirements memReq = m_device.getImageMemoryRequirements(m_rayGenStorage.image);
        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.allocationSize  = memReq.size;
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
        {
            if ((memReq.memoryTypeBits & (1 << i)) && ((memoryProperties.memoryTypes[i].propertyFlags & properties)
                        == properties))
            {
                allocInfo.memoryTypeIndex = i;
                break;
            }
        }
        m_rayGenStorage.memory = m_device.allocateMemory(allocInfo);
        m_device.bindImageMemory(m_rayGenStorage.image, m_rayGenStorage.memory, 0);

        vk::ImageViewCreateInfo imageViewInfo{};
        imageViewInfo.viewType = vk::ImageViewType::e2D;
        imageViewInfo.format = m_surfaceFormat.format;
        imageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;
        imageViewInfo.image = m_rayGenStorage.image;
        m_rayGenStorage.imageView = m_device.createImageView(imageViewInfo);

        vk::CommandBuffer commandBuffer = recordCommandBuffer();
        setImageLayout(commandBuffer, m_rayGenStorage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
                { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        flushCommandBuffer(commandBuffer, m_graphicsQueue);
    }

    void Raytracer::createUniformBuffer()
    {
        //TODO(glTF stage)
    }

    void Raytracer::createRayTracingPipeline()
    {
        //TODO
    }

    void Raytracer::createShaderBindingTable()
    {
        //TODO
    }

    void Raytracer::createDescriptorSets()
    {
        //TODO
    }

    void Raytracer::buildCommandBuffers()
    {
        //TODO
    }

    void Raytracer::draw()
    {
        //TODO
    }

    Raytracer::Raytracer()
    {
        createBLAS();
        createTLAS();
        createStorageImage();
    }

    // TODO(on abstraction stage): unique handles for vulkan objects to clear up this mess in destructor :)
    Raytracer::~Raytracer()
    {
        m_device.freeMemory(m_rayGenStorage.memory);
        m_device.destroy(m_rayGenStorage.image);
        m_device.destroy(m_rayGenStorage.imageView);

        m_device.freeMemory(m_blas.memory);
        m_device.destroy(m_blas.buffer);
        m_device.destroy(m_blas.handle);

        m_device.freeMemory(m_tlas.memory);
        m_device.destroy(m_tlas.buffer);
        m_device.destroy(m_tlas.handle);

        m_device.freeMemory(m_scene.vBufferMemory);
        m_device.freeMemory(m_scene.iBufferMemory);
        m_device.freeMemory(m_scene.tBufferMemory);
        m_device.destroy(m_scene.vBuffer);
        m_device.destroy(m_scene.iBuffer);
        m_device.destroy(m_scene.tBuffer);
    }

}

