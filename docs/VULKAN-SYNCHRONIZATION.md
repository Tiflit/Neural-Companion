# Vulkan Layer Synchronization & Architecture Specification

Status: Proposed technical design specification for [Issue #3](https://github.com/Tiflit/Neural-Companion/issues/3). This document defines the synchronization, queue management, fixed-function blending, and lifecycle rules for the experimental frame capture and reference-effect Vulkan layer across native Vulkan x64 and translated Direct3D 11 x64 (via official DXVK 3.1.1).

---

## 1. Revision History & Review Findings Mapping

This specification incorporates the findings from the project architectural and synchronization reviews:

| Review Finding | Defect / Ambiguity in Earlier Draft | Resolution & Section in this Specification |
| :--- | :--- | :--- |
| **1. Reacquisition & Presentation Retirement** | Assuming image index return alone proves presentation completion. | **§4.2 & §4.3**: Defined the complete acquisition-to-render dependency chain and semaphore lifetime proof. Documented requirement for $M \ge 1$ present wait semaphores. |
| **2. Teardown & Device Idle Contradiction** | Relying on `vkDeviceWaitIdle` to destroy presentation semaphores after acknowledging it does not prove presentation completion. | **§4.4 & §6.2**: Resolved contradiction. Defined deferred swapchain retirement queue ($K \ge \text{imageCount}$ retirement frames) and explicit handling for unacquired/in-flight images. |
| **3. `OUT_OF_DATE` Semaphore Consumption** | Incorrectly claiming `VK_ERROR_OUT_OF_DATE_KHR` leaves wait semaphores unconsumed. | **§4.4 & §6.1**: Corrected to match Vulkan WSI specification: semaphore waits *are* executed by WSI even on out-of-date/suboptimal returns; semaphores are consumed and retired via deferred cleanup. |
| **4. Queue Submission Failure Granularity** | Conflating OOM errors with device loss and claiming all submit failures consume semaphores. | **§6.1**: Distinguished `VK_ERROR_OUT_OF_HOST_MEMORY` / `OUT_OF_DEVICE_MEMORY` (semaphores unconsumed, no downstream present, `pResults` populated) from `VK_ERROR_DEVICE_LOST`. |
| **5. Executable Contract & Synchronization Scopes** | Hand-wavy barrier stages, missing fence wait before command re-recording, unclear old/new swapchain coexistence, and queue vs. capability ambiguity. | **§2, §3, & §5**: Added concrete fence wait/reset in frame sequence, explicit queue handle graphics verification, single-layer/non-protected constraints, and thread-safe internal map locking. |
| **6. Validation Criteria & Overhead Targets** | Inventing an unsupported "$\le 0.1\%$ overhead" gate without baseline evidence. | **§7**: Removed arbitrary target; replaced with empirical frame-time distribution measurements and fault-injection test matrix. |

---

## 2. Supported Configuration Contract

To ensure deterministic execution and prevent undefined driver states, the prototype layer enforces a strict supported-configuration contract. Any configuration outside this boundary is rejected cleanly **prior to command submission**, forwarding host calls and semaphores completely intact.

| Dimension | Supported Configuration | Unsupported / Out-of-Scope | Layer Enforcement Action |
| :--- | :--- | :--- | :--- |
| **API Version** | Vulkan 1.2 or 1.3 (as requested by host) | Vulkan < 1.2 | Pass-through; do not silently upgrade instance/device API version. |
| **Device Scope** | Exactly 1 logical device (`VkDevice`) | Multiple active `VkDevice` instances | Track primary device; secondary devices operate in transparent pass-through. |
| **Present Call Scope** | Exactly 1 swapchain per present call (`swapchainCount == 1`) | Multi-swapchain presents (`swapchainCount > 1`) | Pre-submit rejection: forward original `VkPresentInfoKHR` directly downstream. |
| **Swapchain Coexistence** | 1 active presenting swapchain, plus $\le 1$ retiring `oldSwapchain` during recreation | Multiple independent presentation swapchains | Track active and retiring swapchain contexts in internal handle map. |
| **Surface Formats** | `VK_FORMAT_B8G8R8A8_UNORM`, `VK_FORMAT_R8G8B8A8_UNORM`, `VK_FORMAT_B8G8R8A8_SRGB`, `VK_FORMAT_R8G8B8A8_SRGB` | HDR (`RGBA16F`, `A2B10G10R10`), YCbCr, or formats lacking blend feature | Pre-submit rejection: emit single diagnostic log, forward original present unchanged. |
| **Color Space** | `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR` (SDR) | HDR color spaces (e.g. `EXT_swapchain_colorspace`) | Pre-submit rejection: forward original present unchanged. |
| **Swapchain Usage** | `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT` | Swapchains lacking color attachment usage | Pre-submit rejection: forward original present unchanged. |
| **Image Layers** | Single layer (`imageArrayLayers == 1`) | Multiview / VR (`imageArrayLayers > 1`) | Pre-submit rejection: forward original present unchanged. |
| **Protected Content** | Non-protected swapchains | `VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR` | Pre-submit rejection: forward original present unchanged. |
| **Queue Arrangement** | Present queue handle possesses `VK_QUEUE_GRAPHICS_BIT` | Dedicated present queue lacking graphics capability (`graphicsQueue != presentQueue`) | Pre-submit rejection: split-queue ownership transfers deferred. Forward original present unchanged. |
| **Device Groups** | Single physical device presentation | `VkDeviceGroupPresentInfoKHR` (multi-GPU) | Pre-submit rejection: forward original present unchanged. |
| **Extent State** | Normal non-zero extent (`width > 0 && height > 0`) | Minimized window (`width == 0 \|\| height == 0`) | Pre-submit rejection: bypass processing; forward present without allocating resources. |
| **Host Present Semaphores** | Host provides $M \ge 1$ wait semaphores | $M = 0$ (un-synchronized presentation) | Pre-submit rejection: cannot guarantee acquisition completion without host wait. Forward unchanged. |
| **Layer Modes** | Explicit launch mode: `Pass-Through` or `Reference-Tint` | Dynamic in-game runtime toggling | Selected at process launch via environment variable (`NC_LAYER_MODE`). |

---

## 3. Layer Dispatch & Lifecycle Preservation

The layer integrates into the Vulkan loader chain using the standard Khronos Layer Negotiation Protocol:

### 3.1 Dispatch Interception
* **Instance Negotiation**:
  * Implements `vkGetInstanceProcAddr`, `vkCreateInstance`, and `vkDestroyInstance`.
  * Preserves `pCreateInfo->pApplicationInfo->apiVersion` without modification; does not silently upgrade requested versions.
  * Intercepts `VkLayerInstanceCreateInfo` chain to obtain downstream `pfnGetInstanceProcAddr`.
* **Device Negotiation**:
  * Implements `vkGetDeviceProcAddr`, `vkCreateDevice`, and `vkDestroyDevice`.
  * Preserves enabled device extensions and features requested by the caller; does not inject unrequested features.
  * Intercepts `VkLayerDeviceCreateInfo` chain to obtain downstream `pfnGetDeviceProcAddr`.
* **Queue Tracking**:
  * Intercepts `vkGetDeviceQueue` and `vkGetDeviceQueue2`.
  * Maps every returned `VkQueue` handle to its device and parent `queueFamilyIndex`.
  * Queries and caches the queue family flags (`queueFlags`) to identify graphics capability (`VK_QUEUE_GRAPHICS_BIT`).
* **Swapchain & Presentation Interception**:
  * Intercepts `vkCreateSwapchainKHR`, `vkDestroySwapchainKHR`, `vkGetSwapchainImagesKHR`, `vkAcquireNextImageKHR`, `vkAcquireNextImage2KHR`, and `vkQueuePresentKHR`.

### 3.2 Threading & Caller Memory Preservation
* **Queue Synchronization**: Per the Vulkan specification, `vkQueuePresentKHR` requires external synchronization on the `VkQueue` parameter. The layer executes its command recording and submission strictly on the thread calling `vkQueuePresentKHR`, guaranteeing no concurrent queue access.
* **Internal State Protection**: While host queue calls are externally synchronized, swapchain creation, recreation, and destruction may occur across threads. Internal handle mappings (such as `swapchainMap`) are guarded by an internal mutex (`std::mutex m_swapchainMutex`). Device-wide idle waits (`vkDeviceWaitIdle`) are never invoked across unrelated active host threads during normal frame presentation.
* **Preservation of `VkPresentInfoKHR` Fields**:
  * Caller's `pNext` chain is inspected for unsupported extensions (e.g. device groups); standard structures (e.g. `VkPresentRegionsKHR`) are preserved and forwarded intact.
  * Caller's `pResults` array (if provided) is forwarded to receive downstream per-swapchain `VkResult` codes.
  * The downstream return value of `vkQueuePresentKHR` is returned directly to the caller.

---

## 4. Reference Effect via Fixed-Function Attachment Blending

### 4.1 Elimination of Attachment Sampling
Khronos image-view usage rules dictate that a `VkImageView` cannot grant capabilities absent from its parent `VkImage`. Swapchain images are allocated with `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT`, but typically **lack** `VK_IMAGE_USAGE_SAMPLED_BIT` and `VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT`.

To strictly adhere to `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT` without requiring additional usage flags or intermediate copy/blit textures:
* The layer **does not sample** the swapchain image.
* The layer **does not use input attachments** or subpass inputs.
* The reference tint effect is implemented purely via **Render Pass Attachment Load/Store and Fixed-Function Hardware Blending**.

### 4.2 Render Pass & Framebuffer Configuration
* **Color Attachment**:
  * `format`: Match swapchain format (`VkSwapchainCreateInfoKHR::imageFormat`).
  * `samples`: `VK_SAMPLE_COUNT_1_BIT`.
  * `loadOp`: `VK_ATTACHMENT_LOAD_OP_LOAD` (preserves existing rendered game pixels).
  * `storeOp`: `VK_ATTACHMENT_STORE_OP_STORE` (writes the blended result back to swapchain image).
  * `initialLayout`: `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`.
  * `finalLayout`: `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`.
* **Subpass**:
  * 1 color attachment reference: `layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`.
  * `pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS`.

### 4.3 Blending Mathematics & Pipeline Configuration
The pipeline draws a single fullscreen triangle (`gl_VertexIndex` 0..2) requiring no vertex buffer bindings. The fragment shader outputs a constant tint color $\mathbf{C}_{\text{tint}} = (1.0, 0.85, 0.70, 0.15)$ (a 15% warm amber tint).

We configure `VkPipelineColorBlendAttachmentState` for fixed-function blending:
* `blendEnable = VK_TRUE`
* **Color Blending (Linear Interpolation Tint)**:
  * `colorBlendOp = VK_BLEND_OP_ADD`
  * `srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA`
  * `dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA`
  $$\mathbf{C}_{\text{out}} = \mathbf{C}_{\text{tint}} \times A_{\text{tint}} + \mathbf{C}_{\text{game}} \times (1.0 - A_{\text{tint}})$$
  For $A_{\text{tint}} = 0.15$:
  $$\mathbf{C}_{\text{out}} = 0.15 \times (1.0, 0.85, 0.70) + 0.85 \times \mathbf{C}_{\text{game}}$$
* **Alpha Channel Preservation**:
  * `alphaBlendOp = VK_BLEND_OP_ADD`
  * `srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO`
  * `dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE`
  $$A_{\text{out}} = 0 \times A_{\text{tint}} + 1 \times A_{\text{game}} = A_{\text{game}}$$
* **Write Mask**:
  `colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT`.

### 4.4 Format & Color Space Behavior (UNORM vs. sRGB)
* **UNORM Formats (`VK_FORMAT_B8G8R8A8_UNORM`)**: Blending math operates directly on raw stored non-linear component values.
* **sRGB Formats (`VK_FORMAT_B8G8R8A8_SRGB`)**: Vulkan hardware automatically decodes sRGB values to linear float before blending, executes blending in linear space, and re-encodes to sRGB upon storing to the attachment.
* **Verification**: Blending capability is validated during swapchain creation using `vkGetPhysicalDeviceFormatProperties`:
  `formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT != 0`.

---

## 5. Presentation Semaphore Lifetime & Synchronization Proof

### 5.1 The Semaphore Reuse Hazard
Per the [Khronos Swapchain Semaphore Reuse Guide](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html):
* A fence passed to `vkQueueSubmit` only indicates that GPU execution of that submit has completed. It does **not** indicate that presentation has completed or that the presentation engine has released the wait semaphore passed to `vkQueuePresentKHR`.
* Reusing a binary semaphore that is still referenced by an active presentation request produces undefined behavior and validation layer errors.
* `vkDeviceWaitIdle` waits for queue execution, but does not guarantee completion of the presentation engine's read on all platforms.

### 5.2 Per-Swapchain-Image Association & Acquisition-Chain Proof
To provide an ironclad, standard-compliant lifetime proof:
1. **Per-Image Semaphore Association**:
   * The layer allocates exactly one `renderCompleteSemaphore[i]` for each swapchain image index $i \in [0, \text{imageCount} - 1]$.
   * When image index $i$ is presented, the layer passes `renderCompleteSemaphore[i]` to `vkQueuePresentKHR`.
2. **The Acquisition-Chain Dependency Proof**:
   * Returning an image index from `vkAcquireNextImageKHR` on the CPU indicates that the index was assigned, but GPU presentation release of image $i$ is only completed when the host's acquisition semaphore is signaled.
   * Under standard Vulkan/DXVK execution:
     $$\text{WSI Present Release} \xrightarrow{\text{signals}} \text{hostAcquireSemaphore} \xrightarrow{\text{waited on by}} \text{Host Render Submit} \xrightarrow{\text{signals}} \text{hostWaitSemaphores}$$
   * The host provides these `hostWaitSemaphores` (count $M \ge 1$) to `vkQueuePresentKHR`.
   * The layer's submit explicitly waits on `hostWaitSemaphores`.
   * Therefore, by transitivity through the execution dependency chain, the layer submit is guaranteed not to execute until WSI has fully released image $i$ and retired `renderCompleteSemaphore[i]`.
3. **Contract Requirement on $M$**:
   * If a host passes $M = 0$ (un-synchronized present), the layer cannot prove acquisition completion via host chaining. The layer executes **pre-submit rejection**, forwarding the present call untouched.

### 5.3 Command Resource Retirement (CPU-Side Fences)
* In addition to semaphores, each swapchain image index $i$ owns a dedicated `inFlightFence[i]`.
* Before re-recording or reusing the layer command buffer for image $i$ in `vkQueuePresentKHR`:
  1. `vkWaitForFences(device, 1, &inFlightFence[imageIndex], VK_TRUE, UINT64_MAX)`.
  2. `vkResetFences(device, 1, &inFlightFence[imageIndex])`.
  3. `vkResetCommandBuffer(commandBuffers[imageIndex], 0)`.
* This ensures CPU-side command buffer memory is never reset while previous GPU execution is still active.

### 5.4 Swapchain Recreation & Teardown Policy
* **Out-of-Date / Suboptimal Returns**:
  * Per the Vulkan specification, even when `vkQueuePresentKHR` returns `VK_ERROR_OUT_OF_DATE_KHR` or `VK_SUBOPTIMAL_KHR`, queue operations are enqueued and presentation wait semaphores **are consumed** by WSI.
  * The layer marks `renderCompleteSemaphore[i]` as consumed and does not reuse it immediately.
* **Deferred Retirement Queue for Recreated Swapchains**:
  * When `vkCreateSwapchainKHR` is called with `oldSwapchain != VK_NULL_HANDLE`, the old swapchain's context is moved into a **Deferred Retirement Queue**.
  * If the host enabled `VK_EXT_swapchain_maintenance1`, the layer can utilize `VkSwapchainPresentFenceInfoEXT` to directly observe presentation retirement.
  * If `VK_EXT_swapchain_maintenance1` was **not** enabled by the host (the default baseline):
    The layer keeps the old swapchain and its per-image semaphores alive until:
    1. At least $K$ presentation frames have completed on the new swapchain (where $K \ge \text{imageCount}$ of the old swapchain).
    2. Queue execution is drained (`vkQueueWaitIdle` on the present queue).
  * This guarantees that Windows/DWM or the presentation engine has finished with all frames of the superseded swapchain before the old semaphores and views are destroyed via `vkDestroySemaphore` / `vkDestroyImageView`.
* **Final Device Teardown (`vkDestroyDevice`)**:
  * At application shutdown, `vkDeviceWaitIdle(device)` is invoked to ensure all device queues are completely idle.
  * All remaining swapchains in active or retirement queues are destroyed cleanly.

---

## 6. Concrete Synchronization Pipeline & Barrier Scopes

### 6.1 Step-by-Step Frame Sequence

For each supported frame presented through `vkQueuePresentKHR(queue, pPresentInfo)`:

```
[Host Render Finished]
       │
       ▼ (Signals hostWaitSemaphores[0..M-1])
[Layer vkQueuePresentKHR Intercept]
       │
       ├─► Validation Check (Contract Pass & M >= 1?)
       │      ├─ NO  ──► Forward pPresentInfo unchanged (Pre-Submit Rejection)
       │      └─ YES ──► Proceed to Step 1
       │
[Step 1: CPU Fence Wait & Command Reset]
       │  ├─ vkWaitForFences(device, 1, &inFlightFence[imageIndex], VK_TRUE, UINT64_MAX)
       │  ├─ vkResetFences(device, 1, &inFlightFence[imageIndex])
       │  └─ vkResetCommandBuffer(commandBuffers[imageIndex], 0)
       │
[Step 2: Record Layer Command Buffer]
       │  ├─ Pipeline Barrier 1: PRESENT_SRC_KHR -> COLOR_ATTACHMENT_OPTIMAL
       │  ├─ Begin Render Pass (LOAD_OP_LOAD, STORE_OP_STORE)
       │  ├─ Draw Fullscreen Tint Triangle (Fixed-Function Blend)
       │  ├─ End Render Pass
       │  └─ Pipeline Barrier 2: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
       │
[Step 3: Submit to Graphics Present Queue]
       │  VkSubmitInfo:
       │    waitSemaphoreCount = M
       │    pWaitSemaphores    = pPresentInfo->pWaitSemaphores
       │    pWaitDstStageMask  = [COLOR_ATTACHMENT_OUTPUT_BIT] * M
       │    pCommandBuffers    = [commandBuffers[imageIndex]]
       │    signalSemaphoreCount = 1
       │    pSignalSemaphores  = [renderCompleteSemaphore[imageIndex]]
       │    Fence              = inFlightFence[imageIndex]
       │
       ├─► Submit Succeeded?
       │      ├─ NO (OOM / Lost) ──► See Section 6 (Failure Handling)
       │      └─ YES             ──► Proceed to Step 4
       │
[Step 4: Downstream Presentation]
       │  VkPresentInfoKHR:
       │    waitSemaphoreCount = 1
       │    pWaitSemaphores    = [renderCompleteSemaphore[imageIndex]]
       │    pImageIndices      = &imageIndex
       │    pResults           = pPresentInfo->pResults
       │
       ▼
[Downstream vkQueuePresentKHR Call]
```

### 6.2 Precise Pipeline Barrier Specifications

#### Barrier 1: Pre-Render Pass Layout Transition
* **Target Image**: `swapchainImages[imageIndex]`.
* **Subresource Range**: `aspectMask = VK_IMAGE_ASPECT_COLOR_BIT`, `baseMipLevel = 0`, `levelCount = 1`, `baseArrayLayer = 0`, `layerCount = 1`.
* **Old Layout**: `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`.
* **New Layout**: `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`.
* **Source Stage**: `VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT`.
* **Destination Stage**: `VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT`.
* **Source Access**: `VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT` (host's prior rendering writes).
* **Destination Access**: `VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT` (layer attachment load and blend write).

#### Barrier 2: Post-Render Pass Layout Transition
* **Target Image**: `swapchainImages[imageIndex]`.
* **Subresource Range**: Identical single color subresource.
* **Old Layout**: `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`.
* **New Layout**: `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`.
* **Source Stage**: `VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT`.
* **Destination Stage**: `VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT`.
* **Source Access**: `VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT` (layer attachment store write).
* **Destination Access**: `0` (WSI read access is synchronized downstream by `renderCompleteSemaphore`).

### 6.3 Submission Wait Stage Mapping ($M$ Entry Mapping)
In `VkSubmitInfo`:
* `waitSemaphoreCount = pPresentInfo->waitSemaphoreCount` ($M$).
* When $M > 0$:
  * The layer allocates a contiguous array `VkPipelineStageFlags waitStages[M]`.
  * Every entry is explicitly set to `VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT`:
    $$\forall k \in [0, M-1], \quad \text{waitStages}[k] = \text{VK\_PIPELINE\_STAGE\_COLOR\_ATTACHMENT\_OUTPUT\_BIT}$$

---

## 7. Failure States & Recovery State Table

The layer partitions failure handling into distinct, non-overlapping execution phases to prevent re-waiting consumed semaphores or presenting corrupted frames.

| Execution Phase | Condition / Error Code | Layer Action | Semaphore State | Caller Impact |
| :--- | :--- | :--- | :--- | :--- |
| **Pre-Submit Inspection** | Unsupported format, extent 0x0, non-graphics queue, pass-through mode, multi-swapchain, or $M = 0$. | **Pre-Submit Rejection**: Do not record commands. Do not submit to queue. Forward caller's `VkPresentInfoKHR` directly downstream. | Host semaphores completely untouched and unconsumed by layer. | Clean native presentation; original frame presented. |
| **Queue Submit OOM** | `vkQueueSubmit` returns `VK_ERROR_OUT_OF_HOST_MEMORY` or `VK_ERROR_OUT_OF_DEVICE_MEMORY`. | **Submission OOM Abort**: Per Vulkan spec, state of semaphores is unaffected. **DO NOT call downstream present.** If `pResults` provided, populate with error code. Return error code directly. | Host semaphores remain unconsumed. `renderCompleteSemaphore` remains unsignaled. | Caller receives OOM error code; no presentation issued. |
| **Queue Submit Device Lost** | `vkQueueSubmit` returns `VK_ERROR_DEVICE_LOST`. | **Device Loss Abort**: Mark device lost internally. Do not call downstream present. Populate `pResults` if provided. Return `VK_ERROR_DEVICE_LOST`. | Device in lost state. | Host receives device loss; triggers engine recovery/shutdown. |
| **Downstream WSI Out-of-Date** | Downstream `vkQueuePresentKHR` returns `VK_SUBOPTIMAL_KHR` or `VK_ERROR_OUT_OF_DATE_KHR`. | **WSI Out-of-Date Notification**: Forward return code and `pResults` intact to caller. Enqueue swapchain into Deferred Retirement Queue. | Semaphores consumed by WSI wait operation. Retired via deferred queue. | Host initiates standard swapchain recreation sequence. |
| **Swapchain Recreation** | Host calls `vkCreateSwapchainKHR` with `oldSwapchain != VK_NULL_HANDLE`. | Add `oldSwapchain` to Deferred Retirement Queue. Allocate new per-image resources for new swapchain. | Old semaphores retired after $K$ frames and queue idle. | Clean swapchain transition without handle leaks. |
| **Device Teardown** | Host calls `vkDestroyDevice`. | Drain device queues (`vkDeviceWaitIdle`). Destroy all active and retired swapchain resources. | All semaphores and fences destroyed cleanly. | Zero resource leaks; clean layer unload. |

---

## 8. Experimental Validation & Verification Plan

The layer will be validated using an incremental, four-step verification suite without arbitrary performance assumptions.

### 8.1 Empirical Performance Evaluation (No Arbitrary Limits)
* **Baseline Measurement**: Record frame-time distributions (minimum, median, 95th percentile, 99th percentile) over a fixed, repeatable scene without the layer loaded.
* **Pass-Through Measurement**: Measure frame-time distributions with the layer active in pass-through mode (`NC_LAYER_MODE=passthrough`).
* **Reference-Effect Measurement**: Measure frame-time distributions with the reference tint active (`NC_LAYER_MODE=reference_tint`).
* **Acceptance Standard**: Numerical tolerances will be established relative to the measured baseline variance ($\sigma$) on the test hardware, rather than an ungrounded arbitrary percentage.

### 8.2 Incremental Test Matrix:

1. **Step 1: Diagnostic Sanity & Validation Compliance (Sample Host)**:
   * Host: `vkcube.exe` with discrete RTX 4060.
   * Tooling: LunarG Vulkan Validation Layer (`VK_LAYER_KHRONOS_validation`).
   * Pass Criteria: Zero validation errors, zero semaphore lifetime warnings, zero layout mismatch errors over continuous execution.
2. **Step 2: Scoped Pass-Through Mode**:
   * Mode: `NC_LAYER_MODE=passthrough`.
   * Test: Verify pre-submit rejection path forwards frames and semaphores with zero visual degradation and frame-time distribution matching baseline.
3. **Step 3: Reference Tint Effect (Native Vulkan)**:
   * Mode: `NC_LAYER_MODE=reference_tint`.
   * Test: Verify fixed-function blending executes. Verify image output displays constant 15% warm amber tint without alpha clipping or edge artifacts.
4. **Step 4: Resilience & Fault Injection**:
   * **Dynamic Resize**: Repeated window resizing forcing `VK_ERROR_OUT_OF_DATE_KHR` and `VK_SUBOPTIMAL_KHR`; verify clean recreation without validation errors.
   * **Minimization**: Window minimization (handling extent `0x0`); verify pre-submit bypass without allocation.
   * **Host Wait $M=0$ Check**: Host presentation with 0 wait semaphores; verify clean pre-submit rejection.
   * **Clean Process Shutdown**: Verify zero leaks reported by validation layer on process exit.
5. **Step 5: Translated D3D11 Route via DXVK 3.1.1 (Isolated Game Host)**:
   * Host: Isolated Skyrim SE working copy via verified DXVK 3.1.1 x64 under dedicated test profile.
   * Test: Compare native baseline vs. DXVK baseline vs. DXVK + Neural-Companion reference tint.
   * Pass Criteria: Stable swapchain rendering on RTX 4060, clean exit.
