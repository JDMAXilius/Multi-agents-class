<#
=======================================================================================
BREACHPOINT - Tools/render_weapons/render-weapons.ps1

Serves:   docs/ui/WEAPON-RENDER-PLAN.md - render the three weapon silhouettes from the
          meshes DT_Weapons.csv equips. This is the LANDING MECHANISM for those icons:
          they are derived artifacts and nobody hand-draws them (CLAUDE.md law 7).
Rulings:  R37 (committed plan + receipt), R21/R29/R36 (an editor session must not overlap
          anything that takes the project lock - and `-run=pythonscript` IS that).
Skill:    .claude/skills/ue-editor (headless commandlet, generated-script doctrine).

THE RENDER PATH IS A SPIKE. Nothing under this folder has ever been run against an editor.
Expect the first run to fail and to correct section 3.3 of the plan document. -PlanOnly and
-SelfTest are the two switches that are actually proven, because neither launches anything.

What it does, in order:
  1. resolves the engine from Tools/env.local (never a hardcoded path);
  2. checks the Python Editor Script Plugin is enabled in the .uproject;
  3. R21 guard - refuses to launch while ANY editor process is live, using the SHARED
     Get-BRLiveEditorProcesses from Tools/_BRLadderCommon.ps1. R36 exists because a
     locally-rewritten narrower guard is how this goes wrong: do not reimplement it here;
  4. runs render_weapons.py under UnrealEditor-Cmd, logging to Tools/Logs.

  -PlanOnly   validate the plan and print every framing number WITHOUT an editor.
  -SelfTest   run the no-editor self-test over the plan and the framing math.
  -DryRun     load, frame and report under the editor; render nothing, write nothing.

Exit codes (ladder convention): 0 PASS · 1 FAIL · 2 INCONCLUSIVE · 3 BLOCKED.

BEFORE YOU LAUNCH THIS: check for a live builder by hand. R29.3/R36 make this commandlet a
party to the build lock even though it is not a build, and a builder dispatched over it is
the failure R21 was written for.

PowerShell 5.1 only. No '&&', no ternaries, no '??'.
=======================================================================================
#>
[CmdletBinding()]
param(
    [switch]$PlanOnly,
    [switch]$SelfTest,
    [switch]$DryRun,
    [int]$TimeoutMinutes = 20,
    [switch]$Help
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$genDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$toolsDir = Split-Path -Parent $genDir
. (Join-Path $toolsDir '_BRLadderCommon.ps1')

if ($Help) { Show-BRScriptHeader -Path $MyInvocation.MyCommand.Path; exit $BR_EXIT_PASS }

$repoRoot = Get-BRRepoRoot -ToolsDir $toolsDir
$rung = 'weapon silhouette render (SPIKE)'
Write-BRBanner -Script 'render-weapons.ps1' -Rung $rung -RepoRoot $repoRoot

$planScript   = Join-Path $genDir 'render_plan.py'
$renderScript = Join-Path $genDir 'render_weapons.py'
$testScript   = Join-Path $genDir 'selftest_no_editor.py'

function Get-BRPython {
    $py = Get-Command 'python' -ErrorAction SilentlyContinue
    if ($null -ne $py) { return $py.Source }
    $py = Get-Command 'python3' -ErrorAction SilentlyContinue
    if ($null -ne $py) { return $py.Source }
    return ''
}

# =======================================================================================
# -PlanOnly / -SelfTest: no editor at all. These are the proven paths.
# =======================================================================================
if ($PlanOnly -or $SelfTest) {
    Write-BRRule 'NO EDITOR - nothing is launched, no asset is touched'
    $python = Get-BRPython
    if ($python -eq '') {
        Write-BRBlocked -Rung $rung -Reasons @(
            'no `python` on PATH. These scripts need CPython 3.9+ (stdlib only).',
            'Alternatively: <ENGINE_ROOT>\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
        )
        exit $BR_EXIT_BLOCKED
    }
    if ($SelfTest) { & $python $testScript } else { & $python $planScript '--verbose' }
    $code = $LASTEXITCODE
    Write-BRRule
    if ($code -eq 0) {
        Write-Host 'PASS - static (rung 1) check of a Python table and some arithmetic. It says'
        Write-Host '       NOTHING about the meshes, the rig, or whether anything renders. The'
        Write-Host '       orientation values are still guesses; only a render can correct them.'
        exit $BR_EXIT_PASS
    }
    if ($code -eq 3) {
        Write-BRBlocked -Rung $rung -Reasons @('the plan could not read its inputs (DT_Weapons.csv?).')
        exit $BR_EXIT_BLOCKED
    }
    Write-Host 'FAIL - fix Tools/render_weapons/render_plan.py.'
    exit $BR_EXIT_FAIL
}

# =======================================================================================
# editor path - UNPROVEN. Read the SPIKE CHECKLIST before you trust anything it writes.
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

$pluginOk = $false
try {
    $projJson = Get-Content -LiteralPath $uproject -Raw | ConvertFrom-Json
    if ($null -ne $projJson.Plugins) {
        foreach ($p in $projJson.Plugins) {
            if ($p.Name -eq 'PythonScriptPlugin') {
                if ($p.Enabled -eq $true) { $pluginOk = $true }
            }
        }
    }
} catch {
    Write-Host ('could not parse {0} ({1}); continuing without the plugin pre-check.' -f $uproject, $_.Exception.Message)
    $pluginOk = $true
}
if (-not $pluginOk) {
    Write-BRBlocked -Rung $rung -Reasons @(
        'the Python Editor Script Plugin is not enabled in Breachpoint.uproject, so',
        '-run=pythonscript has nothing to run.',
        'Meanwhile:  Tools\render_weapons\render-weapons.ps1 -PlanOnly   and   -SelfTest'
    )
    exit $BR_EXIT_BLOCKED
}

# ---- R21 / R29 / R36: one editor, and this commandlet takes the project lock. -----------
$liveEditors = Get-BRLiveEditorProcesses
if ($liveEditors.Count -gt 0) {
    Write-BRBlocked -Rung $rung -Reasons @(
        'an Unreal editor is already running on this machine:',
        ('  ' + ($liveEditors -join '; ')),
        'This run spawns actors, captures a scene and writes into Content/UI. R29.1 is one',
        'editor per project, ever; R36 widened R29.3 so that ANYTHING taking the project lock',
        'counts - and a -run=pythonscript commandlet is exactly that without being a build.',
        'Close the editor, or wait for whoever holds it. A block is the law firing, not an',
        'obstacle to route around.',
        'While you wait:  Tools\render_weapons\render-weapons.ps1 -PlanOnly',
        '                 Tools\render_weapons\render-weapons.ps1 -SelfTest'
    )
    exit $BR_EXIT_BLOCKED
}

$pyArgs = @()
if ($DryRun) { $pyArgs += '--dry-run' }

$logDir = New-BRLogDir -ToolsDir $toolsDir
$stamp = Get-BRRunStamp
$logPath = Join-Path $logDir ('rendwpn-{0}.log' -f $stamp)

# render_weapons.py reads BR_RENDERWPN_ARGS because -run=pythonscript arg passing differs
# between launch shapes; the env var is the one that always survives.
$env:BR_RENDERWPN_ARGS = ($pyArgs -join ' ')
Write-Host ('script args : {0}' -f $env:BR_RENDERWPN_ARGS)
Write-Host ('log         : {0}' -f $logPath)

$argList = @(
    ('"{0}"' -f $uproject),
    '-run=pythonscript',
    ('-script="{0}"' -f $renderScript),
    '-stdout', '-unattended', '-nosplash', '-nop4', '-NoLiveCoding'
)

$result = Invoke-BRNative -Executable $editorCmd -Arguments $argList -LogPath $logPath `
    -WorkingDirectory $repoRoot -TimeoutMinutes $TimeoutMinutes

Write-BRLogFile -LogPath $result.LogPath -TailLines 150 -Title 'UnrealEditor-Cmd output'

Write-BRRule ('{0}: result' -f $rung)
Write-Host ('exit code   : {0}' -f $result.ExitCode)
Write-Host ('duration    : {0}s' -f $result.DurationSeconds)
foreach ($line in (Select-BRLogLines -LogPath $result.LogPath -Pattern 'BR-rendwpn' -Max 250)) {
    Write-Host ('  ' + $line)
}

if ($result.TimedOut) {
    Write-Host ('TIMED OUT after {0} minute(s). The editor may still hold the project lock (R21:' -f $TimeoutMinutes)
    Write-Host 'stopping an agent does not stop its children). Kill the process tree before anything'
    Write-Host 'else touches this project.'
    exit $BR_EXIT_FAIL
}
Write-Host ''
Write-Host 'Read Tools/render_weapons/receipts/render-<stamp>.md - it was written AS THE RUN WENT,'
Write-Host 'refusals included, and it is the only reviewable record of what the editor did.'
if ($result.ExitCode -eq 0) {
    Write-Host 'PASS: three PNGs exist and match the plan. That is the GENERATED rung and nothing'
    Write-Host 'more. Still owed: a human eye on the framing (the orientation values were guesses),'
    Write-Host 'the 2px min-feature test, the hole reduction, and a consumer - there is none.'
    exit $BR_EXIT_PASS
}
if ($result.ExitCode -eq 3) {
    Write-BRBlocked -Rung $rung -Reasons @('the run refused or could not start - see the receipt.')
    exit $BR_EXIT_BLOCKED
}
Write-Host 'FAIL - EXPECTED on the early runs. Work the SPIKE CHECKLIST (WEAPON-RENDER-PLAN.md'
Write-Host 'section 11) against the receipt and the raw *_ss4x.png, then correct section 3.3 and'
Write-Host 'the orientation columns in render_plan.py. Do not commit any icon from a failed run.'
exit $BR_EXIT_FAIL
