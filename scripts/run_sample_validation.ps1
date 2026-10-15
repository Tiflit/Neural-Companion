param(
    [string]$BuildDir = "D:\dev\Neural-Companion\build\bin\Release",
    [string]$SdkDir = "D:\dev\VulkanSDK\1.4.363.0",
    [int]$FrameCount = 180,
    [switch]$IncludeResizeTest = $true,
    [string]$LogFile = "D:\dev\Neural-Companion\build\neural_companion_passthrough.log"
)

$ErrorActionPreference = "Stop"

if (-not ([System.Management.Automation.PSTypeName]'NC.Win32').Type) {
    Add-Type -MemberDefinition @'
        [DllImport("user32.dll", EntryPoint = "FindWindowA", CharSet = CharSet.Ansi)]
        public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
        [DllImport("user32.dll")]
        public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
'@ -Name 'Win32' -Namespace 'NC'
}

Write-Host "================================================================="
Write-Host " Neural-Companion: Task 1 Sample Validation & Layer Discovery    "
Write-Host "================================================================="
Write-Host "Build Directory : $BuildDir"
Write-Host "Vulkan SDK Root : $SdkDir"
Write-Host "Log Output File : $LogFile"
Write-Host "Target Frames   : $FrameCount"
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

$missing = @()

if (-not (Test-Path $projectManifest)) { $missing += "Missing project manifest: $projectManifest" }
if (-not (Test-Path $projectDll))      { $missing += "Missing project DLL: $projectDll" }
if (-not (Test-Path $valManifest))     { $missing += "Missing validation layer manifest: $valManifest" }
if (-not (Test-Path $valDll))          { $missing += "Missing validation layer DLL: $valDll" }
if (-not (Test-Path $vkcube))          { $missing += "Missing sample host: $vkcube" }

if ($missing.Count -gt 0) {
    Write-Error "Pre-flight layer discovery failed:`n$($missing -join "`n")"
    exit 1
}

Write-Host "  [PASS] Project Layer Manifest : $projectManifest"
Write-Host "  [PASS] Project Layer DLL      : $projectDll"
Write-Host "  [PASS] Validation Manifest    : $valManifest"
Write-Host "  [PASS] Validation DLL         : $valDll"
Write-Host "  [PASS] Cooperating Host       : $vkcube"
Write-Host ""

# -------------------------------------------------------------------------
# Step 2: Test Run with Both Layers Active (Neural-Companion + Validation)
# -------------------------------------------------------------------------
Write-Host "=== 2. Launching Co-Activated Test (Neural-Companion + Validation) ==="

# Clean prior log file
if (Test-Path $LogFile) {
    Remove-Item $LogFile -Force
}

# Preserve prior environment
$origLayerPath = $env:VK_LAYER_PATH
$origLayers    = $env:VK_INSTANCE_LAYERS
$origLogPath   = $env:NC_LOG_PATH

try {
    # Set child process environment via VK_LAYER_PATH override
    $sdkBin = Join-Path $SdkDir "Bin"
    $env:VK_LAYER_PATH = "$BuildDir;$sdkBin"
    $env:VK_INSTANCE_LAYERS = "VK_LAYER_NEURALCOMPANION_passthrough;VK_LAYER_KHRONOS_validation"
    $env:NC_LOG_PATH = $LogFile

    Write-Host "  Configured VK_LAYER_PATH      : $env:VK_LAYER_PATH"
    Write-Host "  Configured VK_INSTANCE_LAYERS : $env:VK_INSTANCE_LAYERS"
    Write-Host "  Configured NC_LOG_PATH        : $env:NC_LOG_PATH"

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $proc = Start-Process -FilePath $vkcube -ArgumentList "--c $FrameCount --validate --suppress_popups" -PassThru
    if ($IncludeResizeTest) {
        $hwnd = [IntPtr]::Zero
        for ($i = 0; $i -lt 10 -and $hwnd -eq [IntPtr]::Zero; $i++) {
            Start-Sleep -Milliseconds 100
            $proc.Refresh()
            $hwnd = $proc.MainWindowHandle
        }
        if ($hwnd -ne [IntPtr]::Zero) {
            [void][NC.Win32]::SetWindowPos($hwnd, [IntPtr]::Zero, 100, 100, 680, 680, 0x0040)
            Write-Host "  Triggered window resize event (500x500 -> 680x680, HWND: $hwnd)"
        }
    }
    $proc.WaitForExit()
    $stopwatch.Stop()
    $exitCode = $proc.ExitCode

    Write-Host "  vkcube execution time : $($stopwatch.ElapsedMilliseconds) ms"
    Write-Host "  vkcube exit code      : $exitCode"

    if ($exitCode -ne 0) {
        Write-Error "vkcube exited with non-zero exit code: $exitCode"
        exit $exitCode
    }
}
finally {
    # Strictly restore exact prior process environment
    if ($null -ne $origLayerPath) { $env:VK_LAYER_PATH = $origLayerPath } else { Remove-Item Env:\VK_LAYER_PATH -ErrorAction SilentlyContinue }
    if ($null -ne $origLayers)    { $env:VK_INSTANCE_LAYERS = $origLayers } else { Remove-Item Env:\VK_INSTANCE_LAYERS -ErrorAction SilentlyContinue }
    if ($null -ne $origLogPath)   { $env:NC_LOG_PATH = $origLogPath } else { Remove-Item Env:\NC_LOG_PATH -ErrorAction SilentlyContinue }
    Write-Host "  [RESTORED] Exact prior process environment restored."
}

# -------------------------------------------------------------------------
# Step 3: Validate Layer Execution Evidence
# -------------------------------------------------------------------------
Write-Host "`n=== 3. Validating Layer Execution Evidence ==="

if (-not (Test-Path $LogFile)) {
    Write-Error "Layer log file was not generated at: $LogFile"
    exit 1
}

$logLines = Get-Content $LogFile
Write-Host "  Log entries generated: $($logLines.Count)"

$hasNegotiate   = ($logLines | Select-String "negotiating loader-layer interface").Length -gt 0
$hasInstance    = ($logLines | Select-String "nc_vkCreateInstance: instance .* successfully created").Length -gt 0
$hasGpuDisc     = ($logLines | Select-String "Discovered physical device").Length -gt 0
$hasDevice      = ($logLines | Select-String "nc_vkCreateDevice: device .* created successfully").Length -gt 0
$hasQueue       = ($logLines | Select-String "nc_vkGetDeviceQueue: mapped queue").Length -gt 0
$hasSwapchain   = ($logLines | Select-String "nc_vkCreateSwapchainKHR: created swapchain").Length -gt 0
$hasPresent     = ($logLines | Select-String "nc_vkQueuePresentKHR: frame").Length -gt 0
$hasDestroyInst = ($logLines | Select-String "nc_vkDestroyInstance: instance .* destroyed").Length -gt 0

Write-Host "  Interface Negotiation (v2) : $(if ($hasNegotiate) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  Instance Creation          : $(if ($hasInstance) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  GPU Route Discovery        : $(if ($hasGpuDisc) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  Device & Queue Creation    : $(if ($hasDevice -and $hasQueue) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  Swapchain Creation         : $(if ($hasSwapchain) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  Pass-Through Presentation  : $(if ($hasPresent) { '[PASS]' } else { '[FAIL]' })"
Write-Host "  Clean Shutdown & Teardown  : $(if ($hasDestroyInst) { '[PASS]' } else { '[FAIL]' })"

if (-not ($hasNegotiate -and $hasInstance -and $hasGpuDisc -and $hasDevice -and $hasQueue -and $hasSwapchain -and $hasPresent -and $hasDestroyInst)) {
    Write-Error "One or more layer validation criteria failed in log file."
    exit 1
}

# Print key evidence from log
Write-Host "`n--- Summary Evidence from Log ---"
$logLines | Where-Object {
    $_ -match "Discovered physical device" -or
    $_ -match "nc_vkCreateSwapchainKHR" -or
    $_ -match "nc_vkQueuePresentKHR" -or
    $_ -match "nc_vkDestroyInstance"
} | ForEach-Object { Write-Host "    $_" }

# -------------------------------------------------------------------------
# Step 4: Baseline Verification with Neural-Companion Layer Disabled
# -------------------------------------------------------------------------
Write-Host "`n=== 4. Baseline Launch (Neural-Companion Layer Disabled) ==="

try {
    # Only validation layer enabled in discovery path and layer list
    $env:VK_LAYER_PATH = Join-Path $SdkDir "Bin"
    $env:VK_INSTANCE_LAYERS = "VK_LAYER_KHRONOS_validation"

    Write-Host "  Configured VK_LAYER_PATH      : $env:VK_LAYER_PATH"
    Write-Host "  Configured VK_INSTANCE_LAYERS : $env:VK_INSTANCE_LAYERS"

    $stopwatchBaseline = [System.Diagnostics.Stopwatch]::StartNew()
    $procBaseline = Start-Process -FilePath $vkcube -ArgumentList "--c 60 --validate --suppress_popups" -Wait -PassThru
    $stopwatchBaseline.Stop()
    $exitCodeBaseline = $procBaseline.ExitCode

    Write-Host "  vkcube baseline execution time : $($stopwatchBaseline.ElapsedMilliseconds) ms"
    Write-Host "  vkcube baseline exit code      : $exitCodeBaseline"

    if ($exitCodeBaseline -ne 0) {
        Write-Error "Baseline vkcube run failed with exit code: $exitCodeBaseline"
        exit $exitCodeBaseline
    }
    Write-Host "  [PASS] Clean removal and baseline operation verified."
}
finally {
    if ($null -ne $origLayerPath) { $env:VK_LAYER_PATH = $origLayerPath } else { Remove-Item Env:\VK_LAYER_PATH -ErrorAction SilentlyContinue }
    if ($null -ne $origLayers)    { $env:VK_INSTANCE_LAYERS = $origLayers } else { Remove-Item Env:\VK_INSTANCE_LAYERS -ErrorAction SilentlyContinue }
    Write-Host "  [RESTORED] Exact prior process environment restored."
}

Write-Host "`n================================================================="
Write-Host " Task 1 Verification Complete: All Checks Passed!               "
Write-Host "================================================================="
