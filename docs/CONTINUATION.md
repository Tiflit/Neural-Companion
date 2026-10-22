# Continuation checkpoint

Updated: 2026-10-19. This file preserves project continuity independently of an Antigravity conversation. It is a status checkpoint, not evidence of runtime acceptance.

## Current source and research checkpoint

Main `53b232aec479a728164c78990ea71446188cad8a` merged PR #5 and contains explicit Vulkan pass-through code, dispatch tests and a sample-validation harness. This is an inspected source milestone, not an independently repeated coordinator run or proof of rendered-color acquisition, reference processing, neural inference or two-game acceptance. Reacquire current remote state before acting.

Read [curated research](RESEARCH.md), [current source evidence](research/SOURCE-CANDIDATES.md) and [the consolidation record](research/CONSOLIDATION-2026-10-19.md). Earlier planning instructions to begin pass-through from scratch are superseded by review of the existing implementation and evidence. Existing game-isolation and execution gates remain in force. No provider or new implementation task is selected by this update.

The remaining sections preserve the pre-implementation checkpoint and retention/privacy requirements. Historical observations remain attributed and must not be promoted to current validation.

## Read first in a fresh session

1. Confirm this checkout's remote is `Tiflit/Neural-Companion`; inspect branch, HEAD, worktrees, and working-tree status before making changes. Preserve unrelated or uncommitted work.
2. Read this file, [Issue #3](https://github.com/Tiflit/Neural-Companion/issues/3), [Vulkan synchronization](VULKAN-SYNCHRONIZATION.md), [architecture](ARCHITECTURE.md), [source review](SOURCE-REVIEW.md), and [third-party notices](../THIRD-PARTY-NOTICES.md).
3. If repository guidance such as `AGENTS.md` exists in the checkout at that time, read it. No multi-agent workflow template has been imported at this checkpoint.
4. Inspect the retained private local evidence index described below. Missing raw evidence remains unverified; do not reconstruct it by repeating invasive setup.
5. Review the current pass-through source/evidence before preparing a bounded follow-up. The historical milestone below describes the original task, not an instruction to recreate it. This checkpoint does not authorize implementation or game execution.

## Durable baseline

- PRs [#1](https://github.com/Tiflit/Neural-Companion/pull/1), [#2](https://github.com/Tiflit/Neural-Companion/pull/2), and [#4](https://github.com/Tiflit/Neural-Companion/pull/4) are merged.
- PR #4's reviewed head was `569e490495f977bf902dba0db78163fc4a143b3e`; its merge commit is `b5774b07f81fd4730a0d81af32a7d872cbc097e0`.
- At that merge, the repository contained documentation and licence records only. No runtime layer, build target, shader, CI workflow, model, or game content was present.
- Issue #3 remains the open feasibility experiment. A pass-through loader check is an intermediate milestone, not completion of capture/processing or the two-game gate.
- The synchronization document is the reviewed experimental design baseline, not a runtime correctness or general-game compatibility result. Earlier issue/review drafts are historical where superseded by that document.
- ChatGPT coordinates/reviews; Gemini in the correct Neural-Companion Antigravity project performs local implementation/testing. Independent review can be requested for high-risk synchronization work. No automatic delegation or governance-template import is implied.

## Setup evidence and remaining gates

These are owner-supplied Gemini reports, not independently executed Windows checks by the coordinator.

| Area | Retained status | Remaining limitation |
|---|---|---|
| Windows x64 toolchain | C++17/MSVC, CMake, and portable LunarG SDK setup reported | Recheck narrowly against retained records; exact component licences/notices remain required before adoption |
| Vulkan tooling | Corrected validation-required smoke test and separate 180-frame vkcube run reported | Preserve original source/output; vkcube is a tooling sample, not a native-game result |
| Skyrim source/copy | Protected clean backup and ordinary independent working copy reported; 187 matching files before/after reported | Keep inventories private; hardlink checks in the shown trace sampled four files, not every file |
| D3D11 / DXVK route | Native DLL loading and official DXVK loading/swapchain initialization reported | Short startup checks used forced termination; graceful exit, gameplay stability, scene baselines, and performance remain pending |
| Save/config isolation | Earlier active-user shell-folder registry redirection rejected | Dedicated test profile or reviewed effective isolation remains unresolved; no further Skyrim runs |
| Restoration | Expected known-folder paths and unset user/machine Vulkan environment values reported | Exact prior registry types/existence and process overrides were not independently proven |
| Native Vulkan game | Selection remains pending; vkQuake optional if lawful data available | No game purchase or game-data download authorized by Issue #3 |
| Runtime implementation | None at the historical PR #4 merge; pass-through code later merged in PR #5 | Review the current implementation/evidence before extending it |

Tooling pins retained from Issue #3:
- LunarG Vulkan SDK: `1.4.363.0`. This is a tooling pin, not a requirement to enable every header feature or a redistribution approval.
- Official DXVK: `3.1.1`, asset `dxvk-3.1.1.tar.gz`.
- Archive SHA-256: `40565b4a724aadc4433fa4e010b4b23916d9b1f1baeee64e17186db94f54e608`.
- Verify exact retained/downloaded artifacts and applicable terms; do not silently replace pins with latest versions.

## Historical pass-through milestone (implemented in PR #5; review before extending)

Prepare a reviewable task/PR for:
- Minimal Windows x64 explicit Vulkan layer DLL, local manifest, build instructions, and loader negotiation/dispatch.
- Instance/device/queue/swapchain lifecycle tracking needed for the pass-through experiment; cleanup and partial setup failures.
- Unchanged forwarding of presentation, caller structures, supported chains, synchronization, and results. This milestone adds no effect submissions or effect-owned GPU synchronization objects.
- Activation scoped to the selected process launch, with exact prior process environment restored. No persistent registry or machine-wide activation.
- A cooperating Vulkan sample host; verify the actual layer and validation layer are both active, actual GPU/route, sustained presentation, resize/recreation, clean shutdown, and removal.
- Sanitized reproducible build/launch commands, source/dependency provenance, and observed diagnostics. Label unrun tests and remaining limits.

Reference tint follows successful pass-through code/evidence review. Neural inference, public plugin ABI, profile detour DLLs, split-queue support, deployment automation, and DXVK-Companion integration remain outside this milestone. Skyrim execution remains gated on verified profile/save isolation. A sample passing does not satisfy final native-game acceptance.

For later tint work, preserve the reviewed distinctions between command completion, presentation retirement, drain enqueue fact, drain wait outcome, and session failure. Post-submit present enqueue OOM reports terminal device loss on the first affected call. Operational-downstream retirement and actual downstream device-loss cleanup are separate paths. No implementation is validated by these design statements.

## Local retention before clearing Antigravity history

The local operator must complete and verify this checklist. The coordinator cannot inspect the Windows machine from the online repository.

1. Preserve the checkout including uncommitted/untracked work and existing worktrees. Record remote, branch, HEAD, status, and any local-only commits. Do not reset, clean, delete, or rewrite unrelated project history.
2. Copy project-authored smoke sources, build/launch scripts, corrected test wrappers, and relevant Antigravity artifacts into an owner-controlled local archive outside Antigravity's managed history/cache and outside the source checkout. Retain superseded scripts as historical evidence; label the rejected shell-folder redirection method as do-not-run.
3. Retain raw build/validation logs, smoke/sample results, DXVK startup evidence, source/copy hash inventories, and pre/post restoration records locally. Never upload game-derived material as a backup.
4. Write a private local index mapping each retained item to its purpose, local location, test date, expected outcome, and limitations. Record protected backup/working-copy locations, SDK staging, and applicable restoration evidence there, not in public Markdown.
5. Verify retained copies by comparing file counts and SHA-256 hashes and by reopening the index and project-authored source files from the new archive. Record missing material honestly.
6. Keep the protected Skyrim backup and Steam-managed installation unchanged. Clear only the intended Neural-Companion conversation/project history; do not remove shared Antigravity state, other projects, the checkout, SDKs, or local test evidence as collateral cleanup.
7. Once preservation is confirmed, a fresh conversation can use this file and the private local index. Do not repeat broad disk searches, account creation, registry redirection, or game launches to rebuild context.

If raw evidence is missing after a reset, downgrade the associated claim to reported/unverified. Repeat only a specifically reviewed, narrowly scoped check when needed.

## Privacy and content boundaries

- Public material may contain generic game/tool/hardware information approved by the owner. Keep personal account names, profile paths, machine identifiers, credentials, private account/network data, raw local inventories, and unredacted logs private.
- Skyrim binaries, assets, archives, saves, configuration copies, and complete/partial game directories must never be uploaded to GitHub, PR attachments, releases, CI caches/artifacts, cloud storage, external scanners, or any other online service.
- Source and game/SDK staging remain separate. Publish only independently authored project material and reviewed sanitized evidence.
- Original project code uses the repository licence. Check exact third-party source/artifact terms, dependencies, provenance, and notices before reuse. AI rewriting does not erase attribution obligations.
- No neural provider/runtime/weights are selected or approved. Use vendor models as supplied; no extraction, conversion, training, proprietary runtime patching, or hardware-unlock work.

## Public privacy-review snapshot

At the PR #4 merge baseline, the coordinator inspected 13 current files, all 37 unique historical file blobs reachable through 16 commits, four branch tips and three PR head refs, four issue/PR bodies, seven issue comments, two formal review bodies, and commit identities/messages. No inline PR or commit comments, tags, releases, or Actions runs were listed.

No personal local paths, private email addresses, credentials, game files, or attachment links were found in those inspected surfaces. Commit identities used the public project handle and GitHub noreply addresses; NVIDIA's published licensing contact and upstream attribution names are intentional public references. Generic game/hardware/tool information remains public by owner authorization.

This is a bounded inspection, not a guarantee about deleted/unreachable objects, prior edited discussion versions, caches, forks, or an inaccessible separate wiki. No history rewrite or deletion was warranted by the inspected content.
