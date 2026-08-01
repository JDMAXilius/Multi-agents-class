<#
=========================================================================================
BREACHPOINT - table reimport (feeds rungs 2 and 4). Tools/reimport-tables.ps1

Contract clause implemented (docs/contracts/data-and-assets.md):
  "Table source CSVs live at Content/Data/*.csv; reimport is scripted via commandlet
   (Tools/reimport-tables.ps1), never manual. Two kinds, and the DT_/CT_ prefix is the
   tell - they do not import the same way:
     DataTables (DT_)  - each needs a USTRUCT row type, ALL of them in the one header
                         Source/Breachpoint/Data/BRDataRows.h
     CurveTables (CT_) - named curves, NO row struct; do not author an FBRCombatRow and
                         do not add it to BRDataRows.h.
   Getting this backwards fails at first import, not at review."
Also CLAUDE.md law 3 (numbers are rows) and law 7 / R18 (a generated asset is produced by
a committed script, never hand-placed - this is that script).

WHAT IT DOES
  For every table it knows about, it reports PRESENT/ABSENT for each precondition
  (CSV, row struct for DT_ only, destination asset), then drives
    UnrealEditor-Cmd <uproject> -run=ImportAssets -importsettings=<generated json>
  for the tables whose preconditions hold. Tables that are not ready are reported
  BLOCKED with the ticket that owns them; they are never silently skipped.

STATUS, STATED PLAINLY (31 Jul 2026)
  The commandlet invocation has NEVER SUCCEEDED END TO END and is marked UNPROVEN in the
  output, because nothing importable exists yet:
    * Content/Data/DT_Weapons.csv EXISTS (BP13 landed it) but its row struct
      Source/Breachpoint/Data/BRDataRows.h does NOT - that header is BP02's.
    * Content/Data/CT_Combat.csv does not exist at all (BP02 step 4).
  A DataTable import without a row struct is rejected by the importer, so this script
  refuses rather than producing a broken asset. The generated import JSON is written to
  Tools/Logs/ either way, so the next reader can see exactly what would run.

USAGE
  .\Tools\reimport-tables.ps1                 # report + import whatever is ready
  .\Tools\reimport-tables.ps1 -Help
  .\Tools\reimport-tables.ps1 -DryRun         # write the JSON, print the command, run nothing
  .\Tools\reimport-tables.ps1 -Tables DT_Weapons

EXIT CODES (shared across the ladder; only 0 is green)
  0 PASS          every requested table was imported
  1 FAIL          the commandlet ran and failed
  2 INCONCLUSIVE  ran but the result could not be scored
  3 BLOCKED       nothing was importable (missing CSV / row struct / env)

PowerShell 5.1. No '&&', no ternaries, no null-coalescing.
=========================================================================================
#>

[CmdletBinding()]
param(
    [string[]]$Tables = @('DT_Weapons', 'CT_Combat'),
    [int]$TimeoutMinutes = 20,
    [int]$TailLines = 0,
    [switch]$DryRun,
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
    Write-Host '  -Tables <names>      Subset to reimport. Default: DT_Weapons, CT_Combat.'
    Write-Host '  -TimeoutMinutes <n>  Kill the commandlet after n minutes. Default 20.'
    Write-Host '  -TailLines <n>       Print only the last n lines of the log (0 = all).'
    Write-Host '  -DryRun              Write the import JSON, print the command, run nothing.'
    Write-Host '  -Help                This text.'
    Write-Host ''
    Write-Host 'KNOWN TABLES (kind is decided by the prefix, per data-and-assets.md)'
    Write-Host '  DT_Weapons   DataTable   row struct FBRWeaponRow (BRDataRows.h, BP02)   csv: BP13 landed'
    Write-Host '  CT_Combat    CurveTable  NO row struct - named curves                   csv: BP02 step 4'
    Write-Host ''
    Write-Host 'EXIT CODES  0 PASS | 1 FAIL | 2 INCONCLUSIVE | 3 BLOCKED   (only 0 is green)'
    exit $BR_EXIT_PASS
}

Write-BRBanner -Script 'reimport-tables.ps1' -Rung 'DATA - CSV -> DataTable/CurveTable reimport' -RepoRoot $RepoRoot

# ---------------------------------------------------------------------------------------
# The table catalogue. Kind is derived from the prefix, because that is what the contract
# says the prefix is FOR - and getting it backwards fails at import, not at review.
# ---------------------------------------------------------------------------------------
$catalogue = @{
    'DT_Weapons' = @{
        Kind      = 'DataTable'
        Csv       = 'Content\Data\DT_Weapons.csv'
        RowStruct = 'BRWeaponRow'      # FBRWeaponRow in Source/Breachpoint/Data/BRDataRows.h
        Dest      = '/Game/Data'
        Owner     = 'CSV: BP13 (landed) | row struct FBRWeaponRow: BP02'
    }
    'CT_Combat'  = @{
        Kind      = 'CurveTable'
        Csv       = 'Content\Data\CT_Combat.csv'
        RowStruct = ''                 # CurveTables have none. Authoring one is a finding.
        Dest      = '/Game/Data'
        Owner     = 'BP02 step 4'
    }
}

$engine = Resolve-BREngineRoot -ToolsDir $ToolsDir
if (-not $engine.Ok) {
    Write-BRBlocked -Rung 'TABLE REIMPORT' -Reasons $engine.Problems
    exit $BR_EXIT_BLOCKED
}
$EngineRoot = $engine.Root
$EditorCmd = Get-BREditorCmdPath -EngineRoot $EngineRoot
$UProject = Get-BRUProject -RepoRoot $RepoRoot
$RowHeader = Join-Path $RepoRoot 'Source\Breachpoint\Data\BRDataRows.h'

Write-Host ("ENGINE_ROOT : {0}" -f $EngineRoot)
Write-Host ("editor-cmd  : {0}" -f $EditorCmd)
Write-Host ("row header  : {0}  [{1}]" -f $RowHeader, $(if (Test-Path -LiteralPath $RowHeader) { 'present' } else { 'ABSENT (BP02)' }))

Write-BRRule
Write-Host 'PER-TABLE PRECONDITIONS'
Write-Host ('{0,-12} {1,-11} {2,-9} {3,-12} {4,-8} {5}' -f 'table', 'kind', 'csv', 'row struct', 'asset', 'status')
Write-Host ('{0,-12} {1,-11} {2,-9} {3,-12} {4,-8} {5}' -f '-----', '----', '---', '----------', '-----', '------')

$ready = @()
$blockedTables = @()
$rows = @()

foreach ($name in $Tables) {
    if (-not $catalogue.ContainsKey($name)) {
        Write-Host ('{0,-12} {1}' -f $name, 'UNKNOWN TABLE - not in the catalogue; add it here and in data-and-assets.md first')
        $blockedTables += ("{0}: unknown table - this script will not invent an import for a table no contract names." -f $name)
        continue
    }
    $t = $catalogue[$name]
    $csvPath = Join-Path $RepoRoot $t.Csv
    $csvOk = Test-Path -LiteralPath $csvPath

    $structOk = $true
    $structText = 'n/a (CurveTable)'
    if ($t.Kind -eq 'DataTable') {
        $structText = 'ABSENT'
        $structOk = $false
        if (Test-Path -LiteralPath $RowHeader) {
            $hit = @(Select-String -LiteralPath $RowHeader -Pattern ('struct\s+(BREACHPOINT_API\s+)?F' + $t.RowStruct + '\b') -ErrorAction SilentlyContinue)
            if ($hit.Count -gt 0) { $structOk = $true; $structText = ('F' + $t.RowStruct) }
        }
    }

    $assetPath = Join-Path $RepoRoot ('Content\Data\{0}.uasset' -f $name)
    $assetOk = Test-Path -LiteralPath $assetPath

    $status = 'READY'
    $why = @()
    if (-not $csvOk) { $why += ('CSV absent: {0}' -f $t.Csv) }
    if (-not $structOk) { $why += ('row struct F{0} not found in BRDataRows.h' -f $t.RowStruct) }
    if ($why.Count -gt 0) {
        $status = 'BLOCKED'
        $blockedTables += ("{0} ({1}): {2}  [owner: {3}]" -f $name, $t.Kind, ($why -join '; '), $t.Owner)
    }
    else {
        $ready += $name
    }

    Write-Host ('{0,-12} {1,-11} {2,-9} {3,-12} {4,-8} {5}' -f `
            $name, $t.Kind, $(if ($csvOk) { 'present' } else { 'ABSENT' }), $structText, `
            $(if ($assetOk) { 'present' } else { 'absent' }), $status)
    $rows += ('REIMPORT|table={0}|kind={1}|csv={2}|rowstruct={3}|asset={4}|status={5}' -f `
            $name, $t.Kind, $(if ($csvOk) { 'present' } else { 'absent' }), $structText, `
            $(if ($assetOk) { 'present' } else { 'absent' }), $status)
}

# ---------------------------------------------------------------------------------------
# Generate the import descriptor for whatever IS ready (written even on a dry run, so the
# next reader can see the exact payload).
# ---------------------------------------------------------------------------------------
$LogDir = New-BRLogDir -ToolsDir $ToolsDir
$stamp = Get-BRRunStamp
$jsonPath = Join-Path $LogDir ('reimport-{0}.json' -f $stamp)

$groups = @()
foreach ($name in $ready) {
    $t = $catalogue[$name]
    $csvFull = (Join-Path $RepoRoot $t.Csv).Replace('\', '/')
    $settings = @{}
    if ($t.Kind -eq 'DataTable') {
        $settings['ImportType'] = 'ECSV_DataTable'
        $settings['ImportRowStruct'] = ('/Script/Breachpoint.{0}' -f $t.RowStruct)
    }
    else {
        $settings['ImportType'] = 'ECSV_CurveTable'
        $settings['ImportCurveInterpMode'] = 'RCIM_Linear'
    }
    $groups += @{
        GroupName       = $name
        Filenames       = @($csvFull)
        DestinationPath = $t.Dest
        FactoryName     = 'CSVImportFactory'
        bReplaceExisting = $true
        bSkipReadOnly   = $false
        ImportSettings  = $settings
    }
}
$descriptor = @{ ImportGroups = $groups }
$json = ConvertTo-Json -InputObject $descriptor -Depth 8
Set-Content -LiteralPath $jsonPath -Value $json -Encoding UTF8

$importArgs = @(
    ('"{0}"' -f $UProject),
    '-run=ImportAssets',
    ('-importsettings="{0}"' -f $jsonPath),
    '-replaceexisting',
    '-nosourcecontrol',
    '-unattended',
    '-nullrhi',
    '-nosplash',
    '-stdout'
)

Write-BRRule
Write-Host ('import descriptor written : {0}' -f $jsonPath)
Write-Host 'command that would run:'
Write-Host ('"{0}" {1}' -f $EditorCmd, ($importArgs -join ' '))
Write-Host ''
Write-Host 'UNPROVEN: this commandlet invocation has never completed successfully in this'
Write-Host 'repo (there has never been an importable table). Treat the exact argument list'
Write-Host 'as a first draft until a table actually lands, and record the delta in the Log.'

if ($DryRun) {
    Write-Host ''
    Write-Host 'DRY RUN - nothing executed.'
    foreach ($r in $rows) { Write-Host $r }
    exit $BR_EXIT_PASS
}

if ($ready.Count -eq 0) {
    $reasons = @('NO TABLE IS IMPORTABLE - nothing ran.')
    foreach ($b in $blockedTables) { $reasons += $b }
    $reasons += 'A reimport script that exits 0 having imported nothing is how a stale table'
    $reasons += 'ships. This is BLOCKED, not a pass.'
    Write-BRBlocked -Rung 'TABLE REIMPORT' -Reasons $reasons
    Write-Host ''
    foreach ($r in $rows) { Write-Host $r }
    Write-Host ('REIMPORT|imported=0|verdict=BLOCKED')
    exit $BR_EXIT_BLOCKED
}

$live = @(Get-BRLiveBuildProcesses -EngineRoot $EngineRoot)
if ($live.Count -gt 0) {
    $reasons = @('A build is in flight (R21); the editor cannot be launched over it. Live now:')
    foreach ($o in $live) { $reasons += ("  - {0}" -f $o) }
    Write-BRBlocked -Rung 'TABLE REIMPORT' -Reasons $reasons
    exit $BR_EXIT_BLOCKED
}

$logPath = Join-Path $LogDir ('reimport-{0}.log' -f $stamp)
$startTime = Get-Date
Write-Host ''
Write-Host ('start time (before launch) : {0}' -f (Format-BRTime $startTime))
Write-Host ('tables being imported      : {0}' -f ($ready -join ', '))
Write-Host ('log                        : {0}' -f $logPath)

$run = Invoke-BRNative -Executable $EditorCmd -Arguments $importArgs -LogPath $logPath `
    -WorkingDirectory $RepoRoot -TimeoutMinutes $TimeoutMinutes

Write-BRLogFile -LogPath $logPath -TailLines $TailLines -Title 'ImportAssets output'
Write-Host ''
Write-Host ('exit code (captured)       : {0}' -f $run.ExitCode)

# Timestamp proof applies to generated assets too (R19 in spirit): an asset that predates
# the run was not produced by it.
$verdict = 'FAIL'
$importedFresh = 0
foreach ($name in $ready) {
    $assetPath = Join-Path $RepoRoot ('Content\Data\{0}.uasset' -f $name)
    if (Test-Path -LiteralPath $assetPath) {
        $m = (Get-Item -LiteralPath $assetPath).LastWriteTime
        $fresh = ($m -gt $startTime)
        if ($fresh) { $importedFresh = $importedFresh + 1 }
        Write-Host ('  {0}: asset mtime {1}  newer_than_start={2}' -f $name, (Format-BRTime $m), $(if ($fresh) { 'YES' } else { 'NO - STALE, this run did not write it' }))
    }
    else {
        Write-Host ('  {0}: asset ABSENT after import - the import did not produce it' -f $name)
    }
}

$exit = $BR_EXIT_FAIL
if ($run.TimedOut) {
    $verdict = 'FAIL'
}
elseif ($run.ExitCode -ne 0) {
    $verdict = 'FAIL'
}
elseif ($importedFresh -eq $ready.Count) {
    $verdict = 'PASS'
    $exit = $BR_EXIT_PASS
}
else {
    $verdict = 'INCONCLUSIVE'
    $exit = $BR_EXIT_INCONCLUSIVE
}

Write-BRRule 'TABLE REIMPORT SUMMARY'
foreach ($r in $rows) { Write-Host $r }
foreach ($b in $blockedTables) { Write-Host ('REIMPORT-BLOCKED| {0}' -f $b) }
Write-Host ('REIMPORT|requested={0}|ready={1}|freshly_written={2}|exit={3}|verdict={4}' -f `
        $Tables.Count, $ready.Count, $importedFresh, $run.ExitCode, $verdict)
Write-Host ('log  : {0}' -f $logPath)
Write-Host ('json : {0}' -f $jsonPath)
exit $exit
