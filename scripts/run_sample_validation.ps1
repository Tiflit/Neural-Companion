param(
    [string]$BuildDir = (Join-Path $PSScriptRoot "..\build\bin\Release"),
    [string]$SdkDir = $env:VULKAN_SDK,
    [int]$FrameCount = 180,
    [switch]$IncludeResizeTest = $true,
    [string]$LogFile = (Join-Path $PSScriptRoot "..\build\neural_companion_passthrough.log"),
    [string]$HostLogFile = (Join-Path $PSScriptRoot "..\build\vkcube_host.log"),
    [string]$BaselineLogFile = (Join-Path $PSScriptRoot "..\build\vkcube_baseline.log")
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($SdkDir)) {
    # Check common fallback locations: repository-relative sibling directory, C:\VulkanSDK, Program Files
    $candidateRoots = @(
        (Join-Path $PSScriptRoot "..\..\VulkanSDK"),
        "C:\VulkanSDK",
        (Join-Path $env:ProgramFiles "VulkanSDK")
    )
    foreach ($cand in $candidateRoots) {
        if (-not [string]::IsNullOrWhiteSpace($cand) -and (Test-Path $cand)) {
            if (Test-Path (Join-Path $cand "Bin\vkcube.exe")) {
                $SdkDir = $cand
                break
            }
            $subs = Get-ChildItem -Directory $cand -ErrorAction SilentlyContinue | Sort-Object Name -Descending
            foreach ($s in $subs) {
                if (Test-Path (Join-Path $s.FullName "Bin\vkcube.exe")) {
                    $SdkDir = $s.FullName
                    break
                }
            }
            if (-not [string]::IsNullOrWhiteSpace($SdkDir)) {
                break
            }
        }
    }
}

if ([string]::IsNullOrWhiteSpace($SdkDir) -or -not (Test-Path $SdkDir)) {
    Write-Error "Vulkan SDK path not found. Please set the VULKAN_SDK environment variable or pass -SdkDir <path>."
    exit 1
}

if (-not ([System.Management.Automation.PSTypeName]'NC.Win32').Type) {
    Add-Type -MemberDefinition @'
        [DllImport("user32.dll", EntryPoint = "FindWindowA", CharSet = CharSet.Ansi)]
        public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
        [DllImport("user32.dll")]
        public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
'@ -Name 'Win32' -Namespace 'NC'
}

# Resolve paths to canonical absolute paths
$BuildDir         = [System.IO.Path]::GetFullPath($BuildDir)
$SdkDir           = [System.IO.Path]::GetFullPath($SdkDir)
$LogFile          = [System.IO.Path]::GetFullPath($LogFile)
$HostLogFile      = [System.IO.Path]::GetFullPath($HostLogFile)
$BaselineLogFile  = [System.IO.Path]::GetFullPath($BaselineLogFile)

Write-Host "================================================================="
Write-Host " Neural-Companion: Task 1 Sample Validation & Layer Discovery    "
Write-Host "================================================================="
Write-Host "Build Directory  : $BuildDir"
Write-Host "Vulkan SDK Root  : $SdkDir"
Write-Host "Layer Log Output : $LogFile"
Write-Host "Host Log Output  : $HostLogFile"
Write-Host "Baseline Log     : $BaselineLogFile"
Write-Host "Target Calls     : $FrameCount"
Write-Host ""

# -------------------------------------------------------------------------
# Step 1: Pre-flight Verification of Layer Artifacts & Manifests
# -------------------------------------------------------------------------
Write-Host "=== 1. Pre-flight Layer Discovery Checks ==="

$projectManifest = Join-Path $BuildDir "VkLayer_neural_companion_passthrough.json"
$projectDll      = Join-Path $BuildDir "VkLayer_neural_companion_passthrough.dll"
$valManifest     = Join-Path $SdkDir "Bin\VkLayer_khronos_validation.json"
$valDll          = Join-Path $SdkDir "Bin\VkLayer_khronos_validation.dll"
$vkcube          = Join-Path $SdkDir "Bin\vkcube.exe"
$testDispatchExe = Join-Path $BuildDir "test_layer_dispatch.exe"

$missing = @()

if (-not (Test-Path $projectManifest)) { $missing += "Missing project manifest: $projectManifest" }
if (-not (Test-Path $projectDll))      { $missing += "Missing project DLL: $projectDll" }
if (-not (Test-Path $valManifest))     { $missing += "Missing validation layer manifest: $valManifest" }
if (-not (Test-Path $valDll))          { $missing += "Missing validation layer DLL: $valDll" }
if (-not (Test-Path $vkcube))          { $missing += "Missing sample host: $vkcube" }
if (-not (Test-Path $testDispatchExe)) { $missing += "Missing test_layer_dispatch executable: $testDispatchExe" }

if ($missing.Count -gt 0) {
    Write-Error "Pre-flight layer discovery failed:`n$($missing -join "`n")"
    exit 1
}

Write-Host "  [PASS] Project Layer Manifest : $projectManifest"
Write-Host "  [PASS] Project Layer DLL      : $projectDll"
Write-Host "  [PASS] Validation Manifest    : $valManifest"
Write-Host "  [PASS] Validation DLL         : $valDll"
Write-Host "  [PASS] Cooperating Host       : $vkcube"
Write-Host "  [PASS] Dispatch Test Binary   : $testDispatchExe"
Write-Host ""

# Preserve prior environment
$origLayerPath   = $env:VK_LAYER_PATH
$origLayers      = $env:VK_INSTANCE_LAYERS
$origLogPath     = $env:NC_LOG_PATH
$origLoaderDebug = $env:VK_LOADER_DEBUG

# -------------------------------------------------------------------------
# Step 2: Run Focused Layer Dispatch & Query Semantics Unit Test
# -------------------------------------------------------------------------
Write-Host "=== 2. Running Layer Dispatch & Query Semantics Unit Test ==="
try {
    $sdkBin = Join-Path $SdkDir "Bin"
    $env:VK_LAYER_PATH = "$BuildDir;$sdkBin"
    $env:VK_INSTANCE_LAYERS = "VK_LAYER_NEURALCOMPANION_passthrough"
    $env:NC_LOG_PATH = $LogFile

    # Clean prior log file
    if (Test-Path $LogFile) { Remove-Item $LogFile -Force }

    $pinfo = New-Object System.Diagnostics.ProcessStartInfo
    $pinfo.FileName = $testDispatchExe
    $pinfo.UseShellExecute = $false
    $pinfo.RedirectStandardOutput = $true
    $pinfo.RedirectStandardError = $true

    $dispatchProc = [System.Diagnostics.Process]::Start($pinfo)
    [void]$dispatchProc.WaitForExit(10000)
    $dispatchProc.Refresh()
    $dispatchOut = $dispatchProc.StandardOutput.ReadToEnd()
    $dispatchErr = $dispatchProc.StandardError.ReadToEnd()
    $dispatchExit = $dispatchProc.ExitCode

    Write-Host $dispatchOut
    if ($dispatchExit -ne 0) {
        Write-Error "test_layer_dispatch failed with exit code $($dispatchExit):`n$dispatchErr"
        exit $dispatchExit
    }
    Write-Host "  [PASS] Unit test for layer dispatch, query semantics, and unhandled forwarding passed.`n"
}
finally {
    if ($null -ne $origLayerPath)   { $env:VK_LAYER_PATH = $origLayerPath } else { Remove-Item Env:\VK_LAYER_PATH -ErrorAction SilentlyContinue }
    if ($null -ne $origLayers)      { $env:VK_INSTANCE_LAYERS = $origLayers } else { Remove-Item Env:\VK_INSTANCE_LAYERS -ErrorAction SilentlyContinue }
    if ($null -ne $origLogPath)     { $env:NC_LOG_PATH = $origLogPath } else { Remove-Item Env:\NC_LOG_PATH -ErrorAction SilentlyContinue }
}

# -------------------------------------------------------------------------
# Step 3: Test Run with Both Layers Active (Neural-Companion + Validation)
# -------------------------------------------------------------------------
Write-Host "=== 3. Launching Co-Activated Test (Neural-Companion + Validation) ==="

# Clean prior log files
if (Test-Path $LogFile)     { Remove-Item $LogFile -Force }
if (Test-Path $HostLogFile) { Remove-Item $HostLogFile -Force }

try {
    $sdkBin = Join-Path $SdkDir "Bin"
    $env:VK_LAYER_PATH = "$BuildDir;$sdkBin"
    $env:VK_INSTANCE_LAYERS = "VK_LAYER_NEURALCOMPANION_passthrough;VK_LAYER_KHRONOS_validation"
    $env:VK_LOADER_DEBUG = "layer,info"
    $env:NC_LOG_PATH = $LogFile

    Write-Host "  Configured VK_LAYER_PATH      : $env:VK_LAYER_PATH"
    Write-Host "  Configured VK_INSTANCE_LAYERS : $env:VK_INSTANCE_LAYERS"
    Write-Host "  Configured VK_LOADER_DEBUG    : $env:VK_LOADER_DEBUG"
    Write-Host "  Configured NC_LOG_PATH        : $env:NC_LOG_PATH"

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    $outTmp = "$HostLogFile.out.tmp"
    $errTmp = "$HostLogFile.err.tmp"
    if (Test-Path $outTmp) { Remove-Item $outTmp -Force }
    if (Test-Path $errTmp) { Remove-Item $errTmp -Force }

    $proc = Start-Process -FilePath $vkcube -ArgumentList "--c $FrameCount --validate --suppress_popups" `
        -RedirectStandardOutput $outTmp -RedirectStandardError $errTmp -PassThru

    if ($IncludeResizeTest) {
        $hwnd = [IntPtr]::Zero
        # Bounded poll for window handle: up to 30 attempts of 100ms
        for ($i = 0; $i -lt 30 -and $hwnd -eq [IntPtr]::Zero; $i++) {
            Start-Sleep -Milliseconds 100
            $proc.Refresh()
            $hwnd = $proc.MainWindowHandle
        }
        if ($hwnd -eq [IntPtr]::Zero) {
            $proc.Kill()
            Write-Error "Assertion Failed: Unable to acquire valid window handle (HWND) for resize test within 3000ms"
            exit 1
        }

        # Allow vkcube to complete initialization and enter active rendering loop before triggering resize
        Start-Sleep -Milliseconds 400

        $resizeSuccess = [NC.Win32]::SetWindowPos($hwnd, [IntPtr]::Zero, 100, 100, 680, 680, 0x0040)
        if (-not $resizeSuccess) {
            $proc.Kill()
            Write-Error "Assertion Failed: SetWindowPos failed to resize cooperating sample host window"
            exit 1
        }
        Write-Host "  [PASS] Triggered window resize event (500x500 -> 680x680, HWND: $hwnd)"
    }

    $waitTimeoutMs = 15000
    $exited = $proc.WaitForExit($waitTimeoutMs)
    if (-not $exited) {
        $proc.Kill()
        Write-Error "Assertion Failed: vkcube execution timed out after $waitTimeoutMs ms; process terminated"
        exit 1
    }

    [void]$proc.WaitForExit()
    $proc.Refresh()
    $stopwatch.Stop()
    $exitCode = $proc.ExitCode

    # Combine stdout and stderr into HostLogFile
    $combined = ""
    if (Test-Path $outTmp) { $combined += [System.IO.File]::ReadAllText($outTmp); Remove-Item $outTmp -Force }
    if (Test-Path $errTmp) { $combined += "`n" + [System.IO.File]::ReadAllText($errTmp); Remove-Item $errTmp -Force }
    [System.IO.File]::WriteAllText($HostLogFile, $combined)

    Write-Host "  vkcube execution time : $($stopwatch.ElapsedMilliseconds) ms"
    Write-Host "  vkcube exit code      : $exitCode"

    if ($null -ne $exitCode -and $exitCode -ne 0) {
        Write-Error "vkcube exited with non-zero exit code: $exitCode"
        exit $exitCode
    }
}
finally {
    if ($null -ne $origLayerPath)   { $env:VK_LAYER_PATH = $origLayerPath } else { Remove-Item Env:\VK_LAYER_PATH -ErrorAction SilentlyContinue }
    if ($null -ne $origLayers)      { $env:VK_INSTANCE_LAYERS = $origLayers } else { Remove-Item Env:\VK_INSTANCE_LAYERS -ErrorAction SilentlyContinue }
    if ($null -ne $origLogPath)     { $env:NC_LOG_PATH = $origLogPath } else { Remove-Item Env:\NC_LOG_PATH -ErrorAction SilentlyContinue }
    if ($null -ne $origLoaderDebug) { $env:VK_LOADER_DEBUG = $origLoaderDebug } else { Remove-Item Env:\VK_LOADER_DEBUG -ErrorAction SilentlyContinue }
    Write-Host "  [RESTORED] Prior process environment restored."
}

# -------------------------------------------------------------------------
# Step 4: Validate Layer Execution Evidence & Host Diagnostics
# -------------------------------------------------------------------------
Write-Host "`n=== 4. Validating Execution Evidence & Diagnostics ==="

if (-not (Test-Path $LogFile)) {
    Write-Error "Layer log file was not generated at: $LogFile"
    exit 1
}
if (-not (Test-Path $HostLogFile)) {
    Write-Error "Host/Loader diagnostics log was not generated at: $HostLogFile"
    exit 1
}

$logLines = Get-Content $LogFile
$hostLines = Get-Content $HostLogFile
Write-Host "  Layer log entries generated       : $($logLines.Count)"
Write-Host "  Host/Loader log entries generated : $($hostLines.Count)"

# --- Host / Loader Diagnostics Assertions ---
$ncLayerActive  = ($hostLines | Select-String "VK_LAYER_NEURALCOMPANION_passthrough").Length -gt 0
$valLayerActive = ($hostLines | Select-String "VK_LAYER_KHRONOS_validation").Length -gt 0

# Extract selected physical device from host output and layer log
$hostSelectedGpuLine = $hostLines | Select-String "Selected GPU" | Select-Object -First 1
$layerSelectedGpuLine = $logLines | Select-String "nc_vkCreateDevice: creating logical device on selected physical device:" | Select-Object -First 1

# Check for validation errors or warnings emitted by VK_LAYER_KHRONOS_validation
$valErrors = $hostLines | Where-Object {
    $_ -match "VUID-" -or
    $_ -match "VALIDATION_ERROR" -or
    $_ -match "\[ VVL \] Error" -or
    $_ -match "\[ VVL \] Warning"
}

Write-Host "  Loader: Neural-Companion Active   : $(if ($ncLayerActive) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  Loader: Validation Layer Active   : $(if ($valLayerActive) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  Validation Errors/Warnings Count  : $($valErrors.Count) $(if ($valErrors.Count -eq 0) { '[PASS]' } else { '[FAIL]' })"
if ($hostSelectedGpuLine) {
    Write-Host "  Host Selected GPU                 : $($hostSelectedGpuLine.ToString().Trim())"
}
if ($layerSelectedGpuLine) {
    Write-Host "  Layer Tracked Selected GPU        : $($layerSelectedGpuLine.ToString().Trim())"
}

if (-not $ncLayerActive) {
    Write-Error "Assertion Failed: Loader did not activate VK_LAYER_NEURALCOMPANION_passthrough"
    exit 1
}
if (-not $valLayerActive) {
    Write-Error "Assertion Failed: Loader did not activate VK_LAYER_KHRONOS_validation"
    exit 1
}
if ($valErrors.Count -gt 0) {
    Write-Error "Assertion Failed: Validation layer reported errors/warnings:`n$($valErrors -join "`n")"
    exit 1
}

# --- Layer Log Assertions ---
$hasNegotiate   = ($logLines | Select-String "negotiating loader-layer interface").Length -gt 0
$hasInstance    = ($logLines | Select-String "nc_vkCreateInstance: instance .* successfully created").Length -gt 0
$hasGpuDisc     = ($logLines | Select-String "Discovered physical device").Length -gt 0
$hasDevice      = ($logLines | Select-String "nc_vkCreateDevice: device .* created successfully").Length -gt 0
$hasQueue       = ($logLines | Select-String "nc_vkGetDeviceQueue: mapped queue").Length -gt 0
$hasDestroyInst = ($logLines | Select-String "nc_vkDestroyInstance: instance .* destroyed").Length -gt 0

# Extract swapchain and presentation metrics from layer lifecycle logs
$destroySummaryLine = $logLines | Where-Object { $_ -match "nc_vkDestroyInstance: .* Total presentation calls:" } | Select-Object -Last 1
$totalPresentCalls = 0
$successfulPresentCalls = 0
$totalCreated = 0
$totalDestroyed = 0
if ($destroySummaryLine -match "Total presentation calls:\s*(\d+),\s*successful presentation calls:\s*(\d+),\s*swapchains created:\s*(\d+),\s*swapchains destroyed:\s*(\d+)") {
    $totalPresentCalls      = [int]$matches[1]
    $successfulPresentCalls = [int]$matches[2]
    $totalCreated           = [int]$matches[3]
    $totalDestroyed         = [int]$matches[4]
}

$recreationMatches = $logLines | Where-Object { $_ -match "nc_vkCreateSwapchainKHR: .* RECREATION" }
$recreationCount = $recreationMatches.Count

$minExpectedSuccess = if ($IncludeResizeTest) { $FrameCount - 2 } else { $FrameCount }

Write-Host "  Interface Negotiation (v2)        : $(if ($hasNegotiate) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  Instance Creation                 : $(if ($hasInstance) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  GPU Route Discovery               : $(if ($hasGpuDisc) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  Device & Queue Creation           : $(if ($hasDevice -and $hasQueue) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  Swapchain Creations / Destructions: $totalCreated created / $totalDestroyed destroyed $(if ($totalCreated -gt 0 -and $totalCreated -eq $totalDestroyed) { '[PASS - Balanced]' } else { '[FAIL]' })"
if ($IncludeResizeTest) {
    Write-Host "  Swapchain Recreation Count        : $recreationCount $(if ($recreationCount -ge 1) { '[PASS]' } else { '[FAIL]' })"
}
Write-Host "  Total Presentation Calls          : $totalPresentCalls / $FrameCount target $(if ($totalPresentCalls -eq $FrameCount) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  Successful Presentation Calls     : $successfulPresentCalls / $FrameCount target (min expected: $minExpectedSuccess) $(if ($successfulPresentCalls -ge $minExpectedSuccess) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  Clean Shutdown & Teardown         : $(if ($hasDestroyInst) { '[PASS]' } else { '[FAIL]' })"

if (-not ($hasNegotiate -and $hasInstance -and $hasGpuDisc -and $hasDevice -and $hasQueue -and $hasDestroyInst)) {
    Write-Error "Assertion Failed: Basic layer lifecycle checks failed in log."
    exit 1
}

if ($totalCreated -le 0 -or $totalCreated -ne $totalDestroyed) {
    Write-Error "Assertion Failed: Swapchain lifecycle not balanced ($totalCreated created vs $totalDestroyed destroyed)"
    exit 1
}

if ($IncludeResizeTest -and $recreationCount -lt 1) {
    Write-Error "Assertion Failed: Resize test requested but zero swapchain recreation events observed"
    exit 1
}

if ($totalPresentCalls -ne $FrameCount) {
    Write-Error "Assertion Failed: Total presentation call count ($totalPresentCalls) does not match requested target ($FrameCount)"
    exit 1
}

if ($successfulPresentCalls -lt $minExpectedSuccess) {
    Write-Error "Assertion Failed: Successful presentation call count ($successfulPresentCalls) is below minimum expected ($minExpectedSuccess)"
    exit 1
}

# Print key evidence from log
Write-Host "`n--- Summary Evidence from Layer Log ---"
$logLines | Where-Object {
    $_ -match "nc_vkCreateDevice: creating logical device" -or
    $_ -match "nc_vkCreateSwapchainKHR" -or
    $_ -match "nc_vkQueuePresentKHR: presentation call 1 forwarded" -or
    $_ -match "nc_vkQueuePresentKHR: presentation call $FrameCount forwarded" -or
    $_ -match "nc_vkDestroyInstance"
} | ForEach-Object { Write-Host "    $_" }

# -------------------------------------------------------------------------
# Step 5: Baseline Verification with Neural-Companion Layer Disabled
# -------------------------------------------------------------------------
Write-Host "`n=== 5. Baseline Launch (Neural-Companion Layer Disabled) ==="

if (Test-Path $BaselineLogFile) { Remove-Item $BaselineLogFile -Force }

try {
    # Only validation layer enabled in discovery path and layer list
    $env:VK_LAYER_PATH = Join-Path $SdkDir "Bin"
    $env:VK_INSTANCE_LAYERS = "VK_LAYER_KHRONOS_validation"
    $env:VK_LOADER_DEBUG = "layer,info"

    Write-Host "  Configured VK_LAYER_PATH      : $env:VK_LAYER_PATH"
    Write-Host "  Configured VK_INSTANCE_LAYERS : $env:VK_INSTANCE_LAYERS"
    Write-Host "  Configured VK_LOADER_DEBUG    : $env:VK_LOADER_DEBUG"

    $stopwatchBaseline = [System.Diagnostics.Stopwatch]::StartNew()

    $baseOutTmp = "$BaselineLogFile.out.tmp"
    $baseErrTmp = "$BaselineLogFile.err.tmp"
    if (Test-Path $baseOutTmp) { Remove-Item $baseOutTmp -Force }
    if (Test-Path $baseErrTmp) { Remove-Item $baseErrTmp -Force }

    $procBaseline = Start-Process -FilePath $vkcube -ArgumentList "--c 60 --validate --suppress_popups" `
        -RedirectStandardOutput $baseOutTmp -RedirectStandardError $baseErrTmp -PassThru

    $baselineWaitMs = 10000
    $baselineExited = $procBaseline.WaitForExit($baselineWaitMs)
    if (-not $baselineExited) {
        $procBaseline.Kill()
        Write-Error "Assertion Failed: Baseline vkcube execution timed out after $baselineWaitMs ms"
        exit 1
    }

    [void]$procBaseline.WaitForExit()
    $procBaseline.Refresh()
    $stopwatchBaseline.Stop()
    $exitCodeBaseline = $procBaseline.ExitCode

    $baseCombined = ""
    if (Test-Path $baseOutTmp) { $baseCombined += [System.IO.File]::ReadAllText($baseOutTmp); Remove-Item $baseOutTmp -Force }
    if (Test-Path $baseErrTmp) { $baseCombined += "`n" + [System.IO.File]::ReadAllText($baseErrTmp); Remove-Item $baseErrTmp -Force }
    [System.IO.File]::WriteAllText($BaselineLogFile, $baseCombined)

    Write-Host "  vkcube baseline execution time : $($stopwatchBaseline.ElapsedMilliseconds) ms"
    Write-Host "  vkcube baseline exit code      : $exitCodeBaseline"

    if ($null -ne $exitCodeBaseline -and $exitCodeBaseline -ne 0) {
        Write-Error "Baseline vkcube run failed with exit code: $exitCodeBaseline"
        exit $exitCodeBaseline
    }

    # Baseline loader verification
    $baselineLines = Get-Content $BaselineLogFile
    $baseValActive = ($baselineLines | Select-String "VK_LAYER_KHRONOS_validation").Length -gt 0
    $baseNcActive  = ($baselineLines | Select-String "VK_LAYER_NEURALCOMPANION_passthrough").Length -gt 0

    Write-Host "  Baseline: Validation Layer Active      : $(if ($baseValActive) { '[PASS]' } else { '[FAIL]' })"
    Write-Host "  Baseline: Neural-Companion Inactive    : $(if (-not $baseNcActive) { '[PASS]' } else { '[FAIL]' })"

    if (-not $baseValActive) {
        Write-Error "Assertion Failed: Baseline did not load VK_LAYER_KHRONOS_validation"
        exit 1
    }
    if ($baseNcActive) {
        Write-Error "Assertion Failed: Baseline unexpectedly loaded VK_LAYER_NEURALCOMPANION_passthrough"
        exit 1
    }

    Write-Host "  [PASS] Clean removal and baseline operation verified with activation evidence."
}
finally {
    if ($null -ne $origLayerPath)   { $env:VK_LAYER_PATH = $origLayerPath } else { Remove-Item Env:\VK_LAYER_PATH -ErrorAction SilentlyContinue }
    if ($null -ne $origLayers)      { $env:VK_INSTANCE_LAYERS = $origLayers } else { Remove-Item Env:\VK_INSTANCE_LAYERS -ErrorAction SilentlyContinue }
    if ($null -ne $origLoaderDebug) { $env:VK_LOADER_DEBUG = $origLoaderDebug } else { Remove-Item Env:\VK_LOADER_DEBUG -ErrorAction SilentlyContinue }
    Write-Host "  [RESTORED] Prior process environment restored."
}

Write-Host "`n================================================================="
Write-Host " Task 1 Verification Complete: All Checks Passed!               "
Write-Host "================================================================="
