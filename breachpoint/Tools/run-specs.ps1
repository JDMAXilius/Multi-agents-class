<#
=========================================================================================
BREACHPOINT - RUNG 2 (headless unit specs). Tools/run-specs.ps1

Contract clause implemented (docs/contracts/testing.md):
  "2. Headless unit specs (Automation Framework, no rendering):
     UnrealEditor-Cmd <uproject> -ExecCmds='Automation RunTests <Project>.<SuitePrefix>; Quit'
     -unattended -nullrhi -nosplash -log -ReportExportPath=<dir>"
  Fill-ins: suite prefixes 'Breachpoint.Sim.*' (pinned combat/shield/match) and
  'Breachpoint.Bots.*' (determinism).
  "Sim suites are PINNED (exact values + invariants). A suite asserting only 'no crash'
   is a reportable finding."

THE ONE RULE THIS SCRIPT EXISTS FOR
  A test runner that reports success when it ran nothing is the most dangerous script in
  any repo. So: ZERO DISCOVERED TESTS IS NEVER GREEN HERE. It is BLOCKED (exit 3), with
  the reason printed. The same applies to a run whose report file cannot be found or
  parsed - unparseable is BLOCKED, not passed.

TODAY (31 Jul 2026) this script is EXPECTED to report BLOCKED: Source/Breachpoint/Tests/
contains only a .gitkeep. The first suite (Breachpoint.Sim.Combat) is BP00 step 2 and is
owned by sim-builder; it depends on BP02's attribute set existing. That is a gap in the
project, not a defect in this wrapper - and the wrapper says so out loud instead of
exiting 0.

R21 applies here too: the editor cannot be launched for a spec run while a build holds
the tree; and a spec run against binaries a live build is rewriting is meaningless.

USAGE
  .\Tools\run-specs.ps1                    # the rung-2 run (Sim + Bots)
  .\Tools\run-specs.ps1 -Help
  .\Tools\run-specs.ps1 -DryRun            # print the exact command, run nothing
  .\Tools\run-specs.ps1 -Filter Breachpoint.Sim.Combat
  .\Tools\run-specs.ps1 -TimeoutMinutes 30
  .\Tools\run-specs.ps1 -Force             # launch the editor even with no specs authored
                                           # (proves discovery really returns zero)

EXIT CODES (shared across the ladder; only 0 is green)
  0 PASS          at least one test was discovered AND every discovered test passed
  1 FAIL          a test failed, or the editor exited non-zero, or the run timed out
  2 INCONCLUSIVE  ran, produced a report, but the report is ambiguous
  3 BLOCKED       no suites authored / zero tests discovered / env or build-lock problem

PowerShell 5.1. No '&&', no ternaries, no null-coalescing.
=========================================================================================
#>

[CmdletBinding()]
param(
    [string[]]$Filter = @('Breachpoint.Sim', 'Breachpoint.Bots'),
    [int]$TimeoutMinutes = 0,
    [int]$TailLines = 0,
    [switch]$DryRun,
    [switch]$Force,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '_BRLadderCommon.ps1')

$ToolsDir = $PSScriptRoot
$RepoRoot = Get-BRRepoRoot -ToolsDir $ToolsDir
$SpecDir = Join-Path $RepoRoot 'Source\Breachpoint\Tests'

if ($Help) {
    Show-BRScriptHeader -Path $PSCommandPath
    Write-Host ''
    Write-Host 'PARAMETERS'
    Write-Host '  -Filter <prefixes>   Automation filters. Default: Breachpoint.Sim, Breachpoint.Bots'
    Write-Host '                       (testing.md Fill-ins). They are joined with ''+'' for'
    Write-Host '                       "Automation RunTests A+B".'
    Write-Host '  -TimeoutMinutes <n>  Kill the editor after n minutes -> FAIL(timeout).'
    Write-Host '                       Default: SPEC_TIMEOUT_MINUTES from Tools/env.local, else 20.'
    Write-Host '  -TailLines <n>       Print only the last n lines of the editor log (0 = all).'
    Write-Host '  -DryRun              Print the exact UnrealEditor-Cmd line and exit. Runs nothing.'
    Write-Host '  -Force               Launch the editor even when no spec source exists, to'
    Write-Host '                       demonstrate that discovery really returns zero.'
    Write-Host '  -Help                This text.'
    Write-Host ''
    Write-Host 'EXIT CODES  0 PASS | 1 FAIL | 2 INCONCLUSIVE | 3 BLOCKED   (only 0 is green)'
    Write-Host ''
    Write-Host 'ZERO TESTS DISCOVERED => BLOCKED (3). Never PASS. That is the entire point.'
    exit $BR_EXIT_PASS
}

Write-BRBanner -Script 'run-specs.ps1' -Rung 'RUNG 2 - headless unit specs (-nullrhi -unattended)' -RepoRoot $RepoRoot

# ---------------------------------------------------------------------------------------
# Preflight: environment
# ---------------------------------------------------------------------------------------
$engine = Resolve-BREngineRoot -ToolsDir $ToolsDir
if (-not $engine.Ok) {
    Write-BRBlocked -Rung 'RUNG 2' -Reasons $engine.Problems
    exit $BR_EXIT_BLOCKED
}
$EngineRoot = $engine.Root
$EditorCmd = Get-BREditorCmdPath -EngineRoot $EngineRoot
$UProject = Get-BRUProject -RepoRoot $RepoRoot

$envMap = Read-BREnvLocal -ToolsDir $ToolsDir
if ($TimeoutMinutes -le 0) {
    $TimeoutMinutes = 20
    if ($null -ne $envMap) {
        if ($envMap.ContainsKey('SPEC_TIMEOUT_MINUTES')) { $TimeoutMinutes = [int]$envMap['SPEC_TIMEOUT_MINUTES'] }
    }
}

Write-Host ("ENGINE_ROOT : {0}" -f $EngineRoot)
Write-Host ("editor-cmd  : {0}" -f $EditorCmd)
Write-Host ("uproject    : {0}" -f $UProject)
Write-Host ("filters     : {0}" -f ($Filter -join ' + '))
Write-Host ("timeout     : {0} minute(s)" -f $TimeoutMinutes)

$blockers = @()
if (-not (Test-Path -LiteralPath $EditorCmd)) {
    $blockers += ("UnrealEditor-Cmd.exe not found at '{0}' - the engine has not been built." -f $EditorCmd)
}
if (-not (Test-Path -LiteralPath $UProject)) {
    $blockers += ("Breachpoint.uproject not found at '{0}'." -f $UProject)
}
$moduleDll = Join-Path $RepoRoot 'Binaries\Win64\UnrealEditor-Breachpoint.dll'
if (-not (Test-Path -LiteralPath $moduleDll)) {
    $blockers += ("The project's editor module is not built ({0} absent). Rung 2 runs on rung 1's output - run Tools\run-ubt.ps1 first." -f $moduleDll)
}
if ($blockers.Count -gt 0) {
    Write-BRBlocked -Rung 'RUNG 2' -Reasons $blockers
    exit $BR_EXIT_BLOCKED
}

# R21 - a live build makes any spec result meaningless.
$live = @(Get-BRLiveBuildProcesses -EngineRoot $EngineRoot)
if ($live.Count -gt 0) {
    $reasons = @('A build is in flight; binaries are being rewritten under this run (R21). Live now:')
    foreach ($o in $live) { $reasons += ("  - {0}" -f $o) }
    Write-BRBlocked -Rung 'RUNG 2' -Reasons $reasons
    exit $BR_EXIT_BLOCKED
}

# ---------------------------------------------------------------------------------------
# Authoring pre-check: is there anything to discover at all?
# ---------------------------------------------------------------------------------------
$specFiles = @()
if (Test-Path -LiteralPath $SpecDir) {
    $specFiles = @(Get-ChildItem -LiteralPath $SpecDir -Filter '*.cpp' -Recurse -ErrorAction SilentlyContinue)
}
Write-Host ''
Write-Host ("spec source dir : {0}" -f $SpecDir)
Write-Host ("spec .cpp files : {0}" -f $specFiles.Count)
foreach ($f in $specFiles) { Write-Host ("   - {0}" -f $f.Name) }

$ReportDir = Join-Path (New-BRLogDir -ToolsDir $ToolsDir) ('specs-report-' + (Get-BRRunStamp))
$filterArg = ($Filter -join '+')
$execCmds = ('Automation RunTests {0}; Quit' -f $filterArg)
$specArgs = @(
    ('"{0}"' -f $UProject),
    ('-ExecCmds="{0}"' -f $execCmds),
    '-unattended',
    '-nullrhi',
    '-nosplash',
    '-nopause',
    '-nosound',
    '-stdout',
    '-fullstdoutlogoutput',
    '-log',
    ('-ReportExportPath="{0}"' -f $ReportDir)
)

Write-Host ''
Write-Host 'command that defines this rung:'
Write-Host ('"{0}" {1}' -f $EditorCmd, ($specArgs -join ' '))

if ($DryRun) {
    Write-Host ''
    Write-Host 'DRY RUN - nothing executed, no evidence produced. Not a rung-2 result.'
    exit $BR_EXIT_PASS
}

if ($specFiles.Count -eq 0 -and -not $Force) {
    $reasons = @()
    $reasons += 'NO SUITES AUTHORED YET.'
    $reasons += ("{0} contains no *.cpp - there is no Breachpoint.Sim.* or Breachpoint.Bots.* test in the module." -f $SpecDir)
    $reasons += 'Launching the editor would burn minutes to discover zero tests and print a'
    $reasons += 'green-looking "0 failed". This script refuses to manufacture that sentence.'
    $reasons += 'Owner: BP00 step 2 (sim-builder) - first suite Breachpoint.Sim.Combat in'
    $reasons += 'Source/Breachpoint/Tests/BRCombatSpec.cpp, pinned against DT_Weapons/CT_Combat,'
    $reasons += 'blocked in turn on BP02''s attribute set.'
    $reasons += 'Re-run with -Force to launch anyway and watch discovery return zero.'
    Write-BRBlocked -Rung 'RUNG 2' -Reasons $reasons
    Write-Host ''
    Write-Host 'RUNG2|discovered=0|passed=0|failed=0|verdict=BLOCKED|reason=no-suites-authored'
    exit $BR_EXIT_BLOCKED
}

# ---------------------------------------------------------------------------------------
# The run
# ---------------------------------------------------------------------------------------
New-Item -ItemType Directory -Path $ReportDir -Force | Out-Null
$logPath = Join-Path (New-BRLogDir -ToolsDir $ToolsDir) ('specs-' + (Get-BRRunStamp) + '.log')

$startTime = Get-Date
Write-Host ''
Write-Host ("start time (before launch) : {0}" -f (Format-BRTime $startTime))
Write-Host ("report dir                 : {0}" -f $ReportDir)
Write-Host ("log                        : {0}" -f $logPath)

$run = Invoke-BRNative -Executable $EditorCmd -Arguments $specArgs -LogPath $logPath `
    -WorkingDirectory $RepoRoot -TimeoutMinutes $TimeoutMinutes

Write-BRLogFile -LogPath $logPath -TailLines $TailLines -Title 'UnrealEditor-Cmd output'

Write-Host ''
Write-Host ("exit code (captured)       : {0}" -f $run.ExitCode)
Write-Host ("wall duration              : {0}s" -f $run.DurationSeconds)

# ---------------------------------------------------------------------------------------
# Report parsing. Defensive on purpose: anything we cannot read is BLOCKED, not passed.
# ---------------------------------------------------------------------------------------
$indexJson = Join-Path $ReportDir 'index.json'
$discovered = -1
$passed = 0
$failed = 0
$notRun = 0

if (Test-Path -LiteralPath $indexJson) {
    $raw = Get-Content -LiteralPath $indexJson -Raw
    $parsed = $null
    try { $parsed = ConvertFrom-Json $raw } catch { $parsed = $null }
    if ($null -ne $parsed) {
        $names = @()
        foreach ($p in $parsed.PSObject.Properties) { $names += $p.Name }
        if ($names -contains 'succeeded') { $passed = [int]$parsed.succeeded }
        if ($names -contains 'succeededWithWarnings') { $passed = $passed + [int]$parsed.succeededWithWarnings }
        if ($names -contains 'failed') { $failed = [int]$parsed.failed }
        if ($names -contains 'notRun') { $notRun = [int]$parsed.notRun }
        if ($names -contains 'tests') { $discovered = @($parsed.tests).Count }
        else { $discovered = $passed + $failed + $notRun }
    }
}
else {
    # Fall back to the log: the automation controller prints a summary there too.
    $sum = @(Select-BRLogLines -LogPath $logPath -Pattern 'Test Completed|Total Tests|Tests Run|Automation Test Succeeded|Automation Test Failed|No automation tests' -Max 40)
    if ($sum.Count -gt 0) {
        Write-Host ''
        Write-Host 'automation lines found in the log:'
        foreach ($l in $sum) { Write-Host ("  {0}" -f $l) }
    }
}

Write-Host ''
Write-Host ("report index.json          : {0}" -f $(if (Test-Path -LiteralPath $indexJson) { $indexJson } else { 'ABSENT' }))
Write-Host ("tests discovered           : {0}" -f $(if ($discovered -lt 0) { 'UNKNOWN' } else { $discovered }))
Write-Host ("passed / failed / not-run  : {0} / {1} / {2}" -f $passed, $failed, $notRun)

$verdict = 'BLOCKED'
$reason = ''
$exit = $BR_EXIT_BLOCKED

if ($run.TimedOut) {
    $verdict = 'FAIL'
    $reason = ("editor exceeded the {0}-minute timeout and was killed" -f $TimeoutMinutes)
    $exit = $BR_EXIT_FAIL
}
elseif ($discovered -lt 0) {
    $verdict = 'BLOCKED'
    $reason = 'no readable automation report was produced - the run cannot be scored. Unreadable is BLOCKED, never PASS.'
    $exit = $BR_EXIT_BLOCKED
}
elseif ($discovered -eq 0) {
    $verdict = 'BLOCKED'
    $reason = ("ZERO TESTS DISCOVERED for filter(s) '{0}'. The runner ran and found nothing. A runner that calls that success is the most dangerous script in the repo, so this is BLOCKED - no suites authored yet (BP00 step 2, sim-builder)." -f $filterArg)
    $exit = $BR_EXIT_BLOCKED
}
elseif ($failed -gt 0) {
    $verdict = 'FAIL'
    $reason = ("{0} test(s) failed" -f $failed)
    $exit = $BR_EXIT_FAIL
}
elseif ($run.ExitCode -ne 0) {
    $verdict = 'FAIL'
    $reason = ("tests reported no failures but UnrealEditor-Cmd exited {0} - treat as failure until explained" -f $run.ExitCode)
    $exit = $BR_EXIT_FAIL
}
else {
    $verdict = 'PASS'
    $reason = ("{0} test(s) discovered, {1} passed, 0 failed" -f $discovered, $passed)
    $exit = $BR_EXIT_PASS
}

Write-BRRule 'RUNG 2 SUMMARY (paste into the ticket Log)'
Write-Host ("filters   : {0}" -f $filterArg)
Write-Host ("start     : {0}" -f (Format-BRTime $startTime))
Write-Host ("exit code : {0}" -f $run.ExitCode)
Write-Host ("verdict   : {0}" -f $verdict)
Write-Host ("because   : {0}" -f $reason)
Write-Host ''
Write-Host ('RUNG2|filters={0}|discovered={1}|passed={2}|failed={3}|notrun={4}|editor_exit={5}|seconds={6}|verdict={7}' -f `
        $filterArg, $discovered, $passed, $failed, $notRun, $run.ExitCode, $run.DurationSeconds, $verdict)
Write-Host ("log       : {0}" -f $logPath)
Write-Host ("report    : {0}" -f $ReportDir)
Write-Host ''
Write-Host 'Reminder (testing.md): a suite that asserts only "no crash" is a reportable finding.'
Write-Host 'Passing this rung means the PINNED values held - not that the process exited 0.'
exit $exit
