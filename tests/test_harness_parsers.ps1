<#
.SYNOPSIS
    Unit and negative acceptance tests for the Vulkan validation harness parser logic.
.DESCRIPTION
    Verifies that acceptance checks correctly distinguish:
    1. Real layer insertion/activation vs mere discovery or environment variable mentions.
    2. Failed layer library loads vs successful activations.
    3. Ordinary validation warnings and errors without VUID tags.
    4. Exact zero exit codes vs null or non-zero exit codes.
    5. Clean baseline runs vs accidental layer leakage.
#>

$ErrorActionPreference = "Stop"

function Test-LayerActivation([string[]]$lines, [string]$layerName) {
    $inserted = ($lines | Where-Object { 
        $_ -match "Insert(ed)?\s+(instance|device)\s+layer\s+[\`"']?$layerName[\`"']?"
    }).Count -gt 0

    $failed = ($lines | Where-Object {
        $_ -match [regex]::Escape($layerName) -and ($_ -match "failed to load" -or $_ -match "Cannot activate" -or $_ -match "Failed to open")
    }).Count -gt 0

    return ($inserted -and (-not $failed))
}

function Get-ValidationDiagnostics([string[]]$lines) {
    return @($lines | Where-Object {
        # Ignore benign loader environment variable notices
        if ($_ -match "\[Vulkan Loader\]\s+WARNING\s+\|\s+LAYER:\s+env var 'VK_INSTANCE_LAYERS' defined") {
            return $false
        }
        # Ignore normal loader info/debug lines
        if ($_ -match "\[Vulkan Loader\]\s+INFO\s+\|\s+LAYER:") {
            return $false
        }
        # Ignore layer's own info logs
        if ($_ -match "\[Neural-Companion Layer\]\[INFO\]") {
            return $false
        }

        # Recognize validation errors and warnings (with or without VUID)
        return (
            $_ -match "\bVUID-" -or
            $_ -match "\bVALIDATION(_ERROR|_WARNING)?\b" -or
            $_ -match "\[\s*VVL\s*\]\s*(Error|Warning)" -or
            $_ -match "\bValidation\s+(Error|Warning|Performance\s+Warning)\b" -or
            $_ -match "^(ERROR|WARNING)\s*:\s*\[" -or
            $_ -match "\[\s*Validation\s*\]" -or
            ($_ -match "VK_LAYER_KHRONOS_validation" -and ($_ -match "error" -or $_ -match "warning"))
        )
    })
}

function Assert-ProcessExitZero($exitCode, [string]$processName) {
    if ($null -eq $exitCode) {
        throw "Assertion Failed: $processName exit code was null (process wait or exit capture failure)"
    }
    if ($exitCode -ne 0) {
        throw "Assertion Failed: $processName exit code was $exitCode (expected 0)"
    }
    return $true
}

function Assert-BaselineIsolation([string[]]$baselineLines) {
    $valActive = Test-LayerActivation $baselineLines "VK_LAYER_KHRONOS_validation"
    $ncActive = Test-LayerActivation $baselineLines "VK_LAYER_NEURALCOMPANION_passthrough"
    $ncMentioned = ($baselineLines | Where-Object { $_ -match "VK_LAYER_NEURALCOMPANION_passthrough" }).Count -gt 0
    $valDiags = Get-ValidationDiagnostics $baselineLines

    if (-not $valActive) {
        throw "Assertion Failed: Baseline did not activate VK_LAYER_KHRONOS_validation"
    }
    if ($ncActive -or $ncMentioned) {
        throw "Assertion Failed: Baseline unexpectedly loaded or mentioned VK_LAYER_NEURALCOMPANION_passthrough"
    }
    if ($valDiags.Count -gt 0) {
        throw "Assertion Failed: Baseline emitted validation errors/warnings: $($valDiags -join '; ')"
    }
    return $true
}

function Assert-LayerPassThroughSummary(
    [string[]]$logLines,
    [int]$expectedTotalCalls,
    [int]$minExpectedSuccessfulCalls = $expectedTotalCalls,
    [bool]$requireBalancedSwapchain = $true
) {
    $summaryLine = $logLines | Where-Object {
        $_ -match "Total presentation calls:\s*(\d+),\s*successful presentation calls:\s*(\d+),\s*swapchains created:\s*(\d+),\s*swapchains destroyed:\s*(\d+)"
    } | Select-Object -Last 1

    if (-not $summaryLine) {
        throw "Assertion Failed: Could not locate destruction summary line in layer log."
    }

    if ($summaryLine -notmatch "Total presentation calls:\s*(\d+),\s*successful presentation calls:\s*(\d+),\s*swapchains created:\s*(\d+),\s*swapchains destroyed:\s*(\d+)") {
        throw "Assertion Failed: Summary line format mismatch: $summaryLine"
    }

    $totalPresentCalls      = [int]$matches[1]
    $successfulPresentCalls = [int]$matches[2]
    $totalCreated           = [int]$matches[3]
    $totalDestroyed         = [int]$matches[4]

    if ($totalPresentCalls -ne $expectedTotalCalls) {
        throw "Assertion Failed: Total presentation calls ($totalPresentCalls) does not match expected ($expectedTotalCalls)"
    }

    if ($successfulPresentCalls -lt $minExpectedSuccessfulCalls) {
        throw "Assertion Failed: Successful presentation calls ($successfulPresentCalls) is below minimum expected ($minExpectedSuccessfulCalls)"
    }

    if ($requireBalancedSwapchain) {
        if ($totalCreated -le 0) {
            throw "Assertion Failed: No swapchains created ($totalCreated)"
        }
        if ($totalCreated -ne $totalDestroyed) {
            throw "Assertion Failed: Swapchain lifecycle not balanced ($totalCreated created vs $totalDestroyed destroyed)"
        }
    }

    return [PSCustomObject]@{
        TotalCalls      = $totalPresentCalls
        SuccessfulCalls = $successfulPresentCalls
        Created         = $totalCreated
        Destroyed       = $totalDestroyed
    }
}

if (-not ([System.Management.Automation.PSTypeName]'NC.Win32Helper').Type) {
    Add-Type -MemberDefinition @'
        [DllImport("user32.dll")]
        public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
        [DllImport("user32.dll")]
        public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);
        [DllImport("user32.dll")]
        public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
        [DllImport("user32.dll", SetLastError = true)]
        public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
        [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        public static extern int GetClassName(IntPtr hWnd, System.Text.StringBuilder lpClassName, int nMaxCount);

        public static IntPtr FindCubeWindow(uint processId) {
            IntPtr foundHwnd = IntPtr.Zero;
            EnumWindows((hWnd, lParam) => {
                uint pid = 0;
                GetWindowThreadProcessId(hWnd, out pid);
                if (pid == processId) {
                    System.Text.StringBuilder sb = new System.Text.StringBuilder(256);
                    GetClassName(hWnd, sb, 256);
                    if (sb.ToString() == "Vulkan Cube") {
                        foundHwnd = hWnd;
                        return false;
                    }
                }
                return true;
            }, IntPtr.Zero);
            return foundHwnd;
        }
'@ -Name 'Win32Helper' -Namespace 'NC'
}

function Invoke-ProcessBounded {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$false)][string]$Arguments = "",
        [int]$TimeoutMs = 15000,
        [scriptblock]$OnProcessStarted = $null
    )

    $pinfo = New-Object System.Diagnostics.ProcessStartInfo
    $pinfo.FileName = $FilePath
    $pinfo.Arguments = $Arguments
    $pinfo.UseShellExecute = $false
    $pinfo.RedirectStandardOutput = $true
    $pinfo.RedirectStandardError = $true

    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $pinfo

    try {
        [void]$proc.Start()

        # Concurrent background draining of both streams to prevent pipe buffer deadlocks per MS documentation
        $outTask = $proc.StandardOutput.ReadToEndAsync()
        $errTask = $proc.StandardError.ReadToEndAsync()

        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        if ($null -ne $OnProcessStarted) {
            & $OnProcessStarted $proc
        }
        $sw.Stop()

        # Deduct time spent in OnProcessStarted callback from remaining wait timeout
        $remainingMs = [Math]::Max(1, [int]($TimeoutMs - $sw.ElapsedMilliseconds))
        $exited = $proc.WaitForExit($remainingMs)
        if (-not $exited) {
            try { $proc.Kill() } catch {}
            try { [void]$proc.WaitForExit(3000) } catch {}
            throw "Process timed out after $TimeoutMs ms and was terminated: $FilePath $Arguments"
        }

        # Bounded wait for process completion and stream flush
        [void]$proc.WaitForExit(3000)
        $proc.Refresh()

        # Bounded read of stream tasks to avoid indefinite deadlocks
        $streamDeadlineMs = 3000
        $tasksCompleted = [System.Threading.Tasks.Task]::WaitAll(@($outTask, $errTask), $streamDeadlineMs)
        if (-not $tasksCompleted) {
            Write-Warning "Invoke-ProcessBounded: stream read tasks did not complete within $streamDeadlineMs ms"
        }

        $stdout = if ($outTask.IsCompleted) { $outTask.Result } else { "" }
        $stderr = if ($errTask.IsCompleted) { $errTask.Result } else { "" }
        $exitCode = $proc.ExitCode

        return [PSCustomObject]@{
            Process        = $proc
            ExitCode       = $exitCode
            StandardOutput = $stdout
            StandardError  = $stderr
            Combined       = "$stdout`n$stderr"
        }
    }
    finally {
        if ($proc) {
            if (-not $proc.HasExited) {
                try { $proc.Kill() } catch {}
                try { [void]$proc.WaitForExit(2000) } catch {}
            }
            $proc.Dispose()
        }
    }
}

# -------------------------------------------------------------------------
# Test Suite Execution
# -------------------------------------------------------------------------
Write-Host "================================================================="
Write-Host " Harness Acceptance & Negative Parser Test Suite"
Write-Host "================================================================="

$passCount = 0
$failCount = 0

function Run-ParserTest([string]$testName, [scriptblock]$block) {
    try {
        & $block
        Write-Host "  [PASS] $testName"
        $script:passCount++
    } catch {
        Write-Host "  [FAIL] ${testName}: $($_.Exception.Message)"
        $script:failCount++
    }
}

# --- Negative Tests for Layer Activation ---

Run-ParserTest "Negative: Layer mentioned only in env var warning must not count as active" {
    $fixture = @(
        "[Vulkan Loader] WARNING | LAYER: env var 'VK_INSTANCE_LAYERS' defined and adding layers `"VK_LAYER_NEURALCOMPANION_passthrough;VK_LAYER_KHRONOS_validation`"",
        "[Vulkan Loader] INFO | LAYER:   Insert instance layer `"VK_LAYER_KHRONOS_validation`""
    )
    $active = Test-LayerActivation $fixture "VK_LAYER_NEURALCOMPANION_passthrough"
    if ($active) { throw "Expected active=false, got true" }
}

Run-ParserTest "Negative: Failed layer library load must not count as active" {
    $fixture = @(
        "[Vulkan Loader] WARNING | LAYER: env var 'VK_INSTANCE_LAYERS' defined and adding layers `"VK_LAYER_NEURALCOMPANION_passthrough`"",
        "[Vulkan Loader] ERROR | LAYER: Failed to load layer library VkLayer_neural_companion_passthrough.dll"
    )
    $active = Test-LayerActivation $fixture "VK_LAYER_NEURALCOMPANION_passthrough"
    if ($active) { throw "Expected active=false, got true" }
}

Run-ParserTest "Positive: Real loader insertion counts as active" {
    $fixture = @(
        "[Vulkan Loader] INFO | LAYER:   Insert instance layer `"VK_LAYER_NEURALCOMPANION_passthrough`" (<build>/layer.dll)",
        "[Vulkan Loader] INFO | LAYER:   Inserted device layer `"VK_LAYER_NEURALCOMPANION_passthrough`" (<build>/layer.dll)"
    )
    $active = Test-LayerActivation $fixture "VK_LAYER_NEURALCOMPANION_passthrough"
    if (-not $active) { throw "Expected active=true, got false" }
}

# --- Negative Tests for Validation Diagnostics ---

Run-ParserTest "Negative: Ordinary validation warning without VUID must be caught" {
    $fixture = @(
        "[ VVL ] Warning: vkCreateDevice: pCreateInfo->pNext structure type 0x1000 is not recognized by validation"
    )
    $diags = Get-ValidationDiagnostics $fixture
    if ($diags.Count -ne 1) { throw "Expected 1 diagnostic, got $($diags.Count)" }
}

Run-ParserTest "Negative: Bracketed validation error without VUID must be caught" {
    $fixture = @(
        "ERROR: [VALIDATION] Code 0 : vkCreateSwapchainKHR: imageExtent (0, 0) is not within minImageExtent and maxImageExtent"
    )
    $diags = Get-ValidationDiagnostics $fixture
    if ($diags.Count -ne 1) { throw "Expected 1 diagnostic, got $($diags.Count)" }
}

Run-ParserTest "Negative: Standard VUID error must be caught" {
    $fixture = @(
        "VALIDATION [ VUID-VkDeviceCreateInfo-ppEnabledExtensionNames-01387 ] Missing extension: VK_KHR_surface"
    )
    $diags = Get-ValidationDiagnostics $fixture
    if ($diags.Count -ne 1) { throw "Expected 1 diagnostic, got $($diags.Count)" }
}

Run-ParserTest "Positive: Benign loader notifications must not be flagged as validation errors" {
    $fixture = @(
        "[Vulkan Loader] WARNING | LAYER: env var 'VK_INSTANCE_LAYERS' defined and adding layers `"VK_LAYER_NEURALCOMPANION_passthrough;VK_LAYER_KHRONOS_validation`"",
        "[Vulkan Loader] INFO | LAYER:   Insert instance layer `"VK_LAYER_KHRONOS_validation`"",
        "[Neural-Companion Layer][INFO] nc_vkCreateInstance: instance created"
    )
    $diags = Get-ValidationDiagnostics $fixture
    if ($diags.Count -ne 0) { throw "Expected 0 diagnostics, got $($diags.Count): $($diags -join '; ')" }
}

# --- Negative Tests for Process Exit Status ---

Run-ParserTest "Negative: Null exit code must be rejected" {
    $caught = $false
    try {
        Assert-ProcessExitZero $null "TestProcess"
    } catch {
        $caught = $true
    }
    if (-not $caught) { throw "Expected exception on null exit code, but succeeded" }
}

Run-ParserTest "Negative: Non-zero exit code must be rejected" {
    $caught = $false
    try {
        Assert-ProcessExitZero 1 "TestProcess"
    } catch {
        $caught = $true
    }
    if (-not $caught) { throw "Expected exception on non-zero exit code, but succeeded" }
}

Run-ParserTest "Positive: Zero exit code is accepted" {
    $res = Assert-ProcessExitZero 0 "TestProcess"
    if (-not $res) { throw "Expected true, got false" }
}

# --- Negative Tests for Baseline Isolation ---

Run-ParserTest "Negative: Baseline leak with Neural-Companion must be rejected" {
    $fixture = @(
        "[Vulkan Loader] INFO | LAYER:   Insert instance layer `"VK_LAYER_KHRONOS_validation`"",
        "[Vulkan Loader] INFO | LAYER:   Insert instance layer `"VK_LAYER_NEURALCOMPANION_passthrough`""
    )
    $caught = $false
    try {
        Assert-BaselineIsolation $fixture
    } catch {
        $caught = $true
    }
    if (-not $caught) { throw "Expected exception on baseline leak, but succeeded" }
}

Run-ParserTest "Negative: Baseline emitting validation warning must be rejected" {
    $fixture = @(
        "[Vulkan Loader] INFO | LAYER:   Insert instance layer `"VK_LAYER_KHRONOS_validation`"",
        "[ VVL ] Warning: vkDestroyInstance: objects remaining before destroy"
    )
    $caught = $false
    try {
        Assert-BaselineIsolation $fixture
    } catch {
        $caught = $true
    }
    if (-not $caught) { throw "Expected exception on baseline validation warning, but succeeded" }
}

Run-ParserTest "Positive: Clean baseline with validation active is accepted" {
    $fixture = @(
        "[Vulkan Loader] INFO | LAYER:   Insert instance layer `"VK_LAYER_KHRONOS_validation`""
    )
    $res = Assert-BaselineIsolation $fixture
    if (-not $res) { throw "Expected true, got false" }
}

# --- Negative & Positive Tests for Presentation Summary Parser ---

Run-ParserTest "Negative: Forwarded presentation calls where all failed must be rejected" {
    $fixture = @(
        "[Neural-Companion Layer][INFO] nc_vkDestroyInstance: cleanly destroying instance 0x01 (Total presentation calls: 60, successful presentation calls: 0, swapchains created: 1, swapchains destroyed: 1)"
    )
    $caught = $false
    try {
        Assert-LayerPassThroughSummary $fixture -expectedTotalCalls 60 -minExpectedSuccessfulCalls 60
    } catch {
        $caught = $true
    }
    if (-not $caught) { throw "Expected exception when successful calls are 0, but succeeded" }
}

Run-ParserTest "Negative: Partial presentation failure below minimum must be rejected" {
    $fixture = @(
        "[Neural-Companion Layer][INFO] nc_vkDestroyInstance: cleanly destroying instance 0x01 (Total presentation calls: 60, successful presentation calls: 58, swapchains created: 1, swapchains destroyed: 1)"
    )
    $caught = $false
    try {
        Assert-LayerPassThroughSummary $fixture -expectedTotalCalls 60 -minExpectedSuccessfulCalls 60
    } catch {
        $caught = $true
    }
    if (-not $caught) { throw "Expected exception when successful calls (58) < expected (60), but succeeded" }
}

Run-ParserTest "Negative: Unbalanced swapchains in destruction summary must be rejected" {
    $fixture = @(
        "[Neural-Companion Layer][INFO] nc_vkDestroyInstance: cleanly destroying instance 0x01 (Total presentation calls: 60, successful presentation calls: 60, swapchains created: 2, swapchains destroyed: 1)"
    )
    $caught = $false
    try {
        Assert-LayerPassThroughSummary $fixture -expectedTotalCalls 60 -minExpectedSuccessfulCalls 60
    } catch {
        $caught = $true
    }
    if (-not $caught) { throw "Expected exception on unbalanced swapchain lifecycle, but succeeded" }
}

Run-ParserTest "Positive: Valid 60 successful presentation calls and balanced swapchains is accepted" {
    $fixture = @(
        "[Neural-Companion Layer][INFO] nc_vkDestroyInstance: cleanly destroying instance 0x01 (Total presentation calls: 60, successful presentation calls: 60, swapchains created: 1, swapchains destroyed: 1)"
    )
    $res = Assert-LayerPassThroughSummary $fixture -expectedTotalCalls 60 -minExpectedSuccessfulCalls 60
    if ($res.SuccessfulCalls -ne 60 -or $res.Created -ne 1) { throw "Unexpected parsed result" }
}

Write-Host "================================================================="
Write-Host " Parser Test Suite: $passCount Passed, $failCount Failed"
Write-Host "================================================================="

if ($failCount -gt 0) {
    exit 1
}
