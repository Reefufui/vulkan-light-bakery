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
        commandBuffer.buildAccelerationStructuresKHR(1, &buildInfo, buildRangeInfos.data());
        flushCommandBuffer(commandBuffer, m_graphicsQueue);
        vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{m_blas.handle};
        m_blas.deviceAddress = m_device.getAccelerationStructureAddressKHR({m_blas.handle});

        m_device.freeMemory(scratchBuffer.memory);
        m_device.destroy(scratchBuffer.handle);
    }

    void Raytracer::createTLAS()
    {
        //TODO
    }

    void Raytracer::createStorageImage()
    {
        //TODO
    }

    void Raytracer::createUniformBuffer()
    {
        //TODO
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
    }

    Raytracer::~Raytracer()
    {
        m_device.freeMemory(m_blas.memory);
        m_device.destroy(m_blas.buffer);
        m_device.destroy(m_blas.handle);

        m_device.freeMemory(m_scene.vBufferMemory);
        m_device.freeMemory(m_scene.iBufferMemory);
        m_device.freeMemory(m_scene.tBufferMemory);
        m_device.destroy(m_scene.vBuffer);
        m_device.destroy(m_scene.iBuffer);
        m_device.destroy(m_scene.tBuffer);
    }

}

