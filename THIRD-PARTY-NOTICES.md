# Third-party attribution and incorporation record

Status: updated 2026-10-15 (Implementation Task 1: Explicit Vulkan Layer Pass-Through).

## Incorporated components

### CPU fixture header provenance (Issue #11)

The human-approved `layer-cpu-v1` workflow reads official Vulkan-Headers commit [`6802bb4733b63ed5efd3adb308a6c885ef180ea1`](https://github.com/KhronosGroup/Vulkan-Headers/tree/6802bb4733b63ed5efd3adb308a6c885ef180ea1), tagged `vulkan-sdk-1.4.363.0`, into a disposable CI workspace. Approval was granted on 2026-10-22; the workflow is active. This is the existing Vulkan header dependency provisioned for compilation, not a new runtime/provider, implementation import, vendored directory or binary distribution. No upstream header or source is copied into project-authored files; the original checkout retains its licence files and per-file notices. Tests use upstream type declarations directly rather than copied ABI definitions.

Reviewed primary records at that commit: [`LICENSE.md`](https://github.com/KhronosGroup/Vulkan-Headers/blob/6802bb4733b63ed5efd3adb308a6c885ef180ea1/LICENSE.md) and `include/vulkan/{vulkan.h,vulkan_core.h,vk_platform.h,vk_layer.h}`. All four inspected headers declare `Apache-2.0 OR MIT`. The first two identify Copyright 2015–2026 The Khronos Group Inc.; vk_platform.h identifies Copyright 2014–2026 The Khronos Group Inc.; vk_layer.h identifies Copyright 2015–2023 The Khronos Group Inc., Valve Corporation and LunarG, Inc. The repository licence record classifies its files under Apache-2.0/MIT; build-only includes remain under their own notices. No upstream implementation/example script is executed or adapted.

Existing dual-licence texts and requirements below continue to apply. The approved profile uploads neither headers nor compiled binaries; future packaging still requires reconciling actual outputs/transitive notices and cannot rely on a URL alone. API-contract references inform original code, not endorsement or permission to reuse unrelated components. The fixture/repair is original project material; existing MIT terms do not relicense SDK headers or specifications.

None. No third-party implementation code, vendor DLL, model weights, shader, pre-compiled binary, or release asset has been incorporated or distributed by this repository.

The Task 1 explicit Vulkan pass-through layer (`src/layer/`), test runner (`tests/test_layer_dispatch.cpp`), and validation launcher (`scripts/run_sample_validation.ps1`) consist entirely of original, independently authored project material written to conform with the public Khronos Vulkan Loader-Layer Interface specification (`LoaderLayerInterface.md`) and standard Vulkan headers (`vulkan/vulkan.h`, `vulkan/vk_layer.h`). The layer DLL and test binaries compile against headers provided by the external LunarG Vulkan SDK (`1.4.363.0`) and resolve dispatch dynamically via downstream function pointers without linking vendor binaries or static loader libraries into the layer.

The repository's MIT licence applies to project-authored material. It does not relicense third-party specifications or external SDK components.

## Build-time Header Licence and Provenance Record

The project build system references header files from the Khronos Vulkan API registry and LunarG Vulkan SDK during compilation. These headers are not vendored or redistributed in this repository; they are located on the developer's system via the `VULKAN_SDK` environment variable or standard CMake package resolution (`find_package(Vulkan)`).

### Khronos Vulkan Headers: Exact Per-File Provenance and Copyright Record

The installed SDK headers inspected in the local build environment (`1.4.363.0`) contain the following exact per-file copyright and provenance notices:

#### 1. `vulkan/vulkan.h`
- **Origin:** The Khronos Group Inc. (https://github.com/KhronosGroup/Vulkan-Headers)
- **Copyright:** Copyright 2015-2026 The Khronos Group Inc.
- **Licence Terms:** Apache License, Version 2.0 OR MIT (SPDX-License-Identifier: Apache-2.0 OR MIT)

#### 2. `vulkan/vulkan_core.h`
- **Origin:** The Khronos Group Inc. (XML API Registry generation)
- **Copyright:** Copyright 2015-2026 The Khronos Group Inc.
- **Licence Terms:** Apache License, Version 2.0 OR MIT (SPDX-License-Identifier: Apache-2.0 OR MIT)

#### 3. `vulkan/vk_platform.h`
- **Origin:** The Khronos Group Inc.
- **Copyright:** Copyright 2014-2026 The Khronos Group Inc.
- **Licence Terms:** Apache License, Version 2.0 OR MIT (SPDX-License-Identifier: Apache-2.0 OR MIT)

#### 4. `vulkan/vk_layer.h`
- **Origin:** The Khronos Group Inc., Valve Corporation, and LunarG, Inc.
- **Copyright:**
  - Copyright 2015-2023 The Khronos Group Inc.
  - Copyright 2015-2023 Valve Corporation
  - Copyright 2015-2023 LunarG, Inc.
- **Licence Terms:** Apache License, Version 2.0 OR MIT (SPDX-License-Identifier: Apache-2.0 OR MIT)

---

### Dual-Licence Texts (Apache-2.0 OR MIT)

#### Apache License, Version 2.0 Notice:

```
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

#### MIT Licence Notice:

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Research and conceptual references

The planning documentation acknowledges related work in [docs/RESEARCH.md](docs/RESEARCH.md). Pinned upstream licence findings are in [docs/LICENSE-REVIEW.md](docs/LICENSE-REVIEW.md).

The current design discussion references DXVK's API translation, Vulkan layers, feeder/bridge input preparation, external capture tools, and provider separation. These are conceptual references summarized in original wording, not claims that their code has been adopted or that we originated their work.

The initial experiment planning also credits Intel Open Image Denoise and its trained-weights project for documented neural denoising/provider capabilities; Xintao Wang's Real-ESRGAN and Vulkan port, nihui's inherited port work, and Tencent ncnn for restoration/Vulkan inference references; and vkQuake/QuakeSpasm/id Software for the proposed independent Vulkan test host. These are documentation-based research credits, not incorporation or endorsement. Exact source revisions and limits are in [the licence register](docs/LICENSE-REVIEW.md) and [experiment record](docs/INITIAL-EXPERIMENTS.md).

No research citation grants rights to copy source, documentation, screenshots, binaries, or models. No endorsement is implied.

## Required future entries

When work is actually used, record its project/author, immutable source revision and files, our destination, use type, modifications, governing terms, and retained notice locations in the same change. See [source review rules](docs/SOURCE-REVIEW.md).

Include the full required licence/copyright/NOTICE material with any future source and binary distributions. Keep conceptual credits separate from legally required incorporated-component notices. Do not mark a component incorporated merely because it was evaluated.
