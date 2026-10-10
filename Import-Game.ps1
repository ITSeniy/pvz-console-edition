[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Package,
    [ValidateSet('Trial','Full')][string]$Edition = 'Trial',
    [string]$Destination = 'assets')
$ErrorActionPreference = 'Stop'
$packagePath = [System.IO.Path]::GetFullPath($Package)
$destinationPath = if ([System.IO.Path]::IsPathRooted($Destination)) { $Destination }
    else { Join-Path $PSScriptRoot $Destination }
$pythonArgs = @()
if ($Edition -eq 'Full') {
    Write-Host 'Content hashes cannot verify an Xbox purchase.'
    $confirmation = Read-Host 'Confirm this is your own purchased full Xbox copy: type YES'
    if ($confirmation -cne 'YES') { throw 'Full-copy import cancelled. Trial is available without this confirmation.' }
    $pythonArgs += '--confirm-owned-full-copy'
}
$python = Get-Command py,python -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $python) { throw 'Import requires Python 3.10 or newer. Install Python, then run this command again.' }
$launcherArgs = @()
if ($python.Name -match '^py(\.exe)?$') { $launcherArgs = @('-3') }
& $python.Source @launcherArgs (Join-Path $PSScriptRoot 'scripts/import_game.py') $packagePath $destinationPath @pythonArgs
if ($LASTEXITCODE -ne 0) { throw 'Game import failed; see the message above.' }
Write-Host 'Import complete. Restart pvz-console-edition to apply the edition.'
