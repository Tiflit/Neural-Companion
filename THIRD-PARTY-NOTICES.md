# Third-party attribution and incorporation record

Status: planning documentation only, 2026-10-14.

## Incorporated components

None. No third-party implementation code, vendor DLL, model weights, shader, or release asset has been incorporated or distributed by this documentation draft.

The repository's MIT licence applies to project-authored material. It does not relicense future third-party components.

## Research and conceptual references

The planning documentation acknowledges related work in [docs/RESEARCH.md](docs/RESEARCH.md). Pinned upstream licence findings are in [docs/LICENSE-REVIEW.md](docs/LICENSE-REVIEW.md).

The current design discussion references DXVK's API translation, Vulkan layers, feeder/bridge input preparation, external capture tools, and provider separation. These are conceptual references summarized in original wording, not claims that their code has been adopted or that we originated their work.

The initial experiment planning also credits Intel Open Image Denoise and its trained-weights project for documented neural denoising/provider capabilities; Xintao Wang's Real-ESRGAN and Vulkan port, nihui's inherited port work, and Tencent ncnn for restoration/Vulkan inference references; and vkQuake/QuakeSpasm/id Software for the proposed independent Vulkan test host. These are documentation-based research credits, not incorporation or endorsement. Exact source revisions and limits are in [the licence register](docs/LICENSE-REVIEW.md) and [experiment record](docs/INITIAL-EXPERIMENTS.md).

No research citation grants rights to copy source, documentation, screenshots, binaries, or models. No endorsement is implied.

## Required future entries

When work is actually used, record its project/author, immutable source revision and files, our destination, use type, modifications, governing terms, and retained notice locations in the same change. See [source review rules](docs/SOURCE-REVIEW.md).

Include the full required licence/copyright/NOTICE material with any future source and binary distributions. Keep conceptual credits separate from legally required incorporated-component notices. Do not mark a component incorporated merely because it was evaluated.
