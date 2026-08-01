<#
=======================================================================================
BREACHPOINT - Tools/gen_input/build-input.ps1

Serves:   TICKET BP01 step 3b - generate the eleven IA_ input actions, DA_InputConfig and
          the IMC_Default key bindings from Tools/gen_input/input_profile.json. This is the
          LANDING MECHANISM for Content/Input: those .uassets are derived artifacts and
          nobody hand-places them (CLAUDE.md law 7).
Contract: docs/contracts/data-and-assets.md (soft asset refs; one owner per binary, lock
          before editing). Acceptance: UBRInputConfig::IsDataValid, run for real by the
          python half.
Skill:    .claude/skills/ue-editor (headless `-run=pythonscript`, generated-script doctrine).

What it does, in order:
  1. resolves the engine from Tools/env.local (never a hardcoded path - same rule as the
     rest of the ladder);
  2. checks that the Python Editor Script Plugin is actually enabled in the .uproject,
     because `-run=pythonscript` against a project without it fails in a way that reads
     like a script bug rather than a project setting;
  3. R21 guard - refuses to launch while ANY editor process is live. A second editor on
     the same project is how a binary asset gets two writers;
  4. runs Tools/gen_input/build_input.py under UnrealEditor-Cmd, logging to Tools/Logs;
  5. reports the plan digest, which is what makes a re-run checkable.

  -PlanOnly    validate the profile and print the whole plan WITHOUT any editor. Plain
               CPython. Use this while someone else holds the project - it is the entire
               reason the generator is split in two.
  -SelfTest    run the no-editor logic test (fake `unreal`) over build_input.py. Proves the
               generator's decisions and its idempotency; proves nothing about the engine.
  -DryRun      run under the editor but write no asset.
  -AllowDrift  repair a hand-edited generated asset without failing the run. Justify it in
               the ticket Log - the failing exit code exists so an edit is SEEN.
  -NoLockCheck skip the git-lfs lock check (recorded in the run report; justify it).

Exit codes (ladder convention): 0 PASS · 1 FAIL · 2 INCONCLUSIVE · 3 BLOCKED.
A 0 here means "the assets were written and read back as the profile describes them, and
IsDataValid accepted the config". It does NOT mean a key does anything: that needs a pawn,
a mapping context added at runtime and a PIE smoke (BP01 step 5, owner: verifier).

PowerShell 5.1 only. No '&&', no ternaries, no '??'.
=======================================================================================
#>
[CmdletBinding()]
param(
    [switch]$PlanOnly,
    [switch]$SelfTest,
    [switch]$DryRun,
    [switch]$AllowDrift,
    [switch]$NoLockCheck,
    # NOT -Profile: $Profile is a PowerShell automatic variable and shadowing it in a
    # script that dot-sources shared helpers is a trap for the next reader.
    [string]$ProfilePath = '',
    [int]$TimeoutMinutes = 15,
    [switch]$Help
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$genDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$toolsDir = Split-Path -Parent $genDir
. (Join-Path $toolsDir '_BRLadderCommon.ps1')

if ($Help) { Show-BRScriptHeader -Path $MyInvocation.MyCommand.Path; exit $BR_EXIT_PASS }

$repoRoot = Get-BRRepoRoot -ToolsDir $toolsDir
$rung = 'BP01 step 3b input assets (generated)'
Write-BRBanner -Script 'build-input.ps1' -Rung $rung -RepoRoot $repoRoot

$planScript  = Join-Path $genDir 'input_plan.py'
$buildScript = Join-Path $genDir 'build_input.py'
$testScript  = Join-Path $genDir 'selftest_no_editor.py'

function Get-BRPython {
    $py = Get-Command 'python' -ErrorAction SilentlyContinue
    if ($null -ne $py) { return $py.Source }
    $py = Get-Command 'python3' -ErrorAction SilentlyContinue
    if ($null -ne $py) { return $py.Source }
    return ''
}

# =======================================================================================
# -PlanOnly / -SelfTest: no editor at all.
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

    $code = 0
    if ($SelfTest) {
        & $python $testScript
        $code = $LASTEXITCODE
        Write-BRRule
        if ($code -eq 0) {
            Write-Host 'SELFTEST: PASS - the generator''s logic and idempotency hold against a FAKE'
            Write-Host '          editor. This is not an engine claim; no Unreal code ran.'
            exit $BR_EXIT_PASS
        }
        Write-Host 'SELFTEST: FAIL - see the failing case(s) above.'
        exit $BR_EXIT_FAIL
    }

    $planArgs = @('--verbose')
    if ($ProfilePath -ne '') { $planArgs += @('--profile', $ProfilePath) }
    & $python $planScript @planArgs
    $code = $LASTEXITCODE
    Write-BRRule
    if ($code -eq 0) {
        Write-Host 'PLAN: PASS - the profile is buildable. Static (rung 1) check of a JSON file only;'
        Write-Host '      it says nothing about what is currently in Content/Input.'
        exit $BR_EXIT_PASS
    }
    if ($code -eq 3) {
        Write-BRBlocked -Rung $rung -Reasons @('input_plan.py could not read its inputs.')
        exit $BR_EXIT_BLOCKED
    }
    Write-Host 'PLAN: FAIL - the profile violates the contract. Fix input_profile.json.'
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

# ---- the Python Editor Script Plugin must be ENABLED, not merely installed -------------
# Without it, `-run=pythonscript` fails with a message about an unknown commandlet and the
# reader spends an hour on the script instead of on the one line of project config.
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
        '',
        'Add to the "Plugins" array (Breachpoint.uproject is OUTSIDE this packet''s owner_path,',
        'so this is a contract_gap for the ticket, not a fix this generator may make):',
        '    { "Name": "PythonScriptPlugin", "Enabled": true },',
        '    { "Name": "EditorScriptingUtilities", "Enabled": true }',
        '',
        'Tools/blockout (BP07) needs the same two, so this unblocks both generators at once.',
        'Meanwhile:  Tools\gen_input\build-input.ps1 -PlanOnly   and   -SelfTest'
    )
    exit $BR_EXIT_BLOCKED
}

# ---- R21: one editor at a time. Two writers on a binary .uasset is unresolvable. -------
$liveEditors = Get-BRLiveEditorProcesses
if ($liveEditors.Count -gt 0) {
    Write-BRBlocked -Rung $rung -Reasons @(
        'an Unreal editor is already running on this machine:',
        ('  ' + ($liveEditors -join '; ')),
        'This generator rewrites binary .uassets in Content/Input. Close the editor, or wait',
        'for whoever holds it.',
        'While you wait, run:  Tools\gen_input\build-input.ps1 -PlanOnly',
        '                      Tools\gen_input\build-input.ps1 -SelfTest',
        'Neither launches anything.'
    )
    exit $BR_EXIT_BLOCKED
}

# ---- forward the python-side switches -------------------------------------------------
$pyArgs = @()
if ($DryRun)      { $pyArgs += '--dry-run' }
if ($AllowDrift)  { $pyArgs += '--allow-drift' }
if ($NoLockCheck) { $pyArgs += '--no-lock-check' }
if ($ProfilePath -ne '') { $pyArgs += ('--profile "{0}"' -f $ProfilePath) }

$logDir = New-BRLogDir -ToolsDir $toolsDir
$stamp = Get-BRRunStamp
$logPath = Join-Path $logDir ('geninput-{0}.log' -f $stamp)

# build_input.py reads BR_GENINPUT_ARGS because -run=pythonscript arg passing differs
# between launch shapes; the env var is the one that always survives.
$env:BR_GENINPUT_ARGS = ($pyArgs -join ' ')
Write-Host ('script args : {0}' -f $env:BR_GENINPUT_ARGS)
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
foreach ($line in (Select-BRLogLines -LogPath $result.LogPath -Pattern 'BR-geninput' -Max 200)) {
    Write-Host ('  ' + $line)
}

if ($result.TimedOut) {
    Write-Host ('TIMED OUT after {0} minute(s) - the asset state is UNKNOWN. Do not commit it.' -f $TimeoutMinutes)
    Write-Host 'Re-run; the generator is idempotent, so a completed re-run is the cheapest way back'
    Write-Host 'to a known state.'
    exit $BR_EXIT_FAIL
}
if ($result.ExitCode -eq 0) {
    Write-Host ''
    Write-Host 'PASS: the input assets match input_profile.json and UBRInputConfig::IsDataValid'
    Write-Host 'accepted DA_InputConfig. That is the GENERATED rung.'
    Write-Host 'Still owed before anyone says input WORKS: assign DA_InputConfig and IMC_Default on'
    Write-Host 'ABRPlayerController, then a PIE smoke proving the tag reaches the controller stubs'
    Write-Host '(BP01 step 5, owner: verifier). PIE is not multiplayer.'
    exit $BR_EXIT_PASS
}
if ($result.ExitCode -eq 3) {
    Write-BRBlocked -Rung $rung -Reasons @('the generator could not run - see the log above.')
    exit $BR_EXIT_BLOCKED
}
Write-Host 'FAIL: the generator refused, or its own post-write verification rejected the result.'
Write-Host 'Read the BR-geninput lines above: a DRIFT_/CONTRACT_ code means a generated asset had'
Write-Host 'been hand-edited (it has been repaired - re-run for a clean pass); anything else means'
Write-Host 'nothing trustworthy was written. Either way, do not commit Content/Input until a run'
Write-Host 'exits 0.'
exit $BR_EXIT_FAIL
