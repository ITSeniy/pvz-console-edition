<#
T10: one-command proof on immutable profile copies and fixed-step replays.
Inputs: built EXE, baseline profile, five existing CSVs (or recorded step CSVs).
Outputs: summary.json/md, nested determinism manifests, byte-strict comparisons.
Every input/output has SHA-256 provenance. No thresholds or state normalization.
#>
[CmdletBinding()]
param(
    [ValidateSet('b001')][string]$Batch,
    [switch]$Discover,
    [switch]$SupplementalOnly,
    [switch]$RenderOnly,
    [string]$ReuseEvidence,
    [string[]]$Replacements = @('pixel_to_grid_x','tool_rect'),
    [ValidateSet('adventure','versus','minigame','zen','menu')]
    [string[]]$Cases = @('adventure','versus','minigame','zen','menu'),
    [string]$Baseline = 'runtime-data/zen-shop-probe-user',
    [string]$ReplayDirectory = 'tests/replays/native-proof',
    [string]$OutputRoot,
    [string[]]$AdditionalGameArgs = @(),
    [hashtable]$CoverageNotes = @{},
    [switch]$Record,
    [switch]$InjectError,
    [switch]$SelfTest
)
$ErrorActionPreference = 'Stop'
$script:reuse = $null

function Read-NativeCounters([string[]]$LogTexts, [string[]]$Names) {
    $result = [ordered]@{}
    foreach ($name in $Names) {
        $escaped = [regex]::Escape($name)
        $rows = @(); $mismatches = 0
        foreach ($text in $LogTexts) {
            $markers = [regex]::Matches($text, "PvZ native $escaped summary[^\r\n]*")
            foreach ($marker in $markers) {
                if ($marker.Value -notmatch "^PvZ native $escaped summary reason (periodic|flush|replay_end) comparisons (\d+) errors (\d+)(?: callers \[([^\]]*)\] other (\d+):(\d+))?\s*$") {
                    throw "Malformed native counter: $($marker.Value)"
                }
                $row = @{reason=$Matches[1];comparisons=[uint64]$Matches[2];errors=[uint64]$Matches[3]}
                if ($row.reason -eq 'replay_end') {
                    if (-not $Matches.ContainsKey(4)) { throw "Missing caller breakdown: $name" }
                    $entries = $Matches[4]; $otherCalls = [uint64]$Matches[5]; $otherErrors = [uint64]$Matches[6]
                    $callers = [ordered]@{}; $sumCalls = $otherCalls; $sumErrors = $otherErrors
                    if ($otherErrors -gt $otherCalls) { throw "Invalid caller overflow: $name" }
                    if ($entries.Length) {
                        foreach ($entry in $entries.Split(',')) {
                            if ($entry -notmatch '^([0-9A-F]{8}):(\d+):(\d+)$') { throw "Malformed caller: $name" }
                            $lr = $Matches[1]; $calls = [uint64]$Matches[2]; $errors = [uint64]$Matches[3]
                            if ($callers.Contains($lr) -or $errors -gt $calls -or $calls -eq 0) { throw "Invalid/duplicate caller: $name" }
                            $callers[$lr] = @{comparisons=$calls;errors=$errors}
                            $sumCalls += $calls; $sumErrors += $errors
                        }
                    }
                    if ($callers.Count -gt 16 -or $sumCalls -ne $row.comparisons -or $sumErrors -ne $row.errors) {
                        throw "Caller totals do not match summary: $name"
                    }
                    $row.callers = $callers
                    $row.other = @{comparisons=$otherCalls;errors=$otherErrors}
                }
                if ($row.errors -gt $row.comparisons) { throw "Invalid native counters: $name" }
                $rows += $row
            }
            $mismatches += [regex]::Matches($text, "PvZ native $escaped mismatch call \d+ ").Count
        }
        $ends = @($rows | Where-Object {$_.reason -eq 'replay_end'})
        if ($ends.Count -ne 1) { throw "Expected one exact replay_end counter for $name; got $($ends.Count)" }
        $last = $ends[0]
        foreach ($group in ($rows | Group-Object comparisons)) {
            if (@($group.Group.errors | Select-Object -Unique).Count -ne 1) { throw "Conflicting counters: $name" }
        }
        $previousErrors = [uint64]0
        foreach ($row in ($rows | Sort-Object comparisons)) {
            if ($row.errors -lt $previousErrors) { throw "Regressing errors: $name" }
            $previousErrors = $row.errors
        }
        $result[$name] = [ordered]@{comparisons=$last.comparisons;errors=$last.errors;
            logged_mismatches=$mismatches;max_errors=($rows.errors | Measure-Object -Maximum).Maximum;
            callers=$last.callers;other=$last.other}
    }
    return $result
}
function Require-Coverage($Totals, [string[]]$Names) {
    foreach ($name in $Names) { if ($Totals[$name] -le 0) { throw "Zero coverage for replacement: $name" } }
}
function Merge-GameFlags([string[]]$Values) {
    # The SDK command-line parser retains the first occurrence. Resolve our
    # mode overrides before launch so every --name=value appears only once.
    $last = @{}
    for ($i=0; $i -lt $Values.Count; ++$i) {
        if ($Values[$i] -match '^--([^=]+)=') { $last[$Matches[1]]=$i }
    }
    for ($i=0; $i -lt $Values.Count; ++$i) {
        if ($Values[$i] -match '^--([^=]+)=' -and $last[$Matches[1]] -ne $i) { continue }
        $Values[$i]
    }
}
function Render-Configuration([string[]]$Values) {
    # Paths/manifest digests identify each isolated process, not its behavior.
    # All scalar flags, including glyph selection and exact frame targets, stay
    # in the comparison. Non-scalar game/profile inputs are checked by hashes.
    @($Values|Where-Object {$_ -match '^--[^=]+=' -and
        $_ -notmatch '^--(?:pvz_native_render_trace|pvz_state_dump|pvz_state_inputs_sha256)='})
}
function Read-NativeCheckpoint([byte[]]$Bytes) {
    if ($Bytes.Length -ne 56 -or [Text.Encoding]::ASCII.GetString($Bytes,0,8) -ne 'PVZNR001') { throw 'Invalid native checkpoint header' }
    $valid = @()
    foreach ($offset in @(8,32)) {
        $seq = [BitConverter]::ToUInt64($Bytes,$offset)
        $calls = [BitConverter]::ToUInt64($Bytes,$offset+8)
        $errors = [BitConverter]::ToUInt64($Bytes,$offset+16)
        # An untouched zero slot is the committed initial state, including
        # termination between mapping creation and the first comparison.
        if ($seq -eq $calls -and $errors -le $calls) { $valid += @{comparisons=$calls;errors=$errors} }
    }
    if (-not $valid.Count) { throw 'Native checkpoint has no committed comparison' }
    return @($valid | Sort-Object comparisons -Descending)[0]
}
if ($SelfTest) {
    function Assert($value) { if (-not $value) { throw 'Native proof parser self-test failed' } }
    function Reject([scriptblock]$Action) {
        $failed = $false; try { & $Action | Out-Null } catch { $failed = $true }; Assert $failed
    }
    $lines = @(
        'PvZ native test summary reason flush comparisons 1502 errors 1',
        'PvZ native test summary reason periodic comparisons 1000 errors 0',
        'PvZ native test mismatch call 1501 r3 0000 byte=08 native=01 original=00',
        'PvZ native test summary reason replay_end comparisons 1501 errors 1 callers [82120004:1000:0,82130008:500:1] other 1:0')
    $r = Read-NativeCounters $lines @('test')
    Assert ($r.test.comparisons -eq 1501 -and $r.test.errors -eq 1 -and $r.test.logged_mismatches -eq 1)
    Assert ($r.test.callers.Count -eq 2 -and $r.test.callers['82130008'].errors -eq 1 -and $r.test.other.comparisons -eq 1)
    Reject { Read-NativeCounters @('PvZ native test summary reason replay_end comparisons 1 errors 2') @('test') }
    Reject { Read-NativeCounters @('PvZ native test summary reason replay_end comparisons 1 errors') @('test') }
    Reject { Read-NativeCounters $lines @('missing') }
    Reject { Read-NativeCounters ($lines + $lines[-1]) @('test') }
    Reject { Read-NativeCounters ($lines + 'PvZ native test summary reason flush comparisons 1501 errors 0') @('test') }
    Reject { Require-Coverage @{test=0} @('test') }
    Require-Coverage @{test=1} @('test')
    $bytes = New-Object byte[] 56
    [Text.Encoding]::ASCII.GetBytes('PVZNR001').CopyTo($bytes,0)
    $empty = Read-NativeCheckpoint $bytes
    Assert ($empty.comparisons -eq 0 -and $empty.errors -eq 0)
    [BitConverter]::GetBytes([uint64]7).CopyTo($bytes,8)
    [BitConverter]::GetBytes([uint64]7).CopyTo($bytes,16)
    [BitConverter]::GetBytes([uint64]2).CopyTo($bytes,24)
    [BitConverter]::GetBytes([uint64]8).CopyTo($bytes,40) # interrupted, not committed
    $checkpoint = Read-NativeCheckpoint $bytes
    Assert ($checkpoint.comparisons -eq 7 -and $checkpoint.errors -eq 2)
    Reject { Read-NativeCheckpoint (New-Object byte[] 56) }
    $merged = @(Merge-GameFlags @('--pvz_native_test_compare=false','--other=1','--pvz_native_test_compare=true'))
    Assert ($merged.Count -eq 2 -and $merged[0] -eq '--other=1' -and $merged[1] -eq '--pvz_native_test_compare=true')
    $zero = Read-NativeCounters @('PvZ native test summary reason replay_end comparisons 0 errors 0 callers [] other 0:0') @('test')
    Assert ($zero.test.comparisons -eq 0 -and $zero.test.callers.Count -eq 0)
    Reject { Read-NativeCounters @('PvZ native test summary reason replay_end comparisons 1 errors 0') @('test') }
    Reject { Read-NativeCounters @('PvZ native test summary reason replay_end comparisons 2 errors 0 callers [82120004:1:0] other 0:0') @('test') }
    Reject { Read-NativeCounters @('PvZ native test summary reason replay_end comparisons 2 errors 0 callers [82120004:1:0,82120004:1:0] other 0:0') @('test') }
    Reject { Read-NativeCounters @('PvZ native test summary reason replay_end comparisons 1 errors 0 callers [] other 1:2') @('test') }
    Assert (('example.log.checker.log' -replace '\.checker\.log$', '') -eq 'example.log')
    $batchFlags = @(Merge-GameFlags @('--pvz_native_b001=false','--pvz_native_widget_82472958=false','--pvz_native_b001=true'))
    Assert ($batchFlags.Count -eq 2 -and $batchFlags[-1] -eq '--pvz_native_b001=true')
    $addressRows = @([pscustomobject]@{address='0x00000001'},[pscustomobject]@{address='0x00000002'})
    Assert ((@($addressRows | ForEach-Object {$_.address}) -join '|') -eq '0x00000001|0x00000002')
    $ra=@(Render-Configuration @('--pvz_native_render_trace=a','--pvz_state_inputs_sha256=one','--pvz_native_render_steps=6000','--pvz_controller_glyphs=xbox'))
    $rb=@(Render-Configuration @('--pvz_native_render_trace=b','--pvz_state_inputs_sha256=two','--pvz_native_render_steps=6000','--pvz_controller_glyphs=xbox'))
    Assert (($ra -join '|') -eq ($rb -join '|') -and $ra.Count -eq 2)
    Assert (($ra -join '|') -ne ((@(Render-Configuration @('--pvz_native_render_steps=6000','--pvz_controller_glyphs=auto'))) -join '|'))
    Write-Output 'native proof parser: 23 checks passed'
    exit 0
}
Set-Location (Split-Path $PSScriptRoot -Parent)
if ($Record -and $PSBoundParameters.ContainsKey('ReplayDirectory')) { throw 'Use either -Record or -ReplayDirectory' }
if (-not $Replacements.Count -or @($Replacements | Select-Object -Unique).Count -ne $Replacements.Count) { throw 'Supply distinct replacements' }
foreach ($name in $Replacements) { if ($name -notmatch '^[a-z][a-zA-Z0-9_]*$') { throw "Invalid replacement name: $name" } }
if ($Discover -and -not $Batch) { throw '-Discover requires -Batch' }
if ($SupplementalOnly -and (-not $Batch -or -not $Discover)) { throw '-SupplementalOnly is a discovery diagnostic; use -Batch and -Discover' }
if (-not $OutputRoot) { $OutputRoot = 'runtime-data/native-proof/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') }
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw 'OutputRoot must be new; previous evidence is preserved' }
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $OutputRoot 'runner-input.ps1')
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
$summary = [ordered]@{schema=2;passed=$false;replacements=$Replacements;cases=$Cases;inputs=[ordered]@{};
    commands=@();phases=[ordered]@{};counters=[ordered]@{};criteria=@();failure=$null}
function Criterion([string]$Name, [bool]$Pass, $Evidence) {
    $summary.criteria += [ordered]@{name=$Name;passed=$Pass;evidence=$Evidence}
}
function Save-Summary {
    $summary | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath (Join-Path $OutputRoot 'summary.json') -Encoding utf8
    $md = @('# Native replacement proof', '', "Passed: $($summary.passed)", '', '| Criterion | Passed | Evidence |', '|---|---|---|')
    foreach ($c in $summary.criteria) { $md += "| $($c.name) | $($c.passed) | $($c.evidence | ConvertTo-Json -Compress -Depth 8) |" }
    if ($summary.Contains('functions')) {
        $md += @('', '| Address | Function | Mode | Status | Coverage | Errors | Negative coverage | Negative errors |',
            '|---|---|---|---|---:|---:|---:|---:|')
        foreach ($name in $summary.functions.Keys) {
            $f = $summary.functions[$name]
            $md += "| $($f.address) | $name | $($f.mode) | $($f.status) | $($f.coverage) | $($f.errors) | $($f.negative_coverage) | $($f.negative_errors) |"
        }
    }
    $md += @('', '| Phase/case | Replacement | Comparisons at replay end | Errors |', '|---|---|---:|---:|')
    foreach ($phase in $summary.counters.Keys) {
        foreach ($case in $summary.counters[$phase].Keys) {
            foreach ($name in $summary.counters[$phase][$case].Keys) {
                $c = $summary.counters[$phase][$case][$name]
                $md += "| $phase/$case | $name | $($c.comparisons) | $($c.errors) |"
            }
        }
    }
    $md += @('', 'Caller counts at replay_end are diagnostic; equality is not an acceptance threshold.', '',
        '| Phase/case | Replacement | Caller LR | Comparisons | Errors |', '|---|---|---|---:|---:|')
    foreach ($phase in $summary.counters.Keys) {
        foreach ($case in $summary.counters[$phase].Keys) {
            foreach ($name in $summary.counters[$phase][$case].Keys) {
                $c = $summary.counters[$phase][$case][$name]
                if ($c.callers) {
                    foreach ($lr in $c.callers.Keys) {
                        $md += "| $phase/$case | $name | 0x$lr | $($c.callers[$lr].comparisons) | $($c.callers[$lr].errors) |"
                    }
                }
                if ($c.other) { $md += "| $phase/$case | $name | other | $($c.other.comparisons) | $($c.other.errors) |" }
            }
        }
    }
    $md += @('', '| Input | SHA-256 |', '|---|---|')
    foreach ($key in $summary.inputs.Keys) { $md += "| $key | $($summary.inputs[$key]) |" }
    $md += @('', '| Phase manifest | SHA-256 |', '|---|---|')
    foreach ($key in $summary.phases.Keys) { $md += "| $($summary.phases[$key].path) | $($summary.phases[$key].sha256) |" }
    $md += @('', '## Commands', '')
    foreach ($command in $summary.commands) {
        $md += @($command.script, '~~~json', ($command.parameters | ConvertTo-Json -Depth 8), '~~~', '')
    }
    if ($summary.failure) { $md += @('', "Failure: $($summary.failure)") }
    $md | Set-Content -LiteralPath (Join-Path $OutputRoot 'summary.md') -Encoding utf8
}
function Read-Tail([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {return ''}
    $file = [IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
    try {
        $null = $file.Seek([Math]::Max(0,$file.Length-16384),[IO.SeekOrigin]::Begin)
        $reader = [IO.StreamReader]::new($file)
        try {return $reader.ReadToEnd()} finally {$reader.Dispose()}
    } finally {$file.Dispose()}
}
function BatchPhase([string]$Name, [string[]]$Flags, [switch]$ProfileOnly, [string[]]$SelectedCases=$Cases, [switch]$Trace) {
    # Same five T05 scenarios and exact frame counts. Completion is based on
    # delivered input AND complete snapshots/journals, not startup wall time.
    # The 180s deadline is only a bounded failure guard, never a reduced frame target.
    $definitions = @{
        adventure=@{frames=5000;fresh=$true;mode=0}
        versus=@{frames=6000;fresh=$false;mode=76}
        minigame=@{frames=3000;fresh=$false;mode=20}
        zen=@{frames=5000;fresh=$false;mode=44}
        menu=@{frames=5000;fresh=$false;mode=-1}
    }
    $root = Join-Path $OutputRoot $Name
    New-Item -ItemType Directory -Path $root | Out-Null
    $data = [ordered]@{executable_sha256=$exeHash;runs=[ordered]@{};comparisons=[ordered]@{}}
    $journalNames = if ($Name -eq 'original' -or $Trace) {@()} elseif ($Name -eq 'negative') {$safe} else {$Replacements}
    foreach ($case in $SelectedCases) {
        $definition=$definitions[$case]; $directory=Join-Path $root "$case-1"
        New-Item -ItemType Directory -Path $directory | Out-Null
        $user=Join-Path $directory 'user'
        if ($definition.fresh) {
            New-Item -ItemType Directory -Path $user | Out-Null
            Copy-Item -LiteralPath build/pc_defaults.toml -Destination (Join-Path $user 'pvz.toml')
        } else {Copy-Item -LiteralPath $Baseline -Destination $user -Recurse}
        $inputs=[ordered]@{executable_sha256=$exeHash;replay_sha256=(Hash (Join-Path $steps "$case.csv"));
            frame_target=$definition.frames;deadline_seconds=180;profile=[ordered]@{};sources=$summary.inputs}
        foreach ($file in Get-ChildItem -LiteralPath $user -File -Recurse) {
            $inputs.profile[$file.FullName.Substring($user.Length)]=Hash $file.FullName
        }
        $manifest=Join-Path $directory 'inputs.json'
        $inputs|ConvertTo-Json -Depth 20|Set-Content $manifest -Encoding utf8
        $dump=Join-Path $directory 'state.jsonl'; $log=Join-Path $directory 'game.log'
        $gameArgs=@('--game_data_root',[IO.Path]::GetFullPath('assets'),'--user_data_root',$user,
            '--cache_root',(Join-Path $directory 'cache'),'--log_file',$log,'--log_flush_interval=1',
            '--fullscreen=false','--pvz_skip_intro=true','--pvz_render_scale=1',
            '--pvz_mouse_cursor=true','--pvz_input_trace=true','--pvz_test_unlock=true',
            '--pvz_replay_controller=true','--pvz_vsync=false','--pvz_pause_on_focus_loss=false',
            '--pvz_state_profile=true',"--pvz_cursor_mode=$(if($case -eq 'minigame'){'console'}else{'pc'})",
            "--pvz_input_replay=$(Join-Path $steps "$case.csv")",'--pvz_replay_frame_locked=true',
            "--pvz_state_inputs_sha256=$(Hash $manifest)","--pvz_state_frames=$($definition.frames)")
        if (-not $ProfileOnly) {$gameArgs+="--pvz_state_dump=$dump"}
        $tracePath=Join-Path $directory 'render.jsonl'
        if ($Trace) {$gameArgs+=@("--pvz_native_render_trace=$tracePath","--pvz_native_render_steps=$($definition.frames)")}
        # Auto glyphs depend on asynchronous host controller detection. Keep
        # the local multiplayer draw inputs fixed, without normalizing traces.
        if ($Trace -and $case -eq 'versus') {$gameArgs+='--pvz_controller_glyphs=xbox'}
        $gameArgs+=@(Merge-GameFlags ($common+$Flags))
        if ($Trace -and $script:reuse -and $script:reuse.phases.PSObject.Properties[$Name]) {
            $oldPhase=$script:reuse.phases.PSObject.Properties[$Name].Value
            if ((Hash $oldPhase.path) -ne $oldPhase.sha256) {throw "Changed reused render phase: $Name"}
            $oldData=Get-Content $oldPhase.path -Raw|ConvertFrom-Json
            $property=$oldData.runs.PSObject.Properties["$case-1"]
            if ($property -and ((@(Render-Configuration $property.Value.arguments) -join '|') -ceq
                (@(Render-Configuration $gameArgs) -join '|'))) {
                $old=$property.Value
                $oldReceipt=Get-Content $old.receipt -Raw|ConvertFrom-Json
                if ($oldData.executable_sha256 -ne $exeHash -or -not $oldReceipt.complete -or
                    -not $oldReceipt.replay_complete -or $oldReceipt.frames -ne $definition.frames -or
                    (Hash $oldReceipt.input_manifest) -ne $oldReceipt.input_sha256 -or
                    (Hash $old.render) -ne $old.render_sha256 -or (Hash $old.dump) -ne $old.dump_sha256) {
                    throw "Invalid reused render process: $Name/$case"
                }
                for($i=0;$i -lt $old.log_files.Count;++$i) {
                    if((Hash $old.log_files[$i]) -ne $old.log_sha256[$i]) {throw "Changed reused render log: $Name/$case"}
                }
                $data.runs["$case-1"]=$old
                $summary.commands+=@{script='reused immutable render case';parameters=@{phase=$Name;case=$case};
                    phase_manifest=$oldPhase;receipt=$old.receipt;configuration=@(Render-Configuration $gameArgs)}
                $data|ConvertTo-Json -Depth 20|Set-Content (Join-Path $root 'summary.json') -Encoding utf8
                Write-Host "B001 $Name/$case reused (same EXE, scalar flags and hashed evidence)"
                continue
            }
        }
        $summary.commands+=@{script='batch process';parameters=@{phase=$Name;case=$case;arguments=$gameArgs;deadline_seconds=180}}
        Save-Summary
        $quoted=@($gameArgs|ForEach-Object {'"'+$_.Replace('"','\"')+'"'})
        $process=Start-Process -FilePath ([IO.Path]::GetFullPath('build/pvz-console-edition.exe')) -ArgumentList $quoted -WorkingDirectory (Get-Location).Path -WindowStyle Hidden -PassThru
        $watch=[Diagnostics.Stopwatch]::StartNew(); $complete=$false; $frames=0; $ended=$false
        $journals=[Collections.Generic.HashSet[string]]::new()
        $offset=[long]0; $carry=''; $lastScan=-10.0; $traceComplete=-not $Trace
        try {
            while (-not $process.HasExited -and $watch.Elapsed.TotalSeconds -lt 180) {
                $text=''
                if (Test-Path -LiteralPath $log) {
                    $file=[IO.File]::Open($log,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
                    try {
                        if ($file.Length -lt $offset) {$offset=0}
                        $null=$file.Seek($offset,[IO.SeekOrigin]::Begin)
                        $reader=[IO.StreamReader]::new($file)
                        try {$text=$reader.ReadToEnd();$offset=$file.Position} finally {$reader.Dispose()}
                    } finally {$file.Dispose()}
                }
                if (-not $ProfileOnly) {
                    $matches=[regex]::Matches((Read-Tail $dump),'"record":"state","frame":(\d+),')
                    if ($matches.Count) {$frames=[int]$matches[-1].Groups[1].Value+1}
                }
                $chunk=$carry+$text
                # Recover a completion marker moved into a rotated log between polls.
                if (($ProfileOnly -or $frames -eq $definition.frames) -and -not $complete -and
                    $watch.Elapsed.TotalSeconds-$lastScan -ge 3) {
                    $chunk+=(@(Get-ChildItem -LiteralPath $directory -Filter 'game*.log'|ForEach-Object {Get-Content $_.FullName -Raw}) -join "`n")
                    $lastScan=$watch.Elapsed.TotalSeconds
                }
                if ($chunk -match 'PvZ (?:frame )?replay completed ') {$complete=$true}
                if ($Trace -and (Read-Tail $tracePath) -match '"complete":true,"frames":') {$traceComplete=$true}
                foreach ($marker in [regex]::Matches($chunk,'PvZ native ([a-zA-Z0-9_]+) summary reason replay_end ')) {
                    $null=$journals.Add($marker.Groups[1].Value)
                }
                $carry=$chunk.Substring([Math]::Max(0,$chunk.Length-300))
                $missing=@($journalNames|Where-Object {-not $journals.Contains($_)})
                if ($complete -and $traceComplete -and ($ProfileOnly -or $frames -eq $definition.frames) -and -not $missing.Count) {$ended=$true;break}
                Start-Sleep -Milliseconds 250
            }
        } finally {if (-not $process.HasExited) {$process.Kill();$process.WaitForExit()}}
        $logs=@(Get-ChildItem -LiteralPath $directory -Filter 'game*.log'|ForEach-Object {$_.FullName})
        $text=($logs|ForEach-Object {Get-Content $_ -Raw}) -join "`n"
        $receipt=@{elapsed_seconds=$watch.Elapsed.TotalSeconds;complete=$ended;replay_complete=$complete;
            frames=$frames;frame_target=$definition.frames;deadline_seconds=180;arguments=$gameArgs;
            log_files=$logs;log_sha256=@($logs|ForEach-Object {Hash $_});input_manifest=$manifest;input_sha256=(Hash $manifest)}
        $receipt|ConvertTo-Json -Depth 12|Set-Content (Join-Path $directory 'process.json') -Encoding utf8
        if (-not $ended) {throw "Batch process incomplete: $Name/$case ($frames/$($definition.frames), replay=$complete)"}
        if ($text -notmatch 'Direct3D 12 device and OS features' -or $text -match '\[FATAL\]|\[critical\]|Unhandled guest|access violation|PvZ (?:frame )?replay.*(?:timed out|readiness failed|missed row|not ready|invalid)') {throw "Invalid batch process: $Name/$case"}
        if ((Hash 'build/pvz-console-edition.exe') -ne $exeHash) {throw 'Executable changed during batch proof'}
        $run=[ordered]@{log_files=$logs;log_sha256=$receipt.log_sha256;arguments=$gameArgs;receipt=(Join-Path $directory 'process.json')}
        if ($Trace) {$run.render=$tracePath;$run.render_sha256=Hash $tracePath}
        if (-not $ProfileOnly) {
            $validation=Join-Path $directory 'validation.json'
            & python -I scripts/compare_state_dumps.py $dump $dump --output $validation | Out-Null
            if ($LASTEXITCODE -ne 0) {throw "Invalid state dump: $Name/$case"}
            $checked=Get-Content $validation -Raw|ConvertFrom-Json
            if ($checked.a.frames -ne $definition.frames -or ($case -ne 'menu' -and
                (-not $checked.a.board_frames -or $null -eq $checked.a.playing_modes.PSObject.Properties[[string]$definition.mode]))) {throw "Incomplete state coverage: $Name/$case"}
            $run.dump=$dump;$run.dump_sha256=Hash $dump;$run.coverage=$checked.a
        }
        $data.runs["$case-1"]=$run
        $data|ConvertTo-Json -Depth 20|Set-Content (Join-Path $root 'summary.json') -Encoding utf8
        Write-Host "B001 $Name/$case completed ($([int]$watch.Elapsed.TotalSeconds)s)"
    }
    $file=Join-Path $root 'summary.json'
    $summary.phases[$Name]=@{path=$file;sha256=(Hash $file)}
    return (Get-Content $file -Raw|ConvertFrom-Json)
}
function SyntheticProof {
    $path=Join-Path $OutputRoot 'synthetic.json'
    & ./build/b001_synthetic_test.exe $path
    if ($LASTEXITCODE -ne 0) {throw 'Synthetic B001 comparison failed'}
    $data=Get-Content $path -Raw|ConvertFrom-Json
    $script:synthetic=@{}
    foreach ($f in $data.functions) {
        $script:synthetic[$f.address]=$f
        Criterion "synthetic/$($f.address)" ($f.cases -gt 0 -and $f.errors -eq 0 -and $f.negative_errors -eq $f.cases) $f
    }
    Criterion 'synthetic/function_count' ($data.functions.Count -eq 24 -and $data.passed) $data.functions.Count
    $manifest=Join-Path $OutputRoot 'synthetic-inputs.json'
    Copy-Item -LiteralPath build/b001_originals.json -Destination $manifest
    Copy-Item -LiteralPath build/b001_originals.cpp -Destination (Join-Path $OutputRoot 'synthetic-originals.cpp')
    $summary['synthetic']=@{path=$path;sha256=(Hash $path);source_manifest=$manifest;
        source_manifest_sha256=(Hash $manifest);executable_sha256=(Hash 'build/b001_synthetic_test.exe')}
}
function RenderProof {
    $a=BatchPhase 'render-original' @() -Trace
    $b=BatchPhase 'render-repeat' @() -Trace
    $n=BatchPhase 'render-native' @("--pvz_native_$Batch=true") -Trace
    $summary['render']=[ordered]@{}
    foreach ($pair in @(@('repeat',$b),@('native',$n))) {
        foreach ($case in $Cases) {
            $left=$a.runs.PSObject.Properties["$case-1"].Value
            $right=$pair[1].runs.PSObject.Properties["$case-1"].Value
            $path=Join-Path $OutputRoot "render-$($pair[0])-$case.json"
            & python -I tests/compare_render_traces.py $left.render $right.render --output $path | Out-Null
            $code=$LASTEXITCODE;$r=Get-Content $path -Raw|ConvertFrom-Json
            Criterion "render/$($pair[0])/$case" ($code -eq 0 -and $r.equal) $r
            $summary.render["$($pair[0])/$case"]=$r
        }
        Compare-Dumps "render-state-$($pair[0])" $a $pair[1]
    }
    # Choose a scenario that actually reaches Centered; no new replay or
    # weakened expectation is introduced when selecting the negative case.
    $case=@($Cases|Where-Object {
        Select-String -LiteralPath $a.runs.PSObject.Properties["$_-1"].Value.render -SimpleMatch '"scope":"widget_82473730"' -Quiet
    }|Select-Object -First 1)
    if (-not $case.Count) {throw 'No Centered draw in selected scenarios for render negative control'}
    $negative=BatchPhase 'render-negative' @("--pvz_native_$Batch=true",'--pvz_native_widget_82473730_inject_error=true') -Trace -SelectedCases $case
    $path=Join-Path $OutputRoot 'render-negative.json'
    & python -I tests/compare_render_traces.py $a.runs.PSObject.Properties["$($case[0])-1"].Value.render $negative.runs.PSObject.Properties["$($case[0])-1"].Value.render --output $path | Out-Null
    $r=Get-Content $path -Raw|ConvertFrom-Json
    Criterion 'render/negative' (-not $r.equal -and $null -ne $r.first_difference.step -and
        $r.first_difference.a.scope -eq 'widget_82473730' -and $r.first_difference.a.op -eq '8249B1A0' -and
        $r.first_difference.b.x -eq $r.first_difference.a.x+1) $r
    $summary.render.negative=$r
}
function Phase([string]$Name, [string[]]$Flags, [int]$Repeats=1, [switch]$Record, [switch]$ProfileOnly, [string[]]$SelectedCases=$Cases) {
    if ($Batch -and $script:reuse -and $script:reuse.phases.PSObject.Properties[$Name]) {
        $phase=$script:reuse.phases.PSObject.Properties[$Name].Value
        $commands=@($script:reuse.commands|Where-Object {$_.script -eq 'batch process' -and $_.parameters.phase -eq $Name})
        if ($commands.Count -eq $SelectedCases.Count) {
            if ((Hash $phase.path) -ne $phase.sha256) {throw "Changed phase manifest: $Name"}
            $data=Get-Content $phase.path -Raw|ConvertFrom-Json
            if ($data.executable_sha256 -ne $exeHash -or @($data.runs.PSObject.Properties).Count -ne $SelectedCases.Count) {throw "Reused batch differs: $Name"}
            $expectedFlags=@(Merge-GameFlags ($common+$Flags))
            foreach ($case in $SelectedCases) {
                $run=$data.runs.PSObject.Properties["$case-1"].Value
                $actualFlags=@($run.arguments|Where-Object {$_ -match '^--(?:pvz_native_|pvz_deterministic_|log_max_)'})
                if (($actualFlags|ConvertTo-Json -Compress) -ne ($expectedFlags|ConvertTo-Json -Compress)) {throw "Reused flags differ: $Name/$case"}
                $receipt=Get-Content $run.receipt -Raw|ConvertFrom-Json
                if (-not $receipt.complete -or -not $receipt.replay_complete -or
                    (Hash $receipt.input_manifest) -ne $receipt.input_sha256) {throw "Invalid reused receipt: $Name/$case"}
                for ($i=0;$i -lt $run.log_files.Count;$i++) {
                    if ((Hash $run.log_files[$i]) -ne $run.log_sha256[$i]) {throw "Changed reused log: $Name/$case"}
                }
                if (-not $ProfileOnly -and ((Hash $run.dump) -ne $run.dump_sha256 -or
                    $receipt.frames -ne $receipt.frame_target)) {throw "Changed/incomplete reused dump: $Name/$case"}
            }
            $summary.phases[$Name]=@{path=$phase.path;sha256=$phase.sha256;reused=$true}
            $summary.commands+=@{script='reused immutable batch phase';parameters=@{phase=$Name;cases=$SelectedCases};original=$commands}
            return $data
        }
    }
    if ($Batch -and (-not $ReuseEvidence -or -not $script:reuse.phases.PSObject.Properties[$Name])) {return (BatchPhase $Name $Flags -ProfileOnly:$ProfileOnly -SelectedCases $SelectedCases)}
    $path = Join-Path $OutputRoot $Name
    $options = @{Baseline=$Baseline;Cases=$SelectedCases;Repeats=$Repeats;OutputRoot=$path;
        AdditionalGameArgs=@(Merge-GameFlags ($common + $Flags));ProfileOnly=$ProfileOnly;RecordSteps=$Record}
    if (-not $Record) { $options.FrameLocked=$true; $options.ReplayDirectory=$script:steps }
    if ($script:reuse -and $script:reuse.phases.PSObject.Properties[$Name]) {
        $phase = $script:reuse.phases.PSObject.Properties[$Name].Value
        if ((Hash $phase.path) -ne $phase.sha256) { throw "Changed phase manifest: $Name" }
        $old = @($script:reuse.commands | Where-Object {$_.parameters.OutputRoot -eq (Split-Path $phase.path -Parent)})
        if ($old.Count -ne 1) { throw "Missing original command for reused phase: $Name" }
        foreach ($key in @('Baseline','Cases','Repeats','AdditionalGameArgs','ProfileOnly','RecordSteps','FrameLocked','ReplayDirectory')) {
            if (($options[$key] | ConvertTo-Json -Compress) -ne ($old[0].parameters.$key | ConvertTo-Json -Compress)) { throw "Reused phase configuration differs: $Name/$key" }
        }
        $data = Get-Content $phase.path -Raw | ConvertFrom-Json
        if ($data.executable_sha256 -ne $exeHash) { throw "Reused phase EXE differs: $Name" }
        $summary.phases[$Name] = @{path=$phase.path;sha256=$phase.sha256;reused=$true}
        $summary.commands += @{script='reused immutable phase';parameters=$options;original=$old[0]}
        return $data
    }
    $summary.commands += [ordered]@{script='scripts/run_determinism_check.ps1';parameters=$options}
    Save-Summary
    & "$PSScriptRoot/run_determinism_check.ps1" @options | Out-Host
    $file = Join-Path $path 'summary.json'
    $data = Get-Content -LiteralPath $file -Raw | ConvertFrom-Json
    $summary.phases[$Name] = @{path=$file;sha256=(Hash $file)}
    foreach ($comparison in $data.comparisons.PSObject.Properties) {
        Criterion "$Name/$($comparison.Name)/repeat_states" ([bool]$comparison.Value.equal) $comparison.Value
    }
    return $data
}
function Counters([string]$PhaseName, $Data, [string[]]$Names, [switch]$Negative) {
    $summary.counters[$PhaseName] = [ordered]@{}
    foreach ($property in $Data.runs.PSObject.Properties) {
        $logs = @($property.Value.log_files | ForEach-Object { Get-Content -LiteralPath $_ -Raw })
        $counters = [ordered]@{}
        try {
            $counters = Read-NativeCounters $logs $Names
            foreach ($name in $Names) { $counters[$name]['replay_end_valid'] = $true }
        } catch {
            # Preserve the failure and continue collecting independent state
            # evidence; missing counters must never turn into a passing zero.
            Criterion "$PhaseName/$($property.Name)/journal" $false $_.Exception.Message
            foreach ($name in $Names) {
                $counters[$name] = [ordered]@{comparisons=$null;errors=$null;max_errors=$null;
                    logged_mismatches=$null;replay_end_valid=$false}
            }
        }
        foreach ($name in $Names) {
            # T12 supplies a derivative completion-marker log to older checkers.
            # Mapped counters are siblings of the original log_file, never of
            # that derivative. Parse its journal but resolve the original path.
            $paths = @($property.Value.log_files | ForEach-Object {
                $raw = $_ -replace '\.checker\.log$', ''
                "$raw.$name.native-counts.bin"
            } | Where-Object { Test-Path -LiteralPath $_ })
            if ($paths.Count -gt 0 -or $counters[$name].comparisons -gt 0) {
                if ($paths.Count -ne 1) { throw "Missing/ambiguous exact crash checkpoint: $PhaseName/$name" }
                $final = Read-NativeCheckpoint ([IO.File]::ReadAllBytes($paths[0]))
                if ($final.comparisons -lt $counters[$name].comparisons -or $final.errors -lt $counters[$name].errors) { throw 'Checkpoint predates replay end' }
                $counters[$name]['process_end'] = @{counts=$final;path=$paths[0];sha256=(Hash $paths[0])}
            }
        }
        $summary.counters[$PhaseName][$property.Name] = $counters
        foreach ($name in $Names) {
            $c = $counters[$name]
            $pass = if ($Negative -and $Batch -and $c.replay_end_valid -and $c.comparisons -eq 0) {
                        $c.errors -eq 0 -and $c.logged_mismatches -eq 0
                    } elseif ($Negative) { $c.replay_end_valid -and $c.comparisons -gt 0 -and $c.errors -gt 0 -and $c.logged_mismatches -gt 0 }
                    else { $c.replay_end_valid -and $c.errors -eq 0 -and $c.max_errors -eq 0 -and $c.logged_mismatches -eq 0 -and
                        (-not $c.Contains('process_end') -or $c.process_end.counts.errors -eq 0) }
            Criterion "$PhaseName/$($property.Name)/$name" $pass $c
        }
    }
}
function Observed-Count($Counters) {
    if ($Counters.Contains('process_end')) { return [uint64]$Counters.process_end.counts.comparisons }
    if ($null -ne $Counters.comparisons) { return [uint64]$Counters.comparisons }
    return [uint64]0
}
function Compare-Dumps([string]$Label, $Left, $Right) {
    foreach ($case in $Cases) {
        $a = $Left.runs.PSObject.Properties["$case-1"].Value.dump
        $b = $Right.runs.PSObject.Properties["$case-1"].Value.dump
        $out = Join-Path $OutputRoot "$Label-$case.json"
        & python -I "$PSScriptRoot/compare_state_dumps.py" $a $b --output $out
        $code = $LASTEXITCODE
        $result = Get-Content -LiteralPath $out -Raw | ConvertFrom-Json
        Criterion "$Label/$case" ($code -eq 0 -and $result.equal) @{path=$out;sha256=(Hash $out);result=$result}
    }
}
function SupplementalPhase([string]$Name, [string[]]$Flags) {
    $root = Join-Path $OutputRoot $Name
    New-Item -ItemType Directory -Path $root | Out-Null
    $runs = [ordered]@{}
    foreach ($case in @('menu_hover','input_ownership')) {
        $directory = Join-Path $root $case
        New-Item -ItemType Directory -Path $directory | Out-Null
        if ($case -eq 'input_ownership') {
            # This replay enters Adventure directly and waits for Board; a
            # completed Adventure profile opens SeedChooser instead. Use the
            # same fresh-profile starting condition as the Adventure proof.
            New-Item -ItemType Directory -Path (Join-Path $directory 'user') | Out-Null
            Copy-Item -LiteralPath build/pc_defaults.toml -Destination (Join-Path $directory 'user/pvz.toml')
        } else { Copy-Item -LiteralPath $Baseline -Destination (Join-Path $directory 'user') -Recurse }
        $replay = [IO.Path]::GetFullPath("tests/replays/$case.csv")
        $summary.inputs["supplemental/$case.csv"] = Hash $replay
        $options = @{Seconds=$(if ($case -eq 'menu_hover') {82} else {60});ReplayWindow=$true;
            UserDataRoot=(Join-Path $directory 'user');GameArgs=@('--pvz_mouse_cursor=true',
                '--pvz_input_trace=true','--pvz_test_unlock=true','--pvz_replay_controller=true',
                '--pvz_vsync=false','--pvz_pause_on_focus_loss=false',"--pvz_input_replay=$replay",
                "--pvz_capture_frame=$directory/coverage.ppm",'--pvz_capture_delay=1') +
                @(Merge-GameFlags ($common + $Flags))}
        $summary.commands += @{script='smoke.ps1';parameters=$options}
        Save-Summary
        & ./smoke.ps1 @options | Out-Host
        $receipt = Join-Path $directory 'smoke.json'
        Copy-Item -LiteralPath runtime-data/smoke-latest.json -Destination $receipt
        $run = Get-Content $receipt -Raw | ConvertFrom-Json
        $text = ($run.log_files | ForEach-Object {Get-Content $_ -Raw}) -join "`n"
        # smoke's live polling can miss a marker moved into a rotated log. The
        # immutable raw logs are authoritative: require readiness and completion,
        # and reject real replay failures even if the smoke receipt says complete.
        if (-not $run.survived -or $run.fatal.Count -or -not $run.gpu_initialized -or
            $text -notmatch 'PvZ replay wait_title ready' -or
            $text -notmatch 'PvZ (?:frame )?replay completed ' -or
            $text -match 'PvZ (?:frame )?replay.*(?:timed out|readiness failed|missed row|not ready|invalid)') {throw "Supplemental replay failed: $case"}
        $runs[$case] = @{log_files=$run.log_files;receipt=$receipt;receipt_sha256=(Hash $receipt)}
    }
    $file = Join-Path $root 'summary.json'
    $data = [pscustomobject]@{runs=[pscustomobject]$runs}
    $data | ConvertTo-Json -Depth 12 | Set-Content $file -Encoding utf8
    $summary.phases[$Name] = @{path=$file;sha256=(Hash $file)}
    return $data
}
if ($Batch) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    try {
        if ($Record) { throw 'Batch proof uses existing replay CSVs; -Record is not supported' }
        $inventoryPath = 'docs/native-port/data/native-replacements.csv'
        $all = @(Import-Csv $inventoryPath | Where-Object {$_.batch -eq $Batch})
        $expected = @((Import-Csv docs/native-port/data/replacement-batches.csv | Where-Object {$_.batch -eq $Batch.ToUpperInvariant()}).members.Split('|') | Sort-Object)
        $actual = @($all | ForEach-Object {$_.address} | Sort-Object)
        if (($actual -join '|') -cne ($expected -join '|')) {throw 'Batch inventory must contain every member exactly once'}
        $source = Get-Content "src/native/$($Batch)_widget.cpp" -Raw
        $gates = [regex]::Matches($source, '(?m)^(?:LEAF|SNAP)\(([0-9A-F]+), (true|false),')
        if ($gates.Count -ne @($all | Where-Object {$_.status -ne 'existing-hook'}).Count) {throw 'Unexpected source registration count'}
        foreach ($entry in $all | Where-Object {$_.status -ne 'existing-hook'}) {
            $gate = @($gates | Where-Object {$_.Groups[1].Value -eq $entry.address.Substring(2)})
            if ($gate.Count -ne 1 -or (($gate[0].Groups[2].Value -eq 'true') -ne ($entry.status -eq 'replaced'))) {throw "Source eligibility differs from inventory: $($entry.address)"}
        }
        Copy-Item -LiteralPath $inventoryPath -Destination (Join-Path $OutputRoot 'inventory-input.csv')
        $entries = @($all | Where-Object {$_.status -eq 'replaced' -or ($Discover -and $_.status -eq 'unproven')})
        if (-not $entries.Count) { throw 'No eligible batch entries (use -Discover for unproven candidates)' }
        $Replacements = @($entries.flag)
        $safe = @($entries | Where-Object {$_.mode -eq 'DeclaredOutputsOnly'} | ForEach-Object {$_.flag})
        $summary['batch'] = $Batch; $summary['discovery'] = [bool]$Discover
        $summary.replacements = $Replacements
        $summary['functions'] = [ordered]@{}
        foreach ($entry in $all) {
            $key = if ($entry.flag) {$entry.flag} else {"existing_$($entry.address)"}
            $summary.functions[$key] = [ordered]@{address=$entry.address;mode=$entry.mode;status=$entry.status;
                coverage=0;errors=0;negative_coverage=0;negative_errors=0}
        }
        foreach ($file in @('build/pvz-console-edition.exe','.cache/pvz-image.bin',$inventoryPath,
                'scripts/run_native_proof.ps1','scripts/run_determinism_check.ps1','scripts/compare_state_dumps.py',
                'src/native/b001_widget.cpp','src/native/replacement.h','src/native/replacement.cpp','src/native/render_trace.cpp','src/xbox/Widget.h',
                'tests/b001_synthetic_test.cpp','tests/b001_synthetic_sources.py','tests/compare_render_traces.py',
                'CMakeLists.txt','src/controller_assets.cpp','src/native/render_trace.h',
                'src/deterministic_replay.cpp','src/challenge_mouse.cpp','src/mouse_bridge.cpp',
                'tests/render_resource_test.cpp','config/pc_defaults.toml','config/game.lock.json')) { $summary.inputs[$file] = Hash $file }
        foreach ($file in Get-ChildItem -LiteralPath $Baseline -File -Recurse) {
            $summary.inputs["baseline/$($file.FullName.Substring(([IO.Path]::GetFullPath($Baseline)).Length))"] = Hash $file.FullName
        }
        $exeHash = $summary.inputs['build/pvz-console-edition.exe']
        $script:reuse = $null
        if ($ReuseEvidence) {
            $manifest = Join-Path $ReuseEvidence 'summary.json'
            $script:reuse = Get-Content $manifest -Raw | ConvertFrom-Json -AsHashtable
            # Use PSCustomObject only after removing the historical empty entry
            # for existing-hook; that entry never participates in execution.
            $script:reuse.functions.Remove('')
            $script:reuse = $script:reuse | ConvertTo-Json -Depth 40 | ConvertFrom-Json
            if ($script:reuse.inputs.'build/pvz-console-edition.exe' -ne $exeHash -or
                $script:reuse.batch -ne $Batch -or $script:reuse.discovery -ne [bool]$Discover) {throw 'Reuse requires the same EXE, batch and mode'}
            foreach ($key in $summary.inputs.Keys) {
                # Only the verifier itself may change when rechecking evidence.
                if ($key -eq 'scripts/run_native_proof.ps1') {continue}
                if ($script:reuse.inputs.PSObject.Properties[$key].Value -ne $summary.inputs[$key]) {throw "Reused input differs: $key"}
            }
            if (($script:reuse.cases|ConvertTo-Json -Compress) -ne ($Cases|ConvertTo-Json -Compress) -or
                ($script:reuse.replacements|ConvertTo-Json -Compress) -ne ($Replacements|ConvertTo-Json -Compress)) {throw 'Reused case/function set differs'}
            foreach ($case in $Cases) {
                if ($script:reuse.inputs."steps/$case.csv" -ne (Hash (Join-Path $ReplayDirectory "$case.csv"))) {throw 'Reused replay differs'}
            }
            $summary['reused_evidence'] = @{path=[IO.Path]::GetFullPath($manifest);sha256=(Hash $manifest)}
        }
        $off = @('--pvz_native_compare_all=false',"--pvz_native_$Batch=false","--pvz_native_$($Batch)_compare=false")
        foreach ($name in (@('pixel_to_grid_x','tool_rect') + @($all.flag) | Select-Object -Unique)) {
            if (-not $name) { continue }
            $off += @("--pvz_native_$name=false","--pvz_native_$($name)_compare=false","--pvz_native_$($name)_inject_error=false")
        }
        $common = @('--pvz_deterministic_seed=42','--pvz_deterministic_clock=1792195200',
            '--log_max_file_size_mb=64','--log_max_files=128') + $off + $AdditionalGameArgs
        if ($InjectError) { $common += @($safe | ForEach-Object {"--pvz_native_$($_)_inject_error=true"}) }
        $script:steps = [IO.Path]::GetFullPath($ReplayDirectory)
        foreach ($case in $Cases) { $summary.inputs["steps/$case.csv"] = Hash (Join-Path $steps "$case.csv") }
        SyntheticProof
        if ($RenderOnly) {
            RenderProof
            Criterion 'executable_unchanged' ((Hash 'build/pvz-console-edition.exe') -eq $exeHash) $exeHash
            $summary['elapsed_seconds']=$watch.Elapsed.TotalSeconds
            $summary.passed=@($summary.criteria|Where-Object {-not $_.passed}).Count -eq 0
            Save-Summary
            if (-not $summary.passed) {exit 1};exit 0
        }
        if (-not $SupplementalOnly) { $original = Phase 'original' @() }
        $compareFlags = if ($Discover) { @($Replacements | ForEach-Object {"--pvz_native_$($_)_compare=true"}) }
                        else { @("--pvz_native_$($Batch)_compare=true") }
        if (-not $SupplementalOnly) {
            $compared = Phase 'compare' $compareFlags
            Counters 'compare' $compared $Replacements
            Compare-Dumps 'restore-original' $original $compared
        }
        $extraCompared = SupplementalPhase 'compare-extra' $compareFlags
        Counters 'compare-extra' $extraCompared $Replacements
        $nativeFlags = if ($Discover) { @($Replacements | ForEach-Object {"--pvz_native_$_=true"}) }
                       else { @("--pvz_native_$Batch=true") }
        if (-not $SupplementalOnly) {
            $native = Phase 'native' $nativeFlags
            Counters 'native' $native $Replacements
            Compare-Dumps 'native' $original $native
        }
        $extraNative = SupplementalPhase 'native-extra' $nativeFlags
        Counters 'native-extra' $extraNative $Replacements
        # One traversal of the scenarios with all safe injections enabled.
        # Snapshot-only entries never enter the comparison engine.
        $negativeFlags = @($safe | ForEach-Object {"--pvz_native_$($_)_compare=true";"--pvz_native_$($_)_inject_error=true"})
        if (-not $SupplementalOnly) {
            $negative = Phase 'negative' $negativeFlags -ProfileOnly
            Counters 'negative' $negative $safe -Negative
        }
        $extraNegative = SupplementalPhase 'negative-extra' $negativeFlags
        Counters 'negative-extra' $extraNegative $safe -Negative
        foreach ($entry in $entries) {
            $name = $entry.flag; $f = $summary.functions[$name]
            if (-not $SupplementalOnly) { foreach ($case in $Cases) {
                $positive = $summary.counters.compare["$case-1"][$name]
                $f.coverage += [uint64]$positive.comparisons; $f.errors += [uint64]$positive.errors
                if ($entry.mode -eq 'DeclaredOutputsOnly') {
                    $control = $summary.counters.negative["$case-1"][$name]
                    $f.negative_coverage += [uint64]$control.comparisons; $f.negative_errors += [uint64]$control.errors
                }
            } }
            foreach ($case in @('menu_hover','input_ownership')) {
                $positive = $summary.counters['compare-extra'][$case][$name]
                $f.coverage += [uint64]$positive.comparisons; $f.errors += [uint64]$positive.errors
                if ($entry.mode -eq 'DeclaredOutputsOnly') {
                    $control = $summary.counters['negative-extra'][$case][$name]
                    $f.negative_coverage += [uint64]$control.comparisons; $f.negative_errors += [uint64]$control.errors
                }
            }
            # T13 requires real calls, not T10's pilot-specific 10,000-call target.
            if (-not $Discover -or $f.coverage -gt 0) {
                $syntheticResult=$script:synthetic[$entry.address]
                $syntheticPass=$entry.proof_kind -eq 'synthetic' -and $syntheticResult -and $syntheticResult.errors -eq 0 -and $syntheticResult.negative_errors -eq $syntheticResult.cases
                Criterion "coverage/$name" ($f.coverage -gt 0 -or $syntheticPass) @{game=$f;synthetic=$syntheticResult}
                if ($entry.mode -eq 'DeclaredOutputsOnly') {
                    Criterion "negative/$name" (($f.negative_coverage -gt 0 -and $f.negative_errors -gt 0) -or $syntheticPass) @{game=$f;synthetic=$syntheticResult}
                }
            }
        }
        if (-not $SupplementalOnly) {RenderProof}
        Criterion 'executable_unchanged' ((Hash 'build/pvz-console-edition.exe') -eq $exeHash) $exeHash
        $summary['supplemental_only'] = [bool]$SupplementalOnly
        $summary.passed = @($summary.criteria | Where-Object {-not $_.passed}).Count -eq 0
    } catch { $summary.failure = $_.Exception.Message; $summary.passed = $false }
    $summary['elapsed_seconds'] = $watch.Elapsed.TotalSeconds
    Save-Summary
    Write-Output "Batch proof: $OutputRoot/summary.json (passed=$($summary.passed))"
    if (-not $summary.passed) { exit 1 }; exit 0
}
try {
    foreach ($file in @('build/pvz-console-edition.exe','.cache/pvz-image.bin','scripts/run_native_proof.ps1',
        'scripts/run_determinism_check.ps1','scripts/replay_seconds_to_steps.py','scripts/compare_state_dumps.py',
        'src/native/replacement.h','src/native/replacement.cpp','src/native/pixel_to_grid_x.cpp','src/native/tool_rect.cpp',
        'src/input_replay.cpp','src/pvz_app.h','CMakeLists.txt','config/pc_defaults.toml','config/game.lock.json')) {
        $summary.inputs[$file] = Hash $file
    }
    $exeHash = $summary.inputs['build/pvz-console-edition.exe']
    $off = @('--pvz_native_compare_all=false')
    foreach ($name in (@('pixel_to_grid_x','tool_rect') + $Replacements | Select-Object -Unique)) {
        $off += @("--pvz_native_$name=false", "--pvz_native_$($name)_compare=false", "--pvz_native_$($name)_inject_error=false")
    }
    $common = @('--pvz_deterministic_seed=42','--pvz_deterministic_clock=1792195200',
        '--log_max_file_size_mb=64','--log_max_files=128') + $off + $AdditionalGameArgs
    if ($InjectError) { $common += @($Replacements | ForEach-Object { "--pvz_native_$($_)_inject_error=true" }) }
    if ($Record) {
        $script:steps = Join-Path $OutputRoot 'record'
        $null = Phase 'record' @() -Record -ProfileOnly
    } else { $script:steps = [IO.Path]::GetFullPath($ReplayDirectory) }
    foreach ($case in $Cases) {
        $summary.inputs["steps/$case.csv"] = Hash (Join-Path $steps "$case.csv")
        $conversion = Join-Path $steps "$case.csv.json"
        if (Test-Path -LiteralPath $conversion) { $summary.inputs["steps/$case.csv.json"] = Hash $conversion }
    }
    $original = Phase 'original' @()
    $compareFlags = @($Replacements | ForEach-Object { "--pvz_native_$($_)_compare=true" })
    $compared = Phase 'compare' $compareFlags -Repeats 2
    Counters 'compare' $compared $Replacements
    Compare-Dumps 'restore-original' $original $compared
    $totals = @{}
    $repeatTotals = @{}
    foreach ($name in $Replacements) {
        $totals[$name] = [uint64]0
        $repeatTotals[$name] = [uint64]0
        foreach ($case in $Cases) {
            $a = $summary.counters.compare["$case-1"][$name]
            $b = $summary.counters.compare["$case-2"][$name]
            $totals[$name] += [uint64]$a.comparisons
            $repeatTotals[$name] += [uint64]$b.comparisons
            $aPass = $a.replay_end_valid -and $a.errors -eq 0 -and $a.max_errors -eq 0 -and $a.logged_mismatches -eq 0
            $bPass = $b.replay_end_valid -and $b.errors -eq 0 -and $b.max_errors -eq 0 -and $b.logged_mismatches -eq 0
            Criterion "repeat_outcome/$case/$name" ($aPass -and $bPass) @{a=$a;b=$b;counts_are_diagnostic=$true}
        }
        $targetMet = $totals[$name] -ge 10000
        Criterion "coverage/$name" $targetMet @{comparisons=$totals[$name];target=10000;target_met=$targetMet;note=$CoverageNotes[$name];boundary='replay_end, first compare repeat'}
        Criterion "repeat_coverage/$name" ($repeatTotals[$name] -ge 10000) @{comparisons=$repeatTotals[$name];target=10000;boundary='replay_end, second compare repeat'}
        $native = Phase "native-$name" @("--pvz_native_$name=true")
        Compare-Dumps "native-$name" $original $native
        $negativeCase = @($Cases | Sort-Object { Observed-Count $summary.counters.compare["$_-1"][$name] } -Descending)[0]
        $negative = Phase "negative-$name" @("--pvz_native_$($name)_compare=true","--pvz_native_$($name)_inject_error=true") -ProfileOnly -SelectedCases @($negativeCase)
        Counters "negative-$name" $negative @($name) -Negative
        Save-Summary
    }
    Require-Coverage $totals $Replacements
    Criterion 'executable_unchanged' ((Hash 'build/pvz-console-edition.exe') -eq $exeHash) $exeHash
    $summary.passed = @($summary.criteria | Where-Object {-not $_.passed}).Count -eq 0
} catch { $summary.failure = $_.Exception.Message; $summary.passed = $false }
Save-Summary
Write-Output "Native proof: $OutputRoot/summary.json (passed=$($summary.passed))"
if (-not $summary.passed) { exit 1 }
