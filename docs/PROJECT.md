# Project scope

Status: initial planning draft, 2026-10-14. Read [decisions](DECISIONS.md) for established direction and unresolved choices.

## Objective

Provide a common framework that improves game appearance through existing neural enhancement technology, approaching native integration quality when reliable game data is available and using explicit fallbacks when it is not.

"Universal" means an ambition to cover diverse games and hardware through multiple routes. It does not mean every game, operating system, GPU, or backend will work, or that every route can produce equivalent results.

The primary enhancement is lighting and material/detail appearance. Resolution upscaling and frame generation are separate functions; neither defines the project's identity.

## Established boundaries

- Neural-Companion is an independent repository, product, and release lifecycle.
- DXVK-Companion remains focused on compatibility and automation.
- Optional integration with DXVK-Companion is desirable, but standalone operation is required.
- Existing models and proprietary runtimes are used as supplied. Model training, architecture recovery, weight extraction, binary patching, hardware-unlock patches, and proprietary model conversion are outside scope.
- Long-term hardware scope includes NVIDIA, AMD, and Intel. Actual backend support must be demonstrated independently.
- Reuse of third-party code requires source and licence review; a public repository is not blanket permission to reuse every artifact it references.

Licence and provenance checks precede implementation-level use of other repositories. Material conceptual influence is cited; copied/adapted work retains its authors' notices and applicable terms. No blanket source reuse approval follows from this planning draft. See [source review](SOURCE-REVIEW.md).

## Proposed first scope

Windows desktop, offline games, and a Vulkan interception route. Compatible Direct3D games can reach that route through a separately installed DXVK distribution.

An early prototype should cover one DXVK-translated game and one native Vulkan game. The capture/reference experiment can run without a neural provider. Neural execution should target hardware officially supported by the selected unmodified provider; support for the framework and support for each provider are separate claims. Cross-vendor inference is not a prerequisite for proving the capture and input framework.

Candidate validation hardware includes Intel Arc B580, Intel Arc 140V, and NVIDIA RTX 4060. These are available test targets, not declarations that any specific neural model supports them. If none supports the selected unmodified model, use a pass-through/reference provider until a suitable runtime and device are available.

Linux, other graphics APIs, external capture, and 32-bit game bridging are later scope candidates.

## Intended user experience

The user selects a game and an enhancement preset. The application explains the available route and any missing provider component. Once configured, technical settings can be reused for similar games without asking the user to identify buffers or tune motion parameters.

A technical profile may describe resource formats, frame boundaries, and UI passes without changing the user's chosen visual style. Per-game visual overrides remain optional. The proposed default is a predictable user-selected preset; any future automatic visual-quality adjustment should be opt-in and clearly reported.

The interface should distinguish:

- configured: components and settings are present;
- ready: required checks passed;
- active: frames are actually processed and the result is used;
- degraded: a documented fallback is active;
- unavailable: a required capability is missing;
- disabled: the user has turned enhancement off.

A loaded DLL or successful initialization alone does not establish active enhancement.

## Quality priorities

1. Stable output during camera and object movement.
2. Readable, unaffected UI where separation is supported.
3. Consistent application of a shared visual preset.
4. Predictable performance cost and reversible setup.
5. Clear capability and failure reporting.
6. Increasing game coverage.

An enhancement model may deliberately change appearance; Neural-Companion cannot guarantee preservation of artistic intent. Faithfulness, subjective preference, and temporal stability must be evaluated separately.

## Initial exclusions

No universal motion-vector recovery claim, native-equivalent guarantee, game-asset replacement pipeline, online compatibility promise, anti-cheat bypass, or automatic model/runtime download from unverified mirrors.

API generation alone does not establish whether a game supplies motion vectors; inspect and validate its rendering resources. Compatibility heuristics cannot establish anti-cheat or ban safety. The first prototype remains scoped to offline testing.

A 2D game or emulator may have no meaningful 3D depth or engine motion. Such cases need their own route rather than fabricated "native" inputs.

## Success criteria

A later implementation can be called an initial working prototype only when it demonstrates end-to-end processed output in both selected games, reports the actual input sources, preserves normal gameplay when disabled, and records the cost and observed artifacts under repeatable conditions.

Performance targets will be chosen after baseline measurements. A low original game workload does not guarantee sufficient capacity for neural inference.
