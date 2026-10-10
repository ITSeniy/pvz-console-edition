[CmdletBinding()]
param([string[]]$Cases=@('vs_mouse_pad','vs_pad_mouse','vs_two_pads','coop_mouse_pad','coop_pad_mouse','coop_two_pads'), [switch]$Append)
$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$root=Join-Path (Get-Location) ('runtime-data/multiplayer-regression/'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$baseline=Join-Path $root 'baseline'
New-Item -ItemType Directory -Path $baseline|Out-Null
Copy-Item -LiteralPath runtime-data/zen-shop-probe-user -Destination (Join-Path $baseline 'user') -Recurse
$hash=(Get-FileHash build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant()
$runs=@{}
if($Append){
 $previous=Get-Content runtime-data/multiplayer-regression-runs.json -Raw|ConvertFrom-Json
 if($previous.built_executable_sha256 -ne $hash){throw 'Cannot append results from another executable.'}
 $baseline=Split-Path $previous.profile_baseline -Parent
 foreach($entry in $previous.runs.PSObject.Properties){$runs[$entry.Name]=$entry.Value}
}
foreach($name in $Cases){
 $dir=Join-Path $root $name;New-Item -ItemType Directory -Path $dir|Out-Null
 Copy-Item -LiteralPath (Join-Path $baseline 'user') -Destination (Join-Path $dir 'user') -Recurse
 $csv=[System.IO.Path]::GetFullPath('tests/replays/feedback_'+$name+'.csv')
 $seconds=if($name.StartsWith('coop')){110}else{80}
 & ./smoke.ps1 -Seconds $seconds -UserDataRoot (Join-Path $dir 'user') --pvz_mouse_cursor=true --pvz_input_trace=true --pvz_test_unlock=true --pvz_replay_controller=true --pvz_cursor_mode=pc "--pvz_input_replay=$csv" "--pvz_capture_frame=$dir/frame.ppm" --pvz_capture_delay=30 --pvz_capture_count=16 --pvz_capture_interval=5
 $result=Get-Content runtime-data/smoke-latest.json -Raw|ConvertFrom-Json
 if(-not $result.survived -or $result.fatal.Count){throw "Multiplayer run failed: $name"}
 if($hash -ne (Get-FileHash build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant()){throw 'Executable changed during regression.'}
 $runs[$name]=@{log=$result.log_files[0];log_sha256=(Get-FileHash -LiteralPath $result.log_files[0] -Algorithm SHA256).Hash.ToLowerInvariant();replay=$csv;replay_sha256=(Get-FileHash -LiteralPath $csv -Algorithm SHA256).Hash.ToLowerInvariant();frames=$dir;user_data_root=$result.user_data_root}
 @{profile_baseline=(Join-Path $baseline 'user');built_executable_sha256=$hash;runs=$runs}|ConvertTo-Json -Depth 6|Set-Content runtime-data/multiplayer-regression-runs.json -Encoding utf8
}
