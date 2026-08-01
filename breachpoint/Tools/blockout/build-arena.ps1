<#
=======================================================================================
BREACHPOINT - Tools/blockout/build-arena.ps1

Serves:   TICKET BP07 step 2 - regenerate the BR_Arena01 blockout from
          Content/Data/arena_manifest.json. This is the LANDING MECHANISM for that map:
          the .umap is a derived artifact and nobody hand-places it (CLAUDE.md law 7).
Contract: docs/contracts/data-and-assets.md (one owner per binary; lock before editing).
Skill:    .claude/skills/ue-editor (headless `-run=pythonscript`, generated-script doctrine).

What it does, in order:
  1. resolves the engine from Tools/env.local (never a hardcoded path - same rule as the
     rest of the ladder);
  2. R21 guard - refuses to launch while ANY editor process is live. A second editor on
     the same project is how a binary map gets two writers, and the packet that wrote
     this script was itself blocked by exactly that;
  3. runs Tools/blockout/build_arena.py under UnrealEditor-Cmd, logging to Tools/Logs;
  4. reports the plan digest, which is what makes a re-run checkable.

  -PlanOnly    validate the manifest and print the plan WITHOUT any editor. Plain CPython.
               Use this while someone else has the editor - it is the whole reason the
               generator is split in two.
  -Strict      refuse to build elements whose extents were inferred from prose/profile.
  -DryRun      run under the editor but touch no asset.
  -NoLockCheck skip the git-lfs lock check (recorded in the build report; justify it in
               the ticket Log).

Exit codes (ladder convention): 0 PASS · 1 FAIL · 2 INCONCLUSIVE · 3 BLOCKED.
A 0 here means "actors placed, level saved". It does NOT mean the arena plays: nav,
reachability and the manifest's doubts[] are a rung-3 walkthrough (BP07 step 4).

PowerShell 5.1 only. No '&&', no ternaries, no '??'.
=======================================================================================
#>
[CmdletBinding()]
param(
    [switch]$PlanOnly,
    [switch]$Strict,
    [switch]$DryRun,
    [switch]$NoLockCheck,
    # NOT -Profile: $Profile is a PowerShell automatic variable and shadowing it in a
    # script that dot-sources shared helpers is a trap for the next reader.
    [string]$ManifestPath = '',
    [string]$ProfilePath = '',
    [int]$TimeoutMinutes = 20,
    [switch]$Help
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$blockoutDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$toolsDir = Split-Path -Parent $blockoutDir
. (Join-Path $toolsDir '_BRLadderCommon.ps1')

if ($Help) { Show-BRScriptHeader -Path $MyInvocation.MyCommand.Path; exit $BR_EXIT_PASS }

$repoRoot = Get-BRRepoRoot -ToolsDir $toolsDir
$rung = 'BP07 arena blockout (generated)'
Write-BRBanner -Script 'build-arena.ps1' -Rung $rung -RepoRoot $repoRoot

$planScript  = Join-Path $blockoutDir 'arena_plan.py'
$buildScript = Join-Path $blockoutDir 'build_arena.py'

# ---- forward the python-side switches -------------------------------------------------
$pyArgs = @()
if ($Strict)      { $pyArgs += '--strict' }
if ($DryRun)      { $pyArgs += '--dry-run' }
if ($NoLockCheck) { $pyArgs += '--no-lock-check' }
if ($ManifestPath -ne '') { $pyArgs += ('--manifest "{0}"' -f $ManifestPath) }
if ($ProfilePath  -ne '') { $pyArgs += ('--profile "{0}"' -f $ProfilePath) }

# =======================================================================================
# -PlanOnly: no editor at all. Validates the manifest and prints the build plan.
# =======================================================================================
if ($PlanOnly) {
    Write-BRRule 'PLAN ONLY - no editor is launched, no asset is touched'
    $py = Get-Command 'python' -ErrorAction SilentlyContinue
    if ($null -eq $py) {
        Write-BRBlocked -Rung $rung -Reasons @(
            'no `python` on PATH. arena_plan.py needs CPython 3.9+ (stdlib only).',
            'Alternatively run it with the engine python: <ENGINE_ROOT>\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
        )
        exit $BR_EXIT_BLOCKED
    }
    $planArgs = @('--verbose')
    if ($Strict) { $planArgs += '--strict' }
    if ($ManifestPath -ne '') { $planArgs += @('--manifest', $ManifestPath) }
    if ($ProfilePath  -ne '') { $planArgs += @('--profile', $ProfilePath) }
    & $py.Source $planScript @planArgs
    $code = $LASTEXITCODE
    Write-BRRule
    if ($code -eq 0) {
        Write-Host 'PLAN: PASS - the manifest is buildable. Static (rung 1) check only.'
        exit $BR_EXIT_PASS
    }
    if ($code -eq 3) {
        Write-BRBlocked -Rung $rung -Reasons @('arena_plan.py could not read its inputs.')
        exit $BR_EXIT_BLOCKED
    }
    Write-Host 'PLAN: FAIL - the manifest violates its own acceptance numbers. Fix the manifest.'
    exit $BR_EXIT_FAIL
}

# =======================================================================================
# editor path
# =======================================================================================
$engine = Resolve-BREngineRoot -ToolsDir $toolsDir
if (-not $engine.Ok) {
    Write-BRBlocked -Rung $rung -Reasons $engine.Problems
    exit $BR_EXIT_BLOCKED
}
$editorCmd = Get-BREditorCmdPath -EngineRoot $engine.Root
if (-not (Test-Path -LiteralPath $editorCmd)) {
    Write-BRBlocked -Rung $rung -Reasons @(("UnrealEditor-Cmd.exe not found at {0}" -f $editorCmd))
    exit $BR_EXIT_BLOCKED
}
$uproject = Get-BRUProject -RepoRoot $repoRoot
if (-not (Test-Path -LiteralPath $uproject)) {
    Write-BRBlocked -Rung $rung -Reasons @(("Breachpoint.uproject not found at {0}" -f $uproject))
    exit $BR_EXIT_BLOCKED
}

# ---- R21: one editor at a time. Two writers on a binary .umap is unresolvable. --------
$liveEditors = Get-BRLiveEditorProcesses
if ($liveEditors.Count -gt 0) {
    Write-BRBlocked -Rung $rung -Reasons @(
        'an Unreal editor is already running on this machine:',
        ('  ' + ($liveEditors -join '; ')),
        'This generator rewrites a binary .umap. Close the editor, or wait for whoever holds it.',
        'While you wait, run:  Tools\blockout\build-arena.ps1 -PlanOnly   (validates the manifest,',
        'prints the whole build plan, launches nothing).'
    )
    exit $BR_EXIT_BLOCKED
}

$logDir = New-BRLogDir -ToolsDir $toolsDir
$stamp = Get-BRRunStamp
$logPath = Join-Path $logDir ('blockout-{0}.log' -f $stamp)

# build_arena.py reads BR_BLOCKOUT_ARGS because -run=pythonscript arg passing differs
# between launch shapes; the env var is the one that always survives.
$env:BR_BLOCKOUT_ARGS = ($pyArgs -join ' ')
Write-Host ('script args : {0}' -f $env:BR_BLOCKOUT_ARGS)
Write-Host ('log         : {0}' -f $logPath)

$argList = @(
    ('"{0}"' -f $uproject),
    '-run=pythonscript',
    ('-script="{0}"' -f $buildScript),
    '-stdout', '-unattended', '-nosplash', '-nop4', '-NoLiveCoding'
)

$result = Invoke-BRNative -Executable $editorCmd -Arguments $argList -LogPath $logPath `
    -WorkingDirectory $repoRoot -TimeoutMinutes $TimeoutMinutes

Write-BRLogFile -LogPath $result.LogPath -TailLines 120 -Title 'UnrealEditor-Cmd output'

Write-BRRule ('{0}: result' -f $rung)
Write-Host ('exit code   : {0}' -f $result.ExitCode)
Write-Host ('duration    : {0}s' -f $result.DurationSeconds)
foreach ($line in (Select-BRLogLines -LogPath $result.LogPath -Pattern 'BR-blockout' -Max 200)) {
    Write-Host ('  ' + $line)
}

if ($result.TimedOut) {
    Write-Host ('TIMED OUT after {0} minute(s) - the map state is UNKNOWN. Do not commit it.' -f $TimeoutMinutes)
    exit $BR_EXIT_FAIL
}
if ($result.ExitCode -eq 0) {
    Write-Host ''
    Write-Host 'PASS: the generator placed its actors and saved the level.'
    Write-Host 'That is the GENERATED rung, not a working-arena rung. Before anyone calls BR_Arena01'
    Write-Host 'done: nav/reachability sweep, a rung-3 walkthrough, and the four doubts[] in the'
    Write-Host 'manifest landing_note (BP07 step 4, owner: verifier).'
    exit $BR_EXIT_PASS
}
if ($result.ExitCode -eq 3) {
    Write-BRBlocked -Rung $rung -Reasons @('the generator could not run - see the log above.')
    exit $BR_EXIT_BLOCKED
}
Write-Host 'FAIL: the generator refused to build or hit build failures. See the BR-blockout lines above.'
exit $BR_EXIT_FAIL
