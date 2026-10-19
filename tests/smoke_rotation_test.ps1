param([string]$Source,[string]$Helper,[string]$Output)
$ErrorActionPreference='Stop'
$root=Join-Path $Output ('smoke-rotation-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path "$root/build","$root/runtime-data" | Out-Null
Copy-Item -LiteralPath "$Source/smoke.ps1" -Destination $root
Copy-Item -LiteralPath $Helper -Destination "$root/build/pvz-console-edition.exe"
$global:LASTEXITCODE=0
& "$root/smoke.ps1" -Seconds 1 -ReplayWindow --pvz_replay_frame_locked=true --pvz_capture_frame=rotation.ppm
if($LASTEXITCODE -ne 0) {throw 'Smoke failed on a rotated completion marker'}
$r=Get-Content "$root/runtime-data/smoke-latest.json" -Raw|ConvertFrom-Json
if(-not $r.replay_window.complete -or $r.replay_window.timeout -or
    -not $r.capture_window.complete -or $r.capture_window.timeout -or $r.elapsed_seconds -gt 10 -or
    $r.log_files.Count -ne 2 -or (Get-ChildItem "$root/logs" -Filter '*.checker.log').Count) {
    throw 'Rotation receipt inconsistent or waited past completion'
}
@{receipt=$r;sha256=@{source=(Get-FileHash "$Source/smoke.ps1").Hash;helper=(Get-FileHash $Helper).Hash}}|
    ConvertTo-Json -Depth 10|Set-Content "$root/result.json"
'smoke rotation: completion and capture recovered without timeout'
