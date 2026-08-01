<#
=======================================================================================
BREACHPOINT - Tools/audit_blueprints/audit-blueprints.ps1

Serves:   DESIGN RULING R26 (docs/DESIGN-RULINGS.md) - "Blueprint children of BR C++
          classes are permitted as DEFAULT-VALUE CONTAINERS ONLY. Zero graph nodes."
          R26 says its own enforcement is owed: "Until that script exists this ruling is
          enforced by goodwill, and that is stated here rather than assumed." This is the
          wrapper for that script.
Rung:     STATIC gate. Rung 1/2 evidence: it proves what is serialised in Content/, not
          that anything plays. It is intended to run as a rung-2 gate from
          Tools/run-specs.ps1 - see this folder's README for the exact block; that file
          belongs to BP00 and is NOT edited from here.
Rulings:  R21 (one build/editor at a time), R18 as narrowed by R26, law 3 (data is not
          code), law 7 (one owner per binary).

TWO MODES

  (default) OFFLINE - reads the .uasset package headers with plain CPython. No engine, no
          editor, nothing launched, nothing written. Safe while a human has the editor
          open, which is exactly when you want to be able to check. This is the primary
          evidence path.
  -Editor  CROSS-CHECK - runs Tools/audit_blueprints/dump_blueprints.py under
          UnrealEditor-Cmd to compare the offline reading against the asset registry's
          own ParentClass / IsDataOnly / component counts, and fails on disagreement.
          Refuses to launch while any editor or build is live (R21).

  -SelfTest  prove the node detector fires on Blueprints that DO contain logic.
  -JsonOut <path>  write the machine-readable audit (default: Saved/R26/r26-audit.json).
  -NoEditorLockCheck  in OFFLINE mode, allow a PASS even though an editor is running.
          Without it, a live editor caps the offline verdict at INCONCLUSIVE, because a
          green claim about bytes on disk says nothing about unsaved editor state. If you
          use this flag, say why in the ticket Log.

EXIT CODES (Tools/_BRLadderCommon.ps1 vocabulary; only 0 is green)
  0 PASS          assets were found and every one conforms to R26
  1 FAIL          at least one R26 condition is violated
  2 INCONCLUSIVE  ran but proved nothing (unparseable asset, unestablished fact, or a
                  live editor holding unsaved state under an offline scan)
  3 BLOCKED       could not run, or ZERO assets were found. Zero is never PASS.

PowerShell 5.1 only. No '&&', no '||', no ternaries, no '??'.
=======================================================================================
#>
[CmdletBinding()]
param(
    [switch]$Editor,
    [switch]$SelfTest,
    [switch]$NoEditorLockCheck,
    [string]$JsonOut = '',
    [string]$ContentDir = '',
    [int]$TimeoutMinutes = 20,
    [switch]$Help
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$auditDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$toolsDir = Split-Path -Parent $auditDir
. (Join-Path $toolsDir '_BRLadderCommon.ps1')

if ($Help) { Show-BRScriptHeader -Path $MyInvocation.MyCommand.Path; exit $BR_EXIT_PASS }

$repoRoot = Get-BRRepoRoot -ToolsDir $toolsDir
$rung = 'R26 Blueprint audit (static)'
Write-BRBanner -Script 'audit-blueprints.ps1' -Rung $rung -RepoRoot $repoRoot

$auditScript = Join-Path $auditDir 'audit_r26.py'
$dumpScript = Join-Path $auditDir 'dump_blueprints.py'

if ($JsonOut -eq '') {
    $JsonOut = Join-Path $repoRoot 'Saved\R26\r26-audit.json'
}
if ($ContentDir -eq '') {
    $ContentDir = Join-Path $repoRoot 'Content'
}

# ---------------------------------------------------------------------------------------
# Find a python. PATH first, then the engine's bundled interpreter (which is present on
# every machine that can build this project) - never a hardcoded path.
# ---------------------------------------------------------------------------------------
function Resolve-BRPython {
    param([string]$ToolsDir)
    $cmd = Get-Command 'python' -ErrorAction SilentlyContinue
    if ($null -ne $cmd) { return $cmd.Source }
    $engine = Resolve-BREngineRoot -ToolsDir $ToolsDir
    if ($engine.Ok) {
        $bundled = Join-Path $engine.Root 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
        if (Test-Path -LiteralPath $bundled) { return $bundled }
    }
    return ''
}

$python = Resolve-BRPython -ToolsDir $toolsDir
if ($python -eq '') {
    Write-BRBlocked -Rung $rung -Reasons @(
        'no CPython available. audit_r26.py needs python 3.8+ (stdlib only).',
        'Put python on PATH, or set ENGINE_ROOT in Tools/env.local so the engine''s',
        'bundled interpreter (Engine\Binaries\ThirdParty\Python3\Win64\python.exe) can be used.'
    )
    exit $BR_EXIT_BLOCKED
}
Write-Host ("python      : {0}" -f $python)
Write-Host ("content dir : {0}" -f $ContentDir)
Write-Host ("audit json  : {0}" -f $JsonOut)

# ---------------------------------------------------------------------------------------
# -SelfTest: prove the detector fires. No editor, no verdict about R26.
# ---------------------------------------------------------------------------------------
if ($SelfTest) {
    Write-BRRule 'SELF-TEST - does the node detector actually fire?'
    & $python $auditScript '--selftest' '--content' $ContentDir
    $code = $LASTEXITCODE
    Write-BRRule
    if ($code -eq 0) {
        Write-Host 'SELF-TEST PASS. This says the detector works; it says nothing about R26 conformance.'
        exit $BR_EXIT_PASS
    }
    Write-Host 'SELF-TEST INCONCLUSIVE - the detector never fired. Do not trust a PASS until this does.'
    exit $BR_EXIT_INCONCLUSIVE
}

# ---------------------------------------------------------------------------------------
# OFFLINE mode (default): no editor is launched, no asset is touched.
# ---------------------------------------------------------------------------------------
if (-not $Editor) {
    Write-BRRule 'OFFLINE - package headers only; nothing is launched, nothing is written'

    $liveEditors = @(Get-BRLiveEditorProcesses)
    $liveBuilds = @(Get-BRLiveBuildProcesses)
    $pyArgs = @('--content', $ContentDir, '--json', $JsonOut)

    if ($liveEditors.Count -gt 0) {
        Write-Host ''
        Write-Host 'An Unreal editor is live:'
        foreach ($e in $liveEditors) { Write-Host ("  - {0}" -f $e) }
        if ($NoEditorLockCheck) {
            Write-Host '-NoEditorLockCheck given: the verdict below is about the bytes ON DISK only.'
            Write-Host 'Unsaved Blueprint edits are invisible to it. Justify this in the ticket Log.'
        }
        else {
            Write-Host 'A PASS would be a claim about a file, not about the asset, so a PASS will be'
            Write-Host 'downgraded to INCONCLUSIVE. Save and close the editor for a green run, or pass'
            Write-Host '-NoEditorLockCheck and say why in the ticket Log. FAIL is unaffected: a violation'
            Write-Host 'already committed to disk is a violation.'
            $pyArgs += '--live-editor'
        }
    }
    if ($liveBuilds.Count -gt 0) {
        Write-Host ''
        Write-Host 'A build is in flight (R21) - it does not touch Content/, so this static audit still'
        Write-Host 'means what it says. Recorded for the log:'
        foreach ($b in $liveBuilds) { Write-Host ("  - {0}" -f $b) }
    }

    Write-Host ''
    Write-Host ('command: "{0}" "{1}" {2}' -f $python, $auditScript, ($pyArgs -join ' '))
    & $python $auditScript @pyArgs
    $code = $LASTEXITCODE

    Write-BRRule ('{0}: result' -f $rung)
    Write-Host ('exit code : {0}' -f $code)
    if ($code -eq $BR_EXIT_PASS) {
        Write-Host 'PASS - every in-scope Blueprint is a default-value container (R26 conditions'
        Write-Host '1, 2, 3, 5 asserted mechanically). Condition 4 is only partly automatable: the'
        Write-Host 'overridden-property list in the report is printed FOR A HUMAN to read.'
        exit $BR_EXIT_PASS
    }
    if ($code -eq $BR_EXIT_FAIL) {
        Write-Host 'FAIL - see the FINDINGS lines above. Fix the asset, not this script.'
        exit $BR_EXIT_FAIL
    }
    if ($code -eq $BR_EXIT_BLOCKED) {
        Write-BRBlocked -Rung $rung -Reasons @(
            'the audit could not run, or it found ZERO assets to audit.',
            'Zero assets is BLOCKED, never PASS - an auditor that finds nothing and reports',
            'green is the most dangerous script in a repo (same rule as run-specs.ps1).'
        )
        exit $BR_EXIT_BLOCKED
    }
    Write-Host 'INCONCLUSIVE - the run proved nothing. Not green. See the reasons above.'
    exit $BR_EXIT_INCONCLUSIVE
}

# ---------------------------------------------------------------------------------------
# -Editor mode: cross-check under UnrealEditor-Cmd. R21 applies with full force.
# ---------------------------------------------------------------------------------------
Write-BRRule 'EDITOR CROSS-CHECK - compares the offline reading against the asset registry'

$engine = Resolve-BREngineRoot -ToolsDir $toolsDir
if (-not $engine.Ok) {
    Write-BRBlocked -Rung $rung -Reasons $engine.Problems
    exit $BR_EXIT_BLOCKED
}
$editorCmd = Get-BREditorCmdPath -EngineRoot $engine.Root
$uproject = Get-BRUProject -RepoRoot $repoRoot

$blockers = @()
if (-not (Test-Path -LiteralPath $editorCmd)) {
    $blockers += ("UnrealEditor-Cmd.exe not found at {0} - the engine has not been built." -f $editorCmd)
}
if (-not (Test-Path -LiteralPath $uproject)) {
    $blockers += ("Breachpoint.uproject not found at {0}." -f $uproject)
}
$liveEditors = @(Get-BRLiveEditorProcesses)
if ($liveEditors.Count -gt 0) {
    $blockers += 'An Unreal editor is already running on this machine (R21):'
    foreach ($e in $liveEditors) { $blockers += ("  - {0}" -f $e) }
    $blockers += 'A second editor on the same project is how asset state gets two readers.'
    $blockers += 'While you wait, run the OFFLINE audit - it launches nothing:'
    $blockers += '    Tools\audit_blueprints\audit-blueprints.ps1'
}
$liveBuilds = @(Get-BRLiveBuildProcesses -EngineRoot $engine.Root)
if ($liveBuilds.Count -gt 0) {
    $blockers += 'A build is in flight; the binaries this editor would load are being rewritten (R21):'
    foreach ($b in $liveBuilds) { $blockers += ("  - {0}" -f $b) }
}
$patches = @(Get-BRLiveCodingArtifacts -RepoRoot $repoRoot)
if ($patches.Count -gt 0) {
    $blockers += 'Live Coding patch DLLs are present - the loaded module was patched, not built:'
    foreach ($p in $patches) { $blockers += ("  - {0}" -f $p) }
}
if ($blockers.Count -gt 0) {
    Write-BRBlocked -Rung $rung -Reasons $blockers
    exit $BR_EXIT_BLOCKED
}

$logDir = New-BRLogDir -ToolsDir $toolsDir
$stamp = Get-BRRunStamp
$logPath = Join-Path $logDir ('r26-audit-{0}.log' -f $stamp)

$env:BR_R26_ARGS = ('--json "{0}" --content "{1}"' -f $JsonOut, $ContentDir)
Write-Host ('script args : {0}' -f $env:BR_R26_ARGS)
Write-Host ('log         : {0}' -f $logPath)

$argList = @(
    ('"{0}"' -f $uproject),
    '-run=pythonscript',
    ('-script="{0}"' -f $dumpScript),
    '-stdout', '-unattended', '-nosplash', '-nop4', '-nullrhi', '-NoLiveCoding'
)

$result = Invoke-BRNative -Executable $editorCmd -Arguments $argList -LogPath $logPath `
    -WorkingDirectory $repoRoot -TimeoutMinutes $TimeoutMinutes

Write-BRLogFile -LogPath $result.LogPath -TailLines 200 -Title 'UnrealEditor-Cmd output'

Write-BRRule ('{0}: result' -f $rung)
Write-Host ('exit code : {0}' -f $result.ExitCode)
Write-Host ('duration  : {0}s' -f $result.DurationSeconds)
foreach ($line in (Select-BRLogLines -LogPath $result.LogPath -Pattern 'BR-r26|R26AUDIT' -Max 200)) {
    Write-Host ('  ' + $line)
}

if ($result.TimedOut) {
    Write-Host ('TIMED OUT after {0} minute(s) - nothing was proved.' -f $TimeoutMinutes)
    exit $BR_EXIT_INCONCLUSIVE
}
if ($result.ExitCode -eq $BR_EXIT_PASS) {
    Write-Host 'PASS - offline reading and editor reading agree, and every asset conforms.'
    exit $BR_EXIT_PASS
}
if ($result.ExitCode -eq $BR_EXIT_FAIL) {
    Write-Host 'FAIL - R26 violation, or the two readers disagree. Both are findings.'
    exit $BR_EXIT_FAIL
}
if ($result.ExitCode -eq $BR_EXIT_BLOCKED) {
    Write-BRBlocked -Rung $rung -Reasons @('the cross-check could not run - see the log above.')
    exit $BR_EXIT_BLOCKED
}
Write-Host 'INCONCLUSIVE - the cross-check ran but proved nothing. Not green.'
exit $BR_EXIT_INCONCLUSIVE
