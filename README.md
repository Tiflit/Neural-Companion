# Neural-Companion

An independent project for applying neural visual enhancement across games, using the best rendering information each game makes available.

**Status: planning draft. No runtime, installer, supported backend, or verified game/GPU compatibility is implemented yet.** Broad compatibility is the goal, not a current guarantee.

## Purpose

Make older and newer games visually richer with a reusable configuration and minimal per-game tuning. Neural-Companion will prepare rendering inputs for existing enhancement models and runtimes, using them as supplied by their providers.

The initial architectural direction is Vulkan interception, including compatible Direct3D games translated through DXVK. External capture is a proposed fallback. NVIDIA, AMD, and Intel hardware are in the long-term scope, subject to each backend's actual hardware, API, and licensing requirements.

## Project boundaries

- **Neural-Companion:** rendering-resource access, input recovery and estimation, temporal handling, UI protection, enhancement integration, presets, and diagnostics.
- **[DXVK-Companion](https://github.com/Tiflit/DXVK-Companion):** its own compatibility and automation project. Optional future integration may manage Neural-Companion, but neither project should require the other.
- **Model/runtime providers:** supply the enhancement technology. Neural-Companion will not train, reverse engineer, extract weights from, patch, or modify their models or proprietary runtimes.

DLSS 5 neural rendering is an initial integration candidate, not a dependency selected for implementation. An unmodified NVIDIA DLL does not imply AMD or Intel compatibility. No cross-vendor DLSS 5 execution path has been approved.

## Design goals

1. Use genuine game inputs where available; reconstruct or estimate missing information where justified.
2. Distinguish native, reconstructed, estimated, and unavailable inputs in diagnostics.
3. Prefer one reusable visual preset, with automatic technical adaptation and optional exceptions.
4. Preserve frame continuity and UI readability; reject uncertain history rather than presenting estimates as native data.
5. Keep unsupported paths understandable and allow the original image to pass through.
6. Keep proprietary components outside the repository and release packages unless specific redistribution rights are established.

## Documentation

| Document | Contents |
|---|---|
| [Project scope](docs/PROJECT.md) | Goals, boundaries, user experience, and success criteria |
| [Architecture proposal](docs/ARCHITECTURE.md) | Capture routes, input preparation, providers, and integration boundaries |
| [Roadmap and validation](docs/ROADMAP.md) | Evidence gates, experiments, and implementation sequence |
| [Initial experiment candidates](docs/INITIAL-EXPERIMENTS.md) | Provisional test hosts, provider shortlist, and evidence required before implementation |
| [Related projects](docs/RESEARCH.md) | Inspiration, reuse candidates, source limitations, and open investigations |
| [Component policy](docs/COMPONENT-POLICY.md) | Unmodified models, proprietary files, provenance, and licensing review |
| [Source review rules](docs/SOURCE-REVIEW.md) | Required licence checks, provenance, and attribution before code reuse |
| [Licence review register](docs/LICENSE-REVIEW.md) | Pinned findings and blocked/unapproved sources |
| [Third-party notices](THIRD-PARTY-NOTICES.md) | Actual incorporation record and conceptual credits |
| [Decisions and open questions](docs/DECISIONS.md) | Established direction and choices still to resolve |

The proposed validation sequence is unchanged pass-through, an obvious deterministic reference effect, then a small legally usable neural provider. Framework compatibility and each provider's hardware support will be measured separately; a reference effect is not neural rendering, and a restoration model is not proof of neural relighting.

These documents are an initial planning baseline. Architecture details are proposals unless explicitly identified as established direction. There are no release dates or performance promises.

## Third-party work

Before using another repository to guide implementation, inspect its licence and record the intended use. Preserve required notices for any copied/adapted material, and cite material conceptual influence. Credit never substitutes for permission. No third-party implementation has been adopted; see the [source review rules](docs/SOURCE-REVIEW.md).

## Building and Testing (Task 1: Vulkan Layer Pass-Through)

### Prerequisites

- Windows 10/11 x64
- CMake 3.20 or newer
- Visual Studio 2022 (MSVC with C++17 support)
- LunarG Vulkan SDK (version 1.3 or 1.4) with the `VULKAN_SDK` environment variable pointing to the SDK installation directory (e.g. `C:\VulkanSDK\<version>`)

### Build Instructions

1. Configure the build directory:
   ```powershell
   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
   ```
   *Note: If `VULKAN_SDK` is not set in your environment, pass it explicitly with `-DVULKAN_SDK="<path-to-sdk>"`, or let CMake locate Vulkan via `find_package(Vulkan)`.*

2. Compile the layer shared library and dispatch test binary:
   ```powershell
   cmake --build build --config Release
   ```
   Build products (`VkLayer_neural_companion_passthrough.dll`, `VkLayer_neural_companion_passthrough.json`, and `test_layer_dispatch.exe`) are placed in `build/bin/Release/` and excluded from source control.

### Running the Validation Harness

The repository includes an automated validation script (`scripts/run_sample_validation.ps1`) that executes:
1. Pre-flight layer discovery and manifest validation.
2. Parser unit and negative acceptance tests (`tests/test_harness_parsers.ps1`) verifying loader insertion checks, diagnostic parsing of warnings without VUIDs, integer exit code enforcement, and baseline leakage detection.
3. Focused unit tests verifying layer dispatch, query semantics, extension-disabled null returns, unhandled forwarding, and simulated queue bookkeeping allocation failure fallback (`test_layer_dispatch.exe`).
4. Co-activated sample host execution with `VK_LAYER_KHRONOS_validation` on `vkcube.exe`, verifying:
   - Simultaneous loader discovery and insertion of both explicit layers.
   - Zero validation errors or warnings emitted.
   - Dynamic window resizing, swapchain recreation, and balanced swapchain lifecycle (`created == destroyed`).
   - Exact requested presentation call counts forwarded downstream.
5. Live host queue bookkeeping OOM regression test (`vkcube.exe` with `NC_TEST_SIMULATE_QUEUE_OOM=1`), verifying uninterrupted pass-through presentation via non-allocating fallback table.
6. Baseline run verifying clean deactivation and host operation with Neural-Companion disabled.

To run the full validation suite:
```powershell
powershell -ExecutionPolicy Bypass -File scripts/run_sample_validation.ps1
```

Optional parameters:
- `-BuildDir <path>`: Output directory containing build artifacts (default: repository-relative `build\bin\Release`)
- `-SdkDir <path>`: Root directory of LunarG Vulkan SDK (default: `$env:VULKAN_SDK`)
- `-FrameCount <n>`: Target number of presentation calls for the sample run (default: `180`)
- `-IncludeResizeTest` / `-IncludeResizeTest:$false`: Enable or disable window resize recreation testing (default: `$true`)
- `-LogFile <path>`: Dedicated layer diagnostic log destination (default: `build\neural_companion_passthrough.log`)
- `-HostLogFile <path>`: Host stdout/stderr and loader debug diagnostics log (default: `build\vkcube_host.log`)
- `-BaselineLogFile <path>`: Baseline run diagnostics log (default: `build\vkcube_baseline.log`)

## Development workflow

The owner's multi-agent workflow template is still being finalized. It will be applied separately. This draft does not introduce agent roles, an AGENTS.md, execution authorization profiles, CI policy, or a replacement workflow.

## License

Project-authored material is covered by the repository's [MIT License](LICENSE). Third-party software, SDKs, models, and runtimes retain their own terms. This project is independent and is not endorsed by NVIDIA, AMD, Intel, or the DXVK maintainers.
