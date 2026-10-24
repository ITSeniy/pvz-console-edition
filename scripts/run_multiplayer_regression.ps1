[CmdletBinding(DefaultParameterSetName='Deterministic')]
param([string[]]$Cases=@('vs_mouse_pad','vs_pad_mouse','vs_two_pads','coop_mouse_pad','coop_pad_mouse','coop_two_pads',
 'vs_mouse_p2_sun','vs_custom_mouse_plants','vs_custom_mouse_zombies','vs_join_mouse','vs_join_pad','vs_pick_sides'), [switch]$Append,
 [ValidateSet('b001','b002','b003','b005','b008','b010','b013','b014','b017','b018')][string[]]$NativeBatches=@(),
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
$stepReplays=@{
 coop_mouse_pad='feedback_coop_mouse_pad.steps.csv'
 coop_pad_mouse='feedback_coop_pad_mouse.steps.csv'
 coop_two_pads='feedback_coop_two_pads.steps.csv'
}
$regression['step_replays']=[ordered]@{}
if($useDeterministic){
 foreach($key in @($stepReplays.Keys|Sort-Object)){
  $path='tests/replays/regression-steps/'+$stepReplays[$key]
  $regression.step_replays[$key]=[ordered]@{replay=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
 }
}

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
 if(($previous.regression|ConvertTo-Json -Compress) -ne ($regression|ConvertTo-Json -Compress)){throw 'Cannot append results from another regression mode or configuration.'}
 foreach($entry in $previous.runs.PSObject.Properties){$runs[$entry.Name]=$entry.Value}
}
foreach($name in $Cases){
 $dir=Join-Path $root $name;New-Item -ItemType Directory -Path $dir|Out-Null
 Copy-Item -LiteralPath (Join-Path $baseline 'user') -Destination (Join-Path $dir 'user') -Recurse
 $csv=[System.IO.Path]::GetFullPath('tests/replays/feedback_'+$name+'.csv')
 $seconds=if($name.StartsWith('coop')){110}elseif($name.StartsWith('vs_join')){45}else{80}
 # Without diagnostic pads the join window waits for a controller (2P_JOIN_TEXT).
 $pads=if($name -eq 'vs_join_mouse'){'false'}else{'true'}
 $sourceReplay=$csv
 $replayArgs=@($diagnosticArgs)
 $frameLocked=$useDeterministic -and $stepReplays.ContainsKey($name)
 if($frameLocked){
  $csv=[System.IO.Path]::GetFullPath('tests/replays/regression-steps/'+$stepReplays[$name])
  if(-not (Test-Path -LiteralPath $csv)){throw "Missing recorded regression replay: $csv"}
  $replayArgs+='--pvz_replay_frame_locked=true'
 }
 & ./smoke.ps1 @smokeOptions -Seconds $seconds -UserDataRoot (Join-Path $dir 'user') --pvz_mouse_cursor=true --pvz_input_trace=true --pvz_test_unlock=true "--pvz_replay_controller=$pads" --pvz_cursor_mode=pc "--pvz_input_replay=$csv" "--pvz_capture_frame=$dir/frame.ppm" --pvz_capture_delay=30 --pvz_capture_count=16 --pvz_capture_interval=5 @replayArgs
 $result=Get-Content runtime-data/smoke-latest.json -Raw|ConvertFrom-Json
 if($result.replay_window.timeout){throw 'Replay did not complete in the bounded test window.'}
 if($result.capture_window.timeout){throw 'Scheduled captures did not finish in the bounded test window.'}
 if(-not $result.replay_window.ready){throw 'Replay test window did not become ready.'}
 if(-not $result.survived -or $result.fatal.Count){throw "Multiplayer run failed: $name"}
 if($hash -ne (Get-FileHash build/pvz-console-edition.exe -Algorithm SHA256).Hash.ToLowerInvariant()){throw 'Executable changed during regression.'}
 $runs[$name]=@{log=$result.log_files[0];log_sha256=(Get-FileHash -LiteralPath $result.log_files[0] -Algorithm SHA256).Hash.ToLowerInvariant();replay=$csv;replay_sha256=(Get-FileHash -LiteralPath $csv -Algorithm SHA256).Hash.ToLowerInvariant();frames=$dir;user_data_root=$result.user_data_root}
 $runs[$name].execution=@{mode=$regression.mode;seed=$regression.seed;clock_epoch=$regression.clock_epoch;
  replay_delivery=$(if($frameLocked){'logical-steps'}else{'wall-clock-seconds'});
  source_replay=$sourceReplay;source_replay_sha256=(Get-FileHash -LiteralPath $sourceReplay -Algorithm SHA256).Hash.ToLowerInvariant();
  raw_logs=@($result.raw_log_files);log_adaptation=$result.log_adaptation;smoke_window=$result.replay_window;capture_window=$result.capture_window;game_arguments=@($result.game_arguments)}
 @{regression=$regression;profile_baseline=(Join-Path $baseline 'user');built_executable_sha256=$hash;runs=$runs}|ConvertTo-Json -Depth 6|Set-Content runtime-data/multiplayer-regression-runs.json -Encoding utf8
}
