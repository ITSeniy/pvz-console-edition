[CmdletBinding(DefaultParameterSetName='Deterministic')]
param([string[]]$Cases=@('begh_console','twist_console','quarium_pc','quarium_console',
 'whack_pc','whack_console','slot_pc','slot_console','izombie_pc','stars_pc',
 'laststand_pc','laststand_console','scrollbar','heavy_console','vases_pc','vases_console','shovel_console'),
 [string]$Baseline='runtime-data/zen-shop-probe-user', [switch]$Append,
 [ValidateSet('b001','b002','b003','b005','b010','b013','b017')][string[]]$NativeBatches=@(),
 [Parameter(ParameterSetName='Deterministic')][switch]$Deterministic,
 [Parameter(Mandatory=$true,ParameterSetName='WallClock')][switch]$WallClock)
$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
# Fixed before measuring T12; never choose values to make a scenario pass.
$useDeterministic=$PSCmdlet.ParameterSetName -eq 'Deterministic'
$NativeBatches=@($NativeBatches|Sort-Object -Unique)
$regression=[ordered]@{
 mode=$(if($useDeterministic){'deterministic'}else{'wall-clock'})
 seed=$(if($useDeterministic){'42'}else{$null})
 clock_epoch=$(if($useDeterministic){'1792195200'}else{$null})
 time_window=$(if($useDeterministic){'after-replay-title-and-captures'}else{'after-process-start'})
 native_batches=@($NativeBatches)
 runner_sha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
 smoke_sha256=(Get-FileHash -LiteralPath ./smoke.ps1 -Algorithm SHA256).Hash.ToLowerInvariant()
}
$smokeOptions=@{ReplayWindow=$useDeterministic}
$diagnosticArgs=@()
if($useDeterministic){
 $diagnosticArgs=@('--pvz_deterministic_seed=42','--pvz_deterministic_clock=1792195200')
}
$diagnosticArgs+=@($NativeBatches|ForEach-Object {"--pvz_native_$_=true"})
# Only scenarios that still vary with fixed seed/clock need recorded delivery.
$stepReplays=@{laststand_pc='feedback_laststand_pc.steps.csv';laststand_console='feedback_laststand_console.steps.csv'}
$regression['step_replays']=[ordered]@{}
if($useDeterministic){
 foreach($key in @($stepReplays.Keys|Sort-Object)){
  $path='tests/replays/regression-steps/'+$stepReplays[$key]
  $regression.step_replays[$key]=[ordered]@{replay=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
 }
}

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
 if(($previous.regression|ConvertTo-Json -Compress) -ne ($regression|ConvertTo-Json -Compress)){throw 'Cannot append results from another regression mode or configuration.'}
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
 $sourceReplay=$replay
 $replayArgs=@($diagnosticArgs)
 $frameLocked=$useDeterministic -and $stepReplays.ContainsKey($name)
 if($frameLocked){
  $replay=[System.IO.Path]::GetFullPath('tests/replays/regression-steps/'+$stepReplays[$name])
  if(-not (Test-Path -LiteralPath $replay)){throw "Missing recorded regression replay: $replay"}
  $replayArgs+='--pvz_replay_frame_locked=true'
 }
 & ./smoke.ps1 @smokeOptions -Seconds $seconds -UserDataRoot (Join-Path $runRoot 'user') --pvz_mouse_cursor=true --pvz_input_trace=true --pvz_test_unlock=true --pvz_replay_controller=true "--pvz_cursor_mode=$mode" "--pvz_input_replay=$replay" "--pvz_capture_frame=$runRoot/frame.ppm" --pvz_capture_delay=12 --pvz_capture_count=20 --pvz_capture_interval=3 @replayArgs
 $result=Get-Content runtime-data/smoke-latest.json -Raw|ConvertFrom-Json
 if($result.replay_window.timeout){throw 'Replay did not complete in the bounded test window.'}
 if($result.capture_window.timeout){throw 'Scheduled captures did not finish in the bounded test window.'}
 if(-not $result.replay_window.ready){throw 'Replay test window did not become ready.'}
 if(-not $result.survived -or $result.fatal.Count){throw "Feedback run failed: $name"}
 if($exeHash -ne (Get-FileHash build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant()){throw 'Executable changed during regression.'}
 $runs[$name]=@{log=$result.log_files[0];log_sha256=(Get-FileHash -LiteralPath $result.log_files[0] -Algorithm SHA256).Hash.ToLowerInvariant();replay=$replay;replay_sha256=(Get-FileHash -LiteralPath $replay -Algorithm SHA256).Hash.ToLowerInvariant();frames=$runRoot;user_data_root=$result.user_data_root;seconds=$seconds}
 if($name -eq 'shovel_console'){
  $runs[$name].profile_baseline=$caseBaseline
  $runs[$name].profile_sha256=@{'pvz.toml'=(Get-FileHash -LiteralPath (Join-Path $caseBaseline 'pvz.toml') -Algorithm SHA256).Hash.ToLowerInvariant()}
 }
 $runs[$name].execution=@{mode=$regression.mode;seed=$regression.seed;clock_epoch=$regression.clock_epoch;
  replay_delivery=$(if($frameLocked){'logical-steps'}else{'wall-clock-seconds'});
  source_replay=$sourceReplay;source_replay_sha256=(Get-FileHash -LiteralPath $sourceReplay -Algorithm SHA256).Hash.ToLowerInvariant();
  raw_logs=@($result.raw_log_files);log_adaptation=$result.log_adaptation;smoke_window=$result.replay_window;capture_window=$result.capture_window;game_arguments=@($result.game_arguments)}
 @{regression=$regression;profile_baseline=(Join-Path $baselineRoot 'user');built_executable_sha256=$exeHash;runs=$runs}|ConvertTo-Json -Depth 6|Set-Content runtime-data/feedback-regression-runs.json -Encoding utf8
 Write-Output "Completed feedback regression $name"
}
