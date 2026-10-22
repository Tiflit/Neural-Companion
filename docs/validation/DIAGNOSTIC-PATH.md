# Diagnostic environment path — Issue #9

**Profile status:** the human approved `diagnostic-path-cpu-v1`; activation is recorded in [Issue #9](https://github.com/Tiflit/Neural-Companion/issues/9#issuecomment-6346292771). The active `.yml` now contains the reviewed proposal's commands and limits. The retained `.yml.proposed` and preparation text below preserve the original inactive submission. Cloud verification is pending; final revision-bound results supplement this note in PR #10's completion record. No local or full-layer/device execution is authorized by the profile.

## Source repair

The experimental pass-through layer already exists; this change concerns its logging path only. It adds no enhancement effect, provider, loader dispatch or device behavior. README/continuation alignment remains in the separate existing documentation work (#6/#7/#8).

The old `InitLoggingLocked` accepted any positive `GetEnvironmentVariableA` return before passing its fixed buffer to `fopen_s`. [Microsoft's API contract](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getenvironmentvariablea) distinguishes a successful character count excluding the terminator from an insufficient-buffer result including the required terminator space. In the latter case the buffer contents are undefined.

The new internal helper accepts only a positive result strictly below the fixed capacity. An absent, empty or oversized value selects the existing default filename; complete short values retain their existing behavior. The layer calls that same helper before opening a log. No dynamic retry/allocation, longer-path feature, new setting/export, filesystem migration or log-content change. The buffer is also initialized defensively. This is original project-authored code; no upstream implementation was copied. Existing project MIT and SDK notice boundaries remain.

## Prepared verification

Standalone CMake project `tests/diagnostics` compiles the actual helper against the Windows SDK only. It does not configure the root Vulkan build, include Vulkan headers, link the loader, create a device, enable a layer or run the existing GPU harness. Eight CTest cases cover Unset, Empty, Short, MaximumFit, ExactCapacity, OneOverCapacity, LargeValue and StaleAfterLong. They set only their disposable process's `NC_LOG_PATH` to synthetic values; they do not read its previous value, enumerate the environment, open log files or print selected values. Cases run in separate child processes and expire after five seconds each.

At initial submission these tests were prepared, **NOT RUN**. No unchanged-source red run or full-layer compilation was performed. Source/diff self-review is distinct from compiled or runtime evidence. Full layer/Vulkan dispatch, loader, GUI, GPU and game validation remain unverified. The alias-dispatch defect from the backlog is outside this repair.

## Original inactive cloud profile proposal: diagnostic-path-cpu-v1

The file `.github/workflows/diagnostic-path-tests.yml.proposed` is inert: GitHub Actions does not load it as a workflow. Human approval is requested before copying its reviewed content to `.github/workflows/diagnostic-path-tests.yml`. Preparation does not authorize local execution or another host profile.

The proposed active workflow runs one Windows Server 2022 hosted job for relevant pull-request changes and matching main pushes. It uses the hosted image's existing CMake/MSVC/Windows SDK, one build/test worker and a five-minute job timeout. [Runner image inventory](https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md) is a mutable provisioning reference; no exact toolchain version is certified before execution. The timeout/concurrency are enforced workflow settings; no separate memory hard limit is claimed. No package/SDK/model/game download, installation, GPU work, private host files, credentials, artifact upload or cache. Official checkout action is pinned to the v4.2.2 commit; its token is read-only and not persisted. No third-party implementation is incorporated into the layer.

Exact commands in the disposable runner workspace:

```text
cmake -S tests/diagnostics -B build/diagnostics -A x64
cmake --build build/diagnostics --config Debug --parallel 1
ctest --test-dir build/diagnostics -C Debug --parallel 1 --output-on-failure
```

Output stays limited to normal build status, generic runner/build paths and named case outcomes; no environment dump or raw application log. Approval would activate this narrowly filtered CI profile; failed evidence must be retained and investigated, without automatic unchanged-head retries or treating a CPU helper pass as full-layer/device acceptance. Human alone merges.
