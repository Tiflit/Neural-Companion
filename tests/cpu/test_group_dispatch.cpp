#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
const char* failureStage = "setup";
void Require(bool condition, const char* stage) {
    if (!condition) { failureStage = stage; throw std::runtime_error("fixture assertion"); }
}

VkInstance Instance(int owner) {
    return reinterpret_cast<VkInstance>(static_cast<uintptr_t>(0x10000 + owner * 0x10000));
}
VkPhysicalDevice Physical(int owner, int index) {
    return reinterpret_cast<VkPhysicalDevice>(static_cast<uintptr_t>(0x1000 + owner * 0x100 + index));
}
int Owner(VkInstance instance) { return instance == Instance(0) ? 0 : instance == Instance(1) ? 1 : -1; }

bool coreAvailable[2]{}, khrAvailable[2]{};
int creatingOwner = 0, coreCalls = 0, khrCalls = 0, createDeviceCalls = 0;
int lastDeviceLookupOwner = -1, destroyCalls = 0;
VkInstance observedInstance = VK_NULL_HANDLE;
uint32_t* observedCount = nullptr;
VkPhysicalDeviceGroupProperties* observedProperties = nullptr;
VkResult enumerationResult = VK_SUCCESS;

VKAPI_ATTR VkResult VKAPI_CALL MockCreateInstance(const VkInstanceCreateInfo*,
    const VkAllocationCallbacks*, VkInstance* instance) {
    *instance = Instance(creatingOwner);
    return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL MockDestroyInstance(VkInstance, const VkAllocationCallbacks*) { ++destroyCalls; }
VKAPI_ATTR VkResult VKAPI_CALL MockCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo*,
    const VkAllocationCallbacks*, VkDevice* device) {
    ++createDeviceCalls;
    *device = VK_NULL_HANDLE;
    // Do not create even a mock logical device: this result also verifies forwarding.
    return VK_ERROR_FEATURE_NOT_PRESENT;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL MockGdpa(VkDevice, const char*) { return nullptr; }

VkResult Enumerate(VkInstance instance, uint32_t* count, VkPhysicalDeviceGroupProperties* properties) {
    observedInstance = instance; observedCount = count; observedProperties = properties;
    if (enumerationResult != VK_SUCCESS && enumerationResult != VK_INCOMPLETE) return enumerationResult;
    if (properties && *count > 0) {
        properties[0].physicalDeviceCount = 2;
        properties[0].physicalDevices[0] = Physical(Owner(instance), 0);
        properties[0].physicalDevices[1] = Physical(Owner(instance), 1);
        properties[0].subsetAllocation = VK_TRUE;
    }
    *count = 1;
    return enumerationResult;
}
VKAPI_ATTR VkResult VKAPI_CALL MockCore(VkInstance instance, uint32_t* count, VkPhysicalDeviceGroupProperties* properties) {
    ++coreCalls; return Enumerate(instance, count, properties);
}
VKAPI_ATTR VkResult VKAPI_CALL MockKhr(VkInstance instance, uint32_t* count, VkPhysicalDeviceGroupProperties* properties) {
    ++khrCalls; return Enumerate(instance, count, properties);
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL MockGipa(VkInstance instance, const char* name) {
    if (std::strcmp(name, "vkCreateInstance") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(MockCreateInstance);
    const int owner = Owner(instance);
    if (owner < 0) return nullptr;
    if (std::strcmp(name, "vkDestroyInstance") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(MockDestroyInstance);
    if (std::strcmp(name, "vkCreateDevice") == 0) {
        lastDeviceLookupOwner = owner;
        return reinterpret_cast<PFN_vkVoidFunction>(MockCreateDevice);
    }
    if (std::strcmp(name, "vkEnumeratePhysicalDeviceGroups") == 0 && coreAvailable[owner])
        return reinterpret_cast<PFN_vkVoidFunction>(MockCore);
    if (std::strcmp(name, "vkEnumeratePhysicalDeviceGroupsKHR") == 0 && khrAvailable[owner])
        return reinterpret_cast<PFN_vkVoidFunction>(MockKhr);
    return nullptr;
}

struct Fixture {
    HMODULE module = nullptr;
    PFN_vkGetInstanceProcAddr gipa = nullptr;
    bool live[2]{};
    explicit Fixture(const char* dll) {
        Require(SetEnvironmentVariableA("NC_LOG_PATH", "group-dispatch-fixture.log") != 0, "log setup");
        module = LoadLibraryA(dll); // Only the built project DLL; never vulkan-1.dll.
        Require(module != nullptr, "project DLL load");
        gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(module, "vkGetInstanceProcAddr"));
        Require(gipa != nullptr, "project GIPA export");
    }
    void Create(int owner) {
        creatingOwner = owner;
        VkLayerInstanceLink link{}; link.pfnNextGetInstanceProcAddr = MockGipa;
        VkLayerInstanceCreateInfo chain{};
        chain.sType = VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO;
        chain.function = VK_LAYER_LINK_INFO; chain.u.pLayerInfo = &link;
        VkInstanceCreateInfo info{}; info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; info.pNext = &chain;
        VkInstance instance = VK_NULL_HANDLE;
        const auto create = reinterpret_cast<PFN_vkCreateInstance>(gipa(VK_NULL_HANDLE, "vkCreateInstance"));
        Require(create != nullptr && create(&info, nullptr, &instance) == VK_SUCCESS, "mock instance creation");
        Require(instance == Instance(owner), "instance output forwarding");
        live[owner] = true;
    }
    void Destroy(int owner) {
        if (!live[owner]) return;
        const auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(gipa(Instance(owner), "vkDestroyInstance"));
        if (destroy) destroy(Instance(owner), nullptr);
        live[owner] = false;
    }
    VkResult CreateFromPhysical(int queryOwner, VkPhysicalDevice physical) {
        VkLayerDeviceLink link{}; link.pfnNextGetInstanceProcAddr = MockGipa; link.pfnNextGetDeviceProcAddr = MockGdpa;
        VkLayerDeviceCreateInfo chain{}; chain.sType = VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO;
        chain.function = VK_LAYER_LINK_INFO; chain.u.pLayerInfo = &link;
        VkDeviceCreateInfo info{}; info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; info.pNext = &chain;
        VkDevice device = VK_NULL_HANDLE;
        lastDeviceLookupOwner = -1;
        const auto create = reinterpret_cast<PFN_vkCreateDevice>(gipa(Instance(queryOwner), "vkCreateDevice"));
        lastDeviceLookupOwner = -1; // Ignore the query itself; inspect the wrapper's downstream lookup.
        Require(create != nullptr, "create device wrapper availability");
        return create(physical, &info, nullptr, &device);
    }
    ~Fixture() { if (gipa) { Destroy(0); Destroy(1); } if (module) FreeLibrary(module); }
};

void CheckEnumeration(Fixture& fixture, int owner, bool khr, bool countOnly, VkResult expected) {
    enumerationResult = expected;
    const int oldCore = coreCalls, oldKhr = khrCalls;
    const auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDeviceGroups>(
        fixture.gipa(Instance(owner), khr ? "vkEnumeratePhysicalDeviceGroupsKHR" : "vkEnumeratePhysicalDeviceGroups"));
    Require(enumerate != nullptr, "group wrapper availability");
    VkPhysicalDeviceGroupProperties groups[2]{};
    groups[0].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES;
    groups[0].pNext = &groups[1]; // Opaque to the mock and layer, used only as an identity sentinel.
    groups[0].physicalDeviceCount = 2;
    groups[0].physicalDevices[0] = Physical(owner, 0);
    groups[0].physicalDevices[1] = Physical(owner, 1);
    groups[1].physicalDeviceCount = 1; groups[1].physicalDevices[0] = Physical(owner, 2);
    uint32_t count = 2;
    auto* output = countOnly ? nullptr : groups;
    Require(enumerate(Instance(owner), &count, output) == expected, "result forwarding");
    Require(observedInstance == Instance(owner) && observedCount == &count && observedProperties == output, "argument identity");
    Require(coreCalls == oldCore + (khr ? 0 : 1) && khrCalls == oldKhr + (khr ? 1 : 0), "matching alias callback");
    const bool succeeded = expected == VK_SUCCESS || expected == VK_INCOMPLETE;
    Require(count == (succeeded ? 1u : 2u), "returned count");
    Require(groups[0].sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES
        && groups[0].pNext == &groups[1], "caller structure identity");
    for (int index = 0; index < 2; ++index) {
        const int calls = createDeviceCalls;
        const auto result = fixture.CreateFromPhysical(owner, Physical(owner, index));
        if (succeeded && !countOnly) {
            Require(result == VK_ERROR_FEATURE_NOT_PRESENT && createDeviceCalls == calls + 1
                && lastDeviceLookupOwner == owner, "physical group instance association");
        } else {
            // Two live instances prevent the existing single-instance fallback masking missing mappings.
            Require(result == VK_ERROR_INITIALIZATION_FAILED && createDeviceCalls == calls, "no mapping on count or error");
        }
    }
    if (!countOnly) {
        Require(fixture.CreateFromPhysical(owner, Physical(owner, 2)) == VK_ERROR_INITIALIZATION_FAILED,
            "ignore unwritten array tail");
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        const std::string name = argv[2];
        coreAvailable[0] = name != "KhrOnly" && name != "Neither";
        khrAvailable[0] = name != "CoreOnly" && name != "Neither";
        coreAvailable[1] = khrAvailable[1] = true;
        Fixture fixture(argv[1]);
        Require(fixture.gipa(VK_NULL_HANDLE, "vkEnumeratePhysicalDeviceGroups") == nullptr
            && fixture.gipa(VK_NULL_HANDLE, "vkEnumeratePhysicalDeviceGroupsKHR") == nullptr, "null instance queries");
        fixture.Create(0); fixture.Create(1);
        Require((fixture.gipa(Instance(0), "vkEnumeratePhysicalDeviceGroups") != nullptr) == coreAvailable[0], "core gating");
        Require((fixture.gipa(Instance(0), "vkEnumeratePhysicalDeviceGroupsKHR") != nullptr) == khrAvailable[0], "KHR gating");
        if (name != "Neither" && name != "NullQueries") {
            const bool khr = name == "KhrOnly" || name == "BothKhr" || name == "KhrCount"
                || name == "KhrIncomplete" || name == "KhrError";
            const bool countOnly = name == "CoreCount" || name == "KhrCount";
            const auto result = name == "CoreIncomplete" || name == "KhrIncomplete" ? VK_INCOMPLETE
                : name == "CoreError" || name == "KhrError" ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
            CheckEnumeration(fixture, 0, khr, countOnly, result);
            if (name == "InstanceIsolation") CheckEnumeration(fixture, 1, true, false, VK_SUCCESS);
            if (name == "Cleanup") {
                fixture.Destroy(0);
                Require(destroyCalls == 1, "first instance cleanup");
                // Remaining instance has a positive mapping, so no negative single-instance assertion here.
                enumerationResult = VK_SUCCESS;
                uint32_t count = 0;
                const auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDeviceGroups>(
                    fixture.gipa(Instance(1), "vkEnumeratePhysicalDeviceGroupsKHR"));
                Require(enumerate && enumerate(Instance(1), &count, nullptr) == VK_SUCCESS, "remaining instance dispatch");
            }
        }
        fixture.Destroy(0); fixture.Destroy(1);
        Require(destroyCalls == 2, "both downstream destroys");
        std::cout << "PASS\n";
        return 0;
    } catch (...) {
        std::cerr << "FAIL: " << failureStage << '\n';
        return 1;
    }
}
