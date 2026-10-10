[CmdletBinding()]
param([ValidateRange(1,10)][int]$Repeats = 1,
    [string]$BaselineUserRoot = 'runtime-data/input-audit-test-user',
    [string]$GameDataRoot = 'assets')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location $projectRoot
# Preserve one immutable baseline, then give every run a fresh independent copy.
$suiteRoot = Join-Path $projectRoot ('runtime-data/input-audit/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$baselineRoot = Join-Path $suiteRoot 'baseline'
New-Item -ItemType Directory -Path $baselineRoot | Out-Null
# These service scenarios require the prepared profile with Zen/Shop/Almanac
# unlocked. The minigames profile alone intentionally keeps them locked.
$preparedProfile = [System.IO.Path]::GetFullPath($BaselineUserRoot)
if (-not (Test-Path -LiteralPath $preparedProfile -PathType Container)) {
    throw "Prepared input-audit profile not found: $preparedProfile"
}
Copy-Item -LiteralPath $preparedProfile -Destination (Join-Path $baselineRoot 'user') -Recurse
$baselineUserRoot = Join-Path $baselineRoot 'user'
$sourceSnapshot = Join-Path $suiteRoot 'sources.json'
python scripts/input_audit_manifest.py --snapshot $sourceSnapshot
if ($LASTEXITCODE -ne 0) { throw 'Cannot fingerprint audit sources.' }
$sources = Get-Content -LiteralPath $sourceSnapshot -Raw | ConvertFrom-Json -AsHashtable
$profileHashes = @{}
Get-ChildItem -LiteralPath $baselineUserRoot -File -Recurse | ForEach-Object {
    $key = [System.IO.Path]::GetRelativePath($baselineUserRoot, $_.FullName).Replace('\', '/')
    $profileHashes[$key] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}
$exeHash = (Get-FileHash -LiteralPath build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant()
$runs = @(
    @{ name='tree'; seconds=58; capture=25; count=3; interval=10; replay='input_audit_tree_fixture.csv' },
    @{ name='mail'; seconds=36; capture=22; count=1; interval=10; replay='input_audit_services.csv' },
    @{ name='vases'; seconds=55; capture=40; count=1; interval=10; replay='input_audit_vases.csv' },
    @{ name='almanac'; seconds=52; capture=30; count=1; interval=10; replay='input_audit_almanac.csv' },
    @{ name='izombie'; seconds=52; capture=39; count=1; interval=10; replay='input_audit_izombie.csv' }
)
$results = @{}
$attempts = @()
for ($repeat = 1; $repeat -le $Repeats; $repeat++) {
foreach ($run in $runs) {
    $runRoot = Join-Path $suiteRoot "$repeat-$($run.name)"
    New-Item -ItemType Directory -Path $runRoot | Out-Null
    Copy-Item -LiteralPath $baselineUserRoot -Destination $runRoot -Recurse
    $auditUserRoot = Join-Path $runRoot 'user'
    & .\smoke.ps1 -Seconds $run.seconds -UserDataRoot $auditUserRoot -GameDataRoot $GameDataRoot `
        --pvz_mouse_cursor=true --pvz_cursor_mode=pc --pvz_input_trace=true `
        "--pvz_input_replay=tests/replays/$($run.replay)" `
        "--pvz_capture_frame=$runRoot/frame.ppm" `
        "--pvz_capture_delay=$($run.capture)" "--pvz_capture_count=$($run.count)" `
        "--pvz_capture_interval=$($run.interval)"
    $result = Get-Content runtime-data/smoke-latest.json -Raw | ConvertFrom-Json
    if (-not $result.survived -or $result.fatal.Count -gt 0) { throw "Native run failed: $($run.name)" }
    if (-not (Select-String -LiteralPath $result.log_files[0] -SimpleMatch 'PvZ edition: full (license mask 1')) {
        throw 'These service scenarios require a validated Full import. Pass its directory with -GameDataRoot.'
    }
    if ((Get-FileHash -LiteralPath build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant() -ne $exeHash) {
        throw 'Executable changed during the audit runs.'
    }
    $evidence = @{ name=$run.name; repeat=$repeat; log=$result.log_files[0];
        replay="tests/replays/$($run.replay)"; seconds=$run.seconds; user_data_root=$auditUserRoot;
        game_data_root=[System.IO.Path]::GetFullPath($GameDataRoot);
        log_sha256=(Get-FileHash -LiteralPath $result.log_files[0] -Algorithm SHA256).Hash.ToLowerInvariant();
        replay_sha256=(Get-FileHash -LiteralPath "tests/replays/$($run.replay)" -Algorithm SHA256).Hash.ToLowerInvariant() }
    $results[$run.name] = $evidence
    $attempts += $evidence
    [ordered]@{ built_executable_sha256=$exeHash; source_sha256=$sources; profile_baseline=$baselineUserRoot;
        profile_sha256=$profileHashes; rng_seed=$null; runs=$results; attempts=$attempts;
        completed=$attempts.Count -eq ($runs.Count * $Repeats) } |
        ConvertTo-Json -Depth 8 | Set-Content runtime-data/input-audit-runs.json -Encoding UTF8
    python scripts/input_audit_manifest.py --verify runtime-data/input-audit-runs.json
    if ($LASTEXITCODE -ne 0) { throw 'Audit evidence changed during the run.' }
}
}
