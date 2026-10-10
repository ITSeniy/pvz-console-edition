[CmdletBinding()]
param([string[]]$Cases=@('begh_console','twist_console','quarium_pc','quarium_console',
 'whack_pc','whack_console','slot_pc','slot_console','izombie_pc','stars_pc',
 'laststand_pc','laststand_console','scrollbar','heavy_console','vases_pc','vases_console','shovel_console'),
 [string]$Baseline='runtime-data/zen-shop-probe-user', [switch]$Append)
$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$testRoot=Join-Path (Get-Location) ('runtime-data/feedback-regression/'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$baselineRoot=Join-Path $testRoot 'baseline'
New-Item -ItemType Directory -Path $baselineRoot|Out-Null
Copy-Item -LiteralPath $Baseline -Destination (Join-Path $baselineRoot 'user') -Recurse
$exeHash=(Get-FileHash build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant()
$runs=@{}
if($Append){
 $previous=Get-Content runtime-data/feedback-regression-runs.json -Raw|ConvertFrom-Json
 if($previous.built_executable_sha256 -ne $exeHash){throw 'Cannot append results from another executable.'}
 $baselineRoot=Split-Path $previous.profile_baseline -Parent
 foreach($entry in $previous.runs.PSObject.Properties){$runs[$entry.Name]=$entry.Value}
}
foreach($name in $Cases){
 $runRoot=Join-Path $testRoot $name;New-Item -ItemType Directory -Path $runRoot|Out-Null
 $caseBaseline=Join-Path $baselineRoot 'user'
 if($name -eq 'shovel_console'){
  # A completed Adventure profile skips the first shovel lesson, even when
  # fixture_level selects level 5. Keep this scenario on an immutable new user.
  $caseBaseline=Join-Path $runRoot 'fresh-baseline'
  New-Item -ItemType Directory -Path $caseBaseline|Out-Null
  Copy-Item -LiteralPath build/pc_defaults.toml -Destination (Join-Path $caseBaseline 'pvz.toml')
 }
 Copy-Item -LiteralPath $caseBaseline -Destination (Join-Path $runRoot 'user') -Recurse
 $mode=if($name.EndsWith('_console')){'console'}else{'pc'}
 $seconds=if($name -eq 'shovel_console'){105}elseif($name -in 'stars_pc','laststand_pc','laststand_console','izombie_pc'){75}elseif($name.StartsWith('vases')){65}else{52}
 $replayName=if($name -eq 'shovel_console'){'level_5_console_shovel'}else{'feedback_'+$name}
 $replay=[System.IO.Path]::GetFullPath('tests/replays/'+$replayName+'.csv')
 & ./smoke.ps1 -Seconds $seconds -UserDataRoot (Join-Path $runRoot 'user') --pvz_mouse_cursor=true --pvz_input_trace=true --pvz_test_unlock=true --pvz_replay_controller=true "--pvz_cursor_mode=$mode" "--pvz_input_replay=$replay" "--pvz_capture_frame=$runRoot/frame.ppm" --pvz_capture_delay=12 --pvz_capture_count=20 --pvz_capture_interval=3
 $result=Get-Content runtime-data/smoke-latest.json -Raw|ConvertFrom-Json
 if(-not $result.survived -or $result.fatal.Count){throw "Feedback run failed: $name"}
 if($exeHash -ne (Get-FileHash build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant()){throw 'Executable changed during regression.'}
 $runs[$name]=@{log=$result.log_files[0];log_sha256=(Get-FileHash -LiteralPath $result.log_files[0] -Algorithm SHA256).Hash.ToLowerInvariant();replay=$replay;replay_sha256=(Get-FileHash -LiteralPath $replay -Algorithm SHA256).Hash.ToLowerInvariant();frames=$runRoot;user_data_root=$result.user_data_root;seconds=$seconds}
 if($name -eq 'shovel_console'){
  $runs[$name].profile_baseline=$caseBaseline
  $runs[$name].profile_sha256=@{'pvz.toml'=(Get-FileHash -LiteralPath (Join-Path $caseBaseline 'pvz.toml') -Algorithm SHA256).Hash.ToLowerInvariant()}
 }
 @{profile_baseline=(Join-Path $baselineRoot 'user');built_executable_sha256=$exeHash;runs=$runs}|ConvertTo-Json -Depth 6|Set-Content runtime-data/feedback-regression-runs.json -Encoding utf8
 Write-Output "Completed feedback regression $name"
}
