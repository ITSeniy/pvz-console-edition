param([string]$OutputRoot='runtime-data/t15-gamepad-discovery',[switch]$Discover,
 [ValidateSet('t07b_settings_hold_matrix','t07b_pause_hold_matrix','t07b_coop_hold_matrix','t07b_repeat_acceleration')]
 [string[]]$Cases=@('t07b_settings_hold_matrix','t07b_pause_hold_matrix','t07b_coop_hold_matrix','t07b_repeat_acceleration'),
 [ValidateSet('off','native')][string[]]$Modes=@('off','native'))
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
Set-Location $taskRoot
if(Test-Path $OutputRoot){throw 'New OutputRoot required'}
$worker=[IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory $worker|Out-Null
foreach($dir in @('scripts','src','tests','config')){Copy-Item $dir (Join-Path $worker $dir) -Recurse}
foreach($dir in @('assets','.cache')){New-Item -ItemType Junction (Join-Path $worker $dir) -Target (Join-Path $taskRoot $dir)|Out-Null}
Copy-Item CMakeLists.txt,smoke.ps1 $worker
New-Item -ItemType Directory (Join-Path $worker 'build'),(Join-Path $worker 'runtime-data')|Out-Null
Get-ChildItem build -File|Where-Object {$_.Extension -in '.exe','.dll','.toml'}|Copy-Item -Destination (Join-Path $worker 'build')
Copy-Item build/mouse (Join-Path $worker 'build/mouse') -Recurse
$entries=@(Import-Csv docs/native-port/data/native-replacements.csv|Where-Object {$_.batch -eq 'b002' -and $_.flag})
$native=@('--pvz_native_b002=true')
if($Discover){$native+=@($entries|ForEach-Object {"--pvz_native_$($_.flag)=true"})}
$compare=@($entries|ForEach-Object {"--pvz_native_$($_.flag)_compare=true"})
$common=@('--pvz_mouse_cursor=true','--pvz_input_trace=true','--pvz_test_unlock=true','--pvz_replay_controller=true',
 '--pvz_pause_on_focus_loss=false','--pvz_vsync=false','--pvz_deterministic_seed=42','--pvz_deterministic_clock=1792195200',
 '--pvz_gamepad_repeat_fix=true','--pvz_gamepad_repeat_trace=true','--log_max_file_size_mb=64','--log_max_files=128')
$watch=[Diagnostics.Stopwatch]::StartNew();$results=@()
foreach($case in $Cases){
 foreach($mode in $Modes){
  $directory=Join-Path $worker "$mode-$case";New-Item -ItemType Directory $directory|Out-Null
  Copy-Item runtime-data/zen-shop-probe-user (Join-Path $directory 'user') -Recurse
  $csv=[IO.Path]::GetFullPath("tests/replays/$case.csv")
  $seconds=@(Get-Content $csv|Where-Object {$_ -and -not $_.StartsWith('#')}|ForEach-Object {[double]($_.Split(',')[0])}|Measure-Object -Maximum)[0].Maximum+12
  $args=$common+@("--pvz_input_replay=$csv","--pvz_cursor_mode=$(if($case -in 't07b_pause_hold_matrix','t07b_coop_hold_matrix'){'console'}else{'pc'})")+$compare
  if($mode -eq 'native'){$args+=$native}
  $before=@{exe=(Get-FileHash build/pvz-console-edition.exe).Hash;replay=(Get-FileHash $csv).Hash;arguments=$args;
   runner_sha256=(Get-FileHash $PSCommandPath).Hash;baseline=[ordered]@{}}
  foreach($file in Get-ChildItem runtime-data/zen-shop-probe-user -File -Recurse){$before.baseline[$file.FullName.Substring(([IO.Path]::GetFullPath('runtime-data/zen-shop-probe-user')).Length)]=(Get-FileHash $file.FullName).Hash}
  $before|ConvertTo-Json -Depth 6|Set-Content (Join-Path $directory 'inputs.json')
  & (Join-Path $worker 'smoke.ps1') -Seconds ([int][Math]::Ceiling($seconds)) -ReplayWindow -UserDataRoot (Join-Path $directory 'user') -GameDataRoot (Join-Path $worker 'assets') -GameArgs $args *> (Join-Path $directory 'run.log')
  Set-Location $taskRoot
  $receipt=Join-Path $directory 'smoke.json';Copy-Item (Join-Path $worker 'runtime-data/smoke-latest.json') $receipt
  $r=Get-Content $receipt -Raw|ConvertFrom-Json
  $text=($r.log_files|ForEach-Object {Get-Content $_ -Raw}) -join "`n"
  $ok=$r.survived -and $r.gpu_initialized -and -not $r.fatal.Count -and $text -match 'PvZ (?:frame )?replay completed ' -and $text -notmatch 'PvZ (?:frame )?replay.*(?:timed out|readiness failed|missed row|not ready|invalid)'
  $replayOk=$ok
  $field=$true
  if($case -eq 't07b_coop_hold_matrix'){$field=$text -match 'PvZ repeat field[^\r\n]*user 0 mode 79 scene 3' -and $text -match 'PvZ repeat field[^\r\n]*user 1 mode 79 scene 3'}
  if($case -eq 't07b_pause_hold_matrix'){$field=$text -match 'PvZ repeat field[^\r\n]*mode 20 scene 3'}
  $ok=$ok -and $field
  $results+=@{case=$case;mode=$mode;complete=$ok;field_coverage=$field;receipt=$receipt;input_sha256=(Get-FileHash (Join-Path $directory 'inputs.json')).Hash}
  @{runs=$results;elapsed_seconds=$watch.Elapsed.TotalSeconds}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $worker 'summary.json')
  Write-Output "$mode/$case complete=$ok"
  # Preserve all four cases when only field readiness was missed, then return
  # failure below. This permits an explicit same-input Co-op retry without
  # losing the later acceleration evidence. Replay/fatal failures still abort.
  if(-not $replayOk){throw "Gamepad replay failed $case/$mode"}
 }
}
if(@($results|Where-Object {-not $_.complete}).Count){exit 1}
