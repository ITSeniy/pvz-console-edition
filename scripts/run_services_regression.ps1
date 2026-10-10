[CmdletBinding()]
param([string]$Baseline = 'runtime-data/zen-shop-probe-user', [string[]]$Cases=@(), [switch]$Append)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$testRoot = Join-Path (Get-Location) ('runtime-data/services-regression/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$baselineRoot = Join-Path $testRoot 'baseline'
New-Item -ItemType Directory -Path $baselineRoot | Out-Null
Copy-Item -LiteralPath $Baseline -Destination (Join-Path $baselineRoot 'user') -Recurse
$runs = @{}
$exeHash=(Get-FileHash build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant()
if($Append){
    $previous=Get-Content runtime-data/services-feedback-validation.json -Raw|ConvertFrom-Json
    if($previous.built_executable_sha256 -ne $exeHash){throw 'Cannot append services from another executable.'}
    $baselineRoot=Split-Path $previous.profile_baseline -Parent
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
    & ./smoke.ps1 -Seconds $case.seconds -UserDataRoot (Join-Path $runRoot 'user') `
        --pvz_mouse_cursor=true --pvz_input_trace=true --pvz_test_unlock=true --pvz_replay_controller=true `
        --pvz_cursor_mode=pc "--pvz_input_replay=$replay" "--pvz_capture_frame=$runRoot/frame.ppm" `
        "--pvz_capture_delay=$($case.delay)" "--pvz_capture_count=$($case.count)" "--pvz_capture_interval=$($case.interval)"
    $result = Get-Content runtime-data/smoke-latest.json -Raw | ConvertFrom-Json
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
    @{profile_baseline=(Join-Path $baselineRoot 'user'); runs=$runs} | ConvertTo-Json -Depth 6 |
        Set-Content runtime-data/services-feedback-runs.json -Encoding UTF8
    Write-Output "Completed services regression $($case.name)"
}
