# Core/KHR dispatch preparation — #11

Codex continued after human merge of PR #10. Verified main `1c5ea06b191814c4af8635a4540a2b773cd63d18` and successful main diagnostic CPU run 39818796271. Inspected actual alias caching, wrappers and official Khronos contracts; confirmed KHR-only dispatch can be advertised then fail through a missing core pointer. Created bounded #11 before edits.

Cached both entry points independently, added distinct thin wrappers and retained shared result/mapping behavior and downstream query gating. Prepared fourteen mock CPU cases compiling the actual production layer; no public testing hook, root build or GPU harness changes. Reviewed pinned official Vulkan-Headers identity/notices for a proposed build-only checkout; no implementation copied or headers vendored. Existing overlapping research/docs work remains separate.

Source/diff self-review only. The new layer-cpu-v1 workflow is inert pending the human's exact profile decision; the existing eight-test diagnostic profile is unchanged and proves no alias behavior. No local builds/tests/apps/helpers, device/game execution, SDK installation or cleanup. Preserve checkouts/evidence and game/profile-isolation holds. Next: human reviews the concrete profile; Codex activates only if approved, then inspects bounded compilation/mock evidence. Full loader/GPU/game acceptance stays separate. Human alone merges.

## Human-approved activation

The human directly approved “Approve layer CPU profile” for the prepared PR #12 proposal. Activated the already scoped .yml path with the reviewed content, omitting only the inactive-status comment and retaining the proposal. Profile permits pinned official header acquisition, actual layer/mock CPU compilation and fourteen cases under the stated job/worker/time limits. Cloud results pending; no local/device execution or other scope expansion. Final revision-bound evidence follows in the PR completion/blocker record.

## First approved cloud results

At head `83fc1c63a8e95dd1468ac47b2a09bbbb24f098e8`, layer run 39820684484 attempt 1 compiled the actual production DLL and passed 14/14 synthetic cases; diagnostic run 39820684194 passed 8/8. Actual PR checkout was `04540524a4265515c43eac59fc2f39e6b7f3d481` over base `1c5ea06b191814c4af8635a4540a2b773cd63d18`. No compiler/test repair or unchanged-head retry was necessary. Updated current status/provenance to reflect approval and these results. Final-head evidence belongs in PR #12; human alone merges. No local, loader, driver, GPU or game validation was performed.
