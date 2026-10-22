# Related projects and research

Updated 2026-10-19. This is a curated research direction, not a dependency selection or hardware-compatibility result. The repository contains explicit Vulkan pass-through code and a sample-validation harness; their existence does not prove rendered-color acquisition, a reference effect, neural inference or two-game acceptance.

The [current source evidence](research/SOURCE-CANDIDATES.md) supplements the [historical licence register](LICENSE-REVIEW.md). Follow [source review rules](SOURCE-REVIEW.md) before implementation study. No external implementation, runtime or model is adopted by these notes.

## Priority references

| Source | Relevant question | Use and limit |
|---|---|---|
| [ReShade](https://github.com/crosire/reshade), [RenoDX](https://github.com/clshortfuse/renodx), [RenderDoc](https://github.com/baldurk/renderdoc) | Which resources and processing stages can a real game integration expose? | Capture/diagnostic candidates. Different injection APIs and per-game adapters are not universal Vulkan input recovery. |
| [AeonSR](https://github.com/BarbatosAWLS/AeonSR) | What can an estimated-motion route achieve when engine inputs are absent? | Separate external comparison. Native-resolution AA does not reduce scene rendering cost; verify a true reduced-resolution route before calling it upscaling. |
| [xplane-fsr3](https://github.com/JonathanOrr/xplane-fsr3) | How does a game-specific integration recover useful temporal inputs? | One-game reference. Camera reconstruction does not supply complete animated-object motion. |
| [DLSS5-Feeder](https://github.com/jlrouzies-fr/DLSS5-Feeder), [NIGos/dlss5-bridge](https://github.com/NIGos/dlss5-bridge) | How are inputs described and transported across API/process boundaries? | MIT own-code candidates with inherited/third-party notices. Neither grants rights to proprietary neural runtimes or makes estimated inputs native. |
| [numsu/dlss-bridge](https://github.com/numsu/dlss-bridge) | Can acquisition and provider execution remain separate? | The embedded Vulkan bridge has an MIT grant; no umbrella grant was located for the outer project. Limit any study/reuse to a cleared exact scope. |
| [ReScaleFrame](https://github.com/KillerPixelCrew/ReScaleFrame) | What should a frame/provider contract contain? | Its Game SDK is MIT, root is GPL and managed-runtime provenance has retained restrictions. Conditional contract reference; no managed-runtime adoption. |
| [OptiScaler](https://github.com/optiscaler/OptiScaler) | Can an existing provider router consume inputs a game already exposes? | Conditional downstream adapter under GPL obligations. It does not solve arbitrary legacy-game input recovery. |

These projects solve different layers: capture, input recovery, transport, provider routing and inference. Treating them as interchangeable “enhancement engines” hides the main feasibility questions. Keep genuine engine inputs, camera reconstruction, optical flow and unavailable data distinct in diagnostics.

## Consolidated experiment direction

Keep the pass-through/reference-effect gates in [the roadmap](ROADMAP.md) and [initial experiments](INITIAL-EXPERIMENTS.md). A provider control and a missing-input comparison answer different questions:

1. Use a controlled scene/host with documented inputs, reset semantics and an authorized unmodified provider. Measure stock-provider cost before adding bridge/framework overhead.
2. Use one owned offline older game with a repeatable scene and useful depth where available. Compare the original route, the exact DXVK route separately where applicable, pass-through/reference processing, then a separately labelled estimated-motion baseline.
3. Add ReScaleFrame/OptiScaler adapters only after their required input fields can actually be supplied and the concrete licensing/integration basis is resolved.

Do not begin with frame generation or assume a neural-relighting runtime is available. OIDN and Real-ESRGAN remain isolated denoising/restoration laboratory candidates, not proofs of remastering quality or interactive temporal performance. A deterministic tint alone does not prove color read/copy, provider invocation or output use.

Develop/debug on the development laptop; record both the rendering and processing adapters because a dual-GPU route can introduce extra copies or select the wrong device. Repeat a stable package on the target gaming desktop for acceptance. Determine CPU versus GPU limitations before attributing a benefit to reduced resolution. Linux and Proton follow as separate routes, not inferred compatibility.

Record exact source/package hashes, game/API/bitness, driver/OS, adapters, input provenance, scene/input/output extents and processing stage. Measure frame-time distributions, transfer/synchronization cost, VRAM, temporal artifacts and UI readability. Report rendered versus generated/presented FPS separately. Do not claim input-to-display latency without a method that actually measures it. Choose numeric acceptance thresholds before evaluating the comparison.

## Removed and deferred recommendations

Restricted or unclear sources are no longer active implementation suggestions: LumeniteFX, Luma-Framework, NeuralScreen, HelixSR, DXVK-Studio-CE, UNCANNY, TuxScaling, OptiTux, bgfx-dlss5-nr and DLSS5-Patcher. Their specific terms or gaps remain in [source evidence](research/SOURCE-CANDIDATES.md) to prevent accidental reintroduction. VulkanStory likewise needs exact-path provenance resolution.

Model extraction/reconstruction/conversion routes remain excluded, including the reviewed d4r, x4r, DLSSNR-RDNA3, dlss-nr-on-intel and FSR shader-override approaches. Their own code licences do not change the use-as-supplied policy. Do not inherit patched runtimes or model work through an installer.

Linux presentation/profile experiments, offline video pipelines, generic installers and ambiguous Prism projects are deferred until a specific gap justifies investigation. README promises, badges, screenshots and old release-date predictions do not select a dependency. Earlier date-based speculation has been removed; inspect the actual authorized artifact and contract if one becomes relevant.

## Rendering foundations and technical references

- [DXVK](https://github.com/doitsujin/dxvk) normalizes supported Direct3D calls to Vulkan, not camera/motion/UI semantics.
- [Vulkan loader/layer interface](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md) and [Vulkan Guide](https://github.com/KhronosGroup/Vulkan-Guide) describe API/layer rules. Documentation, source and SDK artifact licences differ.
- [AMD FSR inputs](https://gpuopen.com/manuals/fsr_sdk/techniques/super-resolution-upscaler/) illustrate depth, motion scaling, coverage and transparency requirements. They are not a DLSS 5 contract.
- [DXVK Remix](https://github.com/NVIDIAGameWorks/dxvk-remix) and [RTX Remix compatibility](https://github.com/NVIDIAGameWorks/rtx-remix/wiki/Compatibility) concern specialized scene tracking/rendering, not a universal ordinary-DXVK replacement.
- [NVIDIA's DLSS 5 research description](https://research.nvidia.com/labs/adlr/DLSS5/) and [developer overview](https://developer.nvidia.com/blog/whats-new-for-game-developers-dlss-5-with-3d-guided-neural-rendering-nvidia-ace-updates-and-new-rtx-kit-capabilities/) are research/product references. Training supervision is not automatically a mandatory runtime-buffer list, and public articles do not replace an authorized integration contract.

No source reviewed here establishes an authorized unmodified DLSS 5 runtime across NVIDIA, AMD and Intel. Separate framework portability from each backend's requirements.

## Relationship to DXVK-Companion

[DXVK-Companion's component-management research](https://github.com/Tiflit/DXVK-Companion/blob/main/docs/research/COMPANION-SOURCES.md) covers inventory, pinned acquisition, ownership and rollback. Neural-Companion owns rendering/input work; optional management integration follows stable standalone boundaries. Neither project should require the other.

This consolidation changes research guidance only. It does not assign implementation, alter the model/runtime policy, certify licences for all dependencies, execute local checks or change Gemini's active task.
