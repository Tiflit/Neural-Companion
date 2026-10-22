#include "layer.h"
#include "diagnostic_path.h"

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
std::unordered_map<VkSwapchainKHR, neural_companion::DeviceData*> g_swapchainToDeviceMap;

// Fixed-capacity fallback table for queues to guarantee pass-through forwarding
// even when dynamic memory allocation for std::unordered_map fails.
struct FallbackQueueEntry {
    VkQueue queue = VK_NULL_HANDLE;
    neural_companion::DeviceData* devData = nullptr;
};
static constexpr size_t kMaxFallbackQueues = 128;
static FallbackQueueEntry g_fallbackQueues[kMaxFallbackQueues]{};
static size_t g_fallbackQueueCount = 0;

static bool RecordFallbackQueueLocked(VkQueue queue, neural_companion::DeviceData* devData) noexcept {
    if (queue == VK_NULL_HANDLE || !devData) return false;
    for (size_t i = 0; i < g_fallbackQueueCount; ++i) {
        if (g_fallbackQueues[i].queue == queue) {
            g_fallbackQueues[i].devData = devData;
            return true;
        }
    }
    if (g_fallbackQueueCount < kMaxFallbackQueues) {
        g_fallbackQueues[g_fallbackQueueCount].queue = queue;
        g_fallbackQueues[g_fallbackQueueCount].devData = devData;
        ++g_fallbackQueueCount;
        return true;
    }
    return false;
}

static void RemoveFallbackQueuesForDeviceLocked(neural_companion::DeviceData* devData) noexcept {
    if (!devData) return;
    for (size_t i = 0; i < g_fallbackQueueCount; ) {
        if (g_fallbackQueues[i].devData == devData) {
            g_fallbackQueues[i] = g_fallbackQueues[g_fallbackQueueCount - 1];
            g_fallbackQueues[g_fallbackQueueCount - 1] = FallbackQueueEntry{};
            --g_fallbackQueueCount;
        } else {
            ++i;
        }
    }
}

// Fixed-capacity fallback table for swapchains to preserve swapchain-to-device ownership
// even when dynamic memory allocation for std::unordered_map fails.
struct FallbackSwapchainEntry {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    neural_companion::DeviceData* devData = nullptr;
};
static constexpr size_t kMaxFallbackSwapchains = 64;
static FallbackSwapchainEntry g_fallbackSwapchains[kMaxFallbackSwapchains]{};
static size_t g_fallbackSwapchainCount = 0;

static bool RecordFallbackSwapchainLocked(VkSwapchainKHR swapchain, neural_companion::DeviceData* devData) noexcept {
    if (swapchain == VK_NULL_HANDLE || !devData) return false;
    for (size_t i = 0; i < g_fallbackSwapchainCount; ++i) {
        if (g_fallbackSwapchains[i].swapchain == swapchain) {
            g_fallbackSwapchains[i].devData = devData;
            return true;
        }
    }
    if (g_fallbackSwapchainCount < kMaxFallbackSwapchains) {
        g_fallbackSwapchains[g_fallbackSwapchainCount].swapchain = swapchain;
        g_fallbackSwapchains[g_fallbackSwapchainCount].devData = devData;
        ++g_fallbackSwapchainCount;
        return true;
    }
    return false;
}

static void RemoveFallbackSwapchainLocked(VkSwapchainKHR swapchain) noexcept {
    if (swapchain == VK_NULL_HANDLE) return;
    for (size_t i = 0; i < g_fallbackSwapchainCount; ) {
        if (g_fallbackSwapchains[i].swapchain == swapchain) {
            g_fallbackSwapchains[i] = g_fallbackSwapchains[g_fallbackSwapchainCount - 1];
            g_fallbackSwapchains[g_fallbackSwapchainCount - 1] = FallbackSwapchainEntry{};
            --g_fallbackSwapchainCount;
        } else {
            ++i;
        }
    }
}

static void RemoveFallbackSwapchainsForDeviceLocked(neural_companion::DeviceData* devData) noexcept {
    if (!devData) return;
    for (size_t i = 0; i < g_fallbackSwapchainCount; ) {
        if (g_fallbackSwapchains[i].devData == devData) {
            g_fallbackSwapchains[i] = g_fallbackSwapchains[g_fallbackSwapchainCount - 1];
            g_fallbackSwapchains[g_fallbackSwapchainCount - 1] = FallbackSwapchainEntry{};
            --g_fallbackSwapchainCount;
        } else {
            ++i;
        }
    }
}

static neural_companion::DeviceData* ResolveDeviceForQueueLocked(
    VkQueue queue,
    const VkPresentInfoKHR* pPresentInfo) noexcept
{
    if (queue == VK_NULL_HANDLE) return nullptr;

    // 1. Primary lookup in dynamic queue map
    auto it = g_queueToDeviceMap.find(queue);
    if (it != g_queueToDeviceMap.end() && it->second != nullptr) {
        return it->second;
    }

    // 2. Fixed-capacity fallback queue table (populated if map allocation failed)
    for (size_t i = 0; i < g_fallbackQueueCount; ++i) {
        if (g_fallbackQueues[i].queue == queue && g_fallbackQueues[i].devData != nullptr) {
            return g_fallbackQueues[i].devData;
        }
    }

    // 3. Per-device owned queues in active devices
    for (const auto& devPair : g_deviceMap) {
        if (!devPair.second) continue;
        for (VkQueue q : devPair.second->ownedQueues) {
            if (q == queue) {
                return devPair.second.get();
            }
        }
    }

    // 4. Fallback: Lookup device via swapchain ownership in pPresentInfo
    if (pPresentInfo && pPresentInfo->swapchainCount > 0 && pPresentInfo->pSwapchains) {
        for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i) {
            VkSwapchainKHR sc = pPresentInfo->pSwapchains[i];
            if (sc != VK_NULL_HANDLE) {
                // Check primary swapchain map
                auto scIt = g_swapchainToDeviceMap.find(sc);
                if (scIt != g_swapchainToDeviceMap.end() && scIt->second != nullptr) {
                    return scIt->second;
                }
                // Check fallback swapchain table
                for (size_t j = 0; j < g_fallbackSwapchainCount; ++j) {
                    if (g_fallbackSwapchains[j].swapchain == sc && g_fallbackSwapchains[j].devData != nullptr) {
                        return g_fallbackSwapchains[j].devData;
                    }
                }
                // Check per-device owned swapchains in active devices
                for (const auto& devPair : g_deviceMap) {
                    if (!devPair.second) continue;
                    for (VkSwapchainKHR ownedSc : devPair.second->ownedSwapchains) {
                        if (ownedSc == sc) {
                            return devPair.second.get();
                        }
                    }
                }
            }
        }
    }

    // Strict device identity: If the queue is untracked and its swapchains are unowned/untracked,
    // do NOT guess device ownership across unrelated devices. Return nullptr to prevent
    // dispatching through an incorrect device's function pointer.
    return nullptr;
}

neural_companion::LayerStats g_stats;

std::mutex g_logMutex;
FILE* g_logFile = nullptr;

void InitLoggingLocked() {
    if (!g_logFile) {
        char envPath[MAX_PATH]{};
        fopen_s(&g_logFile, neural_companion::detail::ResolveDiagnosticLogPath(envPath), "a");
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
        instData->vkEnumeratePhysicalDeviceGroupsKHR = reinterpret_cast<PFN_vkEnumeratePhysicalDeviceGroupsKHR>(next_gipa(*pInstance, "vkEnumeratePhysicalDeviceGroupsKHR"));
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

static VkResult EnumeratePhysicalDeviceGroupsImpl(
    VkInstance instance,
    uint32_t* pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties,
    bool useKhrAlias) noexcept
{
    neural_companion::InstanceData* instData = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_instanceMap.find(instance);
        if (it != g_instanceMap.end()) {
            instData = it->second.get();
        }
    }

    if (!instData) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const auto enumerate = useKhrAlias ? instData->vkEnumeratePhysicalDeviceGroupsKHR
                                      : instData->vkEnumeratePhysicalDeviceGroups;
    if (!enumerate) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult res = enumerate(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
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

static VKAPI_ATTR VkResult VKAPI_CALL nc_vkEnumeratePhysicalDeviceGroups(
    VkInstance instance,
    uint32_t* pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) noexcept
{
    return EnumeratePhysicalDeviceGroupsImpl(instance, pPhysicalDeviceGroupCount,
        pPhysicalDeviceGroupProperties, false);
}

static VKAPI_ATTR VkResult VKAPI_CALL nc_vkEnumeratePhysicalDeviceGroupsKHR(
    VkInstance instance,
    uint32_t* pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) noexcept
{
    return EnumeratePhysicalDeviceGroupsImpl(instance, pPhysicalDeviceGroupCount,
        pPhysicalDeviceGroupProperties, true);
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

            // Remove associated queue mappings from dynamic map
            for (auto qIt = g_queueToDeviceMap.begin(); qIt != g_queueToDeviceMap.end(); ) {
                if (qIt->second == it->second.get()) {
                    qIt = g_queueToDeviceMap.erase(qIt);
                } else {
                    ++qIt;
                }
            }

            // Remove associated fallback queue entries
            RemoveFallbackQueuesForDeviceLocked(it->second.get());

            // Remove associated swapchains
            for (auto scIt = g_swapchainToDeviceMap.begin(); scIt != g_swapchainToDeviceMap.end(); ) {
                if (scIt->second == it->second.get()) {
                    scIt = g_swapchainToDeviceMap.erase(scIt);
                } else {
                    ++scIt;
                }
            }

            // Remove associated fallback swapchain entries
            RemoveFallbackSwapchainsForDeviceLocked(it->second.get());

            // Clear device-owned tracking lists
            it->second->ownedQueues.clear();
            it->second->ownedSwapchains.clear();

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
            bool simOOM = false;
            char simOOMEnv[8] = {0};
            if (GetEnvironmentVariableA("NC_TEST_SIMULATE_QUEUE_OOM", simOOMEnv, sizeof(simOOMEnv)) > 0 && simOOMEnv[0] == '1') {
                simOOM = true;
            }

            std::lock_guard<std::mutex> lock(g_stateMutex);
            // Record in per-device owned queues list
            try {
                bool alreadyOwned = false;
                for (VkQueue q : devData->ownedQueues) {
                    if (q == *pQueue) { alreadyOwned = true; break; }
                }
                if (!alreadyOwned) {
                    devData->ownedQueues.push_back(*pQueue);
                }
            } catch (...) {}

            try {
                if (simOOM) {
                    throw std::bad_alloc();
                }
                g_queueToDeviceMap[*pQueue] = devData;
                LogInfo("nc_vkGetDeviceQueue: mapped queue 0x%p (family %u, index %u) to device 0x%p",
                        reinterpret_cast<void*>(*pQueue), queueFamilyIndex, queueIndex, reinterpret_cast<void*>(device));
            } catch (const std::exception& e) {
                LogWarn("nc_vkGetDeviceQueue: optional queue map allocation failed: %s; falling back to non-allocating queue table", e.what());
                RecordFallbackQueueLocked(*pQueue, devData);
            } catch (...) {
                LogWarn("nc_vkGetDeviceQueue: optional queue map allocation failed; falling back to non-allocating queue table");
                RecordFallbackQueueLocked(*pQueue, devData);
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
            bool simOOM = false;
            char simOOMEnv[8] = {0};
            if (GetEnvironmentVariableA("NC_TEST_SIMULATE_QUEUE_OOM", simOOMEnv, sizeof(simOOMEnv)) > 0 && simOOMEnv[0] == '1') {
                simOOM = true;
            }

            std::lock_guard<std::mutex> lock(g_stateMutex);
            // Record in per-device owned queues list
            try {
                bool alreadyOwned = false;
                for (VkQueue q : devData->ownedQueues) {
                    if (q == *pQueue) { alreadyOwned = true; break; }
                }
                if (!alreadyOwned) {
                    devData->ownedQueues.push_back(*pQueue);
                }
            } catch (...) {}

            try {
                if (simOOM) {
                    throw std::bad_alloc();
                }
                g_queueToDeviceMap[*pQueue] = devData;
                LogInfo("nc_vkGetDeviceQueue2: mapped queue 0x%p to device 0x%p",
                        reinterpret_cast<void*>(*pQueue), reinterpret_cast<void*>(device));
            } catch (const std::exception& e) {
                LogWarn("nc_vkGetDeviceQueue2: optional queue map allocation failed: %s; falling back to non-allocating queue table", e.what());
                RecordFallbackQueueLocked(*pQueue, devData);
            } catch (...) {
                LogWarn("nc_vkGetDeviceQueue2: optional queue map allocation failed; falling back to non-allocating queue table");
                RecordFallbackQueueLocked(*pQueue, devData);
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
    if (res == VK_SUCCESS && pSwapchain && *pSwapchain != VK_NULL_HANDLE) {
        bool simSwapchainOOM = false;
        char simScEnv[8] = {0};
        if (GetEnvironmentVariableA("NC_TEST_SIMULATE_SWAPCHAIN_OOM", simScEnv, sizeof(simScEnv)) > 0 && simScEnv[0] == '1') {
            simSwapchainOOM = true;
        }

        bool tracked = false;
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            try {
                if (simSwapchainOOM) {
                    throw std::bad_alloc();
                }
                g_swapchainToDeviceMap[*pSwapchain] = devData;
                tracked = true;
            } catch (...) {}

            try {
                if (simSwapchainOOM) {
                    throw std::bad_alloc();
                }
                devData->ownedSwapchains.push_back(*pSwapchain);
                tracked = true;
            } catch (...) {}

            if (RecordFallbackSwapchainLocked(*pSwapchain, devData)) {
                tracked = true;
            }
        }

        if (!tracked) {
            LogError("nc_vkCreateSwapchainKHR: failed to retain swapchain ownership tracking across all storage mechanisms; rolling back downstream swapchain");
            if (devData->vkDestroySwapchainKHR) {
                devData->vkDestroySwapchainKHR(device, *pSwapchain, pAllocator);
            }
            *pSwapchain = VK_NULL_HANDLE;
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

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
    PFN_vkDestroySwapchainKHR nextDestroy = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_deviceMap.find(device);
        if (it != g_deviceMap.end()) {
            devData = it->second.get();
            nextDestroy = devData->vkDestroySwapchainKHR;

            // Clean up from device-owned swapchains
            for (auto scIt = devData->ownedSwapchains.begin(); scIt != devData->ownedSwapchains.end(); ) {
                if (*scIt == swapchain) {
                    scIt = devData->ownedSwapchains.erase(scIt);
                } else {
                    ++scIt;
                }
            }
        }
        g_swapchainToDeviceMap.erase(swapchain);
        RemoveFallbackSwapchainLocked(swapchain);
    }

    if (nextDestroy) {
        uint32_t count = ++g_stats.swapchainDestructions;
        LogInfo("nc_vkDestroySwapchainKHR: destroying swapchain 0x%p (total destroyed: %u)",
                reinterpret_cast<void*>(swapchain), count);
        nextDestroy(device, swapchain, pAllocator);
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
    bool wasFallback = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto it = g_queueToDeviceMap.find(queue);
        if (it != g_queueToDeviceMap.end() && it->second != nullptr) {
            devData = it->second;
        } else {
            devData = ResolveDeviceForQueueLocked(queue, pPresentInfo);
            if (devData) {
                wasFallback = true;
            }
        }
    }

    if (!devData || !devData->vkQueuePresentKHR) {
        LogError("nc_vkQueuePresentKHR: untracked queue 0x%p and could not resolve device fallback or downstream pointer",
                 reinterpret_cast<void*>(queue));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (wasFallback) {
        LogWarn("nc_vkQueuePresentKHR: queue 0x%p was not in primary map; successfully recovered device 0x%p via pass-through fallback",
                reinterpret_cast<void*>(queue), reinterpret_cast<void*>(devData->device));
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
    if (strcmp(pName, "vkEnumeratePhysicalDeviceGroupsKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(nc_vkEnumeratePhysicalDeviceGroupsKHR);
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

// ------------------------------------------------------------------------------------------------
// Exported Test Hooks (Regression Verification)
// ------------------------------------------------------------------------------------------------

VKAPI_ATTR bool VKAPI_CALL ncTestRegisterMockDevice(
    VkDevice device,
    PFN_vkQueuePresentKHR mockPresent,
    neural_companion::DeviceData** outDevData)
{
    if (device == VK_NULL_HANDLE) return false;
    std::lock_guard<std::mutex> lock(g_stateMutex);
    auto devData = std::make_unique<neural_companion::DeviceData>();
    devData->device = device;
    devData->vkQueuePresentKHR = mockPresent;
    if (outDevData) *outDevData = devData.get();
    g_deviceMap[device] = std::move(devData);
    return true;
}

VKAPI_ATTR void VKAPI_CALL ncTestUnregisterMockDevice(VkDevice device) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    auto it = g_deviceMap.find(device);
    if (it != g_deviceMap.end()) {
        neural_companion::DeviceData* devData = it->second.get();
        for (auto qIt = g_queueToDeviceMap.begin(); qIt != g_queueToDeviceMap.end(); ) {
            if (qIt->second == devData) {
                qIt = g_queueToDeviceMap.erase(qIt);
            } else {
                ++qIt;
            }
        }
        RemoveFallbackQueuesForDeviceLocked(devData);
        for (auto scIt = g_swapchainToDeviceMap.begin(); scIt != g_swapchainToDeviceMap.end(); ) {
            if (scIt->second == devData) {
                scIt = g_swapchainToDeviceMap.erase(scIt);
            } else {
                ++scIt;
            }
        }
        RemoveFallbackSwapchainsForDeviceLocked(devData);
        g_deviceMap.erase(it);
    }
}

VKAPI_ATTR bool VKAPI_CALL ncTestRecordFallbackQueue(VkQueue queue, VkDevice device) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    auto it = g_deviceMap.find(device);
    if (it == g_deviceMap.end()) return false;
    return RecordFallbackQueueLocked(queue, it->second.get());
}

VKAPI_ATTR bool VKAPI_CALL ncTestRegisterMockSwapchain(VkSwapchainKHR swapchain, VkDevice device, bool useFallbackTable) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    auto it = g_deviceMap.find(device);
    if (it == g_deviceMap.end()) return false;
    if (useFallbackTable) {
        return RecordFallbackSwapchainLocked(swapchain, it->second.get());
    } else {
        try {
            g_swapchainToDeviceMap[swapchain] = it->second.get();
            return true;
        } catch (...) {
            return RecordFallbackSwapchainLocked(swapchain, it->second.get());
        }
    }
}

VKAPI_ATTR bool VKAPI_CALL ncTestRegisterDeviceOwnedQueue(VkDevice device, VkQueue queue) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    auto it = g_deviceMap.find(device);
    if (it == g_deviceMap.end()) return false;
    it->second->ownedQueues.push_back(queue);
    return true;
}

VKAPI_ATTR bool VKAPI_CALL ncTestRegisterDeviceOwnedSwapchain(VkDevice device, VkSwapchainKHR swapchain) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    auto it = g_deviceMap.find(device);
    if (it == g_deviceMap.end()) return false;
    it->second->ownedSwapchains.push_back(swapchain);
    return true;
}

VKAPI_ATTR size_t VKAPI_CALL ncTestGetFallbackQueueCount() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_fallbackQueueCount;
}

VKAPI_ATTR size_t VKAPI_CALL ncTestGetFallbackSwapchainCount() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_fallbackSwapchainCount;
}

VKAPI_ATTR void VKAPI_CALL ncTestResetFallbackState() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_fallbackQueueCount = 0;
    for (size_t i = 0; i < kMaxFallbackQueues; ++i) {
        g_fallbackQueues[i] = FallbackQueueEntry{};
    }
    g_fallbackSwapchainCount = 0;
    for (size_t i = 0; i < kMaxFallbackSwapchains; ++i) {
        g_fallbackSwapchains[i] = FallbackSwapchainEntry{};
    }
    for (auto& devPair : g_deviceMap) {
        if (devPair.second) {
            devPair.second->ownedQueues.clear();
            devPair.second->ownedSwapchains.clear();
        }
    }
}

VKAPI_ATTR bool VKAPI_CALL ncTestIsSwapchainTracked(VkSwapchainKHR swapchain) {
    if (swapchain == VK_NULL_HANDLE) return false;
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (g_swapchainToDeviceMap.find(swapchain) != g_swapchainToDeviceMap.end()) {
        return true;
    }
    for (size_t i = 0; i < g_fallbackSwapchainCount; ++i) {
        if (g_fallbackSwapchains[i].swapchain == swapchain) {
            return true;
        }
    }
    for (const auto& devPair : g_deviceMap) {
        if (!devPair.second) continue;
        for (VkSwapchainKHR sc : devPair.second->ownedSwapchains) {
            if (sc == swapchain) {
                return true;
            }
        }
    }
    return false;
}

VKAPI_ATTR PFN_vkQueuePresentKHR VKAPI_CALL ncTestGetPresentWrapper() {
    return nc_vkQueuePresentKHR;
}

VKAPI_ATTR PFN_vkCreateSwapchainKHR VKAPI_CALL ncTestGetCreateSwapchainWrapper() {
    return nc_vkCreateSwapchainKHR;
}

} // extern "C"
