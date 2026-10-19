param([ValidateSet('off','on')][string]$Mode='off', [string]$Tag=$Mode,
    [string[]]$Suites=@('feedback','multiplayer','services','shop'))
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$outputRoot=Join-Path $taskRoot 'runtime-data'
$receipts=@()
$processFile=Join-Path $outputRoot "t14-regression-$Tag-processes.json"
if(Test-Path -LiteralPath $processFile){$receipts=@(Get-Content $processFile -Raw|ConvertFrom-Json)}
foreach($suite in $Suites) {
    $worker=Join-Path $outputRoot "t14-regression-$Tag-$suite"
    if(Test-Path -LiteralPath $worker){throw "Worker already exists: $worker"}
    New-Item -ItemType Directory -Path $worker | Out-Null
    foreach($dir in @('scripts','src','tests','config')) {Copy-Item -LiteralPath (Join-Path $taskRoot $dir) -Destination (Join-Path $worker $dir) -Recurse}
    foreach($dir in @('assets','.cache')) {New-Item -ItemType Junction -Path (Join-Path $worker $dir) -Target (Join-Path $taskRoot $dir)|Out-Null}
    Copy-Item -LiteralPath (Join-Path $taskRoot 'CMakeLists.txt') -Destination $worker
    New-Item -ItemType Directory -Path (Join-Path $worker 'build') | Out-Null
    Get-ChildItem (Join-Path $taskRoot 'build') -File | Where-Object {$_.Extension -in '.exe','.dll','.toml'} | Copy-Item -Destination (Join-Path $worker 'build')
    Copy-Item -LiteralPath (Join-Path $taskRoot 'build/mouse') -Destination (Join-Path $worker 'build/mouse') -Recurse
    New-Item -ItemType Directory -Path (Join-Path $worker 'runtime-data') | Out-Null
    Copy-Item -LiteralPath (Join-Path $taskRoot 'runtime-data/zen-shop-probe-user') -Destination (Join-Path $worker 'runtime-data/zen-shop-probe-user') -Recurse
    $smoke=Get-Content (Join-Path $taskRoot 'smoke.ps1') -Raw
    if($Mode -eq 'on') {
        $smoke=$smoke.Replace('$quotedArguments =', '$arguments += ''--pvz_native_b001=true'''+"`n"+'$quotedArguments =')
    }
    $smoke | Set-Content -LiteralPath (Join-Path $worker 'smoke.ps1') -Encoding utf8
    $driver=@'
param([string]$Suite)
$ErrorActionPreference='Stop'
Set-Location $PSScriptRoot
$watch=[Diagnostics.Stopwatch]::StartNew()
try {
    & "./scripts/run_$($Suite)_regression.ps1" *> runtime-data/run.log
    & python "./scripts/check_$($Suite)_regression.py" *> runtime-data/check.log
    $code=$LASTEXITCODE
    @{exit_code=$code;seconds=$watch.Elapsed.TotalSeconds;failure=$null}|ConvertTo-Json|Set-Content runtime-data/receipt.json
} catch {
    @{exit_code=1;seconds=$watch.Elapsed.TotalSeconds;failure=$_.Exception.Message}|ConvertTo-Json|Set-Content runtime-data/receipt.json
}
'@
    $driver | Set-Content -LiteralPath (Join-Path $worker 'invoke.ps1') -Encoding utf8
    $exe=Join-Path $worker 'build/pvz-console-edition.exe'
    $process=Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $worker 'invoke.ps1')+'"'),'-Suite',$suite) -WorkingDirectory $worker -WindowStyle Hidden -PassThru
    $receipts+=@{suite=$suite;mode=$Mode;worker=$worker;pid=$process.Id;exe_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash;started=(Get-Date).ToString('o')}
}
ConvertTo-Json -InputObject $receipts -Depth 5|Set-Content $processFile
$receipts|ForEach-Object {[pscustomobject]$_}|Format-Table suite,mode,pid,worker


