<#
T10: one-command proof on immutable profile copies and fixed-step replays.
Inputs: built EXE, baseline profile, five existing CSVs (or recorded step CSVs).
Outputs: summary.json/md, nested determinism manifests, byte-strict comparisons.
Every input/output has SHA-256 provenance. No thresholds or state normalization.
#>
[CmdletBinding()]
param(
    [string]$Batch,
    [switch]$Discover,
    [switch]$SupplementalOnly,
    [switch]$RenderOnly,
    [switch]$CompareOnly,
    [string[]]$SupplementalCases=@(),
    [string]$DrawThumbReplay,
    [string[]]$ExtraRenderReplays=@(),
    [string]$ExtraRenderEvidence,
    [switch]$ExtraRenderOnly,
    [switch]$NoExtraRender,
    [switch]$DrawThumbOnly,
    [switch]$RenderPartition,
    [string]$ReuseEvidence,
    [switch]$RecheckOnly,
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
$batchNames = if ($Batch) {@($Batch.Split('+'))} else {@()}
# Batch names and source paths are repository data, derived from filenames.
$batchSources = @{}
foreach ($file in Get-ChildItem -LiteralPath (Join-Path (Split-Path $PSScriptRoot -Parent) 'src/native') -Filter 'b0??_*.cpp' -File) {
    if ($file.Name -notmatch '^(b0[0-9]{2})_.+\.cpp$') {continue}
    $name=$Matches[1]
    if ($batchSources.ContainsKey($name)) {throw "Multiple native sources for batch: $name"}
    $batchSources[$name]="src/native/$($file.Name)"
}
$knownBatches = @($batchSources.Keys | Sort-Object)
if ($Batch) {
    if (@($batchNames | Select-Object -Unique).Count -ne $batchNames.Count) { throw 'Batch names must be unique' }
    foreach ($name in $batchNames) { if ($name -notin $knownBatches) { throw "Unknown batch: $name" } }
    # A replacement translates the Xbox body. Calling the original (__imp__) or a
    # FORWARD wrapper would pass every comparison without replacing anything.
    foreach ($name in $batchNames) {
        $lines = @(Get-Content -LiteralPath $batchSources[$name])
        for ($i = 0; $i -lt $lines.Count; $i++) {
            $code = $lines[$i] -replace '//.*$', ''
            if ($code -cmatch '__imp__sub_|\bFORWARD\b') {
                throw "$($batchSources[$name]):$($i + 1): batch sources must not call the original or forward to it"
            }
        }
    }
}
function Batch-Flags([string[]]$Names=$batchNames, [switch]$Compare) {
    foreach ($b in $Names) { "--pvz_native_$b$(if($Compare){'_compare'})=true" }
}
function Native-CVarName([string]$Flag) {
    if ($Flag -match '^(.*_)([0-9A-Fa-f]{8})$') { return $Matches[1] + $Matches[2].ToUpperInvariant() }
    return $Flag
}

function Read-NativeCounters([string[]]$LogTexts, [string[]]$Names) {
    $result = [ordered]@{}
    foreach ($name in $Names) {
        $escaped = [regex]::Escape($name)
        $rows = @(); $mismatches = 0
        foreach ($text in $LogTexts) {
        $markers = [regex]::Matches($text, "PvZ native $escaped summary[^\r\n]*", [Text.RegularExpressions.RegexOptions]::IgnoreCase)
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
            $mismatches += [regex]::Matches($text, "PvZ native $escaped mismatch call \d+ ",
                [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
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
function Test-NegativeCounters($Counters, [bool]$AllowZero) {
    if (-not $Counters.replay_end_valid) {return $false}
    $calls=[uint64]$Counters.comparisons
    $errors=[uint64]$Counters.errors
    if ($Counters.Contains('process_end')) {
        # Mismatches are counted across the complete log. Use the committed
        # process checkpoint for that same interval: calls may occur after the
        # replay_end marker while the host is completing the process.
        $calls=[uint64]$Counters.process_end.counts.comparisons
        $errors=[uint64]$Counters.process_end.counts.errors
    }
    if ($errors -lt $Counters.max_errors) {return $false}
    if ($AllowZero -and $calls -eq 0) {
        return $errors -eq 0 -and $Counters.logged_mismatches -eq 0
    }
    return $calls -gt 0 -and $errors -gt 0 -and $Counters.logged_mismatches -gt 0
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
    $caseVariant = Read-NativeCounters @('PvZ native manager_824A60F0 mismatch call 1 r3 0000 byte=08 native=01 original=00',
        'PvZ native manager_824A60F0 summary reason replay_end comparisons 1 errors 1 callers [825FB348:1:1] other 0:0') @('manager_824a60f0')
    Assert ($caseVariant['manager_824a60f0'].comparisons -eq 1 -and $caseVariant['manager_824a60f0'].logged_mismatches -eq 1)
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
    Assert ((Native-CVarName 'scrollbar_824b9c10') -ceq 'scrollbar_824B9C10')
    Assert ((Native-CVarName 'pixel_to_grid_x') -ceq 'pixel_to_grid_x')
    $addressRows = @([pscustomobject]@{address='0x00000001'},[pscustomobject]@{address='0x00000002'})
    Assert ((@($addressRows | ForEach-Object {$_.address}) -join '|') -eq '0x00000001|0x00000002')
    $ra=@(Render-Configuration @('--pvz_native_render_trace=a','--pvz_state_inputs_sha256=one','--pvz_native_render_steps=6000','--pvz_controller_glyphs=xbox'))
    $rb=@(Render-Configuration @('--pvz_native_render_trace=b','--pvz_state_inputs_sha256=two','--pvz_native_render_steps=6000','--pvz_controller_glyphs=xbox'))
    Assert (($ra -join '|') -eq ($rb -join '|') -and $ra.Count -eq 2)
    Assert (($ra -join '|') -ne ((@(Render-Configuration @('--pvz_native_render_steps=6000','--pvz_controller_glyphs=auto'))) -join '|'))
    $late=[ordered]@{comparisons=0;errors=0;max_errors=0;logged_mismatches=1;replay_end_valid=$true;
        process_end=@{counts=@{comparisons=1;errors=1}}}
    Assert (Test-NegativeCounters $late $true)
    $late.process_end.counts.errors=0
    Assert (-not (Test-NegativeCounters $late $true))
    $late.Remove('process_end')
    Assert (-not (Test-NegativeCounters $late $true))
    $late.logged_mismatches=0
    Assert (Test-NegativeCounters $late $true)
    Assert (-not (Test-NegativeCounters $late $false))
    $late.replay_end_valid=$false
    Assert (-not (Test-NegativeCounters $late $true))
    $late.replay_end_valid=$true;$late.comparisons=1;$late.errors=1;$late.max_errors=1;$late.logged_mismatches=1
    Assert (Test-NegativeCounters $late $false)
    Write-Output 'native proof parser: 34 checks passed'
    exit 0
}
Set-Location (Split-Path $PSScriptRoot -Parent)
if ($Record -and $PSBoundParameters.ContainsKey('ReplayDirectory')) { throw 'Use either -Record or -ReplayDirectory' }
if (-not $Replacements.Count -or @($Replacements | Select-Object -Unique).Count -ne $Replacements.Count) { throw 'Supply distinct replacements' }
foreach ($name in $Replacements) { if ($name -notmatch '^[a-z][a-zA-Z0-9_]*$') { throw "Invalid replacement name: $name" } }
if ($Discover -and -not $Batch) { throw '-Discover requires -Batch' }
if ($RecheckOnly -and (-not $Batch -or -not $ReuseEvidence -or $Record)) {throw '-RecheckOnly requires a batch, immutable ReuseEvidence and no Record'}
if ($SupplementalOnly -and (-not $Batch -or -not $Discover)) { throw '-SupplementalOnly is a discovery diagnostic; use -Batch and -Discover' }
if ($CompareOnly -and (-not $Batch -or $RenderOnly -or $SupplementalOnly)) {throw '-CompareOnly requires a batch and a separate render proof'}
if ($SupplementalCases.Count -and -not $CompareOnly) {throw '-SupplementalCases is restricted to the short CompareOnly preflight; full proof requires every supplemental replay'}
if ($ExtraRenderOnly -and (-not $RenderOnly -or $NoExtraRender -or -not $ExtraRenderEvidence)) {throw '-ExtraRenderOnly requires RenderOnly and completed compare evidence'}
if ($NoExtraRender -and (-not $RenderOnly -or -not $RenderPartition)) {throw '-NoExtraRender is restricted to an explicit render partition; full proof includes supplemental traces'}
if ($DrawThumbOnly -and (-not $RenderOnly -or 'b005' -notin $batchNames -or -not $DrawThumbReplay)) {throw '-DrawThumbOnly requires -RenderOnly, B005 and an existing converted replay'}
if ($RenderPartition -and (-not $Batch -or -not $RenderOnly -or $Cases.Count -ne 1)) {throw '-RenderPartition requires one explicit render-only case; aggregate acceptance must require all five cases and a caught negative control'}
if (-not $OutputRoot) { $OutputRoot = 'runtime-data/native-proof/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') }
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw 'OutputRoot must be new; previous evidence is preserved' }
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $OutputRoot 'runner-input.ps1')
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
$summary = [ordered]@{schema=2;passed=$false;replacements=$Replacements;cases=$Cases;inputs=[ordered]@{};
    commands=@();phases=[ordered]@{};counters=[ordered]@{};criteria=@();failure=$null}
$summary['recheck_only']=[bool]$RecheckOnly
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
function BatchPhase([string]$Name, [string[]]$Flags, [switch]$ProfileOnly, [string[]]$SelectedCases=$Cases, [switch]$Trace, [int]$FrameOverride=0, [switch]$FreshMenu) {
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
    if($FreshMenu) {$definitions.menu.fresh=$true}
    if($FrameOverride) {
        if($SelectedCases.Count -ne 1 -or $SelectedCases[0] -ne 'menu' -or $FrameOverride -lt 5000) {throw 'Extra render override requires one menu replay and at least 5000 frames'}
        $definitions.menu.frames=$FrameOverride
    }
    $deadlineSeconds=if($FrameOverride){[int][Math]::Max(180,[Math]::Ceiling($FrameOverride/40)+30)}else{180}
    $root = Join-Path $OutputRoot $Name
    New-Item -ItemType Directory -Path $root | Out-Null
    $data = [ordered]@{executable_sha256=$exeHash;runs=[ordered]@{};comparisons=[ordered]@{}}
        $journalNames = if ($Name -eq 'original' -or $Trace) {@()} elseif ($Discover) {
            @($entries.flag)
        } elseif ($Name -eq 'negative') {$safe} else {$Replacements}
    foreach ($case in $SelectedCases) {
        $definition=$definitions[$case]; $directory=Join-Path $root "$case-1"
        New-Item -ItemType Directory -Path $directory | Out-Null
        $user=Join-Path $directory 'user'
        if ($definition.fresh) {
            New-Item -ItemType Directory -Path $user | Out-Null
            Copy-Item -LiteralPath build/pc_defaults.toml -Destination (Join-Path $user 'pvz.toml')
        } else {Copy-Item -LiteralPath $Baseline -Destination $user -Recurse}
        $inputs=[ordered]@{executable_sha256=$exeHash;replay_sha256=(Hash (Join-Path $steps "$case.csv"));
            frame_target=$definition.frames;deadline_seconds=$deadlineSeconds;profile=[ordered]@{};sources=$summary.inputs}
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
        if ($RecheckOnly) {throw "RecheckOnly has no matching immutable process: $Name/$case"}
        $summary.commands+=@{script='batch process';parameters=@{phase=$Name;case=$case;arguments=$gameArgs;deadline_seconds=180}}
        Save-Summary
        $quoted=@($gameArgs|ForEach-Object {'"'+$_.Replace('"','\"')+'"'})
        $process=Start-Process -FilePath ([IO.Path]::GetFullPath('build/pvz-console-edition.exe')) -ArgumentList $quoted -WorkingDirectory (Get-Location).Path -WindowStyle Hidden -PassThru
        $watch=[Diagnostics.Stopwatch]::StartNew(); $complete=$false; $frames=0; $ended=$false
        $journals=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        $offset=[long]0; $carry=''; $lastScan=-10.0; $traceComplete=-not $Trace
        try {
            while (-not $process.HasExited -and $watch.Elapsed.TotalSeconds -lt $deadlineSeconds) {
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
            frames=$frames;frame_target=$definition.frames;deadline_seconds=$deadlineSeconds;arguments=$gameArgs;
            log_files=$logs;log_sha256=@($logs|ForEach-Object {Hash $_});input_manifest=$manifest;input_sha256=(Hash $manifest)}
        $receipt|ConvertTo-Json -Depth 12|Set-Content (Join-Path $directory 'process.json') -Encoding utf8
        if (-not $ended) {throw "Batch process incomplete: $Name/$case ($frames/$($definition.frames), replay=$complete, missing_journals=$($missing -join ','))"}
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
    $script:synthetic=@{}
    $proofs=[ordered]@{}
    foreach ($batchName in $batchNames) {
    $path=Join-Path $OutputRoot "synthetic-$batchName.json"
    & "./build/$($batchName)_synthetic_test.exe" $path
    if ($LASTEXITCODE -ne 0) {throw "Synthetic $batchName comparison failed"}
    $data=Get-Content $path -Raw|ConvertFrom-Json
    $sourceManifest=Get-Content "build/$($batchName)_originals.json" -Raw|ConvertFrom-Json
    foreach ($sourceInput in $sourceManifest.sha256.PSObject.Properties) {
        if ((Hash $sourceInput.Name) -cne $sourceInput.Value) {throw "Stale synthetic input: $batchName/$($sourceInput.Name); rebuild first"}
    }
    foreach ($f in $data.functions) {
        $script:synthetic[$f.address]=$f
        Criterion "synthetic/$($f.address)" ($f.cases -gt 0 -and $f.errors -eq 0 -and $f.negative_errors -eq $f.cases) $f
    }
    $expectedFunctions=@(Import-Csv docs/native-port/data/native-replacements.csv |
        Where-Object {$_.batch -eq $batchName -and $_.proof_kind -eq 'synthetic'} |
        ForEach-Object {$_.address} | Sort-Object)
    $expectedCount=$expectedFunctions.Count
    Criterion "synthetic/$batchName/function_count" ($data.functions.Count -eq $expectedCount -and $data.passed) $data.functions.Count
    $actualFunctions=@($data.functions|ForEach-Object {$_.address}|Sort-Object)
    Criterion "synthetic/$batchName/function_set" (($actualFunctions -join '|') -ceq ($expectedFunctions -join '|')) $actualFunctions
    $manifest=Join-Path $OutputRoot "synthetic-$batchName-inputs.json"
    Copy-Item -LiteralPath "build/$($batchName)_originals.json" -Destination $manifest
    Copy-Item -LiteralPath "build/$($batchName)_originals.cpp" -Destination (Join-Path $OutputRoot "synthetic-$batchName-originals.cpp")
    $proofs[$batchName]=@{path=$path;sha256=(Hash $path);source_manifest=$manifest;
        source_manifest_sha256=(Hash $manifest);executable_sha256=(Hash "build/$($batchName)_synthetic_test.exe")}
    }
    $summary['synthetic']=$proofs
    if (@($summary.criteria | Where-Object {$_.name -like 'synthetic/*' -and -not $_.passed}).Count) {
        throw 'Synthetic preflight failed; game phases were not started'
    }
}
function Selected-ExtraRenderReplays {
    if($NoExtraRender){return}
    if($ExtraRenderReplays.Count) {return $ExtraRenderReplays}
    if('b005' -in $batchNames) {'services_almanac_regression'}
    if('b008' -in $batchNames -or 'b014' -in $batchNames) {
        'game_settings';'services_settings_pad';'menu_panels';
        'menu_panels_actions';'audio_settings';'minis_chooser_ui';'oct7_chooser';'input_ownership'
    }
}
function Drawing-Scopes {
    $declarations=@($batchNames|ForEach-Object {
        $text=Get-Content $batchSources[$_] -Raw
        [regex]::Matches($text,'REXCVAR_DECLARE\(bool,\s*pvz_native_(\w+)_inject_error\)') |
            ForEach-Object {$_.Groups[1].Value.ToLowerInvariant()}
    })
    @($entries|Where-Object {$_.flag.ToLowerInvariant() -in $declarations}|ForEach-Object {$_.flag.ToLowerInvariant()})
}
function Read-DrawingCounts([string]$Trace) {
    # Count paint operations, excluding font/color/clip/translation-only scopes.
    $code='import json,sys,collections; c=collections.Counter(); paint={"8249B1A0","8249B588","8249B650","8249BC50","8249C430","8249D0E8","8249D250"}; [c.update(x.get("scope", "").lower() for x in json.loads(line).get("calls", []) if x.get("op") in paint) for line in open(sys.argv[1],encoding="utf-8-sig")]; print(json.dumps(dict(c)))'
    $counts=& python -I -c $code $Trace|ConvertFrom-Json -AsHashtable
    if($LASTEXITCODE){throw 'Could not count painting scopes'}
    return $counts
}
function Accumulate-DrawingCoverage([string]$Mode, $Run) {
    if(-not $summary.Contains('drawing')) {
        $summary['drawing']=[ordered]@{coverage=[ordered]@{};negative=[ordered]@{};traces=@()}
    }
    if(-not $summary.drawing.coverage.Contains($Mode)) {$summary.drawing.coverage[$Mode]=[ordered]@{}}
    $counts=Read-DrawingCounts $Run.render
    foreach($scope in Drawing-Scopes) {
        if(-not $summary.drawing.coverage[$Mode].Contains($scope)) {$summary.drawing.coverage[$Mode][$scope]=[uint64]0}
        $summary.drawing.coverage[$Mode][$scope]+=[uint64]$counts[$scope]
    }
    $summary.drawing.traces+=@{mode=$Mode;path=$Run.render;sha256=(Hash $Run.render);painting=$counts}
    return $counts
}
function Drawing-Negatives($OriginalRun, [string[]]$Flags, [string]$Prefix, [string]$Case='menu', [int]$Frames=0, [switch]$FreshMenu) {
    $counts=Read-DrawingCounts $OriginalRun.render
    foreach($scope in Drawing-Scopes) {
        if(-not $counts[$scope] -or $summary.drawing.negative.Contains($scope)) {continue}
        $negative=BatchPhase "$Prefix-negative-$scope" ($Flags+@("--pvz_native_$(Native-CVarName $scope)_inject_error=true")) -SelectedCases @($Case) -Trace -FrameOverride $Frames -FreshMenu:$FreshMenu
        $path=Join-Path $OutputRoot "$Prefix-negative-$scope.json"
        & python -I tests/compare_render_traces.py $OriginalRun.render $negative.runs."$Case-1".render --output $path|Out-Null
        $r=Get-Content $path -Raw|ConvertFrom-Json
        $caught=-not $r.equal -and $null -ne $r.first_difference.step -and $r.first_difference.a.scope -eq $scope
        Criterion "drawing/negative/$scope" $caught $r
        $summary.drawing.negative[$scope]=@{caught=$caught;comparison=$path;sha256=(Hash $path)}
    }
}
function Require-DrawingCoverage {
    foreach($scope in Drawing-Scopes) {
        $coverage=[ordered]@{}
        foreach($mode in @('original','repeat','native','all')) {$coverage[$mode]=[uint64]$summary.drawing.coverage[$mode][$scope]}
        if($Discover -and -not $coverage.native) {continue}
        Criterion "drawing/coverage/$scope" (@($coverage.Values|Where-Object {$_ -eq 0}).Count -eq 0) $coverage
        Criterion "drawing/negative-covered/$scope" ($summary.drawing.negative.Contains($scope) -and $summary.drawing.negative[$scope].caught) $summary.drawing.negative[$scope]
    }
}
function ExtraRenderProof {
    # A list of existing event streams, each projected from recorded logical delivery.
    $summary['extra_render']=@()
    $definitions=Supplemental-Definitions
    foreach($case in @(Selected-ExtraRenderReplays|Select-Object -Unique)) {
        if(-not $definitions.Contains($case)) {throw "Unknown extra render replay: $case"}
        if($definitions[$case].expected_exit){throw "Quit replay cannot provide a complete fixed-frame Graphics trace: $case; select a non-exiting replay"}
        $source=[IO.Path]::GetFullPath("tests/replays/$($definitions[$case].replay)")
        if ($RecheckOnly) {
            $previous=@($script:reuse.extra_render|Where-Object {$_.case -eq $case})
            if ($previous.Count -ne 1 -or $previous[0].source_sha256 -ne (Hash $source) -or
                $previous[0].replay_sha256 -ne (Hash $previous[0].replay) -or
                $previous[0].delivery_manifest_sha256 -ne (Hash $previous[0].delivery_manifest)) {
                throw "RecheckOnly requires the unchanged projected replay: $case"
            }
            $replay=$previous[0].replay
            $directory=Split-Path $replay -Parent
        } else {
            $directory=Join-Path $OutputRoot "extra-render-steps-$case"
            New-Item -ItemType Directory -Path $directory|Out-Null
            $replay=Join-Path $directory 'menu.csv'
        }
        if ($RecheckOnly) {
            # Read the original delivery projection without rewriting any
            # evidence. The common provenance checks below still apply.
        } elseif($DrawThumbReplay -and $case -eq 'services_almanac_regression') {
            Copy-Item -LiteralPath $DrawThumbReplay -Destination $replay
            Copy-Item -LiteralPath "$DrawThumbReplay.json" -Destination "$replay.json"
        } else {
            $run=$extraCompared.runs.PSObject.Properties[$case].Value
            if(-not $run){throw "Compare evidence with recorded delivery is required for Graphics: $case"}
            & python -I scripts/replay_seconds_to_steps.py $source @($run.log_files) --output $replay|Out-Null
            if($LASTEXITCODE){throw "Could not project recorded delivery: $case"}
        }
        $manifest=Get-Content "$replay.json" -Raw|ConvertFrom-Json
        if($manifest.source_sha256 -ne (Hash $source) -or $manifest.output_sha256 -ne (Hash $replay) -or
           $manifest.converter_sha256 -ne (Hash 'scripts/replay_seconds_to_steps.py') -or
           $manifest.method -ne 'recorded app logical update delivery; no FPS interpolation') {throw "Invalid delivery provenance: $case"}
        foreach($log in $manifest.logs.PSObject.Properties) {if((Hash $log.Name) -ne $log.Value){throw "Recorded delivery log changed: $($log.Name)"}}
        $summary.inputs[$replay]=Hash $replay
        $summary.inputs["$replay.json"]=Hash "$replay.json"
        $maxStep=0
        foreach($line in Get-Content $replay) {if($line.Trim() -and -not $line.TrimStart().StartsWith('#')){$maxStep=[Math]::Max($maxStep,[int]$line.Split(',')[0])}}
        $target=[int][Math]::Max(5000,$maxStep+256)
        $item=[ordered]@{case=$case;source=$source;source_sha256=(Hash $source);replay=$replay;replay_sha256=(Hash $replay);
            delivery_manifest="$replay.json";delivery_manifest_sha256=(Hash "$replay.json");frames=$target;scope_calls=[ordered]@{};comparisons=[ordered]@{}}
        $summary.extra_render+=,$item
        $previousSteps=$script:steps
        try {
            $script:steps=$directory
            $fresh=[bool]$definitions[$case].fresh
            $forced=if($Discover -or $DrawThumbOnly){@($entries|ForEach-Object {"--pvz_native_$(Native-CVarName $_.flag)=true"})}else{@()}
            $nativeFlags=@(Batch-Flags)+$forced
            $allFlags=@(Batch-Flags $knownBatches)+$forced
            $runs=[ordered]@{}
            foreach($mode in @('original','repeat','native','all')) {
                $flags=@(switch($mode){native {$nativeFlags};all {$allFlags};default {@()}})
                $runs[$mode]=BatchPhase "extra-render-$case-$mode" $flags -SelectedCases @('menu') -Trace -FrameOverride $target -FreshMenu:$fresh
                $item.scope_calls[$mode]=Accumulate-DrawingCoverage $mode $runs[$mode].runs.'menu-1'
            }
            foreach($mode in @('repeat','native','all')) {
                $path=Join-Path $OutputRoot "extra-render-$case-$mode.json"
                & python -I tests/compare_render_traces.py $runs.original.runs.'menu-1'.render $runs[$mode].runs.'menu-1'.render --output $path|Out-Null
                $code=$LASTEXITCODE;$comparison=Get-Content $path -Raw|ConvertFrom-Json
                Criterion "extra-render/$case/$mode/trace" ($code -eq 0 -and $comparison.equal) $comparison
                $item.comparisons[$mode]=@{path=$path;sha256=(Hash $path);equal=$comparison.equal}
                $path=Join-Path $OutputRoot "extra-render-state-$case-$mode.json"
                & python -I scripts/compare_state_dumps.py $runs.original.runs.'menu-1'.dump $runs[$mode].runs.'menu-1'.dump --output $path|Out-Null
                $code=$LASTEXITCODE;$state=Get-Content $path -Raw|ConvertFrom-Json
                Criterion "extra-render/$case/$mode/state" ($code -eq 0 -and $state.equal) $state
            }
            Drawing-Negatives $runs.original.runs.'menu-1' $nativeFlags "extra-render-$case" -Frames $target -FreshMenu:$fresh
        } finally {$script:steps=$previousSteps}
    }
}
function RenderProof {
    $a=BatchPhase 'render-original' @() -Trace
    $b=BatchPhase 'render-repeat' @() -Trace
    $summary['render']=[ordered]@{}
    $pairs=@(,@('repeat',$b))
    $allSeven=$knownBatches -join '+'
    $groups=if($batchNames -contains 'b002' -or $batchNames -contains 'b003') {
        @('b002','b003',$allSeven)
    } elseif ($batchNames -contains 'b005' -or $batchNames -contains 'b010' -or
              $batchNames -contains 'b013' -or $batchNames -contains 'b017' -or
              $batchNames -contains 'b008' -or $batchNames -contains 'b014' -or $batchNames -contains 'b018') {
        @($Batch,$allSeven)
    } else {@($Batch)}
    foreach($group in $groups) {
        $groupNames=@($group.Split('+'))
        $flags=@(Batch-Flags $groupNames)
        if($Discover) {$flags+=@($entries|Where-Object {$_.batch -in $groupNames}|ForEach-Object {"--pvz_native_$(Native-CVarName $_.flag)=true"})}
        $label=if($Batch -eq 'b001'){'native'}else{$group}
        $pairs+=,@($label,(BatchPhase "render-$label" $flags -Trace))
    }
    foreach($case in $Cases) {$null=Accumulate-DrawingCoverage 'original' $a.runs.PSObject.Properties["$case-1"].Value}
    foreach ($pair in $pairs) {
        foreach ($case in $Cases) {
            $left=$a.runs.PSObject.Properties["$case-1"].Value
            $right=$pair[1].runs.PSObject.Properties["$case-1"].Value
            $path=Join-Path $OutputRoot "render-$($pair[0])-$case.json"
            & python -I tests/compare_render_traces.py $left.render $right.render --output $path | Out-Null
            $code=$LASTEXITCODE;$r=Get-Content $path -Raw|ConvertFrom-Json
            Criterion "render/$($pair[0])/$case" ($code -eq 0 -and $r.equal) $r
            $summary.render["$($pair[0])/$case"]=$r
        }
        $mode=if($pair[0] -eq 'repeat'){'repeat'}elseif($pair[0] -eq $Batch){'native'}elseif($pair[0] -eq $allSeven){'all'}else{$pair[0]}
        foreach($case in $Cases) {$null=Accumulate-DrawingCoverage $mode $pair[1].runs.PSObject.Properties["$case-1"].Value}
        Compare-Dumps "render-state-$($pair[0])" $a $pair[1]
    }
    $drawFlags=@(Batch-Flags)+$(if($Discover){@($entries|ForEach-Object {"--pvz_native_$(Native-CVarName $_.flag)=true"})}else{@()})
    foreach($drawCase in $Cases) {Drawing-Negatives $a.runs.PSObject.Properties["$drawCase-1"].Value $drawFlags "render-$drawCase" -Case $drawCase}
    # Choose a scenario that actually reaches Centered; no new replay or
    # weakened expectation is introduced when selecting the negative case.
    $case=@($Cases|Where-Object {
        Select-String -LiteralPath $a.runs.PSObject.Properties["$_-1"].Value.render -SimpleMatch '"scope":"widget_82473730"' -Quiet
    }|Select-Object -First 1)
    if (-not $case.Count) {
        if (-not $RenderPartition) {throw 'No Centered draw in selected scenarios for render negative control'}
        $summary.render['negative'] = @{covered=$false;reason='No Centered call in this explicit partition; full acceptance requires a caught negative in another partition'}
        if(@(Selected-ExtraRenderReplays).Count){ExtraRenderProof}
        Require-DrawingCoverage
        return
    }
    $negativeBatches=if($Batch -eq 'b001'){@('b001')}else{@($knownBatches)}
    $negative=BatchPhase 'render-negative' (@(Batch-Flags $negativeBatches)+@('--pvz_native_widget_82473730_inject_error=true')) -Trace -SelectedCases $case
    $path=Join-Path $OutputRoot 'render-negative.json'
    & python -I tests/compare_render_traces.py $a.runs.PSObject.Properties["$($case[0])-1"].Value.render $negative.runs.PSObject.Properties["$($case[0])-1"].Value.render --output $path | Out-Null
    $r=Get-Content $path -Raw|ConvertFrom-Json
    Criterion 'render/negative' (-not $r.equal -and $null -ne $r.first_difference.step -and
        $r.first_difference.a.scope -eq 'widget_82473730' -and $r.first_difference.a.op -eq '8249B1A0' -and
        $r.first_difference.b.x -eq $r.first_difference.a.x+1) $r
    $summary.render.negative=$r
    if(@(Selected-ExtraRenderReplays).Count){ExtraRenderProof}
    Require-DrawingCoverage
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
                $actualFlags=@($run.arguments|Where-Object {$_ -match '^--(?:pvz_native_|pvz_deterministic_|log_max_|pvz_frame_limit=)'})
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
    if ($RecheckOnly) {throw "RecheckOnly has no matching immutable phase: $Name"}
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
function Counters([string]$PhaseName, $Data, [string[]]$Names, [switch]$Negative, [switch]$AllowMissingSnapshots) {
    $summary.counters[$PhaseName] = [ordered]@{}
    foreach ($property in $Data.runs.PSObject.Properties) {
        $logs = @($property.Value.log_files | ForEach-Object { Get-Content -LiteralPath $_ -Raw })
        $counters = [ordered]@{}
        $loggedNames = @(); $missingSnapshotNames = @()
        foreach ($name in $Names) {
            $escaped = [regex]::Escape($name)
            if ($logs -match "PvZ native $escaped summary reason replay_end ") { $loggedNames += $name }
            elseif ($AllowMissingSnapshots -and $summary.functions[$name].mode -eq 'SnapshotsOnly') {
                $missingSnapshotNames += $name
            } else { $loggedNames += $name }
        }
        try {
            $counters = Read-NativeCounters $logs $loggedNames
            foreach ($name in $loggedNames) { $counters[$name]['replay_end_valid'] = $true }
        } catch {
            # Preserve the failure and continue collecting independent state
            # evidence; missing counters must never turn into a passing zero.
            Criterion "$PhaseName/$($property.Name)/journal" $false $_.Exception.Message
            foreach ($name in $loggedNames) {
                $counters[$name] = [ordered]@{comparisons=$null;errors=$null;max_errors=$null;
                    logged_mismatches=$null;replay_end_valid=$false}
            }
        }
        foreach ($name in $missingSnapshotNames) {
            $counters[$name] = [ordered]@{comparisons=[uint64]0;errors=[uint64]0;max_errors=[uint64]0;
                logged_mismatches=0;replay_end_valid=$true;unobserved_snapshot=$true}
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
            $pass = if ($Negative) { Test-NegativeCounters $c ([bool]$Batch) }
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
function Supplemental-Definitions {
    $supplementalDefinitions = [ordered]@{}
    if ($batchNames | Where-Object {$_ -in @('b001','b002','b003')}) {
        $supplementalDefinitions.menu_hover = @{replay='menu_hover.csv';seconds=82;fresh=$false}
        $supplementalDefinitions.input_ownership = @{replay='input_ownership.csv';seconds=60;fresh=$true}
    }
    if ('b005' -in $batchNames) {
        $supplementalDefinitions.feedback_scrollbar = @{replay='feedback_scrollbar.csv';seconds=30;fresh=$false}
        $supplementalDefinitions.minis_scroll = @{replay='minis_scroll.csv';seconds=34;fresh=$false}
        $supplementalDefinitions.menu_panels = @{replay='menu_panels.csv';seconds=32;fresh=$true}
        $supplementalDefinitions.game_settings = @{replay='game_settings.csv';seconds=58;fresh=$false}
    }
    if ('b013' -in $batchNames) {
        $supplementalDefinitions.menu_hover = @{replay='menu_hover.csv';seconds=82;fresh=$false}
        $supplementalDefinitions.input_ownership = @{replay='input_ownership.csv';seconds=60;fresh=$true}
    }
    if ('b010' -in $batchNames) {
        $supplementalDefinitions.shop_regression = @{replay='shop_regression.csv';seconds=88;fresh=$false}
        $supplementalDefinitions.services_zen_regression = @{replay='services_zen_regression.csv';seconds=60;fresh=$false}
        $supplementalDefinitions.services_garden_followup = @{replay='services_garden_followup.csv';seconds=76;fresh=$false}
        $supplementalDefinitions.services_garden_polish = @{replay='services_garden_polish.csv';seconds=34;fresh=$false}
        $supplementalDefinitions.services_menus_regression = @{replay='services_menus_regression.csv';seconds=57;fresh=$false}
        $supplementalDefinitions.services_settings_pad = @{replay='services_settings_pad.csv';seconds=43;fresh=$false}
        $supplementalDefinitions.services_tree_regression = @{replay='services_tree_regression.csv';seconds=90;fresh=$false}
        $supplementalDefinitions.services_tree_feeding = @{replay='services_tree_feeding.csv';seconds=36;fresh=$false}
        $supplementalDefinitions.services_tree_intro = @{replay='services_tree_intro.csv';seconds=66;fresh=$false}
        $supplementalDefinitions.services_tree_purchase = @{replay='services_tree_purchase.csv';seconds=80;fresh=$false}
        $supplementalDefinitions.services_almanac_regression = @{replay='services_almanac_regression.csv';seconds=46;fresh=$false}
    }
    if ('b008' -in $batchNames) {
        $supplementalDefinitions.game_settings = @{replay='game_settings.csv';seconds=58;fresh=$false}
        $supplementalDefinitions.services_quit_confirm = @{replay='services_quit_confirm.csv';seconds=25;fresh=$false;expected_exit=$true}
        $supplementalDefinitions.services_settings_pad = @{replay='services_settings_pad.csv';seconds=43;fresh=$false}
    }
    if ('b014' -in $batchNames) {
        $supplementalDefinitions.menu_panels = @{replay='menu_panels.csv';seconds=32;fresh=$true}
        $supplementalDefinitions.menu_panels_actions = @{replay='menu_panels_actions.csv';seconds=66;fresh=$true}
        $supplementalDefinitions.audio_settings = @{replay='audio_settings.csv';seconds=32;fresh=$false}
        $supplementalDefinitions.minis_chooser_ui = @{replay='minis_chooser_ui.csv';seconds=85;fresh=$false}
        $supplementalDefinitions.oct7_chooser = @{replay='oct7_chooser.csv';seconds=68;fresh=$false}
        $supplementalDefinitions.input_ownership = @{replay='input_ownership.csv';seconds=60;fresh=$true}
    }
    return $supplementalDefinitions
}
function SupplementalPhase([string]$Name, [string[]]$Flags) {
    $root = Join-Path $OutputRoot $Name
    New-Item -ItemType Directory -Path $root | Out-Null
    $runs = [ordered]@{}
    $supplementalDefinitions = Supplemental-Definitions
    $selected=@($supplementalDefinitions.Keys)
    if($SupplementalCases.Count) {
        foreach($selectedName in $SupplementalCases){if(-not $supplementalDefinitions.Contains($selectedName)){throw "Unknown supplemental replay: $selectedName"}}
        $selected=@($selected|Where-Object {$_ -in $SupplementalCases})
    }
    foreach ($case in $selected) {
        $directory = Join-Path $root $case
        New-Item -ItemType Directory -Path $directory | Out-Null
        $definition = $supplementalDefinitions[$case]
        if ($definition.fresh) {
            New-Item -ItemType Directory -Path (Join-Path $directory 'user') | Out-Null
            Copy-Item -LiteralPath build/pc_defaults.toml -Destination (Join-Path $directory 'user/pvz.toml')
        } else { Copy-Item -LiteralPath $Baseline -Destination (Join-Path $directory 'user') -Recurse }
        $replay = [IO.Path]::GetFullPath("tests/replays/$($definition.replay)")
        $summary.inputs["supplemental/$case.csv"] = Hash $replay
        $options = @{Seconds=$definition.seconds;ReplayWindow=$true;
            UserDataRoot=(Join-Path $directory 'user');GameArgs=@('--pvz_mouse_cursor=true',
                '--pvz_input_trace=true','--pvz_test_unlock=true','--pvz_replay_controller=true',
                '--pvz_vsync=false','--pvz_pause_on_focus_loss=false',"--pvz_input_replay=$replay",
                "--pvz_capture_frame=$directory/coverage.ppm",'--pvz_capture_delay=1') +
                @(Merge-GameFlags ($common + $Flags))}
        # Capture actual logical delivery of this existing replay. Its event
        # stream is unchanged; the converter later projects these recorded
        # delivery steps for deterministic original/repeat/native Graphics.
        if($case -in @(Selected-ExtraRenderReplays)) {
            $options.GameArgs=@(Merge-GameFlags ($options.GameArgs+@('--pvz_replay_record_steps=true')))
        }
        if ($script:reuse -and $script:reuse.phases.PSObject.Properties[$Name]) {
            $oldPhase=$script:reuse.phases.PSObject.Properties[$Name].Value
            if ((Hash $oldPhase.path) -ne $oldPhase.sha256) {throw "Changed supplemental phase: $Name"}
            $oldData=Get-Content $oldPhase.path -Raw|ConvertFrom-Json
            $oldProperty=$oldData.runs.PSObject.Properties[$case]
            if ($oldProperty) {
                $old=$oldProperty.Value
                if ((Hash $old.receipt) -ne $old.receipt_sha256) {throw "Changed supplemental receipt: $Name/$case"}
                $oldReceipt=Get-Content $old.receipt -Raw|ConvertFrom-Json
                $expected=@($options.GameArgs|Where-Object {$_ -match '^--(?:pvz_native_|pvz_deterministic_|log_max_|pvz_input_replay=|pvz_replay_record_steps=)'})
                $actual=@($oldReceipt.game_arguments|Where-Object {$_ -match '^--(?:pvz_native_|pvz_deterministic_|log_max_|pvz_input_replay=|pvz_replay_record_steps=)'})
                if (($actual -join "`n") -cne ($expected -join "`n") -or $oldReceipt.duration_seconds -ne $definition.seconds) {throw "Reused supplemental configuration differs: $Name/$case"}
                if (($old.log_files -join "`n") -cne ($oldReceipt.log_files -join "`n") -or -not $oldReceipt.raw_log_files.Count) {throw 'Missing immutable supplemental logs'}
                foreach($log in $oldReceipt.raw_log_files) {if((Hash $log.path) -ne $log.sha256){throw "Changed supplemental raw log: $($log.path)"}}
                $text=($old.log_files|ForEach-Object {Get-Content $_ -Raw}) -join "`n"
                if (-not ($oldReceipt.survived -or ($definition.expected_exit -and $oldReceipt.exit_code -eq 0 -and -not $oldReceipt.stopped_by_test)) -or $oldReceipt.fatal.Count -or -not $oldReceipt.gpu_initialized -or
                    $text -notmatch 'PvZ replay wait_title ready' -or $text -notmatch 'PvZ (?:frame )?replay completed ' -or
                    $text -match 'PvZ (?:frame )?replay.*(?:timed out|readiness failed|missed row|not ready|invalid)') {throw "Invalid supplemental replay: $Name/$case"}
                $runs[$case]=$old
                $summary.commands+=@{script='reused immutable supplemental case';parameters=@{phase=$Name;case=$case};phase_manifest=$oldPhase;receipt=$old.receipt}
                Save-Summary
                Write-Host "Supplemental $Name/$case reused (same inputs, flags and hashed receipt/logs)"
                continue
            }
        }
        if ($RecheckOnly) {throw "RecheckOnly has no matching immutable supplemental process: $Name/$case"}
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
        if (-not ($run.survived -or ($definition.expected_exit -and $run.exit_code -eq 0 -and -not $run.stopped_by_test)) -or $run.fatal.Count -or -not $run.gpu_initialized -or
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
        $all = @(Import-Csv $inventoryPath | Where-Object {$_.batch -in $batchNames})
        $expected = @(Import-Csv docs/native-port/data/replacement-batches.csv | Where-Object {$_.batch.ToLowerInvariant() -in $batchNames} | ForEach-Object {$_.members.Split('|')} | Sort-Object)
        $actual = @($all | ForEach-Object {$_.address} | Sort-Object)
        if (($actual -join '|') -cne ($expected -join '|')) {throw 'Batch inventory must contain every member exactly once'}
        $source = (@($batchNames|ForEach-Object {Get-Content $batchSources[$_] -Raw}) -join "`n")
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
                coverage=$null;errors=$null;negative_coverage=$null;negative_errors=$null}
        }
        foreach ($file in @('build/pvz-console-edition.exe','.cache/pvz-image.bin',$inventoryPath,
                'docs/native-port/data/replacement-batches.csv','docs/native-port/data/xbox-layouts.csv',
                'scripts/run_native_proof.ps1','scripts/run_determinism_check.ps1','scripts/compare_state_dumps.py','scripts/replay_seconds_to_steps.py',
                'src/native/b001_widget.cpp','src/native/guest_call.h','src/native/replacement.h','src/native/replacement.cpp','src/native/render_trace.cpp','src/xbox/Widget.h',
                'tests/b001_synthetic_test.cpp','tests/b001_synthetic_sources.py','tests/compare_render_traces.py',
                'CMakeLists.txt','src/controller_assets.cpp','src/native/render_trace.h',
                'src/deterministic_replay.cpp','src/challenge_mouse.cpp','src/mouse_bridge.cpp',
                'tests/render_resource_test.cpp','config/pc_defaults.toml','config/game.lock.json')) { $summary.inputs[$file] = Hash $file }
        foreach ($file in @($batchNames|ForEach-Object {$batchSources[$_];"tests/$($_)_synthetic_test.cpp"})+@(
            'tests/batch_synthetic.h','tests/batch_synthetic_sources.py','src/xbox/Gamepad.h','src/xbox/BaseGamepadControls.h',
            'src/xbox/WidgetContainer.h','src/xbox/ScrollbarWidget.h',
            'src/xbox/DataObjects.h','src/xbox/WidgetManager.h','src/xbox/BigInt.h',
            'src/xbox/Dialog.h','src/xbox/Buttons.h','src/xbox/EditWidget.h')) {
            $summary.inputs[$file]=Hash $file
        }
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
        $off = @('--pvz_native_compare_all=false')+@($knownBatches|ForEach-Object {"--pvz_native_$_=false";"--pvz_native_$($_)_compare=false"})
        foreach ($name in (@('pixel_to_grid_x','tool_rect') + @($all.flag) | Select-Object -Unique)) {
            if (-not $name) { continue }
            $cvarName = Native-CVarName $name
            $off += @("--pvz_native_$cvarName=false","--pvz_native_$($cvarName)_compare=false","--pvz_native_$($cvarName)_inject_error=false")
        }
        $common = @('--pvz_deterministic_seed=42','--pvz_deterministic_clock=1792195200',
            '--log_max_file_size_mb=64','--log_max_files=128') + $off + $AdditionalGameArgs
        if ($InjectError) { $common += @($safe | ForEach-Object {"--pvz_native_$(Native-CVarName $_)_inject_error=true"}) }
        $script:steps = [IO.Path]::GetFullPath($ReplayDirectory)
        foreach ($case in $Cases) { $summary.inputs["steps/$case.csv"] = Hash (Join-Path $steps "$case.csv") }
        SyntheticProof
        if ($ExtraRenderEvidence) {
            if(-not $RenderOnly){throw '-ExtraRenderEvidence is only for an independent render phase'}
            $evidencePath=[IO.Path]::GetFullPath($ExtraRenderEvidence)
            $evidence=Get-Content $evidencePath -Raw|ConvertFrom-Json
            if($evidence.batch -ne $Batch -or $evidence.inputs.'build/pvz-console-edition.exe' -ne $exeHash -or
               -not $evidence.passed -or $evidence.discovery -ne [bool]$Discover) {throw 'Extra render requires completed matching compare evidence'}
            foreach($input in $summary.inputs.Keys) {
                if($input -eq 'scripts/run_native_proof.ps1'){continue}
                if($evidence.inputs.PSObject.Properties[$input].Value -ne $summary.inputs[$input]) {throw "Extra render input differs: $input"}
            }
            $phase=$evidence.phases.'compare-extra'
            if((Hash $phase.path) -ne $phase.sha256){throw 'Changed extra compare manifest'}
            $extraCompared=Get-Content $phase.path -Raw|ConvertFrom-Json
            foreach($run in $extraCompared.runs.PSObject.Properties.Value) {
                if((Hash $run.receipt) -ne $run.receipt_sha256){throw 'Changed extra compare receipt'}
                $receipt=Get-Content $run.receipt -Raw|ConvertFrom-Json
                foreach($log in $receipt.raw_log_files){if((Hash $log.path) -ne $log.sha256){throw 'Changed extra compare raw log'}}
            }
            $summary['extra_render_evidence']=@{path=$evidencePath;sha256=(Hash $evidencePath);phase=$phase}
        }
        if ($RenderOnly) {
            if($DrawThumbOnly -or $ExtraRenderOnly){ExtraRenderProof;Require-DrawingCoverage}else{RenderProof}
            Criterion 'executable_unchanged' ((Hash 'build/pvz-console-edition.exe') -eq $exeHash) $exeHash
            $summary['elapsed_seconds']=$watch.Elapsed.TotalSeconds
            $summary.passed=@($summary.criteria|Where-Object {-not $_.passed}).Count -eq 0
            Save-Summary
            if (-not $summary.passed) {exit 1};exit 0
        }
        if (-not $SupplementalOnly) { $original = Phase 'original' @() }
        $compareFlags = if ($Discover) { @($Replacements | ForEach-Object {"--pvz_native_$(Native-CVarName $_)=true";"--pvz_native_$(Native-CVarName $_)_compare=true"}) }
                        else { @(Batch-Flags -Compare) }
        if (-not $SupplementalOnly) {
            $compared = Phase 'compare' $compareFlags
            Counters 'compare' $compared $Replacements
            Compare-Dumps 'restore-original' $original $compared
            if ($batchNames -contains 'b002' -or $batchNames -contains 'b003') {
                $comparedAgain = Phase 'compare-repeat' $compareFlags
                Counters 'compare-repeat' $comparedAgain $Replacements -AllowMissingSnapshots:$Discover
                Compare-Dumps 'restore-original-repeat' $original $comparedAgain
            }
        }
        $extraCompared = SupplementalPhase 'compare-extra' $compareFlags
        Counters 'compare-extra' $extraCompared $Replacements
        $nativeFlags = if ($Discover) { @($Replacements | ForEach-Object {"--pvz_native_$(Native-CVarName $_)=true"}) }
                       else { @(Batch-Flags) }
        if (-not $SupplementalOnly -and -not $CompareOnly) {
            $native = Phase 'native' $nativeFlags
            Counters 'native' $native $Replacements -AllowMissingSnapshots:$Discover
            Compare-Dumps 'native' $original $native
        }
        if (-not $CompareOnly) {
            $extraNative = SupplementalPhase 'native-extra' $nativeFlags
            Counters 'native-extra' $extraNative $Replacements -AllowMissingSnapshots:$Discover
        }
        # One traversal of the scenarios with all safe injections enabled.
        # Snapshot-only entries never enter the comparison engine.
        $negativeFlags = @($safe | ForEach-Object {$cvarName = Native-CVarName $_;"--pvz_native_$($cvarName)_compare=true";"--pvz_native_$($cvarName)_inject_error=true"})
        if (-not $SupplementalOnly -and -not $CompareOnly -and $safe.Count) {
            $negative = Phase 'negative' $negativeFlags -ProfileOnly
            Counters 'negative' $negative $safe -Negative
        }
        if (-not $CompareOnly -and $safe.Count) {
            $extraNegative = SupplementalPhase 'negative-extra' $negativeFlags
            Counters 'negative-extra' $extraNegative $safe -Negative
        }
        foreach ($entry in $entries) {
            $name = $entry.flag; $f = $summary.functions[$name]
            # Numeric zero is emitted only after a selected implementation was observed.
            $f.coverage=[uint64]0; $f.errors=[uint64]0
            if ($entry.mode -eq 'DeclaredOutputsOnly' -and -not $CompareOnly) {
                $f.negative_coverage=[uint64]0; $f.negative_errors=[uint64]0
            }
            if (-not $SupplementalOnly) { foreach ($case in $Cases) {
                $positive = $summary.counters.compare["$case-1"][$name]
                $f.coverage += [uint64]$positive.comparisons; $f.errors += [uint64]$positive.errors
                if ($entry.mode -eq 'DeclaredOutputsOnly' -and -not $CompareOnly) {
                    $control = $summary.counters.negative["$case-1"][$name]
                    $f.negative_coverage += [uint64]$control.comparisons; $f.negative_errors += [uint64]$control.errors
                }
            } }
            foreach ($case in $extraCompared.runs.PSObject.Properties.Name) {
                $positive = $summary.counters['compare-extra'][$case][$name]
                $f.coverage += [uint64]$positive.comparisons; $f.errors += [uint64]$positive.errors
                if ($entry.mode -eq 'DeclaredOutputsOnly' -and -not $CompareOnly) {
                    $control = $summary.counters['negative-extra'][$case][$name]
                    $f.negative_coverage += [uint64]$control.comparisons; $f.negative_errors += [uint64]$control.errors
                }
            }
            # T13 requires real calls, not T10's pilot-specific 10,000-call target.
            if (-not $Discover -or $f.coverage -gt 0) {
                $syntheticResult=$script:synthetic[$entry.address]
                $syntheticPass=$entry.proof_kind -eq 'synthetic' -and $syntheticResult -and $syntheticResult.errors -eq 0 -and $syntheticResult.negative_errors -eq $syntheticResult.cases
                Criterion "coverage/$name" ($f.coverage -gt 0 -or $syntheticPass) @{game=$f;synthetic=$syntheticResult}
                if ($entry.mode -eq 'DeclaredOutputsOnly' -and -not $CompareOnly) {
                    Criterion "negative/$name" (($f.negative_coverage -gt 0 -and $f.negative_errors -gt 0) -or $syntheticPass) @{game=$f;synthetic=$syntheticResult}
                }
            } elseif ($Discover -and $entry.batch -eq 'b017' -and $entry.status -eq 'unproven' -and
                      $entry.reason -match '(?i)Discovery-first') {
                Criterion "coverage/$name" $true @{game=$f;synthetic=$null;unproven_no_call=$true}
            }
        }
        if (-not $SupplementalOnly -and -not $CompareOnly) {RenderProof}
        Criterion 'executable_unchanged' ((Hash 'build/pvz-console-edition.exe') -eq $exeHash) $exeHash
        $summary['supplemental_only'] = [bool]$SupplementalOnly
        $summary['compare_only'] = [bool]$CompareOnly
        $summary['supplemental_cases'] = @($SupplementalCases)
        $summary.passed = @($summary.criteria | Where-Object {-not $_.passed}).Count -eq 0
    } catch { $summary.failure = $_.Exception.Message; $summary['failure_details'] = $_.ScriptStackTrace; $summary['failure_position'] = $_.InvocationInfo.PositionMessage; $summary.passed = $false }
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
        $cvarName = Native-CVarName $name
        $off += @("--pvz_native_$cvarName=false", "--pvz_native_$($cvarName)_compare=false", "--pvz_native_$($cvarName)_inject_error=false")
    }
    $common = @('--pvz_deterministic_seed=42','--pvz_deterministic_clock=1792195200',
        '--log_max_file_size_mb=64','--log_max_files=128') + $off + $AdditionalGameArgs
    if ($InjectError) { $common += @($Replacements | ForEach-Object { "--pvz_native_$(Native-CVarName $_)_inject_error=true" }) }
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
    $compareFlags = @($Replacements | ForEach-Object { "--pvz_native_$(Native-CVarName $_)_compare=true" })
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
        $cvarName = Native-CVarName $name
        $native = Phase "native-$name" @("--pvz_native_$cvarName=true")
        Compare-Dumps "native-$name" $original $native
        $negativeCase = @($Cases | Sort-Object { Observed-Count $summary.counters.compare["$_-1"][$name] } -Descending)[0]
        $negative = Phase "negative-$name" @("--pvz_native_$($cvarName)_compare=true","--pvz_native_$($cvarName)_inject_error=true") -ProfileOnly -SelectedCases @($negativeCase)
        Counters "negative-$name" $negative @($name) -Negative
        Save-Summary
    }
    Require-Coverage $totals $Replacements
    Criterion 'executable_unchanged' ((Hash 'build/pvz-console-edition.exe') -eq $exeHash) $exeHash
    $summary.passed = @($summary.criteria | Where-Object {-not $_.passed}).Count -eq 0
} catch { $summary.failure = $_.Exception.Message; $summary['failure_details'] = $_.ScriptStackTrace; $summary['failure_position'] = $_.InvocationInfo.PositionMessage; $summary.passed = $false }
Save-Summary
Write-Output "Native proof: $OutputRoot/summary.json (passed=$($summary.passed))"
if (-not $summary.passed) { exit 1 }
