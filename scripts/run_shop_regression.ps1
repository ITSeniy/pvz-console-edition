[CmdletBinding()]
param([ValidateRange(1,3)][int]$Repeats = 2, [string[]]$Modes = @('pc','console'),
    [string]$Baseline = 'runtime-data/zen-shop-probe-user')
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$shopTestRoot = Join-Path (Get-Location) ('runtime-data/shop-regression/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$shopBaselineRoot = Join-Path $shopTestRoot 'baseline'
New-Item -ItemType Directory -Path $shopBaselineRoot | Out-Null
Copy-Item -LiteralPath $Baseline -Destination (Join-Path $shopBaselineRoot 'user') -Recurse
$shopRuns = @{}
foreach ($shopRepeat in 1..$Repeats) {
    foreach ($shopMode in $Modes) {
        if ($shopMode -notin @('pc','console')) { throw 'Expected pc or console cursor mode.' }
        $shopRunName = "$shopMode-$shopRepeat"
        $shopRunRoot = Join-Path $shopTestRoot $shopRunName
        New-Item -ItemType Directory -Path $shopRunRoot | Out-Null
        Copy-Item -LiteralPath (Join-Path $shopBaselineRoot 'user') -Destination (Join-Path $shopRunRoot 'user') -Recurse
        $shopReplay = [System.IO.Path]::GetFullPath('tests/replays/shop_regression.csv')
        & ./smoke.ps1 -Seconds 105 -UserDataRoot (Join-Path $shopRunRoot 'user') `
            --pvz_mouse_cursor=true --pvz_input_trace=true --pvz_test_unlock=true --pvz_replay_controller=true `
            "--pvz_cursor_mode=$shopMode" "--pvz_input_replay=$shopReplay" `
            "--pvz_capture_frame=$shopRunRoot/frame.ppm" --pvz_capture_delay=20 --pvz_capture_count=16 --pvz_capture_interval=5
        $shopResult = Get-Content runtime-data/smoke-latest.json -Raw | ConvertFrom-Json
        if (-not $shopResult.survived -or $shopResult.fatal.Count) { throw "Shop run failed: $shopRunName" }
        $shopRuns[$shopRunName] = @{ name=$shopRunName; repeat=$shopRepeat; mode=$shopMode;
            log=$shopResult.log_files[0]; log_sha256=(Get-FileHash -LiteralPath $shopResult.log_files[0] -Algorithm SHA256).Hash.ToLowerInvariant();
            replay=$shopReplay; replay_sha256=(Get-FileHash -LiteralPath $shopReplay -Algorithm SHA256).Hash.ToLowerInvariant();
            user_data_root=$shopResult.user_data_root; frames=$shopRunRoot; seconds=105 }
        @{ profile_baseline=(Join-Path $shopBaselineRoot 'user'); runs=$shopRuns } | ConvertTo-Json -Depth 6 |
            Set-Content runtime-data/shop-regression-runs.json -Encoding UTF8
        Write-Output "Completed shop regression $shopRunName"
    }
}
