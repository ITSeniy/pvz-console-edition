[CmdletBinding(DefaultParameterSetName='Deterministic')]
param([ValidateRange(1,3)][int]$Repeats = 2, [string[]]$Modes = @('pc','console'),
    [string]$Baseline = 'runtime-data/zen-shop-probe-user',
 [ValidateSet('b001','b002','b003','b005','b008','b010','b013','b014','b017','b018')][string[]]$NativeBatches=@(),
 [Parameter(ParameterSetName='Deterministic')][switch]$Deterministic,
 [Parameter(Mandatory=$true,ParameterSetName='WallClock')][switch]$WallClock)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
# Fixed before measuring T12; never choose values to make a scenario pass.
$useDeterministic=$PSCmdlet.ParameterSetName -eq 'Deterministic'
$NativeBatches=@($NativeBatches|Sort-Object -Unique)
$regression=[ordered]@{
 mode=$(if($useDeterministic){'deterministic'}else{'wall-clock'})
 seed=$(if($useDeterministic){'42'}else{$null})
 clock_epoch=$(if($useDeterministic){'1792195200'}else{$null})
 time_window=$(if($useDeterministic){'after-replay-title-and-captures'}else{'after-process-start'})
 native_batches=@($NativeBatches)
 runner_sha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
 smoke_sha256=(Get-FileHash -LiteralPath ./smoke.ps1 -Algorithm SHA256).Hash.ToLowerInvariant()
}
$smokeOptions=@{ReplayWindow=$useDeterministic}
$diagnosticArgs=@()
if($useDeterministic){
 $diagnosticArgs=@('--pvz_deterministic_seed=42','--pvz_deterministic_clock=1792195200')
}
$diagnosticArgs+=@($NativeBatches|ForEach-Object {"--pvz_native_$_=true"})
# Only scenarios that still vary with fixed seed/clock need recorded delivery.
$stepReplays=@{
 'pc-1'='shop_pc.steps.csv';'pc-2'='shop_pc.steps.csv';'pc-3'='shop_pc.steps.csv'
 'console-1'='shop_console.steps.csv';'console-2'='shop_console.steps.csv';'console-3'='shop_console.steps.csv'
}
$regression['step_replays']=[ordered]@{}
if($useDeterministic){
 foreach($key in @($stepReplays.Keys|Sort-Object)){
  $path='tests/replays/regression-steps/'+$stepReplays[$key]
  $regression.step_replays[$key]=[ordered]@{replay=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
 }
}

$shopTestRoot = Join-Path (Get-Location) ('runtime-data/shop-regression/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$shopBaselineRoot = Join-Path $shopTestRoot 'baseline'
New-Item -ItemType Directory -Path $shopBaselineRoot | Out-Null
Copy-Item -LiteralPath $Baseline -Destination (Join-Path $shopBaselineRoot 'user') -Recurse
$exeHash=(Get-FileHash build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant()
$shopRuns = @{}
foreach ($shopRepeat in 1..$Repeats) {
    foreach ($shopMode in $Modes) {
        if ($shopMode -notin @('pc','console')) { throw 'Expected pc or console cursor mode.' }
        $shopRunName = "$shopMode-$shopRepeat"
        $shopRunRoot = Join-Path $shopTestRoot $shopRunName
        New-Item -ItemType Directory -Path $shopRunRoot | Out-Null
        Copy-Item -LiteralPath (Join-Path $shopBaselineRoot 'user') -Destination (Join-Path $shopRunRoot 'user') -Recurse
        $shopReplay = [System.IO.Path]::GetFullPath('tests/replays/shop_regression.csv')
        $sourceReplay=$shopReplay
        $replayArgs=@($diagnosticArgs)
        $frameLocked=$useDeterministic -and $stepReplays.ContainsKey($shopRunName)
        if($frameLocked){
         $shopReplay=[System.IO.Path]::GetFullPath('tests/replays/regression-steps/'+$stepReplays[$shopRunName])
         if(-not (Test-Path -LiteralPath $shopReplay)){throw "Missing recorded regression replay: $shopReplay"}
         $replayArgs+='--pvz_replay_frame_locked=true'
        }
        & ./smoke.ps1 @smokeOptions -Seconds 105 -UserDataRoot (Join-Path $shopRunRoot 'user') `
            --pvz_mouse_cursor=true --pvz_input_trace=true --pvz_test_unlock=true --pvz_replay_controller=true `
            "--pvz_cursor_mode=$shopMode" "--pvz_input_replay=$shopReplay" `
            "--pvz_capture_frame=$shopRunRoot/frame.ppm" --pvz_capture_delay=20 --pvz_capture_count=16 --pvz_capture_interval=5 @replayArgs
        $shopResult = Get-Content runtime-data/smoke-latest.json -Raw | ConvertFrom-Json
        if($shopResult.replay_window.timeout){throw 'Replay did not complete in the bounded test window.'}
        if($shopResult.capture_window.timeout){throw 'Scheduled captures did not finish in the bounded test window.'}
        if(-not $shopResult.replay_window.ready){throw 'Replay test window did not become ready.'}
        if (-not $shopResult.survived -or $shopResult.fatal.Count) { throw "Shop run failed: $shopRunName" }
        if($exeHash -ne (Get-FileHash build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant()){throw 'Executable changed during shop regression.'}
        $shopRuns[$shopRunName] = @{ name=$shopRunName; repeat=$shopRepeat; mode=$shopMode;
            log=$shopResult.log_files[0]; log_sha256=(Get-FileHash -LiteralPath $shopResult.log_files[0] -Algorithm SHA256).Hash.ToLowerInvariant();
            replay=$shopReplay; replay_sha256=(Get-FileHash -LiteralPath $shopReplay -Algorithm SHA256).Hash.ToLowerInvariant();
            user_data_root=$shopResult.user_data_root; frames=$shopRunRoot; seconds=105 }
        $shopRuns[$shopRunName].execution=@{mode=$regression.mode;seed=$regression.seed;clock_epoch=$regression.clock_epoch;
         replay_delivery=$(if($frameLocked){'logical-steps'}else{'wall-clock-seconds'});
         source_replay=$sourceReplay;source_replay_sha256=(Get-FileHash -LiteralPath $sourceReplay -Algorithm SHA256).Hash.ToLowerInvariant();
         raw_logs=@($shopResult.raw_log_files);log_adaptation=$shopResult.log_adaptation;smoke_window=$shopResult.replay_window;capture_window=$shopResult.capture_window;game_arguments=@($shopResult.game_arguments)}
        @{ regression=$regression;profile_baseline=(Join-Path $shopBaselineRoot 'user'); built_executable_sha256=$exeHash;runs=$shopRuns } | ConvertTo-Json -Depth 6 |
            Set-Content runtime-data/shop-regression-runs.json -Encoding UTF8
        Write-Output "Completed shop regression $shopRunName"
    }
}
