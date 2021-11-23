// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef ACCELERATION_STRUCTURE_HPP
#define ACCELERATION_STRUCTURE_HPP

namespace vlb {

    class AccelerationStructure
    {
        private:

            vk::AccelerationStructureKHR m_blasHandle{};
            vk::AccelerationStructureKHR m_tlasHandle{};
            uint64_t m_deviceAddress{};

            vk::Device*      m_devicePtr{};
            vk::DeviceMemory m_memory{};
            vk::Buffer       m_buffer{};

            void createBuffer(VkAccelerationStructureBuildSizesInfoKHR a_buildSizeInfo)
            {
                vk::BufferCreateInfo bufferCreateInfo{};
                bufferCreateInfo.size = a_buildSizeInfo.accelerationStructureSize;
                bufferCreateInfo.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
                    | vk::BufferUsageFlagBits::eShaderDeviceAddress;
                m_buffer = m_devicePtr->createBuffer(bufferCreateInfo);

                /*
                VkMemoryRequirements memoryRequirements{};
                vkGetBufferMemoryRequirements(device, accelerationStructure.buffer, &memoryRequirements);
                VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
                memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
                memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
                VkMemoryAllocateInfo memoryAllocateInfo{};
                memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
                memoryAllocateInfo.allocationSize = memoryRequirements.size;
                memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &accelerationStructure.memory));
                VK_CHECK_RESULT(vkBindBufferMemory(device, accelerationStructure.buffer, accelerationStructure.memory, 0));
                */
            }

        public:
            AccelerationStructure(vk::Device* a_devicePtr)
                : m_devicePtr(a_devicePtr)
            {
            }

            ~AccelerationStructure()
            {
            }
    };

}

#endif // ACCELERATION_STRUCTURE_HPP

