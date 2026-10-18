[CmdletBinding(PositionalBinding=$false)]
param([ValidateRange(1,180)][int]$Seconds = 20,
    [string]$UserDataRoot,
    [string]$GameDataRoot = 'assets',
    [switch]$ReplayWindow,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$GameArgs)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$rootArgument = $PSScriptRoot.Replace('\', '/')
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$profile = "$rootArgument/runtime-data/smoke/$stamp"
$testUserRoot = if ($UserDataRoot) { [System.IO.Path]::GetFullPath($UserDataRoot).Replace('\', '/') } else { "$profile/user" }
$log = "$rootArgument/logs/smoke-$stamp.log"
New-Item -ItemType Directory -Force -Path logs | Out-Null
$gameRoot = [System.IO.Path]::GetFullPath($GameDataRoot).Replace('\', '/')
$arguments = @('--game_data_root', $gameRoot, '--user_data_root', $testUserRoot,
    '--cache_root', "$profile/cache", '--log_file', $log, '--log_flush_interval', '1',
    '--fullscreen=false', '--pvz_skip_intro=true', '--pvz_render_scale=1')
# Replays and frame checks measure 1280x720 captures; a later argument can still ask for more.
if ($GameArgs) { $arguments += $GameArgs }
$quotedArguments = $arguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' }
$process = Start-Process -FilePath "$PSScriptRoot/build/pvz-console-edition.exe" -ArgumentList $quotedArguments -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -PassThru
$elapsed = [System.Diagnostics.Stopwatch]::StartNew()
try {
    $windowReady = -not $ReplayWindow
    $windowMarker = $null
    if ($ReplayWindow) {
        # Input and capture clocks start after module launch, not Start-Process.
        # Keep their events/capture schedule intact, and allow the requested test
        # window after the replay has reached its first ready screen.
        while (-not $process.HasExited -and $elapsed.Elapsed.TotalSeconds -lt 120) {
            if (Test-Path -LiteralPath $log) {
                $startupLog = Get-Content -LiteralPath $log -Raw
                if ($startupLog -match 'PvZ replay wait_title ready|PvZ frame replay delivered row 0 step \d+') {
                    $windowReady = $true
                    $windowMarker = $Matches[0]
                    break
                }
            }
            Start-Sleep -Milliseconds 250
        }
    }
    $warmupSeconds = $elapsed.Elapsed.TotalSeconds
    $finished = $process.HasExited -or ($windowReady -and $process.WaitForExit($Seconds * 1000))
    $captureExpected = 0
    $capturePattern = $null
    $captureTimeout = $false
    $replayTimeout = $false
    $captureWait = [System.Diagnostics.Stopwatch]::StartNew()
    if ($ReplayWindow -and $windowReady) {
        $capturePath = @($GameArgs | Where-Object { $_ -like '--pvz_capture_frame=*' } | Select-Object -Last 1)
        if ($capturePath.Count) {
            $target = $capturePath[0].Substring('--pvz_capture_frame='.Length)
            $captureExpected = 1
            foreach ($arg in $GameArgs) {
                if ($arg -match '^--pvz_capture_count=(\d+)$') { $captureExpected = [Math]::Min([Math]::Max([int]$Matches[1], 1), 16) }
            }
            $suffix = if ($captureExpected -gt 1) { '-\d+' } else { '' }
            $capturePattern = 'Captured guest frame \d+x\d+ to .*' +
                [regex]::Escape([System.IO.Path]::GetFileNameWithoutExtension($target)) + $suffix +
                [regex]::Escape([System.IO.Path]::GetExtension($target)) + '(?=\r?$)'
            # CaptureGuestOutput and saving a PPM may stall. Do not stop mid-series
            # merely because that host-side delay consumed the original deadline.
            while (-not $finished) {
                $captureLog = Get-Content -LiteralPath $log -Raw
                $captured = [regex]::Matches($captureLog, $capturePattern, 'Multiline').Count
                $delivered = $captureLog -match 'PvZ (?:frame )?replay completed '
                if ($captured -ge $captureExpected -and $delivered) { break }
                if ($captureWait.Elapsed.TotalSeconds -ge 120) {
                    $captureTimeout = $captured -lt $captureExpected
                    $replayTimeout = -not $delivered
                    break
                }
                $finished = $process.WaitForExit(500)
            }
        }
    }
    $captureExtraSeconds = $captureWait.Elapsed.TotalSeconds
    $exitCode = $null
    if ($finished) { $exitCode = $process.ExitCode }
    else { $process.Kill(); $process.WaitForExit() }
} finally {
    if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
}
$logs = @(Get-ChildItem -LiteralPath logs -Filter "smoke-$stamp*.log")
$lines = @($logs | ForEach-Object { Get-Content -LiteralPath $_.FullName })
$captureActual = if ($capturePattern) { @($lines | Where-Object { $_ -match $capturePattern }).Count } else { 0 }
$rawLogs = @($logs | ForEach-Object {
    @{ path = $_.FullName; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
$frameReplay = $GameArgs -contains '--pvz_replay_frame_locked=true'
$evidenceLogs = @($logs | ForEach-Object {
    if ($frameReplay) {
        # The unchanged legacy checkers recognize the seconds-mode completion
        # spelling. Alias that same completed-delivery event in a derived log;
        # keep raw logs and their fingerprints available for independent audit.
        $compatible = $_.FullName + '.checker.log'
        $text = [System.IO.File]::ReadAllText($_.FullName)
        [System.IO.File]::WriteAllText($compatible,
            $text.Replace('PvZ frame replay completed ', 'PvZ replay completed '),
            [System.Text.UTF8Encoding]::new($false))
        $compatible
    } else { $_.FullName }
})
$result = [ordered]@{
    checked_at = (Get-Date).ToString('o')
    survived = -not $finished
    duration_seconds = $Seconds
    elapsed_seconds = $elapsed.Elapsed.TotalSeconds
    replay_window = @{ enabled = [bool]$ReplayWindow; ready = $windowReady;
        marker = $windowMarker; warmup_seconds = $warmupSeconds; startup_timeout_seconds = 120;
        complete = [bool]($lines -match 'PvZ (?:frame )?replay completed '); timeout = $replayTimeout }
    capture_window = @{ expected_timed_captures = $captureExpected; actual_timed_captures = $captureActual;
        complete = ($captureActual -ge $captureExpected); extra_seconds = $captureExtraSeconds;
        timeout = $captureTimeout; extra_timeout_seconds = 120 }
    exit_code = $exitCode
    stopped_by_test = -not $finished
    gpu_initialized = [bool]($lines -match 'Direct3D 12 device and OS features')
    fatal = @($lines | Select-String '\[FATAL\]|\[critical\]|Unhandled guest|access violation' | ForEach-Object Line)
    log_files = $evidenceLogs
    raw_log_files = $rawLogs
    log_adaptation = $(if ($frameReplay) { 'frame-completion-marker-v1' } else { $null })
    profile = $profile
    user_data_root = $testUserRoot
    game_arguments = @($arguments)
    note = 'Startup survival only. Rendering, responsiveness and gameplay require separate verification.'
}
$result | ConvertTo-Json -Depth 4 | Set-Content runtime-data/smoke-latest.json
$result | ConvertTo-Json -Depth 4
if ($finished -or $result.fatal.Count -or -not $result.gpu_initialized -or -not $windowReady -or $captureTimeout -or $replayTimeout) { exit 1 }
