#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

PFN_vkCreateDebugUtilsMessengerEXT  pfnVkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT( VkInstance                                 instance,
                                                               const VkDebugUtilsMessengerCreateInfoEXT * pCreateInfo,
                                                               const VkAllocationCallbacks *              pAllocator,
                                                               VkDebugUtilsMessengerEXT *                 pMessenger )
{
  return pfnVkCreateDebugUtilsMessengerEXT( instance, pCreateInfo, pAllocator, pMessenger );
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT( VkInstance                    instance,
                                                            VkDebugUtilsMessengerEXT      messenger,
                                                            VkAllocationCallbacks const * pAllocator )
{
  return pfnVkDestroyDebugUtilsMessengerEXT( instance, messenger, pAllocator );
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc( VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
        VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData,
        void * /*pUserData*/ )
{
    std::ostringstream message;

    message << vk::to_string( static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>( messageSeverity ) ) << ": "
        << vk::to_string( static_cast<vk::DebugUtilsMessageTypeFlagsEXT>( messageTypes ) ) << ":\n";
    message << "\t"
        << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
    message << "\t"
        << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
    message << "\t"
        << "message         = <" << pCallbackData->pMessage << ">\n";
    if ( 0 < pCallbackData->queueLabelCount )
    {
        message << "\t"
            << "Queue Labels:\n";
        for ( uint8_t i = 0; i < pCallbackData->queueLabelCount; i++ )
        {
            message << "\t\t"
                << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
        }
    }
    if ( 0 < pCallbackData->cmdBufLabelCount )
    {
        message << "\t"
            << "CommandBuffer Labels:\n";
        for ( uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++ )
        {
            message << "\t\t"
                << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
        }
    }
    if ( 0 < pCallbackData->objectCount )
    {
        message << "\t"
            << "Objects:\n";
        for ( uint8_t i = 0; i < pCallbackData->objectCount; i++ )
        {
            message << "\t\t"
                << "Object " << i << "\n";
            message << "\t\t\t"
                << "objectType   = "
                << vk::to_string( static_cast<vk::ObjectType>( pCallbackData->pObjects[i].objectType ) ) << "\n";
            message << "\t\t\t"
                << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
            if ( pCallbackData->pObjects[i].pObjectName )
            {
                message << "\t\t\t"
                    << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
            }
        }
    }

    std::cout << message.str() << std::endl;

    return false;
}

