# Roadmap and validation

Status: proposed sequence. No implementation or execution is authorized merely by this roadmap, and no dates are assigned.

## Stage 0: feasibility and component selection

- Before implementation study, inspect related projects' licences and notices, pin revisions, and record intended use under [source review rules](SOURCE-REVIEW.md). Review exact code/provenance/input paths only within that basis.
- Resolve each intended incorporation's file-level/dependency terms and attribution; update [third-party notices](../THIRD-PARTY-NOTICES.md). Unclear rights block the affected reuse.
- Select capture/reference experiments independently from neural-provider selection. Find an authorized, unmodified inference path and suitable hardware for the neural stage; lack of a DLSS 5 path does not block transport experiments.
- Verify whether the expected runtime is publicly documented and whether its use and distribution fit the project.
- Select the first two games and record their API, bitness, renderer, and repeatable test scenes.
- Choose initial OS, language/build tooling, capture mechanism, and deployment scope.

**Exit evidence:** a short decision record names the selected route, legal/provenance basis, hardware, test games, dependencies, and known limitations.

**If neural-provider selection is blocked:** proceed to Stage 1 once its capture dependencies, deployment route, hardware, and test pair are reviewed. Keep Stage 2 blocked until a specific model and inference runtime are cleared. Do not change the no-model-modification scope to obtain a working demonstration.

## Stage 1: common Vulkan capture and presentation

Use one native Vulkan game and one DXVK-translated game, in this order:

1. **Pass-through:** acquire and present the frame unchanged. Compare against the game without the layer to detect color, range, orientation, frame-order, or pacing regressions.
2. **Deterministic reference effect:** apply a simple, obvious effect authored for this project to prove that the acquired frame is processed and the processed output is displayed.
3. **Disable and lifecycle checks:** return to original output, resize/recreate resources, and exit cleanly in both games.

Validate GPU synchronization, input/output extents, color conventions, enable/disable behavior, resizing, and cleanup. Report frames acquired, transformed, and presented separately.

**Exit evidence:** repeatable captures and separate baseline/pass-through/reference timings demonstrate both unchanged transport and visible processing in both games. Record differences and unresolved limits. This proves transport only, not neural rendering or native-quality inputs.

## Stage 2: unmodified neural provider

Investigate a small, legally usable neural model with a compatible inference runtime as an early provider. Use the model as supplied; selecting a model format or inference framework alone does not select or clear the model and weights. An available restoration/enhancement model may validate inference without demonstrating neural relighting.

Before integration, record the exact model/weights and runtime versions, separate rights and dependencies, authorized acquisition, documented inputs/output, intended image type, temporal behavior, and supported hardware. Reject candidates requiring prohibited extraction, conversion, or runtime patching.

Integrate the selected provider on hardware it supports. Validate component checks, documented inputs and resets, color conventions, output use, and pass-through after recoverable failures. Do not assume a small model or 1080p resolution guarantees acceptable cost.

**Exit evidence:** processed neural output is visibly used during gameplay, with separate transport and inference costs, identified component versions, and observed temporal limitations. Label the actual enhancement function; inference success is not proof of DLSS 5-like relighting. No compatibility claim based only on initialization.

Stages 2 and 3 may be reordered if provider requirements make input work a prerequisite.

## Stage 3: rendering-input improvements

Compare a documented optical-flow baseline with real engine motion where available, then camera reprojection and supported object-motion recovery. Preserve input provenance in all reports.

Do not assume an existing vector buffer is correct: validate sign, scale, jitter, viewport, and frame alignment.

**Exit evidence:** controlled comparisons show whether each added input path improves temporal stability, and where it regresses. Withdraw heuristics that fail rather than promoting them to universal rules.

## Stage 4: configuration and standalone experience

Add persistent presets, technical profiles, capability explanations, clear component selection, reversible deployment, and diagnostics export.

**Exit evidence:** the same visual preset works in multiple selected games with documented technical adaptation; missing dependencies and unsupported routes produce clear outcomes.

## Stage 5: coverage and optional integration

Investigate additional GPU providers, APIs, 32-bit bridging, external capture, and Linux based on demonstrated demand and available authorized runtimes. Add DXVK-Companion integration after standalone ownership and configuration boundaries are stable.

**Exit evidence:** publish a compatibility matrix tied to exact versions and tested routes. A successful test for one backend/device does not certify a whole vendor.

## Repeatable validation matrix

| Scenario | Observe |
|---|---|
| Stationary scene | Detail change, color/exposure shift, stable output |
| Slow/fast camera pan | Shimmer, trailing, detail attachment |
| Moving/animated objects | Object motion, skinning, occlusion transitions |
| Foliage, particles, transparency | Unstable history, edge contamination |
| Reflections and water | Motion mismatch and changing appearance |
| HUD, subtitles, menus | Readability and mask leakage |
| Camera cut/loading/resize | Correct history reset and resource recreation |
| Disable/missing provider | Original rendering and understandable status |
| Game update/other wrappers | Ownership conflicts and ability to remove only owned changes |

For each run record game/build, scene, route, GPU/driver/OS, display resolution, inference resolution where exposed, provider/component versions, input provenance, preset, and capture method.

Measure baseline without the layer, pass-through, reference-effect, and neural-enhanced modes separately. Report CPU/GPU frame time where measurable, presentation pacing, VRAM, and subjective artifacts. Compare equivalent settings and repeat runs enough to distinguish ordinary variation. Do not rely only on average FPS or screenshots.

GPU tests need real hardware; CI/unit checks can validate contracts and failure handling but cannot certify visual quality, driver behavior, or game compatibility.

## Risks and responses

| Risk | Response |
|---|---|
| No authorized runtime on available hardware | Reference provider and further component research |
| Generic buffer detection fails | Engine profiles, explicit uncertainty, conservative fallback |
| Cross-API transfer dominates cost | Measure a native route or alternate provider before expanding |
| UI separation unreliable | Document final-image mode or leave enhancement unavailable for the affected route |
| Inference exceeds frame budget | Expose supported quality controls and measure; no model modification |
| Feature scope expands into a full remaster engine | Keep asset replacement and unrelated effects outside the initial scope |
| Deployment conflicts or crashes | Isolated test games, ownership tracking, bounded activation, explicit removal validation |

The unfinished multi-agent workflow template will define contribution and execution processes later. This roadmap does not substitute for it.
