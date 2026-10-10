param([switch]$Build, [string]$Package)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$lock = Get-Content sdk.lock.json -Raw | ConvertFrom-Json
$archive = Join-Path $PSScriptRoot ".cache/rexglue-sdk-$($lock.version)-win-amd64.zip"
$sdk = Join-Path $PSScriptRoot 'tools/rexglue-sdk'
New-Item -ItemType Directory -Force -Path .cache | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri $lock.url -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $lock.sha256) {
    throw 'SDK archive SHA256 differs from sdk.lock.json.'
}
if (-not (Test-Path -LiteralPath "$sdk/win-amd64/bin/rexglue.exe")) {
    Expand-Archive -LiteralPath $archive -DestinationPath $sdk -Force
}
if ($Package) {
    & python scripts/extract_stfs.py $Package assets --report .cache/assets-manifest.json
    if ($LASTEXITCODE -ne 0) { throw 'STFS extraction failed.' }
} elseif (-not (Test-Path -LiteralPath assets/default.xex)) {
    $candidates = @(Get-ChildItem -LiteralPath . -Directory | Where-Object Name -Like '*XBLA*' |
        Get-ChildItem -Recurse -File | Where-Object FullName -Match '000D0000')
    if ($candidates.Count -ne 1) { throw 'Specify the XBLA container using -Package <path>.' }
    & python scripts/extract_stfs.py $candidates[0].FullName assets --report .cache/assets-manifest.json
    if ($LASTEXITCODE -ne 0) { throw 'STFS extraction failed.' }
}
$gameLock = Get-Content config/game.lock.json -Raw | ConvertFrom-Json
if ((Get-FileHash -LiteralPath assets/default.xex -Algorithm SHA256).Hash -ne $gameLock.xex_sha256) {
    throw 'Unsupported default.xex: this project targets TitleID 584109FF version 0.0.1.4. Hooks require the exact supported image.'
}
& "$sdk/win-amd64/bin/rexglue.exe" codegen pvz_manifest.toml
if ($LASTEXITCODE -ne 0) { throw 'Code generation failed.' }
if ($Build) {
    & ./build.bat configure
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & ./build.bat
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
}
