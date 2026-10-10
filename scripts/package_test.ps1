[CmdletBinding()]
param([string]$OutputDirectory = 'dist', [switch]$IncludeMinigamesProfile, [switch]$TestUnlock)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location $projectRoot
if ($TestUnlock -and -not $IncludeMinigamesProfile) { throw 'TestUnlock packaging requires the minigames test profile.' }
$outputRoot = [System.IO.Path]::GetFullPath($OutputDirectory)
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$name = "pvz-console-edition-test-$stamp"
$packageRoot = Join-Path $outputRoot $name
$archivePath = Join-Path $outputRoot "$name.zip"
if ((Test-Path -LiteralPath $packageRoot) -or (Test-Path -LiteralPath $archivePath)) {
    throw 'Package output already exists. Run again with a new timestamp or output directory.'
}
$gameLock = Get-Content config/game.lock.json -Raw | ConvertFrom-Json
if ((Get-FileHash assets/default.xex -Algorithm SHA256).Hash -ne $gameLock.xex_sha256) {
    throw 'Game image does not match config/game.lock.json.'
}
$required = @('build/pvz-console-edition.exe','build/rexruntimerd.dll','build/rexgpu-xenosrd.dll',
    'build/TracyClientrd.dll','build/pc_defaults.toml','assets/main.pak','assets/en.pak')
foreach ($file in $required) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing package file: $file" }
}
if ($IncludeMinigamesProfile) {
    foreach ($file in @('Start-Minigames.bat',
        'runtime-data/minigames-test/user/584109FF/profile/User/63E83FFF',
        'runtime-data/minigames-mouse-validation.json',
        'runtime-data/heavy-weapon-fire-buy-latest.json',
        'runtime-data/heavy-weapon-deluxe-validation.json',
        'runtime-data/heavy-weapon-reference.json',
        'runtime-data/input-coverage-validation.json',
        'docs/input-coverage.md',
        'research/input-coverage/handlers.json',
        'research/input-coverage/handlers.md',
        'research/input-coverage/android-families.json')) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing minigames file: $file" }
    }
    foreach ($report in @('runtime-data/minigames-mouse-validation.json',
        'runtime-data/heavy-weapon-fire-buy-latest.json',
        'runtime-data/heavy-weapon-deluxe-validation.json',
        'runtime-data/item-drag-validation.json',
        'runtime-data/services-mouse-validation.json',
        'runtime-data/input-coverage-validation.json',
        'runtime-data/heavy-weapon-latest.json')) {
        $result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
        if ($result.passed -ne $result.total -or $result.total -le 0) {
            throw "Validation did not pass: $report"
        }
    }
    if ($TestUnlock) {
        python scripts/input_audit_manifest.py --verify runtime-data/test-unlock-validation.json
        if ($LASTEXITCODE -ne 0) { throw 'Test unlock validation belongs to a different build.' }
        $unlockValidation = Get-Content runtime-data/test-unlock-validation.json -Raw | ConvertFrom-Json
        if ($unlockValidation.passed -ne $unlockValidation.total -or $unlockValidation.total -le 0) {
            throw 'Test unlock validation did not pass.'
        }
    } else {
    $inputAudit = Get-Content runtime-data/input-coverage-validation.json -Raw | ConvertFrom-Json
    if ($inputAudit.built_executable_sha256 -ne (Get-FileHash build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant()) {
        throw 'Input audit belongs to a different executable. Run scripts/run_input_audit.ps1 and scripts/check_input_coverage.py.'
    }
    foreach ($currentReport in @('runtime-data/input-coverage-validation.json', 'runtime-data/mouse-transport-validation.json')) {
        python scripts/input_audit_manifest.py --verify $currentReport
        if ($LASTEXITCODE -ne 0) { throw "Stale mouse-hook validation: $currentReport" }
        $currentResult = Get-Content -LiteralPath $currentReport -Raw | ConvertFrom-Json
        if ($currentResult.passed -ne $currentResult.total -or $currentResult.total -le 0) {
            throw "Mouse-hook validation did not pass: $currentReport"
        }
    }
    }
    python scripts/input_audit_manifest.py --verify runtime-data/shop-regression-validation.json
    if ($LASTEXITCODE -ne 0) { throw 'Shop regression validation belongs to a different build.' }
    $shopValidation = Get-Content runtime-data/shop-regression-validation.json -Raw | ConvertFrom-Json
    if ($shopValidation.passed -ne $shopValidation.total -or $shopValidation.total -le 0) {
        throw 'Shop regression validation did not pass.'
    }
    python scripts/input_audit_manifest.py --verify runtime-data/services-feedback-validation.json
    if ($LASTEXITCODE -ne 0) { throw 'Services feedback validation belongs to a different build.' }
    $servicesValidation = Get-Content runtime-data/services-feedback-validation.json -Raw | ConvertFrom-Json
    if ($servicesValidation.passed -ne $servicesValidation.total -or $servicesValidation.total -le 0) {
        throw 'Services feedback validation did not pass.'
    }
    foreach ($report in @('runtime-data/feedback-regression-validation.json', 'runtime-data/multiplayer-regression-validation.json')) {
        python scripts/input_audit_manifest.py --verify $report
        if ($LASTEXITCODE -ne 0) { throw "Feedback validation belongs to a different build: $report" }
        $result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
        if ($result.passed -ne $result.total -or $result.total -le 0) { throw "Feedback validation did not pass: $report" }
    }
}
$exeTime = (Get-Item build/pvz-console-edition.exe).LastWriteTimeUtc
if (Get-ChildItem src -Recurse -File | Where-Object { $_.LastWriteTimeUtc -gt $exeTime }) {
    throw 'Sources are newer than pvz-console-edition.exe. Run build.bat first.'
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot = & $vswhere -latest -products * -property installationPath
$redistRoot = Join-Path $vsRoot 'VC/Redist/MSVC'
$crt = Get-ChildItem -LiteralPath $redistRoot -Directory -Recurse |
    Where-Object { $_.Name -match '^Microsoft\.VC\d+\.CRT$' -and $_.Parent.Name -eq 'x64' } |
    Sort-Object FullName -Descending | Select-Object -First 1
if (-not $crt) { throw 'Visual C++ x64 app-local redistributable was not found.' }
New-Item -ItemType Directory -Path $packageRoot | Out-Null
New-Item -ItemType Directory -Path (Join-Path $packageRoot 'build'),(Join-Path $packageRoot 'config') | Out-Null
foreach ($file in $required | Where-Object { $_.StartsWith('build/') }) {
    Copy-Item -LiteralPath $file -Destination (Join-Path $packageRoot $file)
}
Get-ChildItem -LiteralPath $crt.FullName -Filter '*.dll' -File |
    Copy-Item -Destination (Join-Path $packageRoot 'build')
Copy-Item -LiteralPath assets -Destination (Join-Path $packageRoot 'assets') -Recurse
Copy-Item -LiteralPath run.ps1,README.md -Destination $packageRoot
Copy-Item -LiteralPath Import-Game.ps1 -Destination $packageRoot
New-Item -ItemType Directory -Path (Join-Path $packageRoot 'scripts') | Out-Null
Copy-Item -LiteralPath scripts/import_game.py,scripts/extract_stfs.py -Destination (Join-Path $packageRoot 'scripts')
New-Item -ItemType Directory -Force -Path (Join-Path $packageRoot 'docs') | Out-Null
Copy-Item -LiteralPath docs/distribution.md -Destination (Join-Path $packageRoot 'docs')
Copy-Item -LiteralPath docs/mouse-hook-implementation-2026-10-09.md,docs/mouse-hook-audit-2026-10-09.md -Destination (Join-Path $packageRoot 'docs')
# Ownership declarations are personal to the importer, including in tester ZIPs.
$receipt = Join-Path $packageRoot 'assets/.pvz-import.toml'
if (Test-Path -LiteralPath $receipt) { Remove-Item -LiteralPath $receipt }
Copy-Item -LiteralPath config/game.lock.json -Destination (Join-Path $packageRoot 'config/game.lock.json')
Copy-Item -LiteralPath packaging/Start.bat,packaging/TESTING.md -Destination $packageRoot
$testResults = if ($TestUnlock) { 'packaging/TEST-UNLOCK-RESULTS.md' } else { 'packaging/MOUSE-HOOK-TEST-RESULTS.md' }
Copy-Item -LiteralPath $testResults -Destination (Join-Path $packageRoot 'TEST-RESULTS.md')
if ($IncludeMinigamesProfile) {
    New-Item -ItemType Directory -Force -Path (Join-Path $packageRoot 'docs'),
        (Join-Path $packageRoot 'runtime-data/minigames-test/user'),
        (Join-Path $packageRoot 'validation') | Out-Null
    Copy-Item -LiteralPath Start-Minigames.bat -Destination $packageRoot
    foreach ($folder in @('584109FF','B13EBABEBABEBABE')) {
        Copy-Item -LiteralPath "runtime-data/minigames-test/user/$folder" -Recurse -Destination (
            Join-Path $packageRoot 'runtime-data/minigames-test/user')
    }
    # Ship standard settings rather than the workstation's runtime configuration.
    Copy-Item -LiteralPath build/pc_defaults.toml -Destination (
        Join-Path $packageRoot 'runtime-data/minigames-test/user/pvz.toml')
    Copy-Item -LiteralPath docs/minigames-mouse.md,docs/minigames-save.md,
        docs/heavy-weapon.md,docs/heavy-weapon-original.md,
        docs/item-drag.md,
        docs/services-mouse.md,
        docs/input-coverage.md,
        docs/input-feedback-2026-10-09.md,
        docs/testing-item-drag-2026-10-08.md,
        docs/testing-minigames-2026-10-07.md -Destination (
        Join-Path $packageRoot 'docs')
    New-Item -ItemType Directory -Path (Join-Path $packageRoot 'docs/input-inventory') | Out-Null
    Copy-Item -LiteralPath research/input-coverage/handlers.md,
        research/input-coverage/handlers.json,research/input-coverage/android-families.json -Destination (
        Join-Path $packageRoot 'docs/input-inventory')
    Copy-Item -LiteralPath packaging/MINIGAMES-TEST-RESULTS.md -Destination $packageRoot
    Copy-Item -LiteralPath runtime-data/minigames-mouse-validation.json,
        runtime-data/heavy-weapon-fire-buy-latest.json,
        runtime-data/heavy-weapon-deluxe-validation.json,
        runtime-data/heavy-weapon-reference.json,
        runtime-data/item-drag-validation.json,
        runtime-data/services-mouse-validation.json,
        runtime-data/shop-regression-validation.json,
        runtime-data/services-feedback-validation.json,
        runtime-data/feedback-regression-validation.json,
        runtime-data/multiplayer-regression-validation.json,
        runtime-data/input-coverage-validation.json,
        runtime-data/mouse-transport-validation.json,
        runtime-data/heavy-weapon-latest.json -Destination (
        Join-Path $packageRoot 'validation')
    if ($TestUnlock) {
        Copy-Item -LiteralPath runtime-data/test-unlock-validation.json -Destination (Join-Path $packageRoot 'validation')
    }
}
Copy-Item -LiteralPath tools/rexglue-sdk/win-amd64/licenses -Destination (Join-Path $packageRoot 'licenses') -Recurse
Copy-Item -LiteralPath third_party/rexglue-LICENSE.txt -Destination (Join-Path $packageRoot 'licenses/RexGlue-LICENSE.txt')
$inventory = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File | ForEach-Object {
    [ordered]@{ path = [System.IO.Path]::GetRelativePath($packageRoot,$_.FullName).Replace('\','/');
        bytes = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
[ordered]@{ build = $name; created_at = (Get-Date).ToString('o'); sdk_version = '0.10.0';
    architecture = 'x64'; includes_game_assets = $true; includes_user_profile = $false;
    includes_minigames_test_profile = [bool]$IncludeMinigamesProfile;
    heavy_weapon_control_model = 'absolute-center';
    mouse_item_control_model = 'drag-release';
    mouse_services_control_model = 'native-tools-store-almanac';
    mouse_shop_control_model = 'native-hover-focus-house';
    pc_menu_control_model = 'host-achievements-native-cheats-confirm-quit';
    services_feedback_validation_fresh = [bool]$IncludeMinigamesProfile;
    expanded_feedback_validation_fresh = [bool]$IncludeMinigamesProfile;
    local_multiplayer_control_model = 'first-device-two-users';
    mouse_input_audit_model = $(if ($IncludeMinigamesProfile) { 'xbox-rtti-android-vita' } else { $null });
    mouse_event_transport_model = 'ordered-events-generation-cancel';
    mouse_validation_fresh = -not [bool]$TestUnlock;
    test_unlock_enabled = [bool]$TestUnlock;
    files = $inventory } | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $packageRoot 'build-info.json') -Encoding UTF8
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($packageRoot,$archivePath,
    [System.IO.Compression.CompressionLevel]::Optimal,$true)
$archive = Get-Item -LiteralPath $archivePath
$hash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $($archive.Name)" | Set-Content -LiteralPath "$archivePath.sha256" -Encoding ASCII
[ordered]@{ archive = $archive.FullName; bytes = $archive.Length; sha256 = $hash;
    package_directory = $packageRoot } | ConvertTo-Json

