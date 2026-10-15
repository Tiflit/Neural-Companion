#include <vulkan/vulkan.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "layer.h"

#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] Assertion failed: " << msg << " (" << #cond << ")" << std::endl; \
            return 1; \
        } \
    } while (0)

static int g_mockPresentACalls = 0;
static int g_mockPresentBCalls = 0;
static VkQueue g_lastQueueSeenA = VK_NULL_HANDLE;
static VkQueue g_lastQueueSeenB = VK_NULL_HANDLE;

static VKAPI_ATTR VkResult VKAPI_CALL MockPresentA(VkQueue q, const VkPresentInfoKHR* /*pInfo*/) {
    ++g_mockPresentACalls;
    g_lastQueueSeenA = q;
    return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL MockPresentB(VkQueue q, const VkPresentInfoKHR* /*pInfo*/) {
    ++g_mockPresentBCalls;
    g_lastQueueSeenB = q;
    return VK_SUCCESS;
}

int main() {
    std::cout << "=================================================================\n";
    std::cout << " Neural-Companion: Layer Dispatch & Query Semantics Test\n";
    std::cout << "=================================================================\n";

    // 1. Initialize Vulkan Instance
    uint32_t instExtCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &instExtCount, nullptr);
    std::vector<VkExtensionProperties> instExts(instExtCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &instExtCount, instExts.data());

    std::vector<const char*> enabledInstExts;
    for (const auto& ext : instExts) {
        if (strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
            enabledInstExts.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        }
#if defined(_WIN32)
        if (strcmp(ext.extensionName, "VK_KHR_win32_surface") == 0) {
            enabledInstExts.push_back("VK_KHR_win32_surface");
        }
#endif
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "LayerDispatchTest";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "NeuralCompanion";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(enabledInstExts.size());
    instanceInfo.ppEnabledExtensionNames = enabledInstExts.data();

    VkInstance instance = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&instanceInfo, nullptr, &instance);
    TEST_ASSERT(res == VK_SUCCESS && instance != VK_NULL_HANDLE, "vkCreateInstance failed");
    std::cout << "  [PASS] Vulkan instance created successfully (0x" << instance << ", surface ext enabled)\n";

    // 2. Resolve Layer Direct Module Handles
    HMODULE hLayer = GetModuleHandleA("VkLayer_neural_companion_passthrough.dll");
    if (!hLayer) {
        hLayer = LoadLibraryA("VkLayer_neural_companion_passthrough.dll");
    }
    TEST_ASSERT(hLayer != NULL, "VkLayer_neural_companion_passthrough.dll not found in process memory or loadable");
    std::cout << "  [PASS] Layer module resolved in process memory (0x" << hLayer << ")\n";

    auto layerGDPA = reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(hLayer, "vkGetDeviceProcAddr"));
    auto layerGIPA = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(hLayer, "vkGetInstanceProcAddr"));
    TEST_ASSERT(layerGDPA != nullptr && layerGIPA != nullptr, "Layer exports vkGetDeviceProcAddr and vkGetInstanceProcAddr");
    std::cout << "  [PASS] Resolved layer vkGetDeviceProcAddr and vkGetInstanceProcAddr exports\n";

    // Test Instance-level queries
    PFN_vkVoidFunction pfnNonExistentInst = layerGIPA(instance, "vkNonExistentInstanceFunctionEXT");
    TEST_ASSERT(pfnNonExistentInst == nullptr, "layerGIPA must return nullptr for non-existent instance function");
    std::cout << "  [PASS] layerGIPA('vkNonExistentInstanceFunctionEXT') returned nullptr\n";

    PFN_vkVoidFunction pfnEnumPhys = layerGIPA(instance, "vkEnumeratePhysicalDevices");
    TEST_ASSERT(pfnEnumPhys != nullptr, "layerGIPA must return valid wrapper for vkEnumeratePhysicalDevices");
    std::cout << "  [PASS] layerGIPA('vkEnumeratePhysicalDevices') returned valid wrapper\n";

    PFN_vkVoidFunction pfnCreateDev = layerGIPA(instance, "vkCreateDevice");
    TEST_ASSERT(pfnCreateDev != nullptr, "layerGIPA must return valid wrapper for vkCreateDevice");
    std::cout << "  [PASS] layerGIPA('vkCreateDevice') returned valid wrapper\n";

    // 3. Enumerate Physical Devices
    uint32_t gpuCount = 0;
    res = vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
    TEST_ASSERT(res == VK_SUCCESS && gpuCount > 0, "No physical devices enumerated");

    std::vector<VkPhysicalDevice> gpus(gpuCount);
    res = vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());
    TEST_ASSERT(res == VK_SUCCESS, "vkEnumeratePhysicalDevices failed");

    VkPhysicalDevice selectedGpu = gpus[0];
    VkPhysicalDeviceProperties gpuProps{};
    vkGetPhysicalDeviceProperties(selectedGpu, &gpuProps);
    std::cout << "  [PASS] Selected Physical Device: " << gpuProps.deviceName 
              << " (Type: " << gpuProps.deviceType << ", API: " 
              << VK_VERSION_MAJOR(gpuProps.apiVersion) << "."
              << VK_VERSION_MINOR(gpuProps.apiVersion) << "."
              << VK_VERSION_PATCH(gpuProps.apiVersion) << ")\n";

    // Find graphics/compute queue family
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(selectedGpu, &queueFamilyCount, nullptr);
    TEST_ASSERT(queueFamilyCount > 0, "No queue families found");
    std::vector<VkQueueFamilyProperties> queueProps(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(selectedGpu, &queueFamilyCount, queueProps.data());

    uint32_t queueFamilyIndex = 0;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueProps[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
            queueFamilyIndex = i;
            break;
        }
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamilyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    // -------------------------------------------------------------------------
    // TEST 1: Device without VK_KHR_swapchain extension
    // -------------------------------------------------------------------------
    std::cout << "\n--- Test 1: Extension-Disabled Query Semantics ---\n";
    VkDeviceCreateInfo deviceCreateInfoWithoutExt{};
    deviceCreateInfoWithoutExt.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfoWithoutExt.queueCreateInfoCount = 1;
    deviceCreateInfoWithoutExt.pQueueCreateInfos = &queueInfo;
    deviceCreateInfoWithoutExt.enabledExtensionCount = 0;
    deviceCreateInfoWithoutExt.ppEnabledExtensionNames = nullptr;

    VkDevice deviceWithoutExt = VK_NULL_HANDLE;
    res = vkCreateDevice(selectedGpu, &deviceCreateInfoWithoutExt, nullptr, &deviceWithoutExt);
    TEST_ASSERT(res == VK_SUCCESS && deviceWithoutExt != VK_NULL_HANDLE, "vkCreateDevice without extensions failed");
    std::cout << "  [PASS] Created VkDevice without extensions (0x" << deviceWithoutExt << ")\n";

    // Query layer directly: device without VK_KHR_swapchain MUST return nullptr for swapchain functions
    PFN_vkVoidFunction pfnCreateSwapchain = layerGDPA(deviceWithoutExt, "vkCreateSwapchainKHR");
    TEST_ASSERT(pfnCreateSwapchain == nullptr, "layerGDPA must return nullptr for disabled vkCreateSwapchainKHR");
    std::cout << "  [PASS] layerGDPA('vkCreateSwapchainKHR') correctly returned nullptr\n";

    PFN_vkVoidFunction pfnQueuePresent = layerGDPA(deviceWithoutExt, "vkQueuePresentKHR");
    TEST_ASSERT(pfnQueuePresent == nullptr, "layerGDPA must return nullptr for disabled vkQueuePresentKHR");
    std::cout << "  [PASS] layerGDPA('vkQueuePresentKHR') correctly returned nullptr\n";

    // Non-existent function must return nullptr
    PFN_vkVoidFunction pfnNonExistent = layerGDPA(deviceWithoutExt, "vkNonExistentFunctionEXT");
    TEST_ASSERT(pfnNonExistent == nullptr, "layerGDPA must return nullptr for non-existent function");
    std::cout << "  [PASS] layerGDPA('vkNonExistentFunctionEXT') correctly returned nullptr\n";

    // Unhandled core function must be forwarded downstream and return non-null
    PFN_vkVoidFunction pfnCmdDraw = layerGDPA(deviceWithoutExt, "vkCmdDraw");
    TEST_ASSERT(pfnCmdDraw != nullptr, "layerGDPA must return valid downstream pointer for unhandled vkCmdDraw");
    std::cout << "  [PASS] layerGDPA('vkCmdDraw') unhandled function forwarding verified\n";

    // Intercepted core device function must return non-null wrapper
    PFN_vkVoidFunction pfnGetDeviceQueue = layerGDPA(deviceWithoutExt, "vkGetDeviceQueue");
    TEST_ASSERT(pfnGetDeviceQueue != nullptr, "layerGDPA must return valid pointer for intercepted vkGetDeviceQueue");
    std::cout << "  [PASS] layerGDPA('vkGetDeviceQueue') intercepted core function verified\n";

    vkDestroyDevice(deviceWithoutExt, nullptr);
    std::cout << "  [PASS] Device without extensions destroyed cleanly\n";

    // -------------------------------------------------------------------------
    // TEST 2: Device with VK_KHR_swapchain extension enabled
    // -------------------------------------------------------------------------
    std::cout << "\n--- Test 2: Extension-Enabled Query Semantics ---\n";
    uint32_t devExtCount = 0;
    vkEnumerateDeviceExtensionProperties(selectedGpu, nullptr, &devExtCount, nullptr);
    std::vector<VkExtensionProperties> devExts(devExtCount);
    vkEnumerateDeviceExtensionProperties(selectedGpu, nullptr, &devExtCount, devExts.data());

    bool hasSwapchainExt = false;
    for (const auto& ext : devExts) {
        if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            hasSwapchainExt = true;
            break;
        }
    }

    if (hasSwapchainExt) {
        const char* enabledExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDeviceCreateInfo deviceCreateInfoWithExt{};
        deviceCreateInfoWithExt.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfoWithExt.queueCreateInfoCount = 1;
        deviceCreateInfoWithExt.pQueueCreateInfos = &queueInfo;
        deviceCreateInfoWithExt.enabledExtensionCount = 1;
        deviceCreateInfoWithExt.ppEnabledExtensionNames = enabledExts;

        VkDevice deviceWithExt = VK_NULL_HANDLE;
        res = vkCreateDevice(selectedGpu, &deviceCreateInfoWithExt, nullptr, &deviceWithExt);
        TEST_ASSERT(res == VK_SUCCESS && deviceWithExt != VK_NULL_HANDLE, "vkCreateDevice with VK_KHR_swapchain failed");
        std::cout << "  [PASS] Created VkDevice with VK_KHR_swapchain enabled (0x" << deviceWithExt << ")\n";

        pfnCreateSwapchain = layerGDPA(deviceWithExt, "vkCreateSwapchainKHR");
        TEST_ASSERT(pfnCreateSwapchain != nullptr, "layerGDPA must return valid pointer when extension is enabled");
        std::cout << "  [PASS] layerGDPA('vkCreateSwapchainKHR') returned valid wrapper\n";

        pfnQueuePresent = layerGDPA(deviceWithExt, "vkQueuePresentKHR");
        TEST_ASSERT(pfnQueuePresent != nullptr, "layerGDPA must return valid pointer when extension is enabled");
        std::cout << "  [PASS] layerGDPA('vkQueuePresentKHR') returned valid wrapper\n";

        // -------------------------------------------------------------------------
        // TEST 3: Queue Bookkeeping OOM & Pass-Through Fallback Regression
        // -------------------------------------------------------------------------
        std::cout << "\n--- Test 3: Queue Bookkeeping Allocation Failure Regression ---\n";
        PFN_vkGetDeviceQueue pfnLayerGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(
            layerGDPA(deviceWithExt, "vkGetDeviceQueue"));
        TEST_ASSERT(pfnLayerGetDeviceQueue != nullptr, "layerGDPA must return valid vkGetDeviceQueue");

        // Save previous NC_TEST_SIMULATE_QUEUE_OOM environment variable
        char origSimOOM[16] = {0};
        DWORD origSimOOMLen = GetEnvironmentVariableA("NC_TEST_SIMULATE_QUEUE_OOM", origSimOOM, sizeof(origSimOOM));
        bool hadOrigSimOOM = (origSimOOMLen > 0);

        // Simulate queue map allocation failure via test environment variable
        SetEnvironmentVariableA("NC_TEST_SIMULATE_QUEUE_OOM", "1");

        VkQueue queueWithOOM = VK_NULL_HANDLE;
        pfnLayerGetDeviceQueue(deviceWithExt, queueFamilyIndex, 0, &queueWithOOM);
        TEST_ASSERT(queueWithOOM != VK_NULL_HANDLE, "vkGetDeviceQueue must return valid queue handle even under map OOM");
        std::cout << "  [PASS] vkGetDeviceQueue returned valid handle under simulated OOM (queue: 0x" << queueWithOOM << ")\n";

        // Also test vkGetDeviceQueue2 if available
        PFN_vkGetDeviceQueue2 pfnLayerGetDeviceQueue2 = reinterpret_cast<PFN_vkGetDeviceQueue2>(
            layerGDPA(deviceWithExt, "vkGetDeviceQueue2"));
        if (pfnLayerGetDeviceQueue2) {
            VkDeviceQueueInfo2 qInfo2{};
            qInfo2.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
            qInfo2.queueFamilyIndex = queueFamilyIndex;
            qInfo2.queueIndex = 0;
            VkQueue q2 = VK_NULL_HANDLE;
            pfnLayerGetDeviceQueue2(deviceWithExt, &qInfo2, &q2);
            TEST_ASSERT(q2 != VK_NULL_HANDLE, "vkGetDeviceQueue2 must return valid queue handle under map OOM");
            std::cout << "  [PASS] vkGetDeviceQueue2 returned valid handle under simulated OOM (queue: 0x" << q2 << ")\n";
        }

        // Restore prior NC_TEST_SIMULATE_QUEUE_OOM state
        if (hadOrigSimOOM) {
            SetEnvironmentVariableA("NC_TEST_SIMULATE_QUEUE_OOM", origSimOOM);
        } else {
            SetEnvironmentVariableA("NC_TEST_SIMULATE_QUEUE_OOM", nullptr);
        }

        vkDestroyDevice(deviceWithExt, nullptr);
        std::cout << "  [PASS] Device with extensions destroyed cleanly\n";

        // -------------------------------------------------------------------------
        // TEST 4: Fallback Exhaustion & Device Dispatch Isolation Regression Test
        // -------------------------------------------------------------------------
        std::cout << "\n--- Test 4: Fallback Exhaustion & Device Dispatch Isolation ---\n";

        auto pfnRegisterMockDevice = reinterpret_cast<decltype(&ncTestRegisterMockDevice)>(GetProcAddress(hLayer, "ncTestRegisterMockDevice"));
        auto pfnUnregisterMockDevice = reinterpret_cast<decltype(&ncTestUnregisterMockDevice)>(GetProcAddress(hLayer, "ncTestUnregisterMockDevice"));
        auto pfnRecordFallbackQueue = reinterpret_cast<decltype(&ncTestRecordFallbackQueue)>(GetProcAddress(hLayer, "ncTestRecordFallbackQueue"));
        auto pfnRegisterMockSwapchain = reinterpret_cast<decltype(&ncTestRegisterMockSwapchain)>(GetProcAddress(hLayer, "ncTestRegisterMockSwapchain"));
        auto pfnRegisterDeviceOwnedQueue = reinterpret_cast<decltype(&ncTestRegisterDeviceOwnedQueue)>(GetProcAddress(hLayer, "ncTestRegisterDeviceOwnedQueue"));
        auto pfnRegisterDeviceOwnedSwapchain = reinterpret_cast<decltype(&ncTestRegisterDeviceOwnedSwapchain)>(GetProcAddress(hLayer, "ncTestRegisterDeviceOwnedSwapchain"));
        auto pfnGetFallbackQueueCount = reinterpret_cast<decltype(&ncTestGetFallbackQueueCount)>(GetProcAddress(hLayer, "ncTestGetFallbackQueueCount"));
        auto pfnGetFallbackSwapchainCount = reinterpret_cast<decltype(&ncTestGetFallbackSwapchainCount)>(GetProcAddress(hLayer, "ncTestGetFallbackSwapchainCount"));
        auto pfnResetFallbackState = reinterpret_cast<decltype(&ncTestResetFallbackState)>(GetProcAddress(hLayer, "ncTestResetFallbackState"));
        auto pfnGetPresentWrapper = reinterpret_cast<decltype(&ncTestGetPresentWrapper)>(GetProcAddress(hLayer, "ncTestGetPresentWrapper"));

        TEST_ASSERT(pfnRegisterMockDevice && pfnUnregisterMockDevice && pfnRecordFallbackQueue &&
                    pfnRegisterMockSwapchain && pfnRegisterDeviceOwnedQueue && pfnRegisterDeviceOwnedSwapchain &&
                    pfnGetFallbackQueueCount && pfnGetFallbackSwapchainCount && pfnResetFallbackState &&
                    pfnGetPresentWrapper, "All test hooks must be exported and resolvable from layer DLL");

        PFN_vkQueuePresentKHR layerPresent = pfnGetPresentWrapper();
        TEST_ASSERT(layerPresent != nullptr, "Layer presentation wrapper must be non-null");

        VkDevice mockDevA = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(0xA000));
        VkDevice mockDevB = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(0xB000));

        TEST_ASSERT(pfnRegisterMockDevice(mockDevA, MockPresentA, nullptr), "Register mock device A");
        TEST_ASSERT(pfnRegisterMockDevice(mockDevB, MockPresentB, nullptr), "Register mock device B");

        // Part A: Two distinct mocked device dispatch tables under allocation failure
        VkQueue queueA = reinterpret_cast<VkQueue>(static_cast<uintptr_t>(0x101));
        VkQueue queueB = reinterpret_cast<VkQueue>(static_cast<uintptr_t>(0x201));

        TEST_ASSERT(pfnRecordFallbackQueue(queueA, mockDevA), "Record fallback queue for device A");
        TEST_ASSERT(pfnRecordFallbackQueue(queueB, mockDevB), "Record fallback queue for device B");

        VkPresentInfoKHR emptyPresentInfo{};
        emptyPresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        VkResult resA = layerPresent(queueA, &emptyPresentInfo);
        TEST_ASSERT(resA == VK_SUCCESS, "Present queue A should succeed");
        TEST_ASSERT(g_mockPresentACalls == 1 && g_mockPresentBCalls == 0, "Queue A must dispatch exclusively to Device A");
        TEST_ASSERT(g_lastQueueSeenA == queueA, "Device A must receive queue A");

        VkResult resB = layerPresent(queueB, &emptyPresentInfo);
        TEST_ASSERT(resB == VK_SUCCESS, "Present queue B should succeed");
        TEST_ASSERT(g_mockPresentACalls == 1 && g_mockPresentBCalls == 1, "Queue B must dispatch exclusively to Device B");
        TEST_ASSERT(g_lastQueueSeenB == queueB, "Device B must receive queue B");
        std::cout << "  [PASS] Distinct mocked device dispatch tables verified under allocation failure\n";

        // Part B: Fallback table exhaustion
        pfnResetFallbackState();
        TEST_ASSERT(pfnGetFallbackQueueCount() == 0, "Fallback queue table reset");
        TEST_ASSERT(pfnGetFallbackSwapchainCount() == 0, "Fallback swapchain table reset");

        // Fill all 128 fallback entries for Device A
        for (size_t i = 0; i < 128; ++i) {
            VkQueue q = reinterpret_cast<VkQueue>(static_cast<uintptr_t>(0x5000 + i));
            TEST_ASSERT(pfnRecordFallbackQueue(q, mockDevA), "Fill fallback queue entry");
        }
        TEST_ASSERT(pfnGetFallbackQueueCount() == 128, "Fallback queue table must be full (128 entries)");

        // Fill all 64 fallback swapchain entries for Device A
        for (size_t i = 0; i < 64; ++i) {
            VkSwapchainKHR sc = reinterpret_cast<VkSwapchainKHR>(static_cast<uintptr_t>(0x6000 + i));
            TEST_ASSERT(pfnRegisterMockSwapchain(sc, mockDevA, /*useFallbackTable=*/true), "Fill fallback swapchain entry");
        }
        TEST_ASSERT(pfnGetFallbackSwapchainCount() == 64, "Fallback swapchain table must be full (64 entries)");

        // Attempt to add a 129th queue (must fail to fit into fixed table)
        VkQueue queue129 = reinterpret_cast<VkQueue>(static_cast<uintptr_t>(0x9999));
        bool recQ129 = pfnRecordFallbackQueue(queue129, mockDevA);
        TEST_ASSERT(!recQ129, "129th queue must fail to record into full fallback table");
        TEST_ASSERT(pfnGetFallbackQueueCount() == 128, "Fallback table must remain capped at 128");

        // Attempt to add a 65th swapchain (must fail to fit into fixed table)
        VkSwapchainKHR sc65 = reinterpret_cast<VkSwapchainKHR>(static_cast<uintptr_t>(0x9998));
        bool recSc65 = pfnRegisterMockSwapchain(sc65, mockDevA, /*useFallbackTable=*/true);
        TEST_ASSERT(!recSc65, "65th swapchain must fail to record into full fallback table");
        TEST_ASSERT(pfnGetFallbackSwapchainCount() == 64, "Fallback swapchain table must remain capped at 64");

        // Subcase B.1: 129th untracked queue presented with NO swapchain
        // Must be rejected with VK_ERROR_INITIALIZATION_FAILED without cross-device guessing
        int preCallsA = g_mockPresentACalls;
        int preCallsB = g_mockPresentBCalls;
        VkResult resExhausted = layerPresent(queue129, &emptyPresentInfo);
        TEST_ASSERT(resExhausted == VK_ERROR_INITIALIZATION_FAILED,
                    "Untracked queue on exhausted fallback without swapchain must fail with VK_ERROR_INITIALIZATION_FAILED");
        TEST_ASSERT(g_mockPresentACalls == preCallsA && g_mockPresentBCalls == preCallsB,
                    "Exhausted untracked queue must not dispatch to any device (no guessing)");
        std::cout << "  [PASS] Fallback exhaustion: untracked queue safely rejected without guessing\n";

        // Subcase B.2: Dual fallback exhaustion with a valid owned queue/swapchain pair
        // Proves uninterrupted pass-through forwarding when both fixed fallback tables are completely full
        VkQueue validQueueB = reinterpret_cast<VkQueue>(static_cast<uintptr_t>(0x8001));
        VkSwapchainKHR validScB = reinterpret_cast<VkSwapchainKHR>(static_cast<uintptr_t>(0x8002));
        TEST_ASSERT(pfnRegisterDeviceOwnedQueue(mockDevB, validQueueB), "Register owned queue on Device B");
        TEST_ASSERT(pfnRegisterDeviceOwnedSwapchain(mockDevB, validScB), "Register owned swapchain on Device B");

        VkPresentInfoKHR dualExhaustedInfo{};
        dualExhaustedInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        dualExhaustedInfo.swapchainCount = 1;
        dualExhaustedInfo.pSwapchains = &validScB;

        VkResult resDual = layerPresent(validQueueB, &dualExhaustedInfo);
        TEST_ASSERT(resDual == VK_SUCCESS, "Present with valid owned queue/swapchain pair under dual exhaustion must succeed");
        TEST_ASSERT(g_mockPresentBCalls == preCallsB + 1, "Must dispatch strictly to Device B");
        TEST_ASSERT(g_mockPresentACalls == preCallsA, "Must NOT dispatch to Device A");
        TEST_ASSERT(g_lastQueueSeenB == validQueueB, "Device B must receive validQueueB");
        std::cout << "  [PASS] Dual fallback exhaustion: valid owned queue/swapchain pair uninterrupted pass-through verified\n";

        // Teardown mock devices
        pfnUnregisterMockDevice(mockDevA);
        pfnUnregisterMockDevice(mockDevB);
        pfnResetFallbackState();
        std::cout << "  [PASS] Mock devices and fallback state cleaned up cleanly\n";
    } else {
        std::cout << "  [SKIP] Physical device does not support VK_KHR_swapchain; skipping enabled test\n";
    }

    // 4. Clean Instance Teardown
    vkDestroyInstance(instance, nullptr);
    std::cout << "\n  [PASS] Vulkan instance cleanly destroyed\n";
    std::cout << "=================================================================\n";
    std::cout << " ALL DISPATCH & QUERY TESTS PASSED!\n";
    std::cout << "=================================================================\n";
    return 0;
}
