<#
Run five existing replays twice with immutable profile copies; compare strict
JSONL states. Differences are a valid research result, not silently suppressed.
All inputs and per-run artifacts carry SHA-256 provenance. No user save edits.
#>
[CmdletBinding()]
param(
    [string]$Baseline = 'runtime-data/zen-shop-probe-user',
    [ValidateSet('adventure','versus','minigame','zen','menu')][string[]]$Cases = @('adventure','versus','minigame','zen','menu'),
    [ValidateRange(1,2)][int]$Repeats = 2,
    [string]$OutputRoot,
    [switch]$Sensitivity,
    [switch]$ProfileOnly,
    [string[]]$AdditionalGameArgs = @(),
    [string]$ReplayDirectory,
    [switch]$FrameLocked,
    [switch]$RecordSteps
)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$repo = (Get-Location).Path
if (-not $OutputRoot) { $OutputRoot = Join-Path $repo ('runtime-data/state-dumps/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')) }
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw 'OutputRoot must be a new directory; previous evidence is preserved.' }
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
$exe = Join-Path $repo 'build/pvz-console-edition.exe'
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
$exeHash = Hash $exe
$baselinePath = [IO.Path]::GetFullPath($Baseline)
if (-not (Test-Path -LiteralPath $baselinePath)) { throw "Missing baseline: $baselinePath" }
$baselineCopy = Join-Path $OutputRoot 'baseline/user'
New-Item -ItemType Directory -Path (Split-Path $baselineCopy -Parent) | Out-Null
Copy-Item -LiteralPath $baselinePath -Destination $baselineCopy -Recurse
$freshCopy = Join-Path $OutputRoot 'fresh/user'
New-Item -ItemType Directory -Path $freshCopy -Force | Out-Null
Copy-Item -LiteralPath build/pc_defaults.toml -Destination (Join-Path $freshCopy 'pvz.toml')
$definitions = @{
    adventure = @{replay='adventure_mouse.csv';seconds=70;frames=5000;fresh=$true;mode=0}
    versus = @{replay='feedback_vs_two_pads.csv';seconds=80;frames=6000;fresh=$false;mode=76}
    minigame = @{replay='feedback_heavy_console.csv';seconds=52;frames=3000;fresh=$false;mode=20}
    zen = @{replay='services_zen_regression.csv';seconds=64;frames=5000;fresh=$false;mode=44}
    menu = @{replay='services_menus_regression.csv';seconds=64;frames=5000;fresh=$false;mode=-1}
}
$summary = [ordered]@{schema=1;output_root=$OutputRoot;executable_sha256=$exeHash;runs=[ordered]@{};comparisons=[ordered]@{}}
foreach ($name in $Cases) {
    $case = $definitions[$name]
    $replay = [IO.Path]::GetFullPath((Join-Path 'tests/replays' $case.replay))
    if ($ReplayDirectory) { $replay = [IO.Path]::GetFullPath((Join-Path $ReplayDirectory "$name.csv")) }
    $sourceProfile = if ($case.fresh) {$freshCopy} else {$baselineCopy}
    $inputs = [ordered]@{}
    foreach ($file in @($exe,$replay,(Join-Path $repo 'src/state_dump.cpp'),(Join-Path $repo 'scripts/compare_state_dumps.py'),
            (Join-Path $repo 'scripts/run_determinism_check.ps1'),(Join-Path $repo 'smoke.ps1'),(Join-Path $repo 'config/game.lock.json'),
            (Join-Path $repo 'CMakeLists.txt'),(Join-Path $repo 'config/pc_defaults.toml'),(Join-Path $repo '.cache/pvz-image.bin'))) {
        $inputs[$file] = Hash $file
    }
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repo 'src') -File | Where-Object {$_.Extension -in '.cpp','.h'} | Sort-Object FullName) {
        $inputs[$file.FullName] = Hash $file.FullName
    }
    $inputs['baseline_profile'] = [ordered]@{}
    foreach ($file in Get-ChildItem -LiteralPath $sourceProfile -File -Recurse | Sort-Object FullName) {
        $inputs['baseline_profile'][$file.FullName.Substring($sourceProfile.Length).Replace('\','/')] = Hash $file.FullName
    }
    $manifest = Join-Path $OutputRoot "$name-inputs.json"
    $inputs | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifest -Encoding utf8
    $manifestHash = Hash $manifest
    foreach ($repeat in 1..$Repeats) {
        $runName = "$name-$repeat"
        $directory = Join-Path $OutputRoot $runName
        New-Item -ItemType Directory -Path $directory | Out-Null
        Copy-Item -LiteralPath $sourceProfile -Destination (Join-Path $directory 'user') -Recurse
        $dump = Join-Path $directory 'state.jsonl'
        $gameArgs = @('--pvz_mouse_cursor=true','--pvz_input_trace=true','--pvz_test_unlock=true',
            '--pvz_replay_controller=true','--pvz_vsync=false','--pvz_pause_on_focus_loss=false',
            '--pvz_state_profile=true',"--pvz_cursor_mode=$(if($name -eq 'minigame'){'console'}else{'pc'})",
            "--pvz_input_replay=$replay", "--pvz_state_inputs_sha256=$manifestHash", "--pvz_state_frames=$($case.frames)")
        if (-not $ProfileOnly) { $gameArgs += "--pvz_state_dump=$dump" }
        if ($Sensitivity) { $gameArgs += '--pvz_state_mutate_frame=1800' }
        if ($FrameLocked) { $gameArgs += '--pvz_replay_frame_locked=true' }
        if ($RecordSteps) { $gameArgs += '--pvz_replay_record_steps=true' }
        $gameArgs += $AdditionalGameArgs
        Write-Output "T05 starting $runName ($($case.seconds) seconds)"
        & ./smoke.ps1 -Seconds $case.seconds -UserDataRoot (Join-Path $directory 'user') @gameArgs
        $result = Get-Content runtime-data/smoke-latest.json -Raw | ConvertFrom-Json
        if (-not $result.survived -or $result.fatal.Count -or -not $result.gpu_initialized) { throw "Failed smoke: $runName" }
        if ((Hash $exe) -ne $exeHash) { throw 'Executable changed during the experiment.' }
        $text = ($result.log_files | ForEach-Object { Get-Content -LiteralPath $_ -Raw }) -join "`n"
        if ($text -match 'PvZ (?:frame )?replay.*(?:timed out|readiness failed|missed row|not ready|invalid)|Invalid input replay|Cannot open input replay|state dump (?:cannot|write failed)') { throw "Invalid diagnostics/replay: $runName" }
        if ($text -notmatch 'PvZ (?:frame )?replay completed') { throw "Replay did not complete: $runName" }
        if ($RecordSteps) {
            & python -I scripts/replay_seconds_to_steps.py $replay $result.log_files --output (Join-Path $OutputRoot "$name.csv")
            if ($LASTEXITCODE -ne 0) { throw "Step conversion failed: $runName" }
        }
        $timings = @([regex]::Matches($text, 'PvZ state timing batch (\d+) n (\d+) update_us_p50 ([\d.]+) update_us_p95 ([\d.]+) dump_us_p50 ([\d.]+) dump_us_p95 ([\d.]+)') | ForEach-Object {
            [ordered]@{batch=[int]$_.Groups[1].Value;n=[int]$_.Groups[2].Value;
                update_us_p50=[double]::Parse($_.Groups[3].Value,[Globalization.CultureInfo]::InvariantCulture);
                update_us_p95=[double]::Parse($_.Groups[4].Value,[Globalization.CultureInfo]::InvariantCulture);
                dump_us_p50=[double]::Parse($_.Groups[5].Value,[Globalization.CultureInfo]::InvariantCulture);
                dump_us_p95=[double]::Parse($_.Groups[6].Value,[Globalization.CultureInfo]::InvariantCulture)}
        })
        $run = [ordered]@{input_manifest=$manifest;input_manifest_sha256=$manifestHash;profile=$result.user_data_root;
            log_files=$result.log_files;log_sha256=@($result.log_files|ForEach-Object {Hash $_});timings=$timings;arguments=$gameArgs}
        if (-not $ProfileOnly) {
            & python -I scripts/compare_state_dumps.py $dump $dump --output (Join-Path $directory 'validation.json')
            if ($LASTEXITCODE -ne 0) { throw "Invalid state dump: $runName" }
            $validation = Get-Content (Join-Path $directory 'validation.json') -Raw | ConvertFrom-Json
            if ($validation.a.frames -ne $case.frames) { throw "Incomplete fixed-frame capture: $runName" }
            if ($name -ne 'menu' -and (-not $validation.a.board_frames -or
                $null -eq $validation.a.playing_modes.PSObject.Properties[[string]$case.mode])) { throw "Expected playing mode missing: $runName" }
            $run['dump']=$dump; $run['dump_sha256']=Hash $dump; $run['coverage']=$validation.a
            if ($Sensitivity) {
                & python -I scripts/compare_state_dumps.py "$dump.control.jsonl" $dump --output (Join-Path $directory 'sensitivity.json')
                if ($LASTEXITCODE -ne 1) { throw 'Sensitivity test must report a difference.' }
                $sensitive = Get-Content (Join-Path $directory 'sensitivity.json') -Raw | ConvertFrom-Json
                if ($sensitive.first_difference.frame -ne 1800 -or $sensitive.first_difference.field -ne 'sun' -or
                    $text -notmatch 'state sensitivity applied \+1 sun at frame 1800') { throw 'Sensitivity test missed its target frame/field.' }
                $run['sensitivity']=$sensitive
            }
        }
        $summary.runs[$runName]=$run
        $summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $OutputRoot 'summary.json') -Encoding utf8
        Write-Output "T05 finished $runName"
    }
    if ($Repeats -eq 2 -and -not $ProfileOnly) {
        $comparison = Join-Path $OutputRoot "$name-comparison.json"
        $leftDump = $summary.runs["$name-1"].dump
        $rightDump = $summary.runs["$name-2"].dump
        & python -I scripts/compare_state_dumps.py $leftDump $rightDump --output $comparison
        if ($LASTEXITCODE -gt 1) { throw "Invalid comparison: $name" }
        $summary.comparisons[$name] = Get-Content -LiteralPath $comparison -Raw | ConvertFrom-Json
        $summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $OutputRoot 'summary.json') -Encoding utf8
    }
}
Write-Output "T05 evidence: $OutputRoot/summary.json"
