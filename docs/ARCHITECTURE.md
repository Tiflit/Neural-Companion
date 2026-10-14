# Architecture proposal

Status: conceptual design, not an implemented interface or committed dependency selection.

## Components

| Component | Responsibility |
|---|---|
| Standalone application | Game selection, presets, capability reports, component configuration, and lifecycle control |
| Capture adapters | Obtain frame resources and timing through a supported rendering or capture route |
| Input preparation | Validate real game inputs, decode supported formats, reconstruct missing motion, and estimate remaining information |
| Enhancement providers | Invoke unmodified, authorized runtimes through supported integration interfaces |
| Presentation/composition | Use enhanced output, preserve the UI where possible, and preserve original output on recoverable failure |
| Optional Companion integration | Request installation/configuration/status through a future documented boundary |

The provider boundary separates frame acquisition from a particular model. Source, API, language, build system, and process layout remain to be selected after feasibility work.

## Capture routes

| Game route | Proposed handling | Limits |
|---|---|---|
| Native Vulkan | Vulkan layer | Resource semantics still require identification; layer activation is a deployment concern |
| Direct3D 8/9/10/11 through DXVK | Same Vulkan layer where compatible | DXVK compatibility, bitness, wrappers, and operating-system behavior must be validated |
| Native Direct3D 12 | Separate adapter or external capture | DXVK does not translate D3D12; no translation route is selected |
| OpenGL and other APIs | Future adapter, translation, or external capture | No support commitment |
| External window/display capture | Final-image fallback | Usually lacks engine motion, depth, and UI separation |

Do not force a game through translation solely to satisfy the preferred architecture when an existing native integration gives better compatibility or inputs.

A separate Vulkan layer is the preferred experiment before considering a DXVK fork. Changing DXVK itself would require demonstrated access or synchronization benefits that cannot reasonably be obtained through interception.

## Input contract: information and provenance

Each captured frame should carry enough metadata to interpret its resources safely. Proposed fields include frame identity and timing, device/API, extent and viewport, resource formats, color space and exposure convention, synchronization ownership, and source confidence.

| Input | Preferred source | Alternative |
|---|---|---|
| Color | Appropriate game scene image before HUD | Final image with explicit UI limitations |
| Motion | Identified engine motion buffer | Reconstructed camera/object motion; estimated optical flow |
| Depth | Identified game depth | Estimated depth only if a consumer can use it meaningfully |
| Camera | Identified view/projection information | Estimated motion, or unavailable |
| UI separation | Identified UI pass or mask | Estimated protection or processing the completed image |
| Continuity | Verified frame sequence and reset events | Conservative history reset |

Motion metadata must describe direction, scale/units, resolution, jitter convention, validity, and coordinate orientation. Depth needs its encoding and projection conventions. Color needs its transfer function and range. A resource handle without these semantics is not a valid input.

Depth and camera data may support reconstruction even when the enhancement model does not directly consume them. Provider requirements must come from the exact runtime's documentation; do not assume requirements for DLSS Super Resolution or Frame Generation are identical to neural rendering.

For DLSS 5 specifically, NVIDIA's published research description lists rendered color, engine motion vectors, temporal state, and artistic controls at inference. This is research context, not a complete integration ABI. See [research](RESEARCH.md).

## Motion strategy

1. Recognize and validate existing motion resources using known interfaces or profiles.
2. Recover camera motion from usable depth and camera transforms.
3. Recover object motion where previous transforms or deformation history can be identified.
4. Estimate remaining movement from image sequences.
5. Invalidate unreliable history and report reduced input quality.

Camera reprojection handles stationary scenery, not all animation. Draw calls do not supply universal object identities. Reused buffers, changing geometry, skinning, particles, transparency, reflections, and shader deformation complicate reconstruction.

Regional confidence and hybrid motion are research proposals. Before feeding mixed motion to a proprietary runtime, verify that its documented contract accepts the resulting field and history behavior. Do not invent undocumented confidence-mask inputs.

## Temporal and presentation correctness

Account for resizing, minimized windows, skipped/repeated frames, camera cuts, loading screens, device loss, dynamic resolution, and UI changes. Separate integration-controlled history from provider-owned state and use supported reset controls.

Resource sharing must respect GPU lifetime, image layouts, queue ownership, barriers, and synchronization. Vulkan-to-D3D12 sharing may be needed for some providers and is not guaranteed across devices or resource types. Avoid CPU round trips in the intended fast path; measure transfer cost rather than assuming zero-copy.

Preserving an original frame should allow pass-through after a recoverable provider failure. An in-process runtime cannot guarantee recovery from arbitrary driver faults or process crashes.

## Providers and GPU compatibility

A provider declares its supported device families, operating systems, APIs, component versions, inputs, output format, and licensing/provenance status. Capability checks must run before activation.

Cross-vendor framework design does not make proprietary models cross-vendor. Select a genuinely supported unmodified provider, or mark enhancement unavailable. Do not silently substitute a patched binary, extracted weights, or a different model.

The first capture experiment can use a deterministic reference effect to validate transport. It must be labeled a reference provider, not neural enhancement.

## Configuration and DXVK-Companion

Keep visual intent separate from technical profiles and hardware capability selection. Use schema/version identifiers once formats are designed, but this draft defines no public file format.

Neural-Companion owns its runtime and presets. DXVK-Companion may later manage it as an optional component, using a versioned integration boundary. Neither should independently overwrite shared wrapper files or configurations without ownership and conflict handling.

Native Vulkan enhancement eligibility must be separate from DXVK deployment eligibility. Existing DXVK-Companion refusal rules for installing DXVK into native Vulkan/DX12 games remain appropriate.

Deployment mechanisms for Vulkan layers need explicit design, including activation scope and cleanup. Avoid system-wide activation by default. Check whether proposed registration affects portability before claiming a portable implementation.

## Diagnostics

Report selected route, provider/version, actual input provenance, fallback/reset reasons, processed and displayed frame counts, and measured cost. Keep local paths and identifying information out of exported reports by default. No telemetry service or automatic report upload is proposed.
