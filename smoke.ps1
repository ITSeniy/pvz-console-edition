[CmdletBinding(PositionalBinding=$false)]
param([ValidateRange(1,180)][int]$Seconds = 20,
    [string]$UserDataRoot,
    [string]$GameDataRoot = 'assets',
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$GameArgs)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$rootArgument = $PSScriptRoot.Replace('\', '/')
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$profile = "$rootArgument/runtime-data/smoke/$stamp"
$testUserRoot = if ($UserDataRoot) { [System.IO.Path]::GetFullPath($UserDataRoot).Replace('\', '/') } else { "$profile/user" }
$log = "$rootArgument/logs/smoke-$stamp.log"
New-Item -ItemType Directory -Force -Path logs | Out-Null
$gameRoot = [System.IO.Path]::GetFullPath($GameDataRoot).Replace('\', '/')
$arguments = @('--game_data_root', $gameRoot, '--user_data_root', $testUserRoot,
    '--cache_root', "$profile/cache", '--log_file', $log, '--log_flush_interval', '1',
    '--fullscreen=false')
if ($GameArgs) { $arguments += $GameArgs }
$quotedArguments = $arguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' }
$process = Start-Process -FilePath "$PSScriptRoot/build/pvz-console-edition.exe" -ArgumentList $quotedArguments -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -PassThru
$finished = $process.WaitForExit($Seconds * 1000)
$exitCode = $null
if ($finished) { $exitCode = $process.ExitCode }
else { $process.Kill(); $process.WaitForExit() }
$logs = @(Get-ChildItem -LiteralPath logs -Filter "smoke-$stamp*.log")
$lines = @($logs | ForEach-Object { Get-Content -LiteralPath $_.FullName })
$result = [ordered]@{
    checked_at = (Get-Date).ToString('o')
    survived = -not $finished
    duration_seconds = $Seconds
    exit_code = $exitCode
    stopped_by_test = -not $finished
    gpu_initialized = [bool]($lines -match 'Direct3D 12 device and OS features')
    fatal = @($lines | Select-String '\[FATAL\]|\[critical\]|Unhandled guest|access violation' | ForEach-Object Line)
    log_files = @($logs | ForEach-Object FullName)
    profile = $profile
    user_data_root = $testUserRoot
    note = 'Startup survival only. Rendering, responsiveness and gameplay require separate verification.'
}
$result | ConvertTo-Json -Depth 4 | Set-Content runtime-data/smoke-latest.json
$result | ConvertTo-Json -Depth 4
if ($finished -or $result.fatal.Count -or -not $result.gpu_initialized) { exit 1 }
