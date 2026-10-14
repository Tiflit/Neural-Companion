# Initial experiment candidates

Research date: 2026-10-14. Status: proposed experiments, not implementation approval, an adopted dependency, or a compatibility result. This record narrows [the roadmap](ROADMAP.md) without freezing a public provider interface.

## Recommended sequence

1. Prove unchanged frame transport and a project-authored deterministic effect.
2. Benchmark an unmodified neural provider separately from game capture.
3. Attempt live integration only after the provider's exact artifact, dependencies, inputs, and measured cost are acceptable.

No DLSS 5 runtime is selected. No model extraction, conversion, retraining, proprietary runtime patching, or hardware bypass is proposed.

## Capture test pair

| Slot | Candidate | Why investigate it | Unresolved before a run |
|---|---|---|---|
| Native Vulkan, x64 | vkQuake with lawfully acquired Quake data | Upstream documents Vulkan rendering and Windows x64 releases; simple scenes offer a manageable transport test | Confirm game-data availability, pin an official binary and hash, confirm actual architecture/API and local settings |
| Direct3D through DXVK, x64 | A clean Skyrim Special Edition installation | Already relevant to the owner's game testing; proposed D3D11 route exercises a materially different renderer | Confirm a clean installation is available, inspect actual executable/API, select DXVK version, establish baseline stability and a repeatable save |

This pair is provisional. Quake ownership/data availability is unknown. Do not purchase, acquire game data, alter an existing modded setup, or imply a successful DXVK run on the basis of this document. If either candidate is unavailable, select an already-owned offline game satisfying the same route and bitness requirements.

The native candidate is grounded in [vkQuake's pinned README](https://github.com/Novum/vkQuake/blob/47726d899044ded94b20b242091d867b4e1cbb6d/readme.md). Bethesda's [Skyrim Special Edition requirements](https://help.bethesda.net/app/answers/detail/a_id/36331/) establish a 64-bit target, not the actual local loaded rendering route; executable/runtime inspection remains required.

Use vkQuake only as an independently installed test host. Its repository contains GPL version 2 text; that does not license Quake assets or authorize copying engine code into Neural-Companion. No engine code is reused, linked, or bundled here. Any future source-derived use or distribution needs a separate compatibility review.

### First-run conditions

- Windows x64, one GPU at a time, fixed 1920×1080 SDR output as a proposed starting condition.
- Start on one available machine; the Legion RTX 4060 is a practical first candidate. Repeat on the Arc B580 after one route works. Arc 140V follows as an additional device, not a prerequisite.
- Record exact game build, executable bitness, GPU/driver/OS, display mode, wrapper/layer versions, and settings.
- Keep the selected game unmodified except for the explicit test route; use a separate test configuration and preserve the existing installation state.
- Establish baseline game stability before adding the layer. For Skyrim, measure native D3D11 and DXVK without Neural-Companion separately.
- Scope layer activation to the selected process launch. Do not register global activation as an experiment shortcut.
- Fix a repeatable indoor scene, outdoor scene, camera movement, and menu/HUD sequence. Pin saves or demos locally where permitted; do not redistribute game content.

### Evidence to collect

| Mode | Purpose |
|---|---|
| Game without Neural-Companion | Establish the selected route's baseline |
| Layer pass-through | Detect transport, color, frame-order, and pacing regressions |
| Deterministic reference effect | Prove processing and presentation of the processed result |
| Enhancement disabled again | Check restoration of original output and lifecycle behavior |

Repeat each mode under equivalent settings. Record frame-time distributions, observable GPU cost, VRAM, acquired/processed/presented frame counts, resize/minimize behavior, and cleanup. Report unavailable measurements explicitly. Numeric acceptance thresholds should be set after initial baseline measurements, before claiming acceptance.

The test does not require depth, recovered camera transforms, motion estimation, or neural inference. Defer those until transport works in both games.

## Neural-provider candidates

### A. Intel Open Image Denoise: preferred inference laboratory candidate

The [pinned OIDN documentation](https://github.com/RenderKit/oidn/blob/6602ee2ca38a1a2a02135beed8f6e68eed630180/README.md) describes pretrained neural filters for ray-traced image denoising, color-only input as an option, multiple hardware backends, and documented device support including Arc B-series and NVIDIA Ada-class GPUs. This is upstream support documentation, not a successful test on the owner's devices.

The source and its separately pinned weights repository each contain Apache-2.0 licence text. The weights README expressly identifies the trained-weight repository and Apache-2.0 context. However, the source also contains a [binary software licence](https://github.com/RenderKit/oidn/blob/6602ee2ca38a1a2a02135beed8f6e68eed630180/doc/BINARY-LICENSE.txt) and third-party notices. Do not apply the source licence to an arbitrary prebuilt package: choose and inspect the actual package/build route and all shipped terms before approval.

Proposed use: the supplied built-in RT filter, initially color-only LDR, on project-authored synthetic noisy images and short sequences. This tests invocation, device selection, input/output conventions, cost, and failure handling. Preserve supplied models unchanged.

Important limits:

- It removes rendering noise; it does not generate DLSS 5-like lighting/material enhancement.
- Clean rasterized game frames are outside the intended noisy-image use case; smoothing them would not demonstrate remastering quality.
- Multi-vendor support uses different device backends, not a promise of a Vulkan-native inference implementation.
- Vulkan resource interoperability, device matching, synchronization, and copy cost require separate experiments. No zero-copy claim is made.
- Documented hardware support does not establish useful real-time performance or temporal stability.

First benchmark: choose an approved artifact, run supplied inference on controlled inputs at several fixed extents, record device actually used and complete end-to-end latency, and compare output against the known noise-free reference. CPU execution, if tested, is a functional control and must not be presented as GPU success. No provider is selected until that evidence exists.

### B. Real-ESRGAN ncnn Vulkan: secondary restoration/upscaling candidate

The [official port's pinned README](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan/blob/37026f49824c5cf84062e7c6a5dd71445dcf610f/README.md) describes pretrained-model selection and Vulkan inference. Its MIT licence preserves notices for Xintao Wang and inherited nihui work. The parent Real-ESRGAN repository contains BSD-3-Clause text; ncnn has its own licence and third-party terms.

This route is closer to Vulkan inference, but the inspected source tree alone does not clear the exact packaged model files, release archive, or pinned dependencies. Those remain pending. Use only officially supplied model artifacts if their rights and provenance are established; do not introduce a local conversion pipeline.

It is a single-image restoration/super-resolution candidate, not a relighting backend or proof of temporal stability. Anime-specific models are a poor default for the owner's broad-game objective. Full-frame inference cost and resolution changes need measurement. The executable's image-file interface is suitable for an isolated benchmark, not a proposed per-frame subprocess/disk transport architecture.

## Rights and attribution status

Pinned licence evidence and dispositions are in [the register](LICENSE-REVIEW.md). This investigation reads documentation, licence texts, and repository metadata only. It incorporates no third-party implementation, model, game data, or binary. Conceptual credit is recorded in [third-party notices](../THIRD-PARTY-NOTICES.md).

Before an inference experiment, complete the exact artifact/dependency review and record required notices, acquisition source, integrity hashes, and the permitted use. Source/weights licence findings are candidates for further review, not blanket legal clearance. The existing use-as-supplied policy remains controlling.

## Next concrete task

Prepare a capture-only implementation brief once the local test hosts and toolchain are known. Keep the brief bounded to scoped layer activation, unchanged pass-through, one deterministic effect, lifecycle handling, and diagnostic evidence. Conduct neural-provider artifact review and an isolated benchmark separately. Do not make proprietary backend availability a dependency of the capture prototype.
