[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Tag,
    [ValidatePattern('^b0[0-9]{2}$')][string[]]$NativeBatches=@(),
    [ValidateSet('feedback','multiplayer','services','shop')][string[]]$Suites=@('feedback','multiplayer','services','shop'),
    [switch]$Wait
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$runtimeDirectory=Get-Item -LiteralPath (Join-Path $taskRoot 'runtime-data')
# Keep long profile/save paths below the Win32 path limit in managed worktrees.
$runtimeTarget=$runtimeDirectory.ResolveLinkTarget($true)
$outputRoot=if($null -ne $runtimeTarget){$runtimeTarget.FullName}else{$runtimeDirectory.FullName}
$processFile=Join-Path $outputRoot "batch-regression-$Tag-processes.json"
if(Test-Path -LiteralPath $processFile){throw "Process manifest already exists: $processFile"}
$NativeBatches=@($NativeBatches|Sort-Object -Unique)
foreach($batch in $NativeBatches) {
    $sources=@(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'src/native') -Filter "$($batch)_*.cpp" -File)
    if($sources.Count -ne 1) {throw "Unknown or ambiguous native batch: $batch"}
}
$Suites=@($Suites|Select-Object -Unique)
$receipts=@()
$processes=@()
foreach($suite in $Suites){
    $worker=Join-Path $outputRoot "batch-regression-$Tag-$suite"
    if(Test-Path -LiteralPath $worker){throw "Worker already exists: $worker"}
    New-Item -ItemType Directory -Path $worker | Out-Null
    foreach($dir in @('scripts','src','tests','config')){
        Copy-Item -LiteralPath (Join-Path $taskRoot $dir) -Destination (Join-Path $worker $dir) -Recurse
    }
    foreach($dir in @('assets','.cache')){
        New-Item -ItemType Junction -Path (Join-Path $worker $dir) -Target (Join-Path $taskRoot $dir) | Out-Null
    }
    Copy-Item -LiteralPath (Join-Path $taskRoot 'CMakeLists.txt') -Destination $worker
    Copy-Item -LiteralPath (Join-Path $taskRoot 'smoke.ps1') -Destination $worker
    New-Item -ItemType Directory -Path (Join-Path $worker 'build') | Out-Null
    Get-ChildItem (Join-Path $taskRoot 'build') -File |
        Where-Object {$_.Extension -in '.exe','.dll','.toml'} |
        Copy-Item -Destination (Join-Path $worker 'build')
    Copy-Item -LiteralPath (Join-Path $taskRoot 'build/mouse') -Destination (Join-Path $worker 'build/mouse') -Recurse
    New-Item -ItemType Directory -Path (Join-Path $worker 'runtime-data') | Out-Null
    Copy-Item -LiteralPath (Join-Path $taskRoot 'runtime-data/zen-shop-probe-user') -Destination (Join-Path $worker 'runtime-data/zen-shop-probe-user') -Recurse
    $driver=@'
param([string]$Suite,[string]$NativeBatchList='')
$ErrorActionPreference='Stop'
Set-Location $PSScriptRoot
$watch=[Diagnostics.Stopwatch]::StartNew()
$started=[DateTimeOffset]::UtcNow.ToString('o')
$batches=@($NativeBatchList.Split('+')|Where-Object {$_})
$code=1
$failure=$null
try{
    & "./scripts/run_$($Suite)_regression.ps1" -NativeBatches $batches *> runtime-data/run.log
    & python "./scripts/check_$($Suite)_regression.py" *> runtime-data/check.log
    $code=$LASTEXITCODE
}catch{$failure=$_.Exception.Message}
[ordered]@{exit_code=$code;seconds=$watch.Elapsed.TotalSeconds;started_utc=$started;
    ended_utc=[DateTimeOffset]::UtcNow.ToString('o');failure=$failure;native_batches=$batches} |
    ConvertTo-Json | Set-Content runtime-data/receipt.json -Encoding utf8
exit $code
'@
    $driver | Set-Content -LiteralPath (Join-Path $worker 'invoke.ps1') -Encoding utf8
    $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $worker 'invoke.ps1')+'"'),'-Suite',$suite)
    if($NativeBatches.Count){$arguments+=@('-NativeBatchList',($NativeBatches -join '+'))}
    $process=Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList $arguments -WorkingDirectory $worker -WindowStyle Hidden -PassThru
    $processes+=$process
    $receipts+=[ordered]@{suite=$suite;worker=$worker;source_worktree=$taskRoot;pid=$process.Id;native_batches=$NativeBatches;
        executable_sha256=(Get-FileHash (Join-Path $worker 'build/pvz-console-edition.exe') -Algorithm SHA256).Hash.ToLowerInvariant();
        runner_sha256=(Get-FileHash (Join-Path $worker "scripts/run_$($suite)_regression.ps1") -Algorithm SHA256).Hash.ToLowerInvariant();
        checker_sha256=(Get-FileHash (Join-Path $worker "scripts/check_$($suite)_regression.py") -Algorithm SHA256).Hash.ToLowerInvariant();
        smoke_sha256=(Get-FileHash (Join-Path $worker 'smoke.ps1') -Algorithm SHA256).Hash.ToLowerInvariant();
        driver_sha256=(Get-FileHash (Join-Path $worker 'invoke.ps1') -Algorithm SHA256).Hash.ToLowerInvariant();
        started_utc=[DateTimeOffset]::UtcNow.ToString('o')}
    ConvertTo-Json -InputObject $receipts -Depth 6 | Set-Content $processFile -Encoding utf8
}
$receipts|ForEach-Object {[pscustomobject]$_}|Format-Table suite,pid,worker
if($Wait){
    foreach($process in $processes){$process.WaitForExit()}
    $failed=@()
    foreach($entry in $receipts){
        $path=Join-Path $entry.worker 'runtime-data/receipt.json'
        if(-not(Test-Path -LiteralPath $path)){$failed+=$entry.suite;continue}
        $result=Get-Content $path -Raw|ConvertFrom-Json
        Write-Output "$($entry.suite): exit=$($result.exit_code) seconds=$($result.seconds)"
        if($result.exit_code -ne 0 -or $result.failure){$failed+=$entry.suite}
    }
    if($failed.Count){throw "Regression workers failed: $($failed -join ', ')"}
}
