<#
=========================================================================================
BREACHPOINT - RUNG 4 (networked smoke: dedicated server + 2 clients). Tools/run-gauntlet.ps1

Contract clause implemented (docs/contracts/testing.md):
  "4. Networked smoke - the rung that actually proves multiplayer. Dedicated server + 2
   clients via Gauntlet ... both clients join -> client A performs the packet's core
   action -> assert server state, client A view, and client B view AGREE -> run once more
   under net emulation (e.g. -PktLag=120 -PktLoss=5) ... Single-process PIE is NOT a
   substitute; if Gauntlet isn't wired yet, this rung reports BLOCKED - never quietly
   passed."
     RunUAT RunUnreal -project=<uproject> -platform=Win64 -configuration=Development
       -build=editor -test=<YourGauntletTestName>
  Fill-ins: the scenario is BRGauntlet.SmokeTS2C on map BR_Arena01.

STATUS OF THIS WRAPPER, STATED PLAINLY (31 Jul 2026)
  The command construction below has NEVER BEEN EXECUTED END TO END. It cannot be, yet:
    * the Gauntlet node BRGauntlet.SmokeTS2C does not exist  (BP00 step 3, builder)
    * the map BR_Arena01 does not exist                      (BP07)
  So today this script reaches its preflight, prints exactly which pieces are missing,
  and reports BLOCKED (exit 3). It does not launch UAT, and it never reports green for a
  scenario that did not run. When step 3 lands, the first real run is expected to correct
  the argument list - and that delta belongs in BP00's Log.

  Honesty ladder reminder for whoever reads this script's output: rung 4 is the ONLY rung
  in this repo that says anything about multiplayer. PIE is not multiplayer. A rung-1 or
  rung-2 pass says nothing about replication.

USAGE
  .\Tools\run-gauntlet.ps1                  # baseline pass + emulation pass (the rung-4 run)
  .\Tools\run-gauntlet.ps1 -Help
  .\Tools\run-gauntlet.ps1 -DryRun          # print both UAT command lines, run nothing
  .\Tools\run-gauntlet.ps1 -SkipEmulation   # PARTIAL - not a rung-4 result
  .\Tools\run-gauntlet.ps1 -Test BRGauntlet.SomethingElse -Map BR_Other
  .\Tools\run-gauntlet.ps1 -Force           # attempt the run even with preflight failures

EXIT CODES (shared across the ladder; only 0 is green)
  0 PASS          every requested variant ran and passed
  1 FAIL          a variant ran and failed (or timed out)
  2 INCONCLUSIVE  ran but the result could not be scored, or emulation pass was skipped
  3 BLOCKED       scenario/map/env missing - the rung did not run

PowerShell 5.1. No '&&', no ternaries, no null-coalescing.
=========================================================================================
#>

[CmdletBinding()]
param(
    [string]$Test = 'BRGauntlet.SmokeTS2C',
    [string]$Map = 'BR_Arena01',
    [string]$Platform = 'Win64',
    [string]$Configuration = 'Development',
    [string]$Build = 'editor',
    [int]$Clients = 2,
    [int]$PktLag = 120,
    [int]$PktLoss = 5,
    [int]$TimeoutMinutes = 0,
    [int]$TailLines = 0,
    [switch]$SkipEmulation,
    [switch]$DryRun,
    [switch]$Force,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '_BRLadderCommon.ps1')

$ToolsDir = $PSScriptRoot
$RepoRoot = Get-BRRepoRoot -ToolsDir $ToolsDir

if ($Help) {
    Show-BRScriptHeader -Path $PSCommandPath
    Write-Host ''
    Write-Host 'PARAMETERS'
    Write-Host '  -Test <name>         Gauntlet test node. Default BRGauntlet.SmokeTS2C (testing.md).'
    Write-Host '  -Map <name>          Map the scenario loads. Default BR_Arena01 (BP07).'
    Write-Host '  -Clients <n>         Client count. Default 2 - the contract minimum is server + 2,'
    Write-Host '                       because assertions come in threes (server truth, acting'
    Write-Host '                       client, OBSERVING client). 1 client cannot prove replication.'
    Write-Host '  -PktLag <ms>         Emulation pass latency. Default 120 (testing.md).'
    Write-Host '  -PktLoss <pct>       Emulation pass loss. Default 5 (testing.md).'
    Write-Host '  -SkipEmulation       Run only the baseline pass. Reported PARTIAL, exit 2.'
    Write-Host '  -TimeoutMinutes <n>  Kill a variant after n minutes -> FAIL(timeout).'
    Write-Host '                       Default: GAUNTLET_TIMEOUT_MINUTES from env.local, else 30.'
    Write-Host '  -TailLines <n>       Print only the last n lines of each run log (0 = all).'
    Write-Host '  -DryRun              Print both UAT command lines and exit. Runs nothing.'
    Write-Host '  -Force               Attempt the run even when preflight says the scenario or'
    Write-Host '                       map is missing (expected to fail; useful once for evidence).'
    Write-Host '  -Help                This text.'
    Write-Host ''
    Write-Host 'EXIT CODES  0 PASS | 1 FAIL | 2 INCONCLUSIVE | 3 BLOCKED   (only 0 is green)'
    Write-Host ''
    Write-Host 'TODAY THIS REPORTS BLOCKED: the scenario (BP00 step 3) and the map (BP07) do not'
    Write-Host 'exist. A rung-4 wrapper that exits 0 having launched nothing is how "works for the'
    Write-Host 'host" ships.'
    exit $BR_EXIT_PASS
}

Write-BRBanner -Script 'run-gauntlet.ps1' -Rung 'RUNG 4 - networked smoke (dedicated server + 2 clients)' -RepoRoot $RepoRoot

# ---------------------------------------------------------------------------------------
# Preflight. Collect EVERY missing piece, not just the first - the reader wants the whole
# gap list in one pass, and each gap names the ticket that owns it.
# ---------------------------------------------------------------------------------------
$engine = Resolve-BREngineRoot -ToolsDir $ToolsDir
if (-not $engine.Ok) {
    Write-BRBlocked -Rung 'RUNG 4' -Reasons $engine.Problems
    exit $BR_EXIT_BLOCKED
}
$lfs = @(Test-BRLfsPointers -RepoRoot $RepoRoot)
if ($lfs.Count -gt 0) {
    Write-BRBlocked -Rung 'RUNG 4' -Reasons $lfs
    exit $BR_EXIT_BLOCKED
}
$EngineRoot = $engine.Root
$RunUAT = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$UProject = Get-BRUProject -RepoRoot $RepoRoot

$envMap = Read-BREnvLocal -ToolsDir $ToolsDir
if ($TimeoutMinutes -le 0) {
    $TimeoutMinutes = 30
    if ($null -ne $envMap) {
        if ($envMap.ContainsKey('GAUNTLET_TIMEOUT_MINUTES')) { $TimeoutMinutes = [int]$envMap['GAUNTLET_TIMEOUT_MINUTES'] }
    }
}

Write-Host ("ENGINE_ROOT : {0}" -f $EngineRoot)
Write-Host ("RunUAT      : {0}" -f $RunUAT)
Write-Host ("uproject    : {0}" -f $UProject)
Write-Host ("scenario    : {0}" -f $Test)
Write-Host ("map         : {0}" -f $Map)
Write-Host ("clients     : {0}  (+ 1 dedicated server)" -f $Clients)
Write-Host ("emulation   : -PktLag={0} -PktLoss={1}" -f $PktLag, $PktLoss)

Write-BRRule
Write-Host 'PREFLIGHT (each line is present/ABSENT plus the ticket that owns it)'

$blockers = @()

# 1. UAT
if (Test-Path -LiteralPath $RunUAT) {
    Write-Host ('  [ok]     RunUAT.bat                      {0}' -f $RunUAT)
}
else {
    Write-Host ('  [ABSENT] RunUAT.bat                      {0}' -f $RunUAT)
    $blockers += 'RunUAT.bat missing under ENGINE_ROOT - the engine is not a usable source build.'
}

# 2. Gauntlet framework in the engine
$gauntletDir = Join-Path $EngineRoot 'Engine\Source\Programs\AutomationTool\Gauntlet'
if (Test-Path -LiteralPath $gauntletDir) {
    Write-Host ('  [ok]     Gauntlet framework              {0}' -f $gauntletDir)
}
else {
    Write-Host ('  [ABSENT] Gauntlet framework              {0}' -f $gauntletDir)
    $blockers += 'The engine has no Gauntlet framework - rung 4 cannot exist on this install.'
}

# 3. The scenario node itself
$nodeHits = @()
foreach ($searchRoot in @((Join-Path $RepoRoot 'Build'), (Join-Path $RepoRoot 'Source'), (Join-Path $RepoRoot 'Tools'))) {
    if (Test-Path -LiteralPath $searchRoot) {
        $found = @(Get-ChildItem -LiteralPath $searchRoot -Filter '*.cs' -Recurse -ErrorAction SilentlyContinue |
            Select-String -Pattern ([regex]::Escape(($Test -split '\.')[-1])) -List -ErrorAction SilentlyContinue)
        foreach ($f in $found) { $nodeHits += $f.Path }
    }
}
if ($nodeHits.Count -gt 0) {
    Write-Host ('  [ok]     Gauntlet node {0}' -f $Test)
    foreach ($h in $nodeHits) { Write-Host ('             {0}' -f $h) }
}
else {
    Write-Host ('  [ABSENT] Gauntlet node {0}   (owner: BP00 step 3, builder)' -f $Test)
    $blockers += ("SCENARIO NOT AUTHORED: no C# type named '{0}' exists in this repo. BP00 step 3 owns it (dedicated server + 2 clients join, A damages B, assert in threes, rerun under emulation)." -f $Test)
}

# 4. The map
$mapHits = @()
$contentDir = Join-Path $RepoRoot 'Content'
if (Test-Path -LiteralPath $contentDir) {
    $mapHits = @(Get-ChildItem -LiteralPath $contentDir -Filter ($Map + '.umap') -Recurse -ErrorAction SilentlyContinue)
}
if ($mapHits.Count -gt 0) {
    Write-Host ('  [ok]     Map {0}' -f $mapHits[0].FullName)
}
else {
    Write-Host ('  [ABSENT] Map {0}.umap                (owner: BP07)' -f $Map)
    $blockers += ("MAP ABSENT: {0}.umap is not in Content/. BP07 owns the arena blockout; a blockout box is enough for the smoke, but it has to exist." -f $Map)
}

# 5. Rung 1 output the run needs
$serverExe = Join-Path $RepoRoot ('Binaries\{0}\BreachpointServer.exe' -f $Platform)
if (Test-Path -LiteralPath $serverExe) {
    Write-Host ('  [ok]     Server binary                   {0}' -f $serverExe)
}
else {
    Write-Host ('  [ABSENT] Server binary                   {0}' -f $serverExe)
    $blockers += 'BreachpointServer.exe is not built - run Tools\run-ubt.ps1 (rung 1) first. Rung 4 runs on rung 1''s output.'
}

# 6. R21
$live = @(Get-BRLiveBuildProcesses -EngineRoot $EngineRoot)
if ($live.Count -eq 0) {
    Write-Host '  [ok]     Build lock free (R21)'
}
else {
    Write-Host '  [BUSY]   Build lock HELD (R21)'
    foreach ($o in $live) { Write-Host ('             - {0}' -f $o) }
    $blockers += 'A build is in flight (R21). Binaries are being rewritten under this run; any result would be about somebody else''s build.'
}

# ---------------------------------------------------------------------------------------
# Command construction (both variants), printed whether or not we can run.
# ---------------------------------------------------------------------------------------
function Get-BRGauntletArgs {
    param(
        [Parameter(Mandatory = $true)][string]$UProject,
        [Parameter(Mandatory = $true)][string]$Test,
        [Parameter(Mandatory = $true)][string]$Map,
        [Parameter(Mandatory = $true)][string]$Platform,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$Build,
        [Parameter(Mandatory = $true)][int]$Clients,
        [Parameter(Mandatory = $true)][string]$LogDir,
        [bool]$Emulation = $false,
        [int]$PktLag = 120,
        [int]$PktLoss = 5
    )
    $a = @(
        'RunUnreal',
        ('-project="{0}"' -f $UProject),
        ('-platform={0}' -f $Platform),
        ('-configuration={0}' -f $Configuration),
        ('-build={0}' -f $Build),
        ('-test={0}' -f $Test),
        ('-map={0}' -f $Map),
        '-server',                                  # dedicated server role - PIE is not a substitute
        ('-numclients={0}' -f $Clients),            # 2: acting client + OBSERVING client
        ('-tempdir="{0}"' -f $LogDir),
        '-unattended',
        '-nullrhi',
        '-NoTimeouts=false'
    )
    if ($Emulation) {
        # testing.md: "run once more under net emulation (e.g. -PktLag=120 -PktLoss=5)".
        # Passed through to the spawned processes' command lines by the Gauntlet node.
        $a += ('-clientargs="-PktLag={0} -PktLoss={1}"' -f $PktLag, $PktLoss)
        $a += ('-serverargs="-PktLag={0} -PktLoss={1}"' -f $PktLag, $PktLoss)
    }
    return $a
}

$LogDir = New-BRLogDir -ToolsDir $ToolsDir
$stamp = Get-BRRunStamp
$variants = @()
$variants += @{ Name = 'baseline'; Emulation = $false }
if (-not $SkipEmulation) {
    $variants += @{ Name = 'emulation'; Emulation = $true }
}
else {
    Write-Host ''
    Write-Host '!! -SkipEmulation: the emulation pass is part of the contract''s minimum scenario.'
    Write-Host '!! Whatever this prints is PARTIAL and is not a rung-4 result.'
}

Write-BRRule
Write-Host 'COMMANDS THAT DEFINE THIS RUNG'
foreach ($v in $variants) {
    $vLogDir = Join-Path $LogDir ('gauntlet-{0}-{1}' -f $v.Name, $stamp)
    $a = Get-BRGauntletArgs -UProject $UProject -Test $Test -Map $Map -Platform $Platform `
        -Configuration $Configuration -Build $Build -Clients $Clients -LogDir $vLogDir `
        -Emulation ([bool]$v.Emulation) -PktLag $PktLag -PktLoss $PktLoss
    Write-Host ''
    Write-Host ('[{0}]' -f $v.Name)
    Write-Host ('"{0}" {1}' -f $RunUAT, ($a -join ' '))
}

if ($DryRun) {
    Write-Host ''
    Write-Host 'DRY RUN - nothing executed, no evidence produced. Not a rung-4 result.'
    exit $BR_EXIT_PASS
}

if ($blockers.Count -gt 0 -and -not $Force) {
    $reasons = @()
    $reasons += ('SCENARIO NOT AUTHORED (BP00 step 3), MAP {0} ABSENT (BP07) - see the itemised list below.' -f $Map)
    foreach ($b in $blockers) { $reasons += $b }
    $reasons += ''
    $reasons += 'testing.md: "if Gauntlet isn''t wired yet, this rung reports BLOCKED - never'
    $reasons += 'quietly passed." Nothing was launched, so nothing may be claimed - in'
    $reasons += 'particular NO multiplayer claim of any kind is supported by this run.'
    Write-BRBlocked -Rung 'RUNG 4' -Reasons $reasons
    Write-Host ''
    Write-Host ('RUNG4|test={0}|map={1}|variants=0|verdict=BLOCKED|reason=scenario-not-authored+map-absent' -f $Test, $Map)
    exit $BR_EXIT_BLOCKED
}

# ---------------------------------------------------------------------------------------
# The run (UNPROVEN PATH - see the header. First real execution belongs to BP00 step 3.)
# ---------------------------------------------------------------------------------------
$results = @()
foreach ($v in $variants) {
    $vLogDir = Join-Path $LogDir ('gauntlet-{0}-{1}' -f $v.Name, $stamp)
    New-Item -ItemType Directory -Path $vLogDir -Force | Out-Null
    $logPath = Join-Path $LogDir ('gauntlet-{0}-{1}.log' -f $v.Name, $stamp)
    $a = Get-BRGauntletArgs -UProject $UProject -Test $Test -Map $Map -Platform $Platform `
        -Configuration $Configuration -Build $Build -Clients $Clients -LogDir $vLogDir `
        -Emulation ([bool]$v.Emulation) -PktLag $PktLag -PktLoss $PktLoss

    Write-BRRule ('VARIANT {0}' -f $v.Name)
    $startTime = Get-Date
    Write-Host ('start time (before launch) : {0}' -f (Format-BRTime $startTime))
    Write-Host ('log                        : {0}' -f $logPath)

    $run = Invoke-BRNative -Executable $RunUAT -Arguments $a -LogPath $logPath `
        -WorkingDirectory $RepoRoot -TimeoutMinutes $TimeoutMinutes

    Write-BRLogFile -LogPath $logPath -TailLines $TailLines -Title ('Gauntlet output [{0}]' -f $v.Name)
    Write-Host ''
    Write-Host ('exit code (captured)       : {0}' -f $run.ExitCode)
    Write-Host ('wall duration              : {0}s' -f $run.DurationSeconds)

    $ranSomething = @(Select-BRLogLines -LogPath $logPath -Pattern '(Test Result|Result=|Gauntlet.*(Passed|Failed)|Completed test)' -Max 40)
    foreach ($l in $ranSomething) { Write-Host ('  {0}' -f $l) }

    $verdict = 'FAIL'
    if ($run.TimedOut) { $verdict = 'FAIL' }
    elseif ($run.ExitCode -eq 0 -and $ranSomething.Count -eq 0) { $verdict = 'INCONCLUSIVE' }
    elseif ($run.ExitCode -eq 0) { $verdict = 'PASS' }

    $results += @{ Name = $v.Name; Exit = $run.ExitCode; Verdict = $verdict; Log = $logPath; Start = $startTime; Seconds = $run.DurationSeconds }
}

Write-BRRule 'RUNG 4 SUMMARY (paste into the ticket Log)'
$fails = 0
$inconc = 0
foreach ($r in $results) {
    Write-Host ('RUNG4|test={0}|map={1}|variant={2}|exit={3}|start={4}|seconds={5}|verdict={6}' -f `
            $Test, $Map, $r.Name, $r.Exit, (Format-BRTime $r.Start), $r.Seconds, $r.Verdict)
    if ($r.Verdict -eq 'FAIL') { $fails = $fails + 1 }
    if ($r.Verdict -eq 'INCONCLUSIVE') { $inconc = $inconc + 1 }
}
Write-Host ''
Write-Host 'A rung-4 PASS means: server truth, acting client and OBSERVING client agreed,'
Write-Host 'baseline AND under emulation. Anything less is not a multiplayer claim.'

$exit = $BR_EXIT_PASS
if ($fails -gt 0) { $exit = $BR_EXIT_FAIL }
elseif ($inconc -gt 0) { $exit = $BR_EXIT_INCONCLUSIVE }
elseif ($SkipEmulation) { $exit = $BR_EXIT_INCONCLUSIVE }
Write-Host ''
Write-Host ('OVERALL RUNG 4 exit {0}' -f $exit)
exit $exit
