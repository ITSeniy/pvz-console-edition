[CmdletBinding()]
param([string]$OutputDirectory = 'dist')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location $projectRoot
$exe = 'build/pvz-console-edition.exe'
$version = (Get-Item -LiteralPath $exe).VersionInfo
if ($version.ProductName -ne 'pvz-console-edition') { throw 'Build the branded executable first.' }
if ((Get-Content build/pc_defaults.toml -Raw) -notmatch '(?m)^pvz_full_version\s*=\s*false\s*$') {
    throw 'Public defaults must be Trial.'
}
if (Get-ChildItem src,resources,metadata,config -Recurse -File |
        Where-Object { $_.LastWriteTimeUtc -gt (Get-Item $exe).LastWriteTimeUtc }) {
    throw 'Build inputs changed after the executable was built. Run build.bat first.'
}
$outputRoot = [System.IO.Path]::GetFullPath($OutputDirectory)
$name = "pvz-console-edition-$($version.ProductVersion)-win-x64-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
$packageRoot = Join-Path $outputRoot $name
$archivePath = Join-Path $outputRoot "$name.zip"
if ((Test-Path $packageRoot) -or (Test-Path $archivePath)) { throw 'Output already exists.' }
$required = @($exe,'build/rexruntimerd.dll','build/rexgpu-xenosrd.dll',
    'build/TracyClientrd.dll','build/pc_defaults.toml',
    'build/mouse/help_menu_image_mouse_pc.ptx','build/mouse/help_menu_image_mouse_console.ptx','build/mouse/vs_mouse_player1.ptx','build/mouse/vs_mouse_player2.ptx','run.ps1','Import-Game.ps1',
    'config/game.lock.json','scripts/import_game.py','scripts/extract_stfs.py','scripts/extract_xiso.py',
    'docs/distribution.md')
foreach ($file in $required) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing release file: $file" }
}
# Same order as build.bat: PVZ_VS, vswhere, then the standard install folders
# (vswhere finds nothing when the installer's instance registry is missing).
$vsRoot = $env:PVZ_VS
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not $vsRoot -and (Test-Path -LiteralPath $vswhere)) {
    $vsRoot = & $vswhere -latest -prerelease -products * -property installationPath
}
if (-not $vsRoot) {
    $vsRoot = Get-ChildItem -LiteralPath (Join-Path $env:ProgramFiles 'Microsoft Visual Studio'),
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio') -Directory -ErrorAction SilentlyContinue |
        Get-ChildItem -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'VC/Auxiliary/Build/vcvars64.bat') } |
        Select-Object -Last 1 -ExpandProperty FullName
}
if (-not $vsRoot) { throw 'Visual Studio with C++ tools was not found. Set PVZ_VS to its installation folder.' }
$crt = Get-ChildItem -LiteralPath (Join-Path $vsRoot 'VC/Redist/MSVC') -Directory -Recurse |
    Where-Object { $_.Name -match '^Microsoft\.VC\d+\.CRT$' -and $_.Parent.Name -eq 'x64' } |
    Sort-Object FullName -Descending | Select-Object -First 1
if (-not $crt) { throw 'Visual C++ x64 redistributable was not found.' }
foreach ($file in $required) {
    $destination = Join-Path $packageRoot $file
    New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
    Copy-Item -LiteralPath $file -Destination $destination
}
Get-ChildItem -LiteralPath $crt.FullName -Filter '*.dll' -File |
    Copy-Item -Destination (Join-Path $packageRoot 'build')
Copy-Item -LiteralPath packaging/Start.bat -Destination $packageRoot
Copy-Item -LiteralPath packaging/PUBLIC-README.md -Destination (Join-Path $packageRoot 'README.md')
Copy-Item -LiteralPath tools/rexglue-sdk/win-amd64/licenses -Destination (Join-Path $packageRoot 'licenses') -Recurse
Copy-Item -LiteralPath third_party/rexglue-LICENSE.txt -Destination (Join-Path $packageRoot 'licenses/RexGlue-LICENSE.txt')
$inventory = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File | ForEach-Object {
    [ordered]@{ path = $_.FullName.Substring($packageRoot.Length + 1).Replace('\','/');
        bytes = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
if ($inventory.path -match '(^assets/|^runtime-data/|^logs/|\.pvz-import\.toml)') {
    throw 'Unexpected private/game content in public release.'
}
[ordered]@{ product = 'pvz-console-edition'; version = $version.ProductVersion;
    default_edition = 'trial'; full_unlock = 'validated-import-and-ownership-declaration';
    includes_game_assets = $false; includes_user_profile = $false;
    files = $inventory } | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $packageRoot 'build-info.json') -Encoding UTF8
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($packageRoot,$archivePath,
    [System.IO.Compression.CompressionLevel]::Optimal,$true)
$hash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $name.zip" | Set-Content -LiteralPath "$archivePath.sha256" -Encoding ascii
[ordered]@{ archive = $archivePath; bytes = (Get-Item $archivePath).Length; sha256 = $hash;
    default_edition = 'trial'; includes_game_assets = $false; files = $inventory.Count } | ConvertTo-Json
