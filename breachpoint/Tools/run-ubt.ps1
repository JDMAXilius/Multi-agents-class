<#
=========================================================================================
BREACHPOINT - RUNG 1 (clean compile). Tools/run-ubt.ps1

Contract clause implemented (docs/contracts/testing.md):
  "1. Clean compile. UBT for the packet's targets (Editor + Game + Server) ...
   <UE>/Engine/Build/BatchFiles/RunUBT.(bat|sh) <Project>Editor Win64 Development -project=<uproject>"
  Fill-ins: "wrappers: Tools/run-ubt.ps1 ... Targets: BreachpointEditor, Breachpoint,
   BreachpointServer - all three compile on every rung-1 run."

Rulings this script exists to make STRUCTURAL rather than optional
(docs/DESIGN-RULINGS.md, Verification policy, 31 Jul 2026):

  R19  A build claim requires a timestamp proof. For EVERY target this script prints,
       in this order: (1) wall-clock start BEFORE the command launches, (2) the verbatim
       output including Result:/Total execution time:, (3) the exit code captured
       explicitly, (4) the produced binary's path and mtime, (5) an explicit assertion
       that mtime > start. Item 5 is the load-bearing one: a pre-existing artifact
       cannot satisfy it. When the artifact is older than the start time this script
       says STALE ARTIFACT and FAILS, loudly.
       Origin: a verifier reported PASS on three targets having built one, by reading a
       binary's size. That report was possible because the check was a directory
       listing. Here it is not a directory listing.

  R20  -Rebuild is opt-in via a switch and is NEVER the default, because UBT deletes a
       target's binaries before recompiling and an interrupted -Rebuild leaves the tree
       strictly worse than it found it. When UBT reports the target up to date with zero
       actions, this script reports INCONCLUSIVE for from-scratch - never PASS - because
       an up-to-date build proves nothing at all about compilation.

  R21  UE's build lock is global and one build agent runs at a time. This script REFUSES
       to start when another cl.exe/link.exe/UnrealBuildTool is alive, or when Build.bat's
       lock file is held, and says so. A second invocation does not queue politely.

USAGE
  .\Tools\run-ubt.ps1                       # all three targets, incremental (the rung-1 run)
  .\Tools\run-ubt.ps1 -Help
  .\Tools\run-ubt.ps1 -DryRun               # print the exact commands, run nothing
  .\Tools\run-ubt.ps1 -Targets Breachpoint  # PARTIAL - explicitly not a rung-1 result
  .\Tools\run-ubt.ps1 -NoUnity              # force a REAL recompile without deleting
                                            # anything (the non-destructive way out of
                                            # an "up to date" INCONCLUSIVE)
  .\Tools\run-ubt.ps1 -Rebuild              # R20: destructive, opt-in, warned about
  .\Tools\run-ubt.ps1 -TailLines 80         # trim the printed output (log keeps it all)

EXIT CODES (shared across the ladder; only 0 is green)
  0 PASS          every requested target compiled AND its binary is newer than its start
  1 FAIL          a target failed to compile, or produced a stale/missing artifact
  2 INCONCLUSIVE  nothing failed, but at least one target was already up to date and so
                  proved nothing (R20). Not green.
  3 BLOCKED       could not run: env.local/ENGINE_ROOT problem, or build lock held (R21)

PowerShell 5.1. No '&&', no ternaries, no null-coalescing.
=========================================================================================
#>

[CmdletBinding()]
param(
    [string[]]$Targets = @('BreachpointEditor', 'Breachpoint', 'BreachpointServer'),
    [string]$Platform = 'Win64',
    [string]$Configuration = 'Development',
    [switch]$Rebuild,
    [switch]$NoUnity,
    [switch]$DryRun,
    [int]$TailLines = 0,
    [int]$TimeoutMinutes = 0,
    [int]$LockWaitSeconds = 120,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '_BRLadderCommon.ps1')

$ToolsDir = $PSScriptRoot
$RepoRoot = Get-BRRepoRoot -ToolsDir $ToolsDir
$AllTargets = @('BreachpointEditor', 'Breachpoint', 'BreachpointServer')

if ($Help) {
    Show-BRScriptHeader -Path $PSCommandPath
    Write-Host ''
    Write-Host 'RUNG 1 - clean compile (testing.md). All three targets on every rung-1 run:'
    Write-Host ('  {0}' -f ($AllTargets -join ', '))
    Write-Host ''
    Write-Host 'PARAMETERS'
    Write-Host '  -Targets <names>     Subset to build. Fewer than three is reported as PARTIAL'
    Write-Host '                       and is explicitly NOT a rung-1 result.'
    Write-Host '  -Platform <name>     Default Win64.'
    Write-Host '  -Configuration <cfg> Default Development.'
    Write-Host '  -Rebuild             R20: opt-in only. UBT DELETES the binaries first; an'
    Write-Host '                       interrupted -Rebuild leaves you with no working binary.'
    Write-Host '  -NoUnity             Pass -DisableUnity: recompiles every project .cpp'
    Write-Host '                       individually this run WITHOUT deleting anything first.'
    Write-Host '                       Use this when the tree is up to date and you need a'
    Write-Host '                       real compile to point at - it is the non-destructive'
    Write-Host '                       way out of an INCONCLUSIVE result, and it catches the'
    Write-Host '                       missing includes a unity build hides.'
    Write-Host '  -DryRun              Print the exact UBT command lines and exit 0. Runs nothing.'
    Write-Host '  -TailLines <n>       Print only the last n lines of each build log (0 = all).'
    Write-Host '  -TimeoutMinutes <n>  Kill a target build after n minutes (0 = no timeout).'
    Write-Host '  -LockWaitSeconds <n> How long to wait for UBT''s global mutex before each'
    Write-Host '                       target (default 120). The mutex outlives the process'
    Write-Host '                       that held it; a target that never gets it is BLOCKED,'
    Write-Host '                       not FAIL - it never compiled anything.'
    Write-Host '  -Help                This text.'
    Write-Host ''
    Write-Host 'EXIT CODES  0 PASS | 1 FAIL | 2 INCONCLUSIVE | 3 BLOCKED   (only 0 is green)'
    Write-Host ''
    Write-Host 'EVIDENCE PRODUCED PER TARGET (R19 - all five items, every run):'
    Write-Host '  start time (printed before launch) | full output | exit code | binary mtime |'
    Write-Host '  an explicit "mtime > start" PASS/FAIL assertion'
    exit $BR_EXIT_PASS
}

Write-BRBanner -Script 'run-ubt.ps1' -Rung 'RUNG 1 - clean compile (all three targets)' -RepoRoot $RepoRoot

# ---------------------------------------------------------------------------------------
# Preflight: environment
# ---------------------------------------------------------------------------------------
$engine = Resolve-BREngineRoot -ToolsDir $ToolsDir
if (-not $engine.Ok) {
    Write-BRBlocked -Rung 'RUNG 1' -Reasons $engine.Problems
    exit $BR_EXIT_BLOCKED
}
$lfs = @(Test-BRLfsPointers -RepoRoot $RepoRoot)
if ($lfs.Count -gt 0) {
    Write-BRBlocked -Rung 'RUNG 1' -Reasons $lfs
    exit $BR_EXIT_BLOCKED
}
$EngineRoot = $engine.Root
$RunUBT = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUBT.bat'
$UProject = Get-BRUProject -RepoRoot $RepoRoot

Write-Host ("ENGINE_ROOT : {0}" -f $EngineRoot)
Write-Host ("uproject    : {0}" -f $UProject)
Write-Host ("UBT wrapper : {0}" -f $RunUBT)

if (-not (Test-Path -LiteralPath $UProject)) {
    Write-BRBlocked -Rung 'RUNG 1' -Reasons @(("Breachpoint.uproject not found at '{0}' - nothing to build." -f $UProject))
    exit $BR_EXIT_BLOCKED
}
if (-not $engine.IsSourceBuild) {
    Write-Host ''
    Write-Host '!! WARNING: Engine\Build\SourceDistribution.txt is absent under ENGINE_ROOT.'
    Write-Host '!! That marks this as a LAUNCHER install, which ships no server binaries.'
    Write-Host '!! The BreachpointServer target is expected to FAIL here. See Tools/env.local.example.'
}

# Every named target must actually exist as a *.Target.cs, or the run is a lie about scope.
$missingTargets = @()
foreach ($t in $Targets) {
    $tcs = Join-Path $RepoRoot ("Source\{0}.Target.cs" -f $t)
    if (-not (Test-Path -LiteralPath $tcs)) { $missingTargets += ("{0} (no Source\{0}.Target.cs)" -f $t) }
}
if ($missingTargets.Count -gt 0) {
    Write-BRBlocked -Rung 'RUNG 1' -Reasons $missingTargets
    exit $BR_EXIT_BLOCKED
}

$isFullRungOne = $true
foreach ($t in $AllTargets) {
    if ($Targets -notcontains $t) { $isFullRungOne = $false }
}
if (-not $isFullRungOne) {
    Write-Host ''
    Write-Host '!! PARTIAL RUN: testing.md requires all three targets on every rung-1 run.'
    Write-Host ('!! This invocation covers only: {0}' -f ($Targets -join ', '))
    Write-Host '!! Whatever this prints, it is NOT a rung-1 result. Report it as PARTIAL.'
}

# ---------------------------------------------------------------------------------------
# Preflight: R21 - one build at a time. Refuse, do not queue.
# ---------------------------------------------------------------------------------------
Write-Host ''
Write-Host 'R21 preflight - checking for a build already in flight...'
$live = @(Get-BRLiveBuildProcesses -EngineRoot $EngineRoot)
if ($live.Count -gt 0) {
    $reasons = @()
    $reasons += 'Another build is already running. UE build state is GLOBAL - a second UBT does'
    $reasons += 'not queue politely; it corrupts intermediates or blocks on Build.bat''s mutex,'
    $reasons += 'and whatever it reports about your targets is not about YOUR build (R21).'
    $reasons += 'Live now:'
    foreach ($o in $live) { $reasons += ("  - {0}" -f $o) }
    $reasons += 'Wait for it to finish, or kill that process TREE deliberately, then re-run.'
    $reasons += 'Note: stopping the agent that started a build does NOT stop the build.'
    if ($DryRun) {
        # A dry run launches nothing, so it cannot contend for the lock. Report the
        # state and carry on printing the commands - being unable to READ the plan while
        # somebody else compiles would just teach people to bypass the guard.
        Write-Host '!! NOTE: a build is in flight right now (listed below). This dry run launches'
        Write-Host '!! nothing, so it proceeds - but a real run would be BLOCKED here.'
        foreach ($o in $live) { Write-Host ("!!   - {0}" -f $o) }
    }
    else {
        Write-BRBlocked -Rung 'RUNG 1' -Reasons $reasons
        exit $BR_EXIT_BLOCKED
    }
}
Write-Host '  build lock: FREE (no cl.exe / link.exe / UnrealBuildTool; Build.bat lock unheld)'

$liveEditors = @(Get-BRLiveEditorProcesses)
if ($liveEditors.Count -gt 0) {
    Write-Host ''
    Write-Host '!! WARNING: an Unreal editor is running:'
    foreach ($e in $liveEditors) { Write-Host ("!!   - {0}" -f $e) }
    Write-Host '!! It holds UnrealEditor-Breachpoint.dll open; the editor target link will likely'
    Write-Host '!! fail, and any Live Coding patch it applied makes every binary here suspect.'
}
$patches = @(Get-BRLiveCodingArtifacts -RepoRoot $RepoRoot)
if ($patches.Count -gt 0) {
    Write-Host ''
    Write-Host ('!! WARNING: {0} Live Coding patch DLL(s) present in Binaries\Win64:' -f $patches.Count)
    foreach ($p in $patches) { Write-Host ("!!   - {0}" -f $p) }
    Write-Host '!! testing.md: "Live-coding/hot-reload state is discarded first - stale patching lies."'
}

if ($Rebuild) {
    Write-Host ''
    Write-Host '################################################################################'
    Write-Host '# -Rebuild REQUESTED. R20 WARNING, read it before you walk away:'
    Write-Host '#   UBT DELETES this target''s binaries BEFORE it recompiles. If this run is'
    Write-Host '#   interrupted - Ctrl-C, agent stopped, machine sleeps - you are left with NO'
    Write-Host '#   working binary at all, strictly worse than before you started. BP01 lost its'
    Write-Host '#   game executable exactly this way while the report read PASS.'
    Write-Host '#   Prefer an incremental build that demonstrably completes (this script proves'
    Write-Host '#   completion by timestamp) over a -Rebuild that may not.'
    Write-Host '################################################################################'
}

# ---------------------------------------------------------------------------------------
# Artifact map: what each target is supposed to have produced.
# ---------------------------------------------------------------------------------------
function Get-BRTargetArtifact {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$Target,
        [Parameter(Mandatory = $true)][string]$Platform,
        [Parameter(Mandatory = $true)][string]$Configuration
    )
    $binDir = Join-Path $RepoRoot ("Binaries\{0}" -f $Platform)
    $suffix = ''
    if ($Configuration -ne 'Development') { $suffix = ("-{0}-{1}" -f $Platform, $Configuration) }
    if ($Target -eq 'BreachpointEditor') {
        # Modular editor target: the project's own module DLL is the artifact that proves
        # THIS project's code compiled (UnrealEditor.exe belongs to the engine).
        return (Join-Path $binDir ("UnrealEditor-Breachpoint{0}.dll" -f $suffix))
    }
    if ($Target -eq 'Breachpoint') { return (Join-Path $binDir ("Breachpoint{0}.exe" -f $suffix)) }
    if ($Target -eq 'BreachpointServer') { return (Join-Path $binDir ("BreachpointServer{0}.exe" -f $suffix)) }
    return ''
}

function Get-BRUbtArguments {
    param(
        [Parameter(Mandatory = $true)][string]$Target,
        [Parameter(Mandatory = $true)][string]$Platform,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$UProject,
        [bool]$DoRebuild,
        [bool]$DoNoUnity = $false
    )
    # -NoHotReloadFromIDE: if an editor happens to be running, UBT would otherwise emit a
    #   *.patch_N.dll hot-reload artifact and leave the real DLL untouched - which would
    #   look like a stale artifact and is not a build. Fail with a clear message instead.
    # NO -WaitMutex on purpose (R21): without it UBT ABORTS when another instance holds the
    #   global mutex, which is the honest outcome. -WaitMutex would silently queue behind
    #   somebody else's build and hand us their timings.
    $a = @($Target, $Platform, $Configuration, ('-project="{0}"' -f $UProject), '-NoHotReloadFromIDE')
    if ($DoNoUnity) {
        # -DisableUnity invalidates the module's compile environment, so every project .cpp
        # is compiled individually THIS run - a genuine recompile - while leaving the
        # existing binaries in place until the new ones link. That is R20's preferred
        # shape: "an incremental build that demonstrably completes" rather than a
        # -Rebuild that deletes first and may not finish. It also catches the missing
        # includes that unity blobs hide.
        $a += '-DisableUnity'
    }
    if ($DoRebuild) { $a += '-Rebuild' }
    return $a
}

# ---------------------------------------------------------------------------------------
# Dry run
# ---------------------------------------------------------------------------------------
if ($DryRun) {
    Write-BRRule 'DRY RUN - these are the exact commands; nothing was executed'
    foreach ($t in $Targets) {
        $a = Get-BRUbtArguments -Target $t -Platform $Platform -Configuration $Configuration -UProject $UProject -DoRebuild ([bool]$Rebuild) -DoNoUnity ([bool]$NoUnity)
        Write-Host ('"{0}" {1}' -f $RunUBT, ($a -join ' '))
        Write-Host ('    artifact asserted newer-than-start: {0}' -f (Get-BRTargetArtifact -RepoRoot $RepoRoot -Target $t -Platform $Platform -Configuration $Configuration))
    }
    Write-Host ''
    Write-Host 'DRY RUN produced no evidence and is not a rung-1 result.'
    exit $BR_EXIT_PASS
}

# ---------------------------------------------------------------------------------------
# The run
# ---------------------------------------------------------------------------------------
$LogDir = New-BRLogDir -ToolsDir $ToolsDir
$stamp = Get-BRRunStamp
$results = @()

foreach ($target in $Targets) {

    $artifact = Get-BRTargetArtifact -RepoRoot $RepoRoot -Target $target -Platform $Platform -Configuration $Configuration
    $ubtArgs = Get-BRUbtArguments -Target $target -Platform $Platform -Configuration $Configuration -UProject $UProject -DoRebuild ([bool]$Rebuild) -DoNoUnity ([bool]$NoUnity)
    $logPath = Join-Path $LogDir ("ubt-{0}-{1}-{2}-{3}.log" -f $target, $Platform, $Configuration, $stamp)

    Write-BRRule ("TARGET {0} | {1} | {2}" -f $target, $Platform, $Configuration)

    # --- R21 per-target gate: the mutex outlives the process that held it ---------------
    # Checked before EVERY target, not once per run. See Wait-BRBuildLockFree's comment:
    # this wrapper's own second run lost a target to a mutex held by the previous
    # target's already-exited UBT.
    $lockFree = Wait-BRBuildLockFree -EngineRoot $EngineRoot -TimeoutSeconds $LockWaitSeconds
    if (-not $lockFree) {
        Write-Host ("R21 GATE: build lock still held after {0}s - refusing to launch {1}." -f $LockWaitSeconds, $target)
        Write-Host '         This target is BLOCKED, not failed: it never compiled anything.'
        $results += @{
            Target = $target; Exit = -1; Start = (Get-Date); Artifact = $artifact
            Exists = (Test-Path -LiteralPath $artifact); Mtime = '(not attempted)'; Newer = $false
            UpToDate = $false; Actions = 0; Verdict = 'BLOCKED'
            Reason = 'build lock held by another process (R21) - target never launched'
            Log = '(not launched)'; Duration = 0
        }
        continue
    }

    # --- R19 item 1: wall clock captured and PRINTED BEFORE the build launches ----------
    $startTime = Get-Date
    Write-Host ("R19(1) start time (before launch) : {0}" -f (Format-BRTime $startTime))
    Write-Host ("       artifact under assertion   : {0}" -f $artifact)
    $preExisted = Test-Path -LiteralPath $artifact
    if ($preExisted) {
        $preMtime = (Get-Item -LiteralPath $artifact).LastWriteTime
        Write-Host ("       artifact BEFORE this run   : exists, mtime {0}  <- must be superseded" -f (Format-BRTime $preMtime))
    }
    else {
        Write-Host '       artifact BEFORE this run   : absent'
    }
    Write-Host ("       command                    : `"{0}`" {1}" -f $RunUBT, ($ubtArgs -join ' '))
    Write-Host ("       log                        : {0}" -f $logPath)
    Write-Host ''

    $run = Invoke-BRNative -Executable $RunUBT -Arguments $ubtArgs -LogPath $logPath `
        -WorkingDirectory $RepoRoot -TimeoutMinutes $TimeoutMinutes

    # --- R19 item 2: the verbatim output ------------------------------------------------
    Write-BRLogFile -LogPath $logPath -TailLines $TailLines -Title ("UBT output [{0}]" -f $target)

    # --- R19 item 3: exit code captured explicitly --------------------------------------
    $exitCode = $run.ExitCode
    Write-Host ''
    Write-Host ("R19(3) exit code (captured)        : {0}" -f $exitCode)
    Write-Host ("       wall duration               : {0}s" -f $run.DurationSeconds)
    if ($run.TimedOut) { Write-Host '       TIMED OUT - process tree killed' }

    # --- R19 item 4: the produced binary's path and mtime -------------------------------
    $artifactExists = Test-Path -LiteralPath $artifact
    $mtime = $null
    $mtimeText = '(no artifact)'
    $sizeText = '-'
    if ($artifactExists) {
        $item = Get-Item -LiteralPath $artifact
        $mtime = $item.LastWriteTime
        $mtimeText = Format-BRTime $mtime
        $sizeText = ("{0:N0} bytes" -f $item.Length)
    }
    Write-Host ("R19(4) artifact                    : {0}" -f $artifact)
    Write-Host ("       artifact mtime / size       : {0} / {1}" -f $mtimeText, $sizeText)
    Write-Host '       (size is NOT evidence of anything. It is printed for context only.)'

    # Interesting lines pulled out of the log for the reader.
    $summaryLines = @(Select-BRLogLines -LogPath $logPath -Pattern '^\s*(Result:|Total execution time:|Target is up to date|Building \d+ action|.*executor to run \d+ action|Determining|Nothing to compile)' -Max 20)
    $errorLines = @(Select-BRLogLines -LogPath $logPath -Pattern '(error [A-Z]+[0-9]+|: error |: fatal error |ERROR:)' -Max 60)

    # Did any compile/link action actually EXECUTE? Counting the executor's own
    # "[n/m] Compile/Link ..." lines is evidence, not a message we hope UBT prints - the
    # wording of "Target is up to date" has changed across engine versions, and a check
    # that depends on wording is a check that silently stops working.
    $executed = @(Select-BRLogLines -LogPath $logPath -Pattern '^\[\d+/\d+\]' -Max 400)
    $actionCount = $executed.Count
    $upToDate = ($actionCount -eq 0)
    foreach ($l in $summaryLines) {
        if ($l -match 'Target is up to date') { $upToDate = $true }
        if ($l -match 'Nothing to compile') { $upToDate = $true }
        if ($l -match 'to run (\d+) action') { $actionCount = [int]$Matches[1] }
        if ($l -match 'Building (\d+) action') { $actionCount = [int]$Matches[1] }
    }
    if ($actionCount -eq 0) { $upToDate = $true }
    Write-Host ("       executed build actions      : {0}" -f $actionCount)

    if ($summaryLines.Count -gt 0) {
        Write-Host ''
        Write-Host '       UBT summary lines:'
        foreach ($l in $summaryLines) { Write-Host ("         {0}" -f $l) }
    }
    if ($errorLines.Count -gt 0) {
        Write-Host ''
        Write-Host '       ERROR LINES (verbatim, for the ticket Log):'
        foreach ($l in $errorLines) { Write-Host ("         {0}" -f $l) }
    }

    # --- R19 item 5: the load-bearing assertion -----------------------------------------
    # This is the one thing a pre-existing artifact cannot satisfy. Everything above can
    # be faked by a directory listing; this cannot.
    $newer = $false
    if ($artifactExists) { $newer = ($mtime -gt $startTime) }

    $verdict = 'FAIL'
    $reason = ''
    if (Test-BRConflictingInstance -LogPath $logPath) {
        # R21: UBT never got the global mutex, so it never compiled. That is BLOCKED.
        # Calling it FAIL would be a compile claim about a build that did not happen -
        # the mirror image of calling a stale artifact a PASS.
        $verdict = 'BLOCKED'
        $reason = 'UBT lost the global mutex race (ConflictingInstance) - another build held the lock, so nothing was compiled here. R21: report BLOCKED, never a compile verdict.'
    }
    elseif ($run.TimedOut) {
        $verdict = 'FAIL'
        $reason = ("build exceeded -TimeoutMinutes {0} and was killed" -f $TimeoutMinutes)
    }
    elseif ($exitCode -eq -999) {
        $verdict = 'FAIL'
        $reason = 'the process exit code could not be read. An unreadable exit code is not a zero - it is an unscored run, and it fails until somebody explains it.'
    }
    elseif ($exitCode -ne 0) {
        $verdict = 'FAIL'
        $reason = ("UBT exited {0} - compile/link failed (see ERROR LINES above)" -f $exitCode)
    }
    elseif (-not $artifactExists) {
        $verdict = 'FAIL'
        $reason = 'UBT exited 0 but no artifact exists at the expected path - the target did not produce what rung 1 claims it produces'
    }
    elseif ($newer) {
        $verdict = 'PASS'
        $reason = 'artifact was written during this run'
    }
    elseif ($upToDate) {
        $verdict = 'INCONCLUSIVE'
        $reason = 'UBT reported the target already up to date (zero actions). The artifact predates this run, so this run proves NOTHING about compilation (R20). Not a pass.'
    }
    else {
        $verdict = 'FAIL'
        $reason = 'STALE ARTIFACT - UBT exited 0 and reported work, yet the binary is OLDER than this run''s start time. Something built somewhere else, or nothing built at all. This is exactly the false-PASS shape R19 exists to catch.'
    }

    Write-Host ''
    Write-Host '       ---- R19(5) TIMESTAMP ASSERTION ----'
    Write-Host ("       assert  mtime > start   :  {0} > {1}" -f $mtimeText, (Format-BRTime $startTime))
    if ($artifactExists) {
        $delta = ($mtime - $startTime).TotalSeconds
        if ($newer) {
            Write-Host ("       result                  :  TRUE   (artifact is {0:N1}s NEWER than start - it was built by this run)" -f $delta)
        }
        else {
            Write-Host ("       result                  :  FALSE  (artifact is {0:N1}s OLDER than start - STALE ARTIFACT)" -f ([math]::Abs($delta)))
            Write-Host '       *** A FILE EXISTING IS NOT EVIDENCE THAT YOU BUILT IT (R19). ***'
            Write-Host '       *** This binary predates the command above. Whatever produced it, it'
            Write-Host '       *** was not this run, and it must not be reported as a compile pass.'
        }
    }
    else {
        Write-Host '       result                  :  FALSE  (no artifact to timestamp at all)'
    }
    Write-Host ("       VERDICT {0} : {1}" -f $target, $verdict)
    Write-Host ("       because : {0}" -f $reason)

    $results += @{
        Target    = $target
        Exit      = $exitCode
        Start     = $startTime
        Artifact  = $artifact
        Exists    = $artifactExists
        Mtime     = $mtimeText
        Newer     = $newer
        UpToDate  = $upToDate
        Actions   = $actionCount
        Verdict   = $verdict
        Reason    = $reason
        Log       = $logPath
        Duration  = $run.DurationSeconds
    }

    # Cross-target artifact census. Observed 31 Jul 2026, on this wrapper's own runs:
    # BreachpointServer.exe existed at 23:41:58 and was GONE by 23:59:12, and
    # Breachpoint.exe vanished the same way - neither deleted by anything this script did.
    # Cause, per commit 8f21056: a concurrent MSBuild-driven build in the same tree
    # deleted them. That is R21 in the wild, and it is the point of printing this census:
    # binaries you did not just build can disappear WHILE you are building, so a
    # directory listing taken at the end of a run does not tell you what this run
    # produced. Only the per-target start-time-vs-mtime assertion does (R19).
    Write-Host ''
    Write-Host '       cross-target artifact census (which target binaries exist right now):'
    foreach ($other in $AllTargets) {
        $otherArtifact = Get-BRTargetArtifact -RepoRoot $RepoRoot -Target $other -Platform $Platform -Configuration $Configuration
        if (Test-Path -LiteralPath $otherArtifact) {
            $om = (Get-Item -LiteralPath $otherArtifact).LastWriteTime
            Write-Host ('         {0,-18} present  mtime {1}' -f $other, (Format-BRTime $om))
        }
        else {
            Write-Host ('         {0,-18} ABSENT   <- deleted or never built' -f $other)
        }
    }
}

# ---------------------------------------------------------------------------------------
# Machine-readable summary - designed to be pasted straight into a ticket Log.
# ---------------------------------------------------------------------------------------
$fails = 0
$inconclusive = 0
$blocked = 0
foreach ($r in $results) {
    if ($r.Verdict -eq 'FAIL') { $fails = $fails + 1 }
    if ($r.Verdict -eq 'INCONCLUSIVE') { $inconclusive = $inconclusive + 1 }
    if ($r.Verdict -eq 'BLOCKED') { $blocked = $blocked + 1 }
}

Write-BRRule 'RUNG 1 SUMMARY (paste into the ticket Log)'
Write-Host ("run stamp   : {0}" -f $stamp)
Write-Host ("engine      : {0}" -f $EngineRoot)
Write-Host ("mode        : {0}" -f $(if ($Rebuild) { 'REBUILD (R20 destructive)' } elseif ($NoUnity) { 'incremental + -DisableUnity (every project .cpp recompiled, nothing deleted)' } else { 'incremental' }))
Write-Host ("scope       : {0}" -f $(if ($isFullRungOne) { 'FULL rung 1 - all three targets' } else { 'PARTIAL - NOT a rung-1 result' }))
Write-Host ''
Write-Host ('{0,-18} {1,-5} {2,-24} {3,-24} {4,-6} {5}' -f 'target', 'exit', 'start', 'artifact mtime', 'newer', 'verdict')
Write-Host ('{0,-18} {1,-5} {2,-24} {3,-24} {4,-6} {5}' -f '------', '----', '-----', '--------------', '-----', '-------')
foreach ($r in $results) {
    $newerText = 'NO'
    if ($r.Newer) { $newerText = 'YES' }
    Write-Host ('{0,-18} {1,-5} {2,-24} {3,-24} {4,-6} {5}' -f $r.Target, $r.Exit, (Format-BRTime $r.Start), $r.Mtime, $newerText, $r.Verdict)
}
Write-Host ''
Write-Host 'machine-readable:'
foreach ($r in $results) {
    $newerText = 'NO'
    if ($r.Newer) { $newerText = 'YES' }
    Write-Host ('RUNG1|target={0}|exit={1}|start={2}|artifact={3}|mtime={4}|newer_than_start={5}|actions={6}|uptodate={7}|seconds={8}|verdict={9}' -f `
            $r.Target, $r.Exit, (Format-BRTime $r.Start), $r.Artifact, $r.Mtime, $newerText, $r.Actions, $r.UpToDate, $r.Duration, $r.Verdict)
}
Write-Host ''
foreach ($r in $results) {
    Write-Host ('log[{0}] = {1}' -f $r.Target, $r.Log)
}

$overall = $BR_EXIT_PASS
$overallText = 'PASS'
if ($fails -gt 0) {
    $overall = $BR_EXIT_FAIL
    $overallText = ("FAIL ({0} target(s))" -f $fails)
}
elseif ($blocked -gt 0) {
    $overall = $BR_EXIT_BLOCKED
    $overallText = ("BLOCKED ({0} target(s) never compiled - build lock, R21)" -f $blocked)
}
elseif ($inconclusive -gt 0) {
    $overall = $BR_EXIT_INCONCLUSIVE
    $overallText = ("INCONCLUSIVE ({0} target(s) already up to date - nothing was compiled)" -f $inconclusive)
}
elseif (-not $isFullRungOne) {
    $overall = $BR_EXIT_INCONCLUSIVE
    $overallText = 'INCONCLUSIVE (partial target set - rung 1 requires all three)'
}

Write-Host ''
Write-Host ("OVERALL RUNG 1 : {0}   (exit {1})" -f $overallText, $overall)
if ($overall -eq $BR_EXIT_INCONCLUSIVE) {
    Write-Host 'INCONCLUSIVE is not green. To get a real from-scratch compile you must either'
    Write-Host 'change source (which is what makes an incremental build meaningful) or accept'
    Write-Host 'the R20 risk of -Rebuild. Reporting "up to date" as PASS is the lie R19 was'
    Write-Host 'written to stop.'
}
Write-Host ("finished    : {0}" -f (Format-BRTime (Get-Date)))
exit $overall
