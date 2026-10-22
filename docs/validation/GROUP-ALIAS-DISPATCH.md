# Physical-device-group aliases — Issue #11

## Defect and repair

The previous layer queried both group-enumeration names for availability but cached and called only `vkEnumeratePhysicalDeviceGroups`. When a downstream Vulkan 1.0 extension implementation exposed only `vkEnumeratePhysicalDeviceGroupsKHR`, the layer returned its wrapper and then failed initialization because the core pointer was null.

[Khronos group enumeration](https://docs.vulkan.org/refpages/latest/refpages/source/vkEnumeratePhysicalDeviceGroups.html) defines equivalent core 1.1 and KHR extension commands. [Instance function lookup](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetInstanceProcAddr.html) still has version/extension availability rules: equivalence does not justify advertising an unavailable name.

Cache each downstream entry independently. Distinct thin wrappers select their corresponding cached pointer, then share the existing result forwarding and physical-device mapping code. Existing downstream-first function lookup remains the availability authority. No cross-alias fallback, extension enablement, public export, ownership/synchronization redesign or provider/effect is added. Success/incomplete enumeration still records returned physical devices; count-only/error paths do not. Instance destruction keeps existing map cleanup.

## Prepared CPU fixture

The standalone `tests/cpu` CMake project builds the actual production `layer.cpp` and existing export definition as a DLL, then loads only that project DLL. Synthetic loader links supply every downstream function. No `vulkan-1` link/load, driver query, physical device, logical device, queue, surface, presentation or GPU execution occurs. Mock logical-device creation deliberately returns a fixed error so instance association can be checked without device creation. Two live mock instances prevent the layer's existing single-instance fallback from masking missing physical-device mappings.

Fourteen prepared cases: CoreOnly, KhrOnly, BothCore, BothKhr, Neither, CoreCount, KhrCount, CoreIncomplete, KhrIncomplete, CoreError, KhrError, InstanceIsolation, Cleanup and NullQueries. They check name gating, distinct callbacks, pointer/count/result forwarding, returned physical-device association, ignored unwritten array tails, count/error non-mapping and downstream instance cleanup. Mock structure sentinels are opaque to the fixture; this is not an actual loader/device conformance run. Test output uses fixed case/stage labels only. DLL diagnostics contain synthetic handles in a runner-local file; they are not uploaded.

**Status: prepared, NOT RUN.** Source/diff self-review only initially. The already approved diagnostic-path profile may still run its eight existing environment tests, which do not compile or verify this dispatch change. Existing root CMake, GPU harness, README/continuation and research PR #7 remain untouched. No local builds/tests/apps or games are authorized here.

## Inactive proposal: layer-cpu-v1

`.github/workflows/layer-cpu-tests.yml.proposed` is inert. Human approval is requested before copying it to the already scoped active `.yml` path. It extends the prior CPU-only setup with an official public build-time header checkout and real project-DLL/mock compilation. Exact source: [Vulkan-Headers 6802bb4](https://github.com/KhronosGroup/Vulkan-Headers/tree/6802bb4733b63ed5efd3adb308a6c885ef180ea1), SDK tag `vulkan-sdk-1.4.363.0`; provenance/license record is in THIRD-PARTY-NOTICES.md. No header is vendored into this repository or packaged for release.

One Windows-2022 hosted job per relevant PR/main change; five-minute job timeout; one build/test worker; ten-second per-case timeout. Official checkout action pinned, token contents:read and not persisted. Preinstalled MSVC/CMake/Windows SDK, no SDK installer/package manager, external executable, Vulkan loader/driver, model/game, host inventory, secret, cache or artifact upload. The runner may write build outputs and synthetic fixture diagnostics only in its disposable workspace. No extra RAM hard limit is claimed. No automatic unchanged-head retry; preserve any failure evidence before routine in-profile repairs.

```text
cmake -S tests/cpu -B build/cpu -A x64 -DNC_VULKAN_HEADERS=<runner-workspace>/.ci/vulkan-headers
cmake --build build/cpu --config Debug --parallel 1
ctest --test-dir build/cpu -C Debug --parallel 1 --output-on-failure
```

Approval would activate this narrowly filtered CI profile, including the pinned public header fetch. Passing would establish bounded mock dispatch and production-source compilation, not real Vulkan-loader activation, driver behavior, game compatibility or synchronization safety. Existing device/game/profile-isolation holds remain; human alone merges.
