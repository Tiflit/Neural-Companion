# Third-party attribution and incorporation record

Status: updated 2026-10-15 (Implementation Task 1: Explicit Vulkan Layer Pass-Through).

## Incorporated components

None. No third-party implementation code, vendor DLL, model weights, shader, pre-compiled binary, or release asset has been incorporated or distributed by this repository.

The Task 1 explicit Vulkan pass-through layer (`src/layer/`), test runner (`tests/test_layer_dispatch.cpp`), and validation launcher (`scripts/run_sample_validation.ps1`) consist entirely of original, independently authored project material written to conform with the public Khronos Vulkan Loader-Layer Interface specification (`LoaderLayerInterface.md`) and standard Vulkan headers (`vulkan/vulkan.h`, `vulkan/vk_layer.h`). The layer DLL and test binaries compile against headers provided by the external LunarG Vulkan SDK (`1.4.363.0`) and resolve dispatch dynamically via downstream function pointers without linking vendor binaries or static loader libraries into the layer.

The repository's MIT licence applies to project-authored material. It does not relicense third-party specifications or external SDK components.

## Build-time Header Licence and Provenance Record

The project build system references header files from the Khronos Vulkan API registry and LunarG Vulkan SDK during compilation. These headers are not vendored or redistributed in this repository; they are located on the developer's system via the `VULKAN_SDK` environment variable or standard CMake package resolution (`find_package(Vulkan)`).

### Khronos Vulkan Headers (`vulkan/vulkan.h`, `vulkan/vk_layer.h`, `vulkan/vulkan_core.h`)

- **Component:** Khronos Vulkan Headers / Registry
- **Origin:** The Khronos Group Inc. (https://github.com/KhronosGroup/Vulkan-Headers)
- **Version Pinned in SDK:** 1.4.363.0 (Vulkan API version 1.4)
- **Licence Terms:** Apache License, Version 2.0 OR MIT
- **Copyright:** Copyright 2015-2024 The Khronos Group Inc.

#### Apache License, Version 2.0 Notice:

```
Copyright 2015-2024 The Khronos Group Inc.

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
Copyright 2015-2024 The Khronos Group Inc.

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
