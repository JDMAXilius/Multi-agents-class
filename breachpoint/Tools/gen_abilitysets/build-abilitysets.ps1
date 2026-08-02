<#
=======================================================================================
BREACHPOINT - Tools/gen_abilitysets/build-abilitysets.ps1

Serves:   TICKET BP20 steps B1, B2 (populate DA_AbilitySet_Core and the three weapon sets)
          and B3 (clear GM_BR's four class overrides). B4 and B5 are AUDITED and reported;
          this generator never writes to PC_BR or BP_BRcharacter, because a write is exactly
          the failure mode B4 warns about.
          This is the LANDING MECHANISM for Content/Abilities: those .uassets are derived
          artifacts and nobody hand-places them (CLAUDE.md law 7). The committed, reviewable
          artifact is this script and the JSON receipt - never the .uasset alone.
Contract: docs/contracts/data-and-assets.md (soft asset refs; one owner per binary, lock
          before editing). Acceptance: UBRAbilitySet::IsDataValid, run for real by the
          python half, on every one of the four sets.
Skill:    .claude/skills/ue-editor (headless `-run=pythonscript`, generated-script doctrine).

Why this exists at all: BP20 measured that the ability-set assets are empty, so every verb
except jump is dead. Jump works because ABRPlayerState::GiveNativeAbility grants it from C++.
The fix is asset data, and asset data that anyone can re-derive is a script.

What it does, in order:
  1. resolves the engine from Tools/env.local (never a hardcoded path - same rule as the
     rest of the ladder);
  2. checks that the Python Editor Script Plugin is actually enabled in the .uproject,
     because `-run=pythonscript` against a project without it fails in a way that reads like
     a script bug rather than a project setting;
  3. R21 guard - refuses to launch while ANY editor process is live. A second editor on the
     same project is how a binary asset gets two writers;
  4. runs Tools/gen_abilitysets/build_abilitysets.py under UnrealEditor-Cmd, logging to
     Tools/Logs;
  5. reports the plan digest, which is what makes a re-run checkable.

  -PlanOnly       validate the profile and print the whole plan WITHOUT any editor. Plain
                  CPython. It cross-reads BRGameplayTags.cpp, the ability headers,
                  DT_Weapons.csv and DefaultGame.ini, so it catches a wrong class path or a
                  wrong tag with nothing installed. Use this while someone else holds the
                  project - it is the entire reason the generator is split in two.
  -SelfTest       run the no-editor logic test (fake `unreal`) over build_abilitysets.py.
                  Proves the generator's decisions and its idempotency; proves nothing about
                  the engine.
  -DryRun         run under the editor but write no asset.
  -AllowDrift     repair a hand-edited generated asset without failing the run. Justify it in
                  the ticket Log - the failing exit code exists so an edit is SEEN.
  -NoLockCheck    skip the git-lfs lock check (recorded in the run report; justify it).
  -SkipGameMode   do B1/B2 only and leave GM_BR alone (B3 not done).

Exit codes (ladder convention): 0 PASS · 1 FAIL · 2 INCONCLUSIVE · 3 BLOCKED.
A 0 here means "the four sets were written and read back as the profile describes them, and
UBRAbilitySet::IsDataValid accepted every one". It does NOT mean a key does anything: that is
BP20's PIE run, which must log GRANTED: for every ability with the right tag and GA ACTIVATED:
on melee, grenade and grapple with no ensure. And fire/reload/swap cannot be exercised at all
until something equips a starting weapon - see BP20's "Blocked" section (C++ lane).

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
    [switch]$SkipGameMode,
    # NOT -Profile: $Profile is a PowerShell automatic variable and shadowing it in a script
    # that dot-sources shared helpers is a trap for the next reader.
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
$rung = 'BP20 B1/B2/B3 ability-set assets (generated)'
Write-BRBanner -Script 'build-abilitysets.ps1' -Rung $rung -RepoRoot $repoRoot

$planScript  = Join-Path $genDir 'abilityset_plan.py'
$buildScript = Join-Path $genDir 'build_abilitysets.py'
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
            'no `python` on PATH. These scripts need CPython 3.8+ (stdlib only).',
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
        Write-Host 'PLAN: PASS - the profile is buildable, and every ability class, gameplay tag,'
        Write-Host '      DT_Weapons reference and the DefaultGame.ini StartupAbilitySet pin agree'
        Write-Host '      with it. Static (rung 1) check of text files only; it says nothing about'
        Write-Host '      what is currently in Content/Abilities.'
        exit $BR_EXIT_PASS
    }
    if ($code -eq 3) {
        Write-BRBlocked -Rung $rung -Reasons @('abilityset_plan.py could not read its inputs.')
        exit $BR_EXIT_BLOCKED
    }
    Write-Host 'PLAN: FAIL - the profile violates the contract. Fix abilityset_profile.json.'
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
        'Add to the "Plugins" array (Breachpoint.uproject is OUTSIDE BP20''s owner_path, so this',
        'is a contract_gap for the ticket, not a fix this generator may make):',
        '    { "Name": "PythonScriptPlugin", "Enabled": true },',
        '    { "Name": "EditorScriptingUtilities", "Enabled": true }',
        '',
        'Tools/gen_input (BP01) and Tools/blockout (BP07) need the same two.',
        'Meanwhile:  Tools\gen_abilitysets\build-abilitysets.ps1 -PlanOnly   and   -SelfTest'
    )
    exit $BR_EXIT_BLOCKED
}

# ---- R21: one editor at a time. Two writers on a binary .uasset is unresolvable. -------
# @() around the call on purpose: Get-BRLiveEditorProcesses returns an empty array, which
# PowerShell unrolls to $null on return, and `$null.Count` is a hard error under
# Set-StrictMode 2.0 - i.e. it throws in the NORMAL case, when no editor is running.
# Tools/_BRLadderCommon.ps1 is outside this packet's owner path (law 5), so the fix is here
# rather than in the shared helper; noted in BP20's Log for whoever owns the ladder.
$liveEditors = @(Get-BRLiveEditorProcesses)
if ($liveEditors.Count -gt 0) {
    Write-BRBlocked -Rung $rung -Reasons @(
        'an Unreal editor is already running on this machine:',
        ('  ' + ($liveEditors -join '; ')),
        'This generator rewrites binary .uassets in Content/Abilities and Content/Core. Close',
        'the editor, or wait for whoever holds it.',
        'While you wait, run:  Tools\gen_abilitysets\build-abilitysets.ps1 -PlanOnly',
        '                      Tools\gen_abilitysets\build-abilitysets.ps1 -SelfTest',
        'Neither launches anything.'
    )
    exit $BR_EXIT_BLOCKED
}

# ---- forward the python-side switches -------------------------------------------------
$pyArgs = @()
if ($DryRun)       { $pyArgs += '--dry-run' }
if ($AllowDrift)   { $pyArgs += '--allow-drift' }
if ($NoLockCheck)  { $pyArgs += '--no-lock-check' }
if ($SkipGameMode) { $pyArgs += '--skip-game-mode' }
# Quoted: the python half parses this env var with shlex in POSIX mode, where an unquoted
# backslash is an escape character and a Windows path arrives with its separators eaten.
if ($ProfilePath -ne '') { $pyArgs += ('--profile "{0}"' -f $ProfilePath) }

$logDir = New-BRLogDir -ToolsDir $toolsDir
$stamp = Get-BRRunStamp
$logPath = Join-Path $logDir ('genabilitysets-{0}.log' -f $stamp)

# build_abilitysets.py reads BR_GENABILITYSETS_ARGS because -run=pythonscript arg passing
# differs between launch shapes; the env var is the one that always survives.
$env:BR_GENABILITYSETS_ARGS = ($pyArgs -join ' ')
Write-Host ('script args : {0}' -f $env:BR_GENABILITYSETS_ARGS)
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
foreach ($line in (Select-BRLogLines -LogPath $result.LogPath -Pattern 'BR-genability' -Max 200)) {
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
    Write-Host 'PASS: the four ability sets match abilityset_profile.json and'
    Write-Host 'UBRAbilitySet::IsDataValid accepted every one. That is the GENERATED rung.'
    Write-Host 'Still owed before anyone says the verbs WORK (BP20 "Done when"):'
    Write-Host '  - a PIE run logging GRANTED: for every ability above, at startup, with the right tag;'
    Write-Host '  - melee, grenade and grapple each logging GA ACTIVATED: with no ensure firing;'
    Write-Host '  - the verbatim log pasted into the ticket Log. A claim without it is not a result.'
    Write-Host 'fire/reload/swap CANNOT be exercised yet: nothing calls UBREquipmentComponent::GiveWeapon,'
    Write-Host 'so no weapon is ever equipped (BP20 "Blocked", C++ lane). PIE is not multiplayer.'
    exit $BR_EXIT_PASS
}
if ($result.ExitCode -eq 3) {
    Write-BRBlocked -Rung $rung -Reasons @('the generator could not run - see the log above.')
    exit $BR_EXIT_BLOCKED
}
Write-Host 'FAIL: the generator refused, or its own post-write verification rejected the result.'
Write-Host 'Read the BR-genability lines above. DRIFT_SET_ROWS means a generated asset had been'
Write-Host 'hand-edited (it has been repaired - re-run for a clean pass). EMPTY_ON_DISK and'
Write-Host 'INCOMPLETE_ON_DISK are NOT failures - they are BP20''s starting state. Anything else'
Write-Host 'means nothing trustworthy was written. Either way, do not commit Content/Abilities'
Write-Host 'until a run exits 0.'
exit $BR_EXIT_FAIL
