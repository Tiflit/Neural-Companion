#include <vulkan/vulkan.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

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

int main() {
    std::cout << "=================================================================\n";
    std::cout << " Neural-Companion: Layer Dispatch & Query Semantics Test\n";
    std::cout << "=================================================================\n";

    // 1. Initialize Vulkan Instance
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

    VkInstance instance = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&instanceInfo, nullptr, &instance);
    TEST_ASSERT(res == VK_SUCCESS && instance != VK_NULL_HANDLE, "vkCreateInstance failed");
    std::cout << "  [PASS] Vulkan instance created successfully (0x" << instance << ")\n";

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

        vkDestroyDevice(deviceWithExt, nullptr);
        std::cout << "  [PASS] Device with extensions destroyed cleanly\n";
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
