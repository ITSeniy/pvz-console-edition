[CmdletBinding(DefaultParameterSetName='Deterministic')]
param([string]$Baseline = 'runtime-data/zen-shop-probe-user', [string[]]$Cases=@(), [switch]$Append,
 [Parameter(ParameterSetName='Deterministic')][switch]$Deterministic,
 [Parameter(Mandatory=$true,ParameterSetName='WallClock')][switch]$WallClock)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
# Fixed before measuring T12; never choose values to make a scenario pass.
$useDeterministic=$PSCmdlet.ParameterSetName -eq 'Deterministic'
$regression=[ordered]@{
 mode=$(if($useDeterministic){'deterministic'}else{'wall-clock'})
 seed=$(if($useDeterministic){'42'}else{$null})
 clock_epoch=$(if($useDeterministic){'1792195200'}else{$null})
 time_window=$(if($useDeterministic){'after-replay-title-and-captures'}else{'after-process-start'})
 runner_sha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
 smoke_sha256=(Get-FileHash -LiteralPath ./smoke.ps1 -Algorithm SHA256).Hash.ToLowerInvariant()
}
$smokeOptions=@{ReplayWindow=$useDeterministic}
$diagnosticArgs=@()
if($useDeterministic){
 $diagnosticArgs=@('--pvz_deterministic_seed=42','--pvz_deterministic_clock=1792195200')
}
# Only scenarios that still vary with fixed seed/clock need recorded delivery.
$stepReplays=@{settings='services_settings_pad.steps.csv';garden_followup='services_garden_followup.steps.csv'}
$regression['step_replays']=[ordered]@{}
if($useDeterministic){
    foreach($key in @($stepReplays.Keys|Sort-Object)){
        $path='tests/replays/regression-steps/'+$stepReplays[$key]
        $regression.step_replays[$key]=[ordered]@{replay=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
    }
}

$testRoot = Join-Path (Get-Location) ('runtime-data/services-regression/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$baselineRoot = Join-Path $testRoot 'baseline'
New-Item -ItemType Directory -Path $baselineRoot | Out-Null
Copy-Item -LiteralPath $Baseline -Destination (Join-Path $baselineRoot 'user') -Recurse
$runs = @{}
$exeHash=(Get-FileHash build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant()
if($Append){
    $previous=Get-Content runtime-data/services-regression-runs.json -Raw|ConvertFrom-Json
    if($previous.built_executable_sha256 -ne $exeHash){throw 'Cannot append services from another executable.'}
    $baselineRoot=Split-Path $previous.profile_baseline -Parent
    if(($previous.regression|ConvertTo-Json -Compress) -ne ($regression|ConvertTo-Json -Compress)){throw 'Cannot append results from another regression mode or configuration.'}
    foreach($entry in $previous.runs.PSObject.Properties){$runs[$entry.Name]=$entry.Value}
}
foreach ($case in @(
    @{name='zen'; seconds=64; replay='services_zen_regression.csv'; delay=14; count=8; interval=6},
    @{name='garden_followup'; seconds=84; replay='services_garden_followup.csv'; delay=18; count=16; interval=4},
    @{name='garden_polish'; seconds=40; replay='services_garden_polish.csv'; delay=6; count=12; interval=2},
    @{name='menus'; seconds=64; replay='services_menus_regression.csv'; delay=11; count=8; interval=5},
    @{name='settings'; seconds=48; replay='services_settings_pad.csv'; delay=14; count=6; interval=4},
    @{name='tree'; seconds=95; replay='services_tree_regression.csv'; delay=16; count=10; interval=5},
    @{name='tree_feeding'; seconds=44; replay='services_tree_feeding.csv'; delay=16; count=8; interval=3},
    @{name='tree_intro'; seconds=60; replay='services_tree_intro.csv'; delay=15; count=10; interval=4},
    @{name='tree_purchase'; seconds=74; replay='services_tree_purchase.csv'; delay=28; count=10; interval=4},
    @{name='almanac'; seconds=56; replay='services_almanac_regression.csv'; delay=18; count=6; interval=4},
    @{name='quit'; seconds=25; replay='services_quit_confirm.csv'; delay=14; count=2; interval=3}
)) {
    if($Cases.Count -and $case.name -notin $Cases){continue}
    $runRoot = Join-Path $testRoot $case.name
    New-Item -ItemType Directory -Path $runRoot | Out-Null
    Copy-Item -LiteralPath (Join-Path $baselineRoot 'user') -Destination (Join-Path $runRoot 'user') -Recurse
    $replay = [System.IO.Path]::GetFullPath('tests/replays/' + $case.replay)
    # smoke.ps1 treats every early exit as failure. Here Quit's expected exit
    # is validated independently from its log and process exit code below.
    $sourceReplay=$replay
    $replayArgs=@($diagnosticArgs)
    $frameLocked=$useDeterministic -and $stepReplays.ContainsKey($case.name)
    if($frameLocked){
     $replay=[System.IO.Path]::GetFullPath('tests/replays/regression-steps/'+$stepReplays[$case.name])
     if(-not (Test-Path -LiteralPath $replay)){throw "Missing recorded regression replay: $replay"}
     $replayArgs+='--pvz_replay_frame_locked=true'
    }
    & ./smoke.ps1 @smokeOptions -Seconds $case.seconds -UserDataRoot (Join-Path $runRoot 'user') `
        --pvz_mouse_cursor=true --pvz_input_trace=true --pvz_test_unlock=true --pvz_replay_controller=true `
        --pvz_cursor_mode=pc "--pvz_input_replay=$replay" "--pvz_capture_frame=$runRoot/frame.ppm" `
        "--pvz_capture_delay=$($case.delay)" "--pvz_capture_count=$($case.count)" "--pvz_capture_interval=$($case.interval)" @replayArgs
    $result = Get-Content runtime-data/smoke-latest.json -Raw | ConvertFrom-Json
    if($result.replay_window.timeout){throw 'Replay did not complete in the bounded test window.'}
    if($result.capture_window.timeout){throw 'Scheduled captures did not finish in the bounded test window.'}
    if(-not $result.replay_window.ready){throw 'Replay test window did not become ready.'}
    if($exeHash -ne (Get-FileHash build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant()){throw 'Executable changed during services regression.'}
    if ($result.fatal.Count -or -not $result.gpu_initialized) { throw "Services run failed: $($case.name)" }
    if ($case.name -eq 'quit') {
        if ($result.survived -or $result.exit_code -ne 0) { throw 'Quit did not terminate successfully.' }
    } elseif (-not $result.survived) { throw "Unexpected exit: $($case.name)" }
    $runs[$case.name] = @{name=$case.name; log=$result.log_files[0];
        log_sha256=(Get-FileHash -LiteralPath $result.log_files[0] -Algorithm SHA256).Hash.ToLowerInvariant();
        replay=$replay; replay_sha256=(Get-FileHash -LiteralPath $replay -Algorithm SHA256).Hash.ToLowerInvariant();
        frames=$runRoot; expected_frames=$case.count; survived=$result.survived; exit_code=$result.exit_code;
        user_data_root=$result.user_data_root}
    $runs[$case.name].execution=@{mode=$regression.mode;seed=$regression.seed;clock_epoch=$regression.clock_epoch;
     replay_delivery=$(if($frameLocked){'logical-steps'}else{'wall-clock-seconds'});
     source_replay=$sourceReplay;source_replay_sha256=(Get-FileHash -LiteralPath $sourceReplay -Algorithm SHA256).Hash.ToLowerInvariant();
     raw_logs=@($result.raw_log_files);log_adaptation=$result.log_adaptation;smoke_window=$result.replay_window;capture_window=$result.capture_window;game_arguments=@($result.game_arguments)}
    @{regression=$regression;profile_baseline=(Join-Path $baselineRoot 'user'); built_executable_sha256=$exeHash; runs=$runs} | ConvertTo-Json -Depth 6 |
        Set-Content runtime-data/services-regression-runs.json -Encoding UTF8
    # Existing checker consumes this historical name; keep the same manifest.
    Copy-Item runtime-data/services-regression-runs.json runtime-data/services-feedback-runs.json -Force
    Write-Output "Completed services regression $($case.name)"
}
