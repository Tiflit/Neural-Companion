#include "layer.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace {

const char* kLayerName = "VK_LAYER_NEURALCOMPANION_passthrough";
const char* kLayerDescription = "Neural-Companion Explicit Vulkan Pass-Through Layer";
const uint32_t kLayerImplementationVersion = 1;
const uint32_t kLayerSpecVersion = VK_MAKE_VERSION(1, 3, 0);

std::mutex g_stateMutex;
std::unordered_map<VkInstance, std::unique_ptr<neural_companion::InstanceData>> g_instanceMap;
std::unordered_map<VkPhysicalDevice, neural_companion::InstanceData*> g_physDeviceToInstanceMap;
std::unordered_map<VkDevice, std::unique_ptr<neural_companion::DeviceData>> g_deviceMap;
std::unordered_map<VkQueue, neural_companion::DeviceData*> g_queueToDeviceMap;

neural_companion::LayerStats g_stats;

std::mutex g_logMutex;
FILE* g_logFile = nullptr;

void InitLoggingLocked() {
    if (!g_logFile) {
        char envPath[MAX_PATH];
        DWORD len = GetEnvironmentVariableA("NC_LOG_PATH", envPath, MAX_PATH);
        if (len > 0 && envPath[0] != '\0') {
            fopen_s(&g_logFile, envPath, "a");
        } else {
            fopen_s(&g_logFile, "neural_companion_passthrough.log", "a");
        }
    }
}

void CloseLoggingLocked() {
    if (g_logFile) {
        fflush(g_logFile);
        fclose(g_logFile);
        g_logFile = nullptr;
    }
}

void LogMsg(const char* level, const char* fmt, ...) {
    char msgBuf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
    va_end(args);

    char formatted[1200];
    snprintf(formatted, sizeof(formatted), "[Neural-Companion Layer][%s] %s\n", level, msgBuf);

    // Separately owned diagnostic sinks: debugger output and owned log file.
    // Host streams (stdout/stderr) are never altered, redirected, or reopened.
#ifdef _WIN32
    OutputDebugStringA(formatted);
#endif

    std::lock_guard<std::mutex> lock(g_logMutex);
    InitLoggingLocked();
    if (g_logFile) {
        fputs(formatted, g_logFile);
        fflush(g_logFile);
    }
}

#define LogInfo(...) LogMsg("INFO", __VA_ARGS__)
#define LogWarn(...) LogMsg("WARN", __VA_ARGS__)
#define LogError(...) LogMsg("ERROR", __VA_ARGS__)

} // namespace

// Forward declaration of internal intercepted dispatchers
extern "C" {
static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL nc_vkGetInstanceProcAddr(VkInstance instance, const char* pName) noexcept;
static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL nc_vkGetDeviceProcAddr(VkDevice device, const char* pName) noexcept;
}

// ------------------------------------------------------------------------------------------------
// Instance & Physical Device Interception
// ------------------------------------------------------------------------------------------------

static VKAPI_ATTR VkResult VKAPI_CALL nc_vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* pInstance) noexcept
{
    LogInfo("nc_vkCreateInstance: initializing Vulkan instance");

    if (!pCreateInfo || !pInstance) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Traverse pCreateInfo->pNext for loader instance create info
    VkLayerInstanceCreateInfo* layerCreateInfo = const_cast<VkLayerInstanceCreateInfo*>(
        reinterpret_cast<const VkLayerInstanceCreateInfo*>(pCreateInfo->pNext));

    while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO ||
                               layerCreateInfo->function != VK_LAYER_LINK_INFO)) {
        layerCreateInfo = const_cast<VkLayerInstanceCreateInfo*>(
            reinterpret_cast<const VkLayerInstanceCreateInfo*>(layerCreateInfo->pNext));
    }

    if (!layerCreateInfo || !layerCreateInfo->u.pLayerInfo) {
        LogError("nc_vkCreateInstance: missing VkLayerInstanceCreateInfo VK_LAYER_LINK_INFO");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr next_gipa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    if (!next_gipa) {
        LogError("nc_vkCreateInstance: null pfnNextGetInstanceProcAddr in layer info");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Advance link info for downstream layer in chain
    VkLayerInstanceLink* originalLink = layerCreateInfo->u.pLayerInfo;
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateInstance nextCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        next_gipa(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!nextCreateInstance) {
        layerCreateInfo->u.pLayerInfo = originalLink;
        LogError("nc_vkCreateInstance: failed to resolve downstream vkCreateInstance");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult result = nextCreateInstance(pCreateInfo, pAllocator, pInstance);
    if (result != VK_SUCCESS) {
        layerCreateInfo->u.pLayerInfo = originalLink;
        LogWarn("nc_vkCreateInstance: downstream vkCreateInstance returned error %d", result);
        return result;
    }

    try {
        auto instData = std::make_unique<neural_companion::InstanceData>();
        instData->instance = *pInstance;
        instData->next_gipa = next_gipa;
        instData->vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(next_gipa(*pInstance, "vkDestroyInstance"));
        instData->vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(next_gipa(*pInstance, "vkEnumeratePhysicalDevices"));
        instData->vkEnumeratePhysicalDeviceGroups = reinterpret_cast<PFN_vkEnumeratePhysicalDeviceGroups>(next_gipa(*pInstance, "vkEnumeratePhysicalDeviceGroups"));
        instData->vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(next_gipa(*pInstance, "vkCreateDevice"));

        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_instanceMap[*pInstance] = std::move(instData);
    } catch (const std::exception& e) {
        LogError("nc_vkCreateInstance: memory allocation failed during instance tracking: %s. Rolling back instance.", e.what());
        PFN_vkDestroyInstance pfnDestroy = reinterpret_cast<PFN_vkDestroyInstance>(next_gipa(*pInstance, "vkDestroyInstance"));
        if (pfnDestroy) {
            pfnDestroy(*pInstance, pAllocator);
        }
        *pInstance = VK_NULL_HANDLE;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    } catch (...) {
        LogError("nc_vkCreateInstance: unknown exception during instance tracking. Rolling back instance.");
        PFN_vkDestroyInstance pfnDestroy = reinterpret_cast<PFN_vkDestroyInstance>(next_gipa(*pInstance, "vkDestroyInstance"));
        if (pfnDestroy) {
            pfnDestroy(*pInstance, pAllocator);
        }
        *pInstance = VK_NULL_HANDLE;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    LogInfo("nc_vkCreateInstance: instance 0x%p successfully created", reinterpret_cast<void*>(*pInstance));
    return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL nc_vkDestroyInstance(
    VkInstance instance,
    const VkAllocationCallbacks* pAllocator) noexcept
{
    PFN_vkDestroyInstance nextDestroy = nullptr;
    bool lastInstance = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_instanceMap.find(instance);
        if (it != g_instanceMap.end()) {
            nextDestroy = it->second->vkDestroyInstance;

            // Remove associated physical device mappings
            for (auto pIt = g_physDeviceToInstanceMap.begin(); pIt != g_physDeviceToInstanceMap.end(); ) {
                if (pIt->second == it->second.get()) {
                    pIt = g_physDeviceToInstanceMap.erase(pIt);
                } else {
                    ++pIt;
                }
            }
            g_instanceMap.erase(it);
            lastInstance = g_instanceMap.empty();
        }
    }

    LogInfo("nc_vkDestroyInstance: instance 0x%p destroyed. Total presentation calls: %llu, successful presentation calls: %llu, swapchains created: %u, swapchains destroyed: %u",
            reinterpret_cast<void*>(instance),
            static_cast<unsigned long long>(g_stats.totalPresentationCalls.load()),
            static_cast<unsigned long long>(g_stats.successfulPresentationCalls.load()),
            g_stats.swapchainCreations.load(),
            g_stats.swapchainDestructions.load());

    if (lastInstance) {
        std::lock_guard<std::mutex> lock(g_logMutex);
        CloseLoggingLocked();
    }

    if (nextDestroy) {
        nextDestroy(instance, pAllocator);
    }
}

static VKAPI_ATTR VkResult VKAPI_CALL nc_vkEnumeratePhysicalDevices(
    VkInstance instance,
    uint32_t* pPhysicalDeviceCount,
    VkPhysicalDevice* pPhysicalDevices) noexcept
{
    neural_companion::InstanceData* instData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_instanceMap.find(instance);
        if (it != g_instanceMap.end()) {
            instData = it->second.get();
        }
    }

    if (!instData || !instData->vkEnumeratePhysicalDevices) {
        LogError("nc_vkEnumeratePhysicalDevices: instance 0x%p not found or missing downstream pointer",
                 reinterpret_cast<void*>(instance));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult res = instData->vkEnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
    if ((res == VK_SUCCESS || res == VK_INCOMPLETE) && pPhysicalDevices && pPhysicalDeviceCount) {
        PFN_vkGetPhysicalDeviceProperties pfnGetProps = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
            instData->next_gipa(instance, "vkGetPhysicalDeviceProperties"));

        try {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            for (uint32_t i = 0; i < *pPhysicalDeviceCount; ++i) {
                VkPhysicalDevice pd = pPhysicalDevices[i];
                g_physDeviceToInstanceMap[pd] = instData;

                if (pfnGetProps) {
                    VkPhysicalDeviceProperties props{};
                    pfnGetProps(pd, &props);
                    LogInfo("Discovered physical device [%u]: %s (DeviceType: %u, DriverVersion: 0x%x, API: %u.%u.%u)",
                            i, props.deviceName, props.deviceType, props.driverVersion,
                            VK_VERSION_MAJOR(props.apiVersion),
                            VK_VERSION_MINOR(props.apiVersion),
                            VK_VERSION_PATCH(props.apiVersion));
                }
            }
        } catch (...) {
            LogError("nc_vkEnumeratePhysicalDevices: memory allocation failure during physical device mapping");
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }
    return res;
}

static VKAPI_ATTR VkResult VKAPI_CALL nc_vkEnumeratePhysicalDeviceGroups(
    VkInstance instance,
    uint32_t* pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) noexcept
{
    neural_companion::InstanceData* instData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_instanceMap.find(instance);
        if (it != g_instanceMap.end()) {
            instData = it->second.get();
        }
    }

    if (!instData || !instData->vkEnumeratePhysicalDeviceGroups) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult res = instData->vkEnumeratePhysicalDeviceGroups(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
    if ((res == VK_SUCCESS || res == VK_INCOMPLETE) && pPhysicalDeviceGroupProperties && pPhysicalDeviceGroupCount) {
        try {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            for (uint32_t i = 0; i < *pPhysicalDeviceGroupCount; ++i) {
                for (uint32_t j = 0; j < pPhysicalDeviceGroupProperties[i].physicalDeviceCount; ++j) {
                    g_physDeviceToInstanceMap[pPhysicalDeviceGroupProperties[i].physicalDevices[j]] = instData;
                }
            }
        } catch (...) {
            LogError("nc_vkEnumeratePhysicalDeviceGroups: memory allocation failure during group mapping");
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }
    return res;
}

// ------------------------------------------------------------------------------------------------
// Device & Queue Interception
// ------------------------------------------------------------------------------------------------

static VKAPI_ATTR VkResult VKAPI_CALL nc_vkCreateDevice(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice) noexcept
{
    neural_companion::InstanceData* instData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_physDeviceToInstanceMap.find(physicalDevice);
        if (it != g_physDeviceToInstanceMap.end()) {
            instData = it->second;
        } else if (g_instanceMap.size() == 1) {
            // Fallback for single instance if physical device wasn't enumerated through our wrapper
            instData = g_instanceMap.begin()->second.get();
        }
    }

    if (!instData) {
        LogError("nc_vkCreateDevice: failed to resolve physical device 0x%p to instance dispatch",
                 reinterpret_cast<void*>(physicalDevice));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Log the selected physical device name explicitly
    PFN_vkGetPhysicalDeviceProperties pfnGetProps = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        instData->next_gipa(instData->instance, "vkGetPhysicalDeviceProperties"));
    if (pfnGetProps) {
        VkPhysicalDeviceProperties selectedProps{};
        pfnGetProps(physicalDevice, &selectedProps);
        LogInfo("nc_vkCreateDevice: creating logical device on selected physical device: %s (DeviceType: %u)",
                selectedProps.deviceName, selectedProps.deviceType);
    }

    // Traverse pCreateInfo->pNext for loader device create info
    VkLayerDeviceCreateInfo* layerCreateInfo = const_cast<VkLayerDeviceCreateInfo*>(
        reinterpret_cast<const VkLayerDeviceCreateInfo*>(pCreateInfo->pNext));

    while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO ||
                               layerCreateInfo->function != VK_LAYER_LINK_INFO)) {
        layerCreateInfo = const_cast<VkLayerDeviceCreateInfo*>(
            reinterpret_cast<const VkLayerDeviceCreateInfo*>(layerCreateInfo->pNext));
    }

    if (!layerCreateInfo || !layerCreateInfo->u.pLayerInfo) {
        LogError("nc_vkCreateDevice: missing VkLayerDeviceCreateInfo VK_LAYER_LINK_INFO");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr next_gipa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr next_gdpa = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;

    if (!next_gipa || !next_gdpa) {
        LogError("nc_vkCreateDevice: null next_gipa or next_gdpa in layer link info");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Advance link info for downstream layer in chain
    VkLayerDeviceLink* originalLink = layerCreateInfo->u.pLayerInfo;
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateDevice nextCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(
        next_gipa(instData->instance, "vkCreateDevice"));
    if (!nextCreateDevice) {
        nextCreateDevice = instData->vkCreateDevice;
    }

    if (!nextCreateDevice) {
        layerCreateInfo->u.pLayerInfo = originalLink;
        LogError("nc_vkCreateDevice: failed to resolve downstream vkCreateDevice");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult result = nextCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (result != VK_SUCCESS) {
        layerCreateInfo->u.pLayerInfo = originalLink;
        LogWarn("nc_vkCreateDevice: downstream vkCreateDevice failed with %d", result);
        return result;
    }

    try {
        auto devData = std::make_unique<neural_companion::DeviceData>();
        devData->device = *pDevice;
        devData->instanceData = instData;
        devData->next_gdpa = next_gdpa;

        // Resolve device-level function pointers using next_gdpa
        devData->vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(next_gdpa(*pDevice, "vkDestroyDevice"));
        devData->vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(next_gdpa(*pDevice, "vkGetDeviceQueue"));
        devData->vkGetDeviceQueue2 = reinterpret_cast<PFN_vkGetDeviceQueue2>(next_gdpa(*pDevice, "vkGetDeviceQueue2"));
        devData->vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(next_gdpa(*pDevice, "vkCreateSwapchainKHR"));
        devData->vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(next_gdpa(*pDevice, "vkDestroySwapchainKHR"));
        devData->vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(next_gdpa(*pDevice, "vkGetSwapchainImagesKHR"));
        devData->vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(next_gdpa(*pDevice, "vkAcquireNextImageKHR"));
        devData->vkAcquireNextImage2KHR = reinterpret_cast<PFN_vkAcquireNextImage2KHR>(next_gdpa(*pDevice, "vkAcquireNextImage2KHR"));
        devData->vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(next_gdpa(*pDevice, "vkQueuePresentKHR"));

        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_deviceMap[*pDevice] = std::move(devData);
    } catch (const std::exception& e) {
        LogError("nc_vkCreateDevice: memory allocation failed during device tracking: %s. Rolling back device.", e.what());
        PFN_vkDestroyDevice pfnDestroy = reinterpret_cast<PFN_vkDestroyDevice>(next_gdpa(*pDevice, "vkDestroyDevice"));
        if (pfnDestroy) {
            pfnDestroy(*pDevice, pAllocator);
        }
        *pDevice = VK_NULL_HANDLE;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    } catch (...) {
        LogError("nc_vkCreateDevice: unknown exception during device tracking. Rolling back device.");
        PFN_vkDestroyDevice pfnDestroy = reinterpret_cast<PFN_vkDestroyDevice>(next_gdpa(*pDevice, "vkDestroyDevice"));
        if (pfnDestroy) {
            pfnDestroy(*pDevice, pAllocator);
        }
        *pDevice = VK_NULL_HANDLE;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    LogInfo("nc_vkCreateDevice: device 0x%p created successfully for physical device 0x%p",
            reinterpret_cast<void*>(*pDevice), reinterpret_cast<void*>(physicalDevice));
    return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL nc_vkDestroyDevice(
    VkDevice device,
    const VkAllocationCallbacks* pAllocator) noexcept
{
    PFN_vkDestroyDevice nextDestroy = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_deviceMap.find(device);
        if (it != g_deviceMap.end()) {
            nextDestroy = it->second->vkDestroyDevice;

            // Remove associated queue mappings
            for (auto qIt = g_queueToDeviceMap.begin(); qIt != g_queueToDeviceMap.end(); ) {
                if (qIt->second == it->second.get()) {
                    qIt = g_queueToDeviceMap.erase(qIt);
                } else {
                    ++qIt;
                }
            }
            g_deviceMap.erase(it);
        }
    }

    LogInfo("nc_vkDestroyDevice: device 0x%p cleanly destroyed", reinterpret_cast<void*>(device));
    if (nextDestroy) {
        nextDestroy(device, pAllocator);
    }
}

static VKAPI_ATTR void VKAPI_CALL nc_vkGetDeviceQueue(
    VkDevice device,
    uint32_t queueFamilyIndex,
    uint32_t queueIndex,
    VkQueue* pQueue) noexcept
{
    neural_companion::DeviceData* devData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_deviceMap.find(device);
        if (it != g_deviceMap.end()) {
            devData = it->second.get();
        }
    }

    if (devData && devData->vkGetDeviceQueue) {
        devData->vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
        if (pQueue && *pQueue != VK_NULL_HANDLE) {
            try {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                g_queueToDeviceMap[*pQueue] = devData;
                LogInfo("nc_vkGetDeviceQueue: mapped queue 0x%p (family %u, index %u) to device 0x%p",
                        reinterpret_cast<void*>(*pQueue), queueFamilyIndex, queueIndex, reinterpret_cast<void*>(device));
            } catch (...) {
                LogError("nc_vkGetDeviceQueue: memory allocation failure while tracking queue");
            }
        }
    }
}

static VKAPI_ATTR void VKAPI_CALL nc_vkGetDeviceQueue2(
    VkDevice device,
    const VkDeviceQueueInfo2* pQueueInfo,
    VkQueue* pQueue) noexcept
{
    neural_companion::DeviceData* devData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_deviceMap.find(device);
        if (it != g_deviceMap.end()) {
            devData = it->second.get();
        }
    }

    if (devData && devData->vkGetDeviceQueue2) {
        devData->vkGetDeviceQueue2(device, pQueueInfo, pQueue);
        if (pQueue && *pQueue != VK_NULL_HANDLE) {
            try {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                g_queueToDeviceMap[*pQueue] = devData;
                LogInfo("nc_vkGetDeviceQueue2: mapped queue 0x%p to device 0x%p",
                        reinterpret_cast<void*>(*pQueue), reinterpret_cast<void*>(device));
            } catch (...) {
                LogError("nc_vkGetDeviceQueue2: memory allocation failure while tracking queue");
            }
        }
    }
}

// ------------------------------------------------------------------------------------------------
// Swapchain & Presentation Interception (Unchanged Pass-Through)
// ------------------------------------------------------------------------------------------------

static VKAPI_ATTR VkResult VKAPI_CALL nc_vkCreateSwapchainKHR(
    VkDevice device,
    const VkSwapchainCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkSwapchainKHR* pSwapchain) noexcept
{
    neural_companion::DeviceData* devData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_deviceMap.find(device);
        if (it != g_deviceMap.end()) {
            devData = it->second.get();
        }
    }

    if (!devData || !devData->vkCreateSwapchainKHR) {
        LogError("nc_vkCreateSwapchainKHR: device 0x%p missing downstream vkCreateSwapchainKHR",
                 reinterpret_cast<void*>(device));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult res = devData->vkCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
    if (res == VK_SUCCESS && pSwapchain) {
        uint32_t count = ++g_stats.swapchainCreations;
        bool isRecreation = (pCreateInfo && pCreateInfo->oldSwapchain != VK_NULL_HANDLE);
        LogInfo("nc_vkCreateSwapchainKHR: created swapchain 0x%p (extent: %ux%u, format: %d, %s, total created: %u)",
                reinterpret_cast<void*>(*pSwapchain),
                pCreateInfo->imageExtent.width, pCreateInfo->imageExtent.height,
                pCreateInfo->imageFormat,
                isRecreation ? "RECREATION" : "INITIAL",
                count);
    }
    return res;
}

static VKAPI_ATTR void VKAPI_CALL nc_vkDestroySwapchainKHR(
    VkDevice device,
    VkSwapchainKHR swapchain,
    const VkAllocationCallbacks* pAllocator) noexcept
{
    neural_companion::DeviceData* devData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_deviceMap.find(device);
        if (it != g_deviceMap.end()) {
            devData = it->second.get();
        }
    }

    if (devData && devData->vkDestroySwapchainKHR) {
        uint32_t count = ++g_stats.swapchainDestructions;
        LogInfo("nc_vkDestroySwapchainKHR: destroying swapchain 0x%p (total destroyed: %u)",
                reinterpret_cast<void*>(swapchain), count);
        devData->vkDestroySwapchainKHR(device, swapchain, pAllocator);
    }
}

static VKAPI_ATTR VkResult VKAPI_CALL nc_vkGetSwapchainImagesKHR(
    VkDevice device,
    VkSwapchainKHR swapchain,
    uint32_t* pSwapchainImageCount,
    VkImage* pSwapchainImages) noexcept
{
    neural_companion::DeviceData* devData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_deviceMap.find(device);
        if (it != g_deviceMap.end()) {
            devData = it->second.get();
        }
    }

    if (!devData || !devData->vkGetSwapchainImagesKHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return devData->vkGetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);
}

static VKAPI_ATTR VkResult VKAPI_CALL nc_vkAcquireNextImageKHR(
    VkDevice device,
    VkSwapchainKHR swapchain,
    uint64_t timeout,
    VkSemaphore semaphore,
    VkFence fence,
    uint32_t* pImageIndex) noexcept
{
    neural_companion::DeviceData* devData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_deviceMap.find(device);
        if (it != g_deviceMap.end()) {
            devData = it->second.get();
        }
    }

    if (!devData || !devData->vkAcquireNextImageKHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return devData->vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
}

static VKAPI_ATTR VkResult VKAPI_CALL nc_vkAcquireNextImage2KHR(
    VkDevice device,
    const VkAcquireNextImageInfoKHR* pAcquireInfo,
    uint32_t* pImageIndex) noexcept
{
    neural_companion::DeviceData* devData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_deviceMap.find(device);
        if (it != g_deviceMap.end()) {
            devData = it->second.get();
        }
    }

    if (!devData || !devData->vkAcquireNextImage2KHR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return devData->vkAcquireNextImage2KHR(device, pAcquireInfo, pImageIndex);
}

static VKAPI_ATTR VkResult VKAPI_CALL nc_vkQueuePresentKHR(
    VkQueue queue,
    const VkPresentInfoKHR* pPresentInfo) noexcept
{
    neural_companion::DeviceData* devData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_queueToDeviceMap.find(queue);
        if (it != g_queueToDeviceMap.end()) {
            devData = it->second;
        }
    }

    if (!devData || !devData->vkQueuePresentKHR) {
        LogError("nc_vkQueuePresentKHR: untracked queue 0x%p or missing downstream pointer",
                 reinterpret_cast<void*>(queue));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Pass-through: unmodified forwarding of pPresentInfo downstream
    VkResult res = devData->vkQueuePresentKHR(queue, pPresentInfo);

    uint64_t total = ++g_stats.totalPresentationCalls;
    if (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR) {
        uint64_t count = ++g_stats.successfulPresentationCalls;
        if (count == 1 || count % 60 == 0) {
            LogInfo("nc_vkQueuePresentKHR: presentation call %llu forwarded downstream (VkResult: %d, total calls: %llu)",
                    static_cast<unsigned long long>(count), res, static_cast<unsigned long long>(total));
        }
    } else {
        LogWarn("nc_vkQueuePresentKHR: downstream presentation call %llu failed (VkResult: %d)",
                static_cast<unsigned long long>(total), res);
    }
    return res;
}

// ------------------------------------------------------------------------------------------------
// Layer Properties & Extension Enumeration
// ------------------------------------------------------------------------------------------------

VKAPI_ATTR VkResult VKAPI_CALL nc_vkEnumerateInstanceLayerProperties(
    uint32_t* pPropertyCount,
    VkLayerProperties* pProperties) noexcept
{
    if (pProperties == nullptr) {
        if (pPropertyCount) *pPropertyCount = 1;
        return VK_SUCCESS;
    }
    if (pPropertyCount == nullptr || *pPropertyCount == 0) {
        return VK_INCOMPLETE;
    }

    *pPropertyCount = 1;
    memset(&pProperties[0], 0, sizeof(VkLayerProperties));
    strncpy_s(pProperties[0].layerName, sizeof(pProperties[0].layerName), kLayerName, _TRUNCATE);
    pProperties[0].specVersion = kLayerSpecVersion;
    pProperties[0].implementationVersion = kLayerImplementationVersion;
    strncpy_s(pProperties[0].description, sizeof(pProperties[0].description), kLayerDescription, _TRUNCATE);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL nc_vkEnumerateInstanceExtensionProperties(
    const char* pLayerName,
    uint32_t* pPropertyCount,
    VkExtensionProperties* /* pProperties */) noexcept
{
    if (pLayerName && strcmp(pLayerName, kLayerName) == 0) {
        if (pPropertyCount) *pPropertyCount = 0;
        return VK_SUCCESS;
    }
    return VK_ERROR_LAYER_NOT_PRESENT;
}

static VKAPI_ATTR VkResult VKAPI_CALL nc_vkEnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice,
    const char* pLayerName,
    uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties) noexcept
{
    if (pLayerName && strcmp(pLayerName, kLayerName) == 0) {
        if (pPropertyCount) *pPropertyCount = 0;
        return VK_SUCCESS;
    }

    neural_companion::InstanceData* instData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_physDeviceToInstanceMap.find(physicalDevice);
        if (it != g_physDeviceToInstanceMap.end()) {
            instData = it->second;
        } else if (g_instanceMap.size() == 1) {
            instData = g_instanceMap.begin()->second.get();
        }
    }

    if (instData && instData->next_gipa) {
        auto nextFunc = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
            instData->next_gipa(instData->instance, "vkEnumerateDeviceExtensionProperties"));
        if (nextFunc) {
            return nextFunc(physicalDevice, pLayerName, pPropertyCount, pProperties);
        }
    }
    return VK_ERROR_LAYER_NOT_PRESENT;
}

// ------------------------------------------------------------------------------------------------
// Dispatch Address Resolution & Unhandled Function Forwarding
// ------------------------------------------------------------------------------------------------

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL nc_vkGetInstanceProcAddr(
    VkInstance instance,
    const char* pName) noexcept
{
    if (!pName) return nullptr;

    // Global / Instance-level functions queryable before instance creation
    if (strcmp(pName, "vkGetInstanceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkGetInstanceProcAddr);
    if (strcmp(pName, "vkCreateInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkCreateInstance);
    if (strcmp(pName, "vkEnumerateInstanceLayerProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkEnumerateInstanceLayerProperties);
    if (strcmp(pName, "vkEnumerateInstanceExtensionProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkEnumerateInstanceExtensionProperties);

    if (instance == VK_NULL_HANDLE) {
        return nullptr;
    }

    neural_companion::InstanceData* instData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_instanceMap.find(instance);
        if (it != g_instanceMap.end()) {
            instData = it->second.get();
        }
    }

    if (!instData || !instData->next_gipa) {
        return nullptr;
    }

    // Query downstream first to ensure the function is supported downstream
    PFN_vkVoidFunction downstream = instData->next_gipa(instance, pName);
    if (!downstream) {
        return nullptr;
    }

    // Intercepted instance-level functions
    if (strcmp(pName, "vkDestroyInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkDestroyInstance);
    if (strcmp(pName, "vkEnumeratePhysicalDevices") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkEnumeratePhysicalDevices);
    if (strcmp(pName, "vkEnumeratePhysicalDeviceGroups") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkEnumeratePhysicalDeviceGroups);
    if (strcmp(pName, "vkEnumeratePhysicalDeviceGroupsKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkEnumeratePhysicalDeviceGroups);
    if (strcmp(pName, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkCreateDevice);
    if (strcmp(pName, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkGetDeviceProcAddr);
    if (strcmp(pName, "vkEnumerateDeviceExtensionProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkEnumerateDeviceExtensionProperties);

    // Device-level functions queried through vkGetInstanceProcAddr
    if (strcmp(pName, "vkDestroyDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkDestroyDevice);
    if (strcmp(pName, "vkGetDeviceQueue") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkGetDeviceQueue);
    if (strcmp(pName, "vkGetDeviceQueue2") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkGetDeviceQueue2);
    if (strcmp(pName, "vkCreateSwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkCreateSwapchainKHR);
    if (strcmp(pName, "vkDestroySwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkDestroySwapchainKHR);
    if (strcmp(pName, "vkGetSwapchainImagesKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkGetSwapchainImagesKHR);
    if (strcmp(pName, "vkAcquireNextImageKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkAcquireNextImageKHR);
    if (strcmp(pName, "vkAcquireNextImage2KHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkAcquireNextImage2KHR);
    if (strcmp(pName, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkQueuePresentKHR);

    // Unhandled function forwarding
    return downstream;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL nc_vkGetDeviceProcAddr(
    VkDevice device,
    const char* pName) noexcept
{
    if (!pName) return nullptr;

    if (strcmp(pName, "vkGetDeviceProcAddr") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(nc_vkGetDeviceProcAddr);
    }

    if (device == VK_NULL_HANDLE) {
        return nullptr;
    }

    neural_companion::DeviceData* devData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_deviceMap.find(device);
        if (it != g_deviceMap.end()) {
            devData = it->second.get();
        }
    }

    if (!devData || !devData->next_gdpa) {
        return nullptr;
    }

    // Preserve downstream availability and Vulkan query semantics:
    // Query downstream first. If downstream returns null (e.g. extension not enabled or unknown function),
    // return null instead of manufacturing an unbacked wrapper.
    PFN_vkVoidFunction downstream = devData->next_gdpa(device, pName);
    if (!downstream) {
        return nullptr;
    }

    // Intercepted device-level functions
    if (strcmp(pName, "vkDestroyDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkDestroyDevice);
    if (strcmp(pName, "vkGetDeviceQueue") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkGetDeviceQueue);
    if (strcmp(pName, "vkGetDeviceQueue2") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkGetDeviceQueue2);
    if (strcmp(pName, "vkCreateSwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkCreateSwapchainKHR);
    if (strcmp(pName, "vkDestroySwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkDestroySwapchainKHR);
    if (strcmp(pName, "vkGetSwapchainImagesKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkGetSwapchainImagesKHR);
    if (strcmp(pName, "vkAcquireNextImageKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkAcquireNextImageKHR);
    if (strcmp(pName, "vkAcquireNextImage2KHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkAcquireNextImage2KHR);
    if (strcmp(pName, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkQueuePresentKHR);

    // Forward unhandled device functions downstream
    return downstream;
}

// ------------------------------------------------------------------------------------------------
// Public C Exports for Loader Negotiation
// ------------------------------------------------------------------------------------------------

extern "C" {

VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(
    VkNegotiateLayerInterface* pVersionStruct)
{
    LogInfo("vkNegotiateLoaderLayerInterfaceVersion: negotiating loader-layer interface");

    if (!pVersionStruct || pVersionStruct->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT) {
        LogError("vkNegotiateLoaderLayerInterfaceVersion: invalid sType or null struct");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Support interface versions up to CURRENT_LOADER_LAYER_INTERFACE_VERSION (2)
    if (pVersionStruct->loaderLayerInterfaceVersion >= 2) {
        pVersionStruct->loaderLayerInterfaceVersion = 2;
    } else if (pVersionStruct->loaderLayerInterfaceVersion < 1) {
        LogError("vkNegotiateLoaderLayerInterfaceVersion: unsupported loader interface version %u",
                 pVersionStruct->loaderLayerInterfaceVersion);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    pVersionStruct->pfnGetInstanceProcAddr = nc_vkGetInstanceProcAddr;
    pVersionStruct->pfnGetDeviceProcAddr = nc_vkGetDeviceProcAddr;
    pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;

    LogInfo("vkNegotiateLoaderLayerInterfaceVersion: interface version %u successfully negotiated",
            pVersionStruct->loaderLayerInterfaceVersion);
    return VK_SUCCESS;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(
    VkInstance instance,
    const char* pName)
{
    return nc_vkGetInstanceProcAddr(instance, pName);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(
    VkDevice device,
    const char* pName)
{
    return nc_vkGetDeviceProcAddr(device, pName);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(
    uint32_t* pPropertyCount,
    VkLayerProperties* pProperties)
{
    return nc_vkEnumerateInstanceLayerProperties(pPropertyCount, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(
    const char* pLayerName,
    uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties)
{
    return nc_vkEnumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties);
}

} // extern "C"
