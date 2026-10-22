#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace neural_companion {

struct InstanceData {
    VkInstance instance = VK_NULL_HANDLE;
    PFN_vkGetInstanceProcAddr next_gipa = nullptr;
    PFN_vkDestroyInstance vkDestroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
    PFN_vkEnumeratePhysicalDeviceGroups vkEnumeratePhysicalDeviceGroups = nullptr;
    PFN_vkEnumeratePhysicalDeviceGroupsKHR vkEnumeratePhysicalDeviceGroupsKHR = nullptr;
    PFN_vkCreateDevice vkCreateDevice = nullptr;
};

struct DeviceData {
    VkDevice device = VK_NULL_HANDLE;
    InstanceData* instanceData = nullptr;
    PFN_vkGetDeviceProcAddr next_gdpa = nullptr;
    PFN_vkDestroyDevice vkDestroyDevice = nullptr;
    PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
    PFN_vkGetDeviceQueue2 vkGetDeviceQueue2 = nullptr;
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
    PFN_vkAcquireNextImage2KHR vkAcquireNextImage2KHR = nullptr;
    PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;

    std::vector<VkQueue> ownedQueues;
    std::vector<VkSwapchainKHR> ownedSwapchains;
};

struct LayerStats {
    std::atomic<uint64_t> totalPresentationCalls{0};
    std::atomic<uint64_t> successfulPresentationCalls{0};
    std::atomic<uint32_t> swapchainCreations{0};
    std::atomic<uint32_t> swapchainDestructions{0};
};

} // namespace neural_companion

#ifdef __cplusplus
extern "C" {
#endif

// Exported loader negotiation functions per Khronos LoaderLayerInterface specification
VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* pVersionStruct);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* pName);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char* pName);
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount, VkLayerProperties* pProperties);
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties);

// Exported test hooks for verifying device isolation, dispatch tables, and fallback exhaustion
VKAPI_ATTR bool VKAPI_CALL ncTestRegisterMockDevice(VkDevice device, PFN_vkQueuePresentKHR mockPresent, neural_companion::DeviceData** outDevData);
VKAPI_ATTR void VKAPI_CALL ncTestUnregisterMockDevice(VkDevice device);
VKAPI_ATTR bool VKAPI_CALL ncTestRecordFallbackQueue(VkQueue queue, VkDevice device);
VKAPI_ATTR bool VKAPI_CALL ncTestRegisterMockSwapchain(VkSwapchainKHR swapchain, VkDevice device, bool useFallbackTable);
VKAPI_ATTR bool VKAPI_CALL ncTestRegisterDeviceOwnedQueue(VkDevice device, VkQueue queue);
VKAPI_ATTR bool VKAPI_CALL ncTestRegisterDeviceOwnedSwapchain(VkDevice device, VkSwapchainKHR swapchain);
VKAPI_ATTR size_t VKAPI_CALL ncTestGetFallbackQueueCount();
VKAPI_ATTR size_t VKAPI_CALL ncTestGetFallbackSwapchainCount();
VKAPI_ATTR void VKAPI_CALL ncTestResetFallbackState();
VKAPI_ATTR bool VKAPI_CALL ncTestIsSwapchainTracked(VkSwapchainKHR swapchain);
VKAPI_ATTR PFN_vkQueuePresentKHR VKAPI_CALL ncTestGetPresentWrapper();
VKAPI_ATTR PFN_vkCreateSwapchainKHR VKAPI_CALL ncTestGetCreateSwapchainWrapper();

#ifdef __cplusplus
}
#endif
