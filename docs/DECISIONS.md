# Decisions and open questions

Status: initial planning record, 2026-10-14.

"Established direction" records explicit owner choices from project planning. "Proposed" items are architecture recommendations for review, not implementation approval or proven capabilities.

## Established direction

| ID | Direction | Consequence |
|---|---|---|
| D-001 | Name: Neural-Companion | Independent identity that can support future enhancement technology |
| D-002 | Independent from DXVK-Companion | Separate repository/lifecycle; standalone operation; optional integration |
| D-003 | DXVK-Companion stays focused on compatibility and automation | Rendering and model execution belong here |
| D-004 | Use vendor models as supplied | No model training, reverse engineering, weight extraction/conversion, or proprietary runtime patching |
| D-005 | Seek the best available game inputs with fallbacks | Native inputs preferred; reconstructed/estimated sources must be identified honestly |
| D-006 | Broad game/vendor compatibility and minimal per-game tuning are goals | Multiple providers/routes may be necessary; no all-games/all-GPUs promise |
| D-007 | Draft documentation before implementation | This change contains planning only |
| D-008 | Multi-agent workflow template will be applied later | No new agent governance or copied workflow template in this draft |
| D-009 | Check licences before implementation inspiration/reuse; credit actual influence and incorporated work | Pinned source review, exact-path obligations, and attribution records; unclear rights block reuse |

## Proposed architecture

| ID | Proposal | What would confirm or change it |
|---|---|---|
| P-001 | Start with a separate Vulkan layer and DXVK route | Two-game transport experiment and deployment feasibility |
| P-002 | Separate capture/input preparation from inference providers | Provider requirements and interoperability experiment |
| P-003 | Windows desktop first | Owner review and availability of test/runtime support |
| P-004 | Reference provider before neural inference when needed | Provides progress if authorized runtime or hardware is unavailable |
| P-005 | Shared visual preset plus technical engine profiles | Cross-game quality tests show acceptable tuning effort |
| P-006 | External capture as later fallback | Need, latency, quality, and deployment tradeoffs justify it |
| P-007 | Conservative component distribution baseline | Exact governing licences permit any later exceptions |

## Questions before runtime implementation

1. Which enhancement provider can be integrated as supplied, with established rights and a documented interface?
2. Which available GPU supports that provider? The B580, 140V, and RTX 4060 are candidates for testing, not assumed DLSS 5 devices.
3. Which native Vulkan and DXVK games form the first repeatable test pair?
4. What minimum OS/driver/Vulkan capabilities are necessary?
5. How will layer activation remain scoped, reversible, and compatible with intended portability?
6. Does the chosen provider require cross-API or cross-process transport?
7. What language/toolchain and dependencies should the runtime and optional application use?
8. Which motion source is the initial baseline, and what evidence will justify adding reconstruction?
9. What measurable overhead and temporal-quality thresholds should govern acceptance after baseline data exists?

## Deferred product decisions

- Public provider/plugin API and configuration schemas.
- Whether DXVK is bundled, obtained separately, or managed through Companion.
- 32-bit game helper architecture.
- Linux and non-Vulkan route ordering.
- Distribution/signing/update mechanisms.
- Supported online-game policy and compatibility handling.
- Telemetry or shared compatibility services; none is proposed by this draft.
- Optional resolution upscaling or frame-generation integrations.
- Multi-agent workflow adoption and CI/release rules.

## Change record

- 2026-10-14: Initial documentation draft. No implementation, runtime adoption, legal clearance, hardware validation, or external outreach performed.

- 2026-10-14: Owner requested a conservative rights/provenance baseline. Added pinned licence triage, source-review and attribution rules, and an empty incorporation register. No code-reuse approval or legal certification granted.
