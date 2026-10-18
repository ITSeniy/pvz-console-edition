<#
T10: one-command proof on immutable profile copies and fixed-step replays.
Inputs: built EXE, baseline profile, five existing CSVs (or recorded step CSVs).
Outputs: summary.json/md, nested determinism manifests, byte-strict comparisons.
Every input/output has SHA-256 provenance. No thresholds or state normalization.
#>
[CmdletBinding()]
param(
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
    Write-Output 'native proof parser: 18 checks passed'
    exit 0
}
Set-Location (Split-Path $PSScriptRoot -Parent)
if ($Record -and $PSBoundParameters.ContainsKey('ReplayDirectory')) { throw 'Use either -Record or -ReplayDirectory' }
if (-not $Replacements.Count -or @($Replacements | Select-Object -Unique).Count -ne $Replacements.Count) { throw 'Supply distinct replacements' }
foreach ($name in $Replacements) { if ($name -notmatch '^[a-z][a-z0-9_]*$') { throw "Invalid replacement name: $name" } }
if (-not $OutputRoot) { $OutputRoot = 'runtime-data/native-proof/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') }
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw 'OutputRoot must be new; previous evidence is preserved' }
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
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
function Phase([string]$Name, [string[]]$Flags, [int]$Repeats=1, [switch]$Record, [switch]$ProfileOnly, [string[]]$SelectedCases=$Cases) {
    $path = Join-Path $OutputRoot $Name
    $options = @{Baseline=$Baseline;Cases=$SelectedCases;Repeats=$Repeats;OutputRoot=$path;
        AdditionalGameArgs=@(Merge-GameFlags ($common + $Flags));ProfileOnly=$ProfileOnly;RecordSteps=$Record}
    if (-not $Record) { $options.FrameLocked=$true; $options.ReplayDirectory=$script:steps }
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
            $paths = @($property.Value.log_files | ForEach-Object { "$($_).$name.native-counts.bin" } | Where-Object { Test-Path -LiteralPath $_ })
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
            $pass = if ($Negative) { $c.replay_end_valid -and $c.comparisons -gt 0 -and $c.errors -gt 0 -and $c.logged_mismatches -gt 0 }
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
