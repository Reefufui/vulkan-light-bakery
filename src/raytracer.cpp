// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include "raytracer.hpp"

namespace vlb {

    // TODO: abstract this AS classes
    void Raytracer::createBLAS()
    {
        /////////////////////////
        // Hello triangle
        /////////////////////////
        struct Vertex {
            float pos[3];
        };
        std::vector<Vertex> vertices = {
            { {  1.0f,  1.0f, 0.0f } },
            { { -1.0f,  1.0f, 0.0f } },
            { {  0.0f, -1.0f, 0.0f } }
        };
        size_t vBufferSize = sizeof(Vertex) * vertices.size();
        std::vector<uint32_t> indices = { 0, 1, 2 };
        uint32_t indexCount = static_cast<uint32_t>(indices.size());
        size_t iBufferSize = sizeof(uint32_t) * indices.size();

        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
        bufferInfo.size  = vBufferSize;
        vk::Buffer vBuffer = m_device.createBuffer(bufferInfo);
        bufferInfo.size  = iBufferSize;
        vk::Buffer iBuffer = m_device.createBuffer(bufferInfo);

        // NOTE: it is ok not to stage the vertex data to the GPU memory for now
        vk::PhysicalDeviceMemoryProperties memoryProperties = m_physicalDevice.getMemoryProperties();
        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        auto getMemoryForBuffer = [&](vk::Buffer& a_buffer)
        {
            vk::MemoryRequirements memReq = m_device.getBufferMemoryRequirements(a_buffer);
            vk::MemoryAllocateInfo allocInfo{};
            allocInfo.allocationSize  = memReq.size;
            vk::MemoryAllocateFlagsInfo allocExt{};
            allocExt.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
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

        vk::DeviceMemory vBufferMemory = getMemoryForBuffer(vBuffer);
        vk::DeviceMemory iBufferMemory = getMemoryForBuffer(iBuffer);

        uint8_t *pData = static_cast<uint8_t *>(m_device.mapMemory(vBufferMemory, 0, vBufferSize));
        memcpy(pData, vertices.data(), vBufferSize);
        m_device.unmapMemory(vBufferMemory);
        m_device.bindBufferMemory(vBuffer, vBufferMemory, 0);

        pData = static_cast<uint8_t *>(m_device.mapMemory(iBufferMemory, 0, iBufferSize));
        memcpy(pData, indices.data(), iBufferSize);
        m_device.unmapMemory(iBufferMemory);
        m_device.bindBufferMemory(iBuffer, iBufferMemory, 0);

        vk::DeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
        vk::DeviceOrHostAddressConstKHR indexBufferDeviceAddress{};

        // NOTE: should allocate memory with proper flags
        vertexBufferDeviceAddress.deviceAddress = m_device.getBufferAddressKHR({vBuffer});
        indexBufferDeviceAddress.deviceAddress  = m_device.getBufferAddressKHR({iBuffer});

        vk::AccelerationStructureGeometryKHR accelerationStructureGeometry{};
        accelerationStructureGeometry.flags        = vk::GeometryFlagBitsKHR::eOpaque;
        accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eTriangles;
        accelerationStructureGeometry.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
        accelerationStructureGeometry.geometry.triangles.vertexData   = vertexBufferDeviceAddress;
        accelerationStructureGeometry.geometry.triangles.maxVertex    = 3;
        accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(Vertex);
        accelerationStructureGeometry.geometry.triangles.indexType    = vk::IndexType::eUint32;
        accelerationStructureGeometry.geometry.triangles.indexData    = indexBufferDeviceAddress;
        //accelerationStructureGeometry.geometry.triangles.transformData = transformBufferDeviceAddress;

        vk::AccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
        accelerationStructureBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
        accelerationStructureBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
        accelerationStructureBuildGeometryInfo.geometryCount = 1;
        accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

        const uint32_t numTriangles = 1;
        vk::AccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
        m_device.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice,
                &accelerationStructureBuildGeometryInfo,
                &numTriangles,
                &accelerationStructureBuildSizesInfo);

        /////////////////////////
        // Hello triangle
        /////////////////////////

        m_device.freeMemory(vBufferMemory);
        m_device.freeMemory(iBufferMemory);
        m_device.destroyBuffer(vBuffer);
        m_device.destroyBuffer(iBuffer);
    }

    void Raytracer::draw()
    {
    }

    Raytracer::Raytracer()
    {
        createBLAS();
    }

    Raytracer::~Raytracer()
    {
    }

}

