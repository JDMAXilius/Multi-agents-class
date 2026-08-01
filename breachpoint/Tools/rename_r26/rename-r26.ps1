<#
=======================================================================================
BREACHPOINT - Tools/rename_r26/rename-r26.ps1

Serves:   R26 condition 5. The five founder Blueprints were authored as GM_BR / GS_BR /
          PC_BR / PS_BR / BP_BRcharacter; the ruling specifies BP_<CppClassWithoutPrefix>.
          audit_r26.py flags all five as written, so the mismatch cannot just sit there.

Why a script, not `git mv`: a .uasset stores its package name INSIDE the file. Renaming
          the file on disk leaves that name stale and the editor refuses to load the
          asset. Only `EditorAssetLibrary.rename_asset` rewrites the package AND fixes up
          every referencing asset. This is Tier 3 of BREACHPOINT-AUTHORING-MATRIX.md:
          a binary output from a committed, reviewable generator.

Atomicity: the python half repoints Config/DefaultEngine.ini's GlobalDefaultGameMode
          ONLY after every rename succeeded. The repo is therefore never inconsistent -
          all-old (PIE works) or all-new (PIE works), never an ini naming a missing asset.
          Idempotent: an already-correct name reports ALREADY, so re-running after a
          partial failure is safe.

Contract: docs/contracts/data-and-assets.md - one owner per binary, lock before editing.
Skill:    .claude/skills/ue-editor - headless `-run=pythonscript`, generated-script doctrine.

  -PlanOnly     print the five renames and the ini edit WITHOUT an editor. Plain text; use
                it while someone else holds the project.
  -NoLockCheck  skip the git-lfs lock check (recorded in the report; justify it in the Log).

Exit codes (ladder convention): 0 PASS · 1 FAIL · 2 INCONCLUSIVE · 3 BLOCKED.
A 0 means the five assets carry their R26 names, referencing assets were fixed up, and the
ini agrees. It does NOT mean PIE runs - that is BP01 step 5, owner: verifier.

PowerShell 5.1 only. No '&&', no ternaries, no '??'.
=======================================================================================
#>
[CmdletBinding()]
param(
    [switch]$PlanOnly,
    [switch]$NoLockCheck,
    [int]$TimeoutMinutes = 10,
    [switch]$Help
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$genDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$toolsDir = Split-Path -Parent $genDir
. (Join-Path $toolsDir '_BRLadderCommon.ps1')

if ($Help) { Show-BRScriptHeader -Path $MyInvocation.MyCommand.Path; exit $BR_EXIT_PASS }

$repoRoot = Get-BRRepoRoot -ToolsDir $toolsDir
Write-BRBanner -Script 'rename-r26.ps1' -Rung 'R26 condition 5 - founder BP naming' -RepoRoot $repoRoot

# The five renames, mirrored from rename_r26.py so -PlanOnly needs no editor.
$plan = @(
    @{ From = '/Game/Core/GM_BR';               To = '/Game/Core/BP_BRGameMode' },
    @{ From = '/Game/Core/GS_BR';               To = '/Game/Core/BP_BRGameState' },
    @{ From = '/Game/Core/PC_BR';               To = '/Game/Core/BP_BRPlayerController' },
    @{ From = '/Game/Core/PS_BR';               To = '/Game/Core/BP_BRPlayerState' },
    @{ From = '/Game/Characters/BP_BRcharacter'; To = '/Game/Characters/BP_BRCharacter' }
)
$lfsFiles = @(
    'Content/Core/GM_BR.uasset',
    'Content/Core/GS_BR.uasset',
    'Content/Core/PC_BR.uasset',
    'Content/Core/PS_BR.uasset',
    'Content/Characters/BP_BRcharacter.uasset'
)

Write-Host ''
Write-Host 'PLAN - five asset renames (R26 condition 5):'
foreach ($p in $plan) { Write-Host ("  {0,-38} -> {1}" -f $p.From, $p.To) }
Write-Host ''
Write-Host 'PLAN - one config edit, applied ONLY if all five renames succeed:'
Write-Host '  Config/DefaultEngine.ini  GlobalDefaultGameMode'
Write-Host '    /Game/Core/GM_BR.GM_BR_C  ->  /Game/Core/BP_BRGameMode.BP_BRGameMode_C'
Write-Host ''
Write-Host 'NOTE - the case-only rename (BP_BRcharacter -> BP_BRCharacter) goes via a'
Write-Host '       temporary name; a case-insensitive filesystem rejects the direct form.'
Write-Host ''

if ($PlanOnly) {
    Write-Host 'PlanOnly: no editor was launched, nothing was changed.'
    exit $BR_EXIT_PASS
}

# R21 - refuse to launch while any editor holds the project. Two editors on one binary is
# how an asset gets two writers, which is the law-7 failure this whole tool exists inside.
$live = Get-BRLiveEditorProcesses
if ($live -and $live.Count -gt 0) {
    Write-Host ''
    Write-Host 'BLOCKED (R21): an editor process is live. Close it and re-run -'
    foreach ($p in $live) { Write-Host ("  PID {0}  {1}" -f $p.Id, $p.ProcessName) }
    exit $BR_EXIT_BLOCKED
}

# Law 7 - one owner per binary. Lock before editing; these five are LFS-tracked.
if (-not $NoLockCheck) {
    Write-Host 'Checking git-lfs locks on the five assets (law 7)...'
    $lockOut = & git lfs locks 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'INCONCLUSIVE: `git lfs locks` failed. Re-run with -NoLockCheck and'
        Write-Host '  justify it in the ticket Log, or fix the LFS remote first.'
        Write-Host $lockOut
        exit $BR_EXIT_INCONCLUSIVE
    }
    foreach ($f in $lfsFiles) {
        $held = $lockOut | Select-String -SimpleMatch $f
        if ($held) { Write-Host ("  locked: {0}" -f $f) }
        else       { Write-Host ("  NOT locked: {0}  (run: git lfs lock {0})" -f $f) }
    }
    Write-Host ''
}

$engineRoot = Resolve-BREngineRoot -RepoRoot $repoRoot
$editorCmd  = Get-BREditorCmdPath -EngineRoot $engineRoot
$uproject   = Get-BRUProject -RepoRoot $repoRoot
$pyScript   = Join-Path $genDir 'rename_r26.py'
$logDir     = New-BRLogDir -RepoRoot $repoRoot
$logFile    = Join-Path $logDir ('rename-r26-' + (Get-BRRunStamp) + '.log')

Write-Host ("Running: {0}" -f $pyScript)
$args = @($uproject, '-run=pythonscript', ('-script="' + $pyScript + '"'),
          '-unattended', '-nopause', '-nosplash', '-stdout')
$res = Invoke-BRNative -FilePath $editorCmd -Arguments $args -TimeoutMinutes $TimeoutMinutes
Write-BRLogFile -Path $logFile -Content $res.Output

Select-BRLogLines -Content $res.Output -Pattern '\[rename_r26\]'

if ($res.ExitCode -ne 0) {
    Write-Host ''
    Write-Host ("FAIL - editor exited {0}. Full log: {1}" -f $res.ExitCode, $logFile)
    Write-Host 'The ini is only rewritten after all five renames succeed, so the project'
    Write-Host 'is still self-consistent. This script is idempotent: fix and re-run.'
    exit $BR_EXIT_FAIL
}

Write-Host ''
Write-Host ("PASS - five assets renamed, ini repointed. Log: {0}" -f $logFile)
Write-Host 'NEXT:'
Write-Host '  1. git lfs unlock the five paths (they now live under their new names)'
Write-Host '  2. Tools/audit_blueprints/audit-blueprints.ps1 - condition 5 should be clean'
Write-Host '  3. PIE smoke: the game mode still resolves (BP01 step 5, owner: verifier)'
exit $BR_EXIT_PASS
