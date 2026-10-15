[CmdletBinding(PositionalBinding=$false)]
param([switch]$Mouse,
    [ValidateSet('PC','Console')][string]$CursorMode,
    [string]$UserDataRoot = 'runtime-data/user',
    [string]$GameDataRoot = 'assets',
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$GameArgs)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$exe = Join-Path $PSScriptRoot 'build/pvz-console-edition.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Run ./setup.ps1 -Build first.' }
# Without game files the game opens its first-run import window itself.
$xex = Join-Path $GameDataRoot 'default.xex'
if (Test-Path -LiteralPath $xex) {
    $gameLock = Get-Content config/game.lock.json -Raw | ConvertFrom-Json
    if ((Get-FileHash -LiteralPath $xex -Algorithm SHA256).Hash -ne $gameLock.xex_sha256) {
        throw 'Unsupported default.xex. See config/game.lock.json.'
    }
}
New-Item -ItemType Directory -Force -Path runtime-data,logs | Out-Null
# Start-Process fixes the working directory below. Relative CLI paths avoid
# the SDK's narrow-string conversion of absolute Unicode Windows paths.
$logArgument = "logs/run-$(Get-Date -Format 'yyyyMMdd-HHmmss').log"
$mouseArguments = @()
if ($Mouse -or $CursorMode) {
    $mouseArguments = @('--pvz_mouse_cursor=true')
    if ($CursorMode) { $mouseArguments += "--pvz_cursor_mode=$($CursorMode.ToLowerInvariant())" }
}
$arguments = @('--game_data_root', $GameDataRoot, '--user_data_root', $UserDataRoot,
    '--cache_root', 'runtime-data/cache', '--log_file', $logArgument,
    '--log_flush_interval', '1') + $mouseArguments
if ($GameArgs) { $arguments += $GameArgs }
# Windows PowerShell 5.1 can return immediately from a GUI executable. Wait
# explicitly so Start.bat also receives the actual game's exit code.
$quotedArguments = $arguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' }
$process = Start-Process -FilePath $exe -ArgumentList $quotedArguments -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
exit $process.ExitCode
