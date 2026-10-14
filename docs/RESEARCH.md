# Related projects and research

Status: documentation-based reconnaissance, 2026-10-14. None of these projects has been executed or technically audited for Neural-Companion. Descriptions summarize upstream claims, not certified capabilities.

No source, dependency, binary, or model from this list has been adopted.

## Priority references

| Project | Relevant idea | Caveat / proposed use |
|---|---|---|
| [DLSS5-Feeder](https://github.com/jlrouzies-fr/DLSS5-Feeder) | Builds a synthetic DLAA request from color, depth, and estimated motion; Vulkan/DXVK and separate 64-bit helper routes | Study transport and input contracts. Its MIT code does not license separate neural add-ons or vendor runtimes. |
| [dlss5-bridge](https://github.com/NIGos/dlss5-bridge) | Bridges D3D11/Vulkan and distinguishes game-provided inputs from substitute inputs | Study resource sharing, fallback quality, and diagnostics; NVIDIA runtime requirements remain separate. |
| [OptiScaler](https://github.com/optiscaler/OptiScaler) | Existing upscaler interception and backend selection | Useful when native hooks exist. Does not solve arbitrary legacy-game input recovery. |
| [wilsjo2 neural-rendering fork](https://github.com/wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass) | Neural-pass placement, processing-resolution controls, and color handling | Experimental fork, distinct from mainline; inspect licences and exact supported runtime before reuse. |
| [bgfx-dlss5-nr](https://github.com/andrewmd5/bgfx-dlss5-nr) | External effect package with DirectX/Vulkan matrix capability selection | Product inspiration. The public repository does not establish complete model provenance or redistribution rights. |
| [NeuralScreen](https://github.com/perseval-BLR/NeuralScreen) | Desktop/window capture, reduced-resolution neural processing, composition | Study external fallback UX and measured support. Runtime bundling in another project is not permission to bundle it here. |
| [UNCANNY](https://github.com/coye2/UNCANNY) | Standalone remastering product, profiles, motion protection, optional neural provider | Broad but unfinished claims; Vulkan/OpenGL parity and AMD proof are explicitly incomplete. Asset reconstruction is beyond our initial scope. |
| [DLSS5-Autopilot](https://github.com/Kizzuwatnaa/DLSS5-Autopilot) | Route selection, component lifecycle, and dependency notices | Deployment inspiration. Downloads from community mirrors do not establish rights. |
| [DLSS5-Patcher](https://github.com/MRHRTZ/DLSS5-Patcher/releases) | Automated installation for multiple game routes | Existing name and product overlap; no reuse decision or runtime-rights validation. |

## Technically close but outside the selected model policy

[dlss-nr-on-intel](https://github.com/Uzbekunknown/dlss-nr-on-intel) describes Intel XMX inference through Vulkan cooperative matrices, a Vulkan presentation layer, and DXVK routes on Windows. Its code is Apache-2.0, but it requires weights extracted from a user-supplied NVIDIA DLL and independently reimplements the inference graph.

That inference route is **not a selected backend**: architecture recovery and weight extraction conflict with this project's use-as-supplied boundary. Potentially separable capture or transport ideas may still be studied subject to code and licence review.

Likewise, patched hardware-unlock runtimes and proprietary model conversion should not become implicit dependencies through another project's installer.

## Rendering foundations

- [DXVK](https://github.com/doitsujin/dxvk): Direct3D 8/9/10/11 translation to Vulkan. API normalization does not normalize camera or motion-buffer semantics.
- [Vulkan loader/layer interface](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md): interception and call-chain rules.
- [DXVK Remix](https://github.com/NVIDIAGameWorks/dxvk-remix): relevant scene tracking and motion-history research. This is a specialized renderer, not a universal replacement for ordinary DXVK.
- [RTX Remix compatibility](https://github.com/NVIDIAGameWorks/rtx-remix/wiki/Compatibility): explains the narrower fixed-function compatibility target.
- [AMD FSR input documentation](https://gpuopen.com/manuals/fsr_sdk/techniques/super-resolution-upscaler/): concrete examples of motion scaling, coverage, depth, and transparency limitations. These are not DLSS 5 requirements.

## Neural-rendering inputs

[NVIDIA's DLSS 5 research description](https://research.nvidia.com/labs/adlr/DLSS5/) lists the current rendered frame, engine motion vectors, temporal state, and artistic controls at inference. Training also uses renderer-derived supervision; that does not mean all those scene attributes are mandatory runtime buffers.

The [developer overview](https://developer.nvidia.com/blog/whats-new-for-game-developers-dlss-5-with-3d-guided-neural-rendering-nvidia-ace-updates-and-new-rtx-kit-capabilities/) describes masking controls and the benefit of richer source rendering. Neither page is a substitute for an authorized integration SDK contract.

## Investigation checklist

For promising candidates, inspect:

1. Actual implementation versus README promises.
2. Exact commit/release and all licences, including bundled files.
3. Where color is captured, whether UI is included, and color-space handling.
4. Whether motion is native, camera-reconstructed, optical-flow-estimated, or absent.
5. Resource sharing and synchronization across API/process boundaries.
6. Real runtime/device restrictions and behavior on missing components.
7. Repeatable performance and motion-quality evidence.
8. Whether the useful part can be reused without prohibited model work.

No current source establishes an authorized, unmodified DLSS 5 runtime usable across NVIDIA, AMD, and Intel devices. Broad framework support and backend hardware support must remain separate.

The announced October 16 NVIDIA model update is not assumed to be available to any community tool or this project. Recheck acquisition, terms, compatibility, and measured behavior when an actual component is selected.
