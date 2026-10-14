# Model and third-party component policy

Status: proposed engineering/release policy implementing the owner's direction to use models as supplied. This is not a legal opinion or certification.

## Scope

Neural-Companion prepares game inputs and invokes existing enhancement technology. It does not train models, reverse engineer proprietary model architectures, extract model weights from binaries, convert proprietary models, patch vendor binaries, or bypass hardware/authentication restrictions.

Preparing image resources, decoding documented game-buffer formats, and reconstructing game motion are distinct from modifying a model. Provider integration must still respect the exact runtime's terms and supported interface.

## Distribution baseline

Until specific rights are established:

- Do not commit or package proprietary vendor DLLs, weights, patched runtimes, or leaked components.
- Do not add community-mirror downloaders for components with unclear distribution permission.
- Do not run proprietary files in public CI or upload them as logs, fixtures, artifacts, or caches.
- Permit a user to select a locally supplied component only after its accepted version, source requirements, and usage basis are documented.
- Never claim that user possession of a DLL, a research disclaimer, or widespread modding establishes authorization.

User-supplied files reduce distribution exposure; they do not establish permission for extraction, reuse in another application, conversion, or cross-vendor execution.

"Use as-is" is a scope constraint, not automatic legal clearance. A component supplied unmodified can still carry restrictions on use and redistribution.

## Component review record

Before approving a component, record:

| Field | Required information |
|---|---|
| Identity | Name, exact version, publisher, source URL, integrity hash where available |
| Function | Capture, reconstruction, inference, deployment, or other role |
| Licence | Exact governing file/terms and version/date |
| Acquisition | User-provided, official download, or permitted redistribution |
| Rights assessment | Use, redistribution, notices, linking/integration, and modifications permitted |
| Hardware/API | Actual documented and tested requirements |
| Dependency chain | Required add-ons, runtimes, weights, and their separate rights |
| Status | Candidate, approved for specified use, blocked, or retired |
| Evidence | Source review, tests, unresolved questions, and reviewer/date |

Open-source licensing of a wrapper or reimplementation does not license proprietary runtimes, extracted weights, or bundled dependencies. Inspect actual release contents and transitive terms before reuse.

## NVIDIA DLSS 5 questions

The published NVIDIA RTX SDK licence permits certain distribution inside applications with conditions, and restricts reverse engineering, modification outside the grant, circumvention, and standalone SDK distribution.

Do not assume those grants cover every file bearing a DLSS name. The exact neural-rendering runtime's origin and governing terms must be established separately, including any game-distributed component.

Questions to resolve before selecting a DLSS 5 backend:

1. Which officially supplied SDK/runtime permits this independent application integration?
2. May the required neural-rendering DLL be used outside its original game?
3. Is redistribution allowed, or must users acquire it through an official route?
4. Does the supported API permit Vulkan interoperability and the intended input preparation?
5. Which GPUs are supported without patches, weight extraction, or runtime modification?
6. Which notices, attribution, notification, or commercial-release obligations apply?

NVIDIA's published licensing contact is nvidia-rtx-license-questions@nvidia.com. No inquiry has been sent and no permission has been obtained by this project.

## Open-source and branding boundaries

The repository's MIT licence covers project-authored work. Third-party components keep their own terms. Review copyleft obligations and proprietary SDK compatibility before linking or copying code; a process boundary alone is not a blanket licensing exemption.

Use vendor and project names factually to describe compatibility. Do not imply endorsement or use logos without an applicable permission basis. Neural-Companion is the independent product identity.

## Release gate

Release review should establish component provenance, permitted acquisition/distribution, required notices, supported integration, and absence of prohibited artifacts from the shipped files. Track unresolved legal questions explicitly and obtain qualified advice where needed.

This gate does not block writing original capture/reconstruction code or testing it with a reference provider. It does block describing an uncertain proprietary backend as legally cleared.

## Sources

Reviewed for planning on 2026-10-14; recheck exact versions before integration:

- [NVIDIA DLSS repository licence](https://github.com/NVIDIA/DLSS/blob/main/LICENSE.txt)
- [NVIDIA RTX SDK licence PDF](https://developer.nvidia.com/downloads/nvidia-rtx-sdks-license-23jan2023pdf)
- [NVIDIA developer DLSS page](https://developer.nvidia.com/rtx/dlss)
- [DLSS5-Autopilot licensing disclosures](https://github.com/Kizzuwatnaa/DLSS5-Autopilot#credits-and-licensing)

Third-party licence commentary is evidence of uncertainty, not permission from NVIDIA.
