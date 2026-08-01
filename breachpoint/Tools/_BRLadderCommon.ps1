# =====================================================================================
# BREACHPOINT - validation ladder, shared helpers (dot-sourced; not run directly)
#
# Serves: every rung. Implements the plumbing the rung wrappers share so the honesty
#         properties are written ONCE and cannot drift between scripts.
# Contract: docs/contracts/testing.md ("The ladder", "Fill-ins - BREACHPOINT").
# Rulings:  R19 (timestamp proof), R20 (-Rebuild is not retryable), R21 (one build at a time).
#
# PowerShell 5.1 ONLY. No '&&', no '||', no ternaries, no '??'. Never '2>&1' on a native
# exe from PowerShell (it wraps stderr in ErrorRecords and flips $? even on exit 0) -
# all native redirection happens inside a generated .cmd shim instead.
# =====================================================================================

Set-StrictMode -Version 2.0

# ---- Ladder exit codes (identical in every wrapper; CI reads these) -------------------
#   0 PASS          the thing ran and the evidence supports the claim
#   1 FAIL          the thing ran and failed (or produced a stale artifact)
#   2 INCONCLUSIVE  the thing ran but proved nothing (e.g. "up to date", zero actions)
#   3 BLOCKED       the thing could not run (missing env, build lock held, nothing authored)
# Only 0 is green. 2 and 3 are NOT green - that is the whole point of this file.
$BR_EXIT_PASS         = 0
$BR_EXIT_FAIL         = 1
$BR_EXIT_INCONCLUSIVE = 2
$BR_EXIT_BLOCKED      = 3

function Show-BRScriptHeader {
    <#
      Prints the leading block-comment header of a wrapper script (the lines between the
      opening and closing comment markers). Every wrapper's -Help uses this, so the rung
      it serves and the contract clause it implements are always one flag away and can
      never drift from the file that implements them.
    #>
    param([Parameter(Mandatory = $true)][string]$Path)
    $lines = @(Get-Content -LiteralPath $Path)
    $inBlock = $false
    foreach ($l in $lines) {
        if (-not $inBlock) {
            if ($l.Trim().StartsWith('<#')) { $inBlock = $true; continue }
            continue
        }
        if ($l.Trim().StartsWith('#>')) { break }
        Write-Host $l
    }
}

function Get-BRRepoRoot {
    # Derive the repo root from the Tools/ dir this file lives in, so every wrapper
    # works from any CWD.
    param([Parameter(Mandatory = $true)][string]$ToolsDir)
    return (Split-Path -Parent $ToolsDir)
}

function Format-BRTime {
    param([Parameter(Mandatory = $true)][datetime]$When)
    return $When.ToString('yyyy-MM-ddTHH:mm:ss.fff')
}

function Write-BRRule {
    param([string]$Text = '')
    if ($Text -eq '') {
        Write-Host ('-' * 86)
    }
    else {
        Write-Host ''
        Write-Host ('=' * 86)
        Write-Host $Text
        Write-Host ('=' * 86)
    }
}

function Write-BRBanner {
    param(
        [Parameter(Mandatory = $true)][string]$Script,
        [Parameter(Mandatory = $true)][string]$Rung,
        [Parameter(Mandatory = $true)][string]$RepoRoot
    )
    Write-BRRule ("BREACHPOINT ladder :: {0} :: {1}" -f $Script, $Rung)
    Write-Host ("repo root   : {0}" -f $RepoRoot)
    Write-Host ("host        : {0}  (PowerShell {1})" -f $env:COMPUTERNAME, $PSVersionTable.PSVersion.ToString())
    Write-Host ("script start: {0}" -f (Format-BRTime (Get-Date)))
}

# ---- env.local -----------------------------------------------------------------------

function Get-BREnvLocalPath {
    param([Parameter(Mandatory = $true)][string]$ToolsDir)
    return (Join-Path $ToolsDir 'env.local')
}

function Read-BREnvLocal {
    <#
      Parses Tools/env.local (KEY=VALUE, '#' comments). Returns a hashtable.
      Returns $null when the file is absent - the caller reports BLOCKED, never guesses
      a path. There is deliberately NO hardcoded engine fallback anywhere in this ladder.
    #>
    param([Parameter(Mandatory = $true)][string]$ToolsDir)

    $path = Get-BREnvLocalPath -ToolsDir $ToolsDir
    if (-not (Test-Path -LiteralPath $path)) { return $null }

    $map = @{}
    foreach ($line in (Get-Content -LiteralPath $path)) {
        $trimmed = $line.Trim()
        if ($trimmed -eq '') { continue }
        if ($trimmed.StartsWith('#')) { continue }
        $idx = $trimmed.IndexOf('=')
        if ($idx -lt 1) { continue }
        $key = $trimmed.Substring(0, $idx).Trim()
        $val = $trimmed.Substring($idx + 1).Trim()
        # strip optional surrounding quotes
        if ($val.Length -ge 2) {
            if (($val.StartsWith('"') -and $val.EndsWith('"')) -or ($val.StartsWith("'") -and $val.EndsWith("'"))) {
                $val = $val.Substring(1, $val.Length - 2)
            }
        }
        $map[$key] = $val
    }
    return $map
}

function Resolve-BREngineRoot {
    <#
      Returns a hashtable: Ok, Root, IsSourceBuild, Problems[].
      Never throws - the caller decides how loudly to report BLOCKED.
    #>
    param([Parameter(Mandatory = $true)][string]$ToolsDir)

    $result = @{ Ok = $false; Root = ''; IsSourceBuild = $false; Problems = @() }
    $envPath = Get-BREnvLocalPath -ToolsDir $ToolsDir
    $envMap = Read-BREnvLocal -ToolsDir $ToolsDir

    if ($null -eq $envMap) {
        $result.Problems += ("Tools/env.local not found at '{0}'." -f $envPath)
        $result.Problems += "Copy Tools/env.local.example to Tools/env.local and set ENGINE_ROOT."
        return $result
    }
    if (-not $envMap.ContainsKey('ENGINE_ROOT')) {
        $result.Problems += ("Tools/env.local exists but declares no ENGINE_ROOT (file: {0})." -f $envPath)
        return $result
    }

    $root = $envMap['ENGINE_ROOT']
    $result.Root = $root

    if ($root -eq '') {
        $result.Problems += 'ENGINE_ROOT is empty in Tools/env.local.'
        return $result
    }
    if (-not (Test-Path -LiteralPath $root)) {
        $result.Problems += ("ENGINE_ROOT does not exist on this machine: '{0}'" -f $root)
        return $result
    }
    foreach ($rel in @('Engine\Build\BatchFiles\RunUBT.bat', 'Engine\Build\BatchFiles\RunUAT.bat')) {
        $probe = Join-Path $root $rel
        if (-not (Test-Path -LiteralPath $probe)) {
            $result.Problems += ("ENGINE_ROOT does not look like a UE root - missing {0}" -f $rel)
        }
    }
    $srcMarker = Join-Path $root 'Engine\Build\SourceDistribution.txt'
    $result.IsSourceBuild = (Test-Path -LiteralPath $srcMarker)

    if ($result.Problems.Count -eq 0) { $result.Ok = $true }
    return $result
}

function Get-BREditorCmdPath {
    param([Parameter(Mandatory = $true)][string]$EngineRoot)
    return (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe')
}

function Get-BRUProject {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)
    return (Join-Path $RepoRoot 'Breachpoint.uproject')
}

# ---- R21: one build agent at a time ---------------------------------------------------

function Get-BRLiveBuildProcesses {
    <#
      R21. Returns an array of human-readable offender strings; empty means the build
      lock is free.

      Deliberately precise about what counts:
        * cl.exe / link.exe / lib.exe          - a compile or link is in flight
        * UnrealBuildTool.exe                  - UBT apphost
        * dotnet.exe hosting UnrealBuildTool.dll - how RunUBT.bat/Build.bat actually run UBT
        * an exclusively-held Build.bat lock file in %TEMP% (Build.bat's own mutex scheme)
      Deliberately NOT counted: idle MSBuild node-reuse workers
      ("dotnet.exe ... MSBuild.dll /nodemode:1 /nodeReuse:true"). Those linger for
      minutes after a build ENDS; treating them as a live build would make this guard
      cry wolf, and a guard that cries wolf gets bypassed.
    #>
    param([string]$EngineRoot = '')

    $offenders = @()

    foreach ($name in @('cl', 'link', 'lib', 'UnrealBuildTool')) {
        $procs = @(Get-Process -Name $name -ErrorAction SilentlyContinue)
        foreach ($p in $procs) {
            $offenders += ("{0}.exe (pid {1})" -f $name, $p.Id)
        }
    }

    $dotnets = @()
    try {
        $dotnets = @(Get-CimInstance -ClassName Win32_Process -Filter "Name='dotnet.exe'" -ErrorAction Stop)
    }
    catch {
        $dotnets = @()
    }
    foreach ($d in $dotnets) {
        $cmdline = ''
        if ($null -ne $d.CommandLine) { $cmdline = $d.CommandLine }
        if ($cmdline -match 'UnrealBuildTool\.dll') {
            $offenders += ("dotnet.exe hosting UnrealBuildTool.dll (pid {0})" -f $d.ProcessId)
        }
    }

    if ($EngineRoot -ne '') {
        $lockFile = Get-BRBuildBatLockPath -EngineRoot $EngineRoot
        if (Test-Path -LiteralPath $lockFile) {
            $held = $false
            try {
                $fs = [System.IO.File]::Open($lockFile, 'Open', 'Write', 'None')
                $fs.Close()
                $fs.Dispose()
            }
            catch {
                $held = $true
            }
            if ($held) {
                $offenders += ("Build.bat lock file is held by another process: {0}" -f $lockFile)
            }
        }
    }

    return $offenders
}

function Get-BRBuildBatLockPath {
    # Mirrors Build.bat's own scheme: %TEMP%\<full path, '\'->'-', ':' removed>.lock
    param([Parameter(Mandatory = $true)][string]$EngineRoot)
    $bat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
    $mangled = $bat.Replace('\', '-').Replace(':', '')
    return (Join-Path $env:TEMP ($mangled + '.lock'))
}

function Wait-BRBuildLockFree {
    <#
      Waits until no build process is live, up to $TimeoutSeconds. Returns $true if the
      lock went free, $false if it was still held when we gave up.

      Why this exists (observed 31 Jul 2026, on this wrapper's own second run): the
      BreachpointServer target died 13 seconds after the previous target finished, with
          "A conflicting instance of Global\UnrealBuildTool_Mutex_... is already running.
           Result: Failed (ConflictingInstance)"
      A concurrent build actor in the same tree held the mutex (see commit 8f21056 - the
      same actor deleted two target binaries). Whether the holder is somebody else's
      build or a straggler of your own does not change the remedy: a one-shot check
      before the FIRST target is not enough, the lock has to be re-checked and waited on
      before EVERY target, and a target that never gets the lock is BLOCKED, not FAIL.
    #>
    param(
        [string]$EngineRoot = '',
        [int]$TimeoutSeconds = 120,
        [int]$PollSeconds = 3
    )
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $announced = $false
    while ($true) {
        $live = @(Get-BRLiveBuildProcesses -EngineRoot $EngineRoot)
        if ($live.Count -eq 0) {
            if ($announced) { Write-Host '  build lock: now FREE' }
            return $true
        }
        if (-not $announced) {
            Write-Host ("  build lock HELD - waiting up to {0}s for it to clear (R21):" -f $TimeoutSeconds)
            foreach ($o in $live) { Write-Host ("    - {0}" -f $o) }
            $announced = $true
        }
        if ((Get-Date) -gt $deadline) { return $false }
        Start-Sleep -Seconds $PollSeconds
    }
}

function Test-BRConflictingInstance {
    # UBT's own words when it loses the mutex race, plus Build.bat's queueing message.
    # A run that never got the lock did not fail to compile - it never compiled, and the
    # honest verdict for that is BLOCKED (R21), not FAIL.
    param([Parameter(Mandatory = $true)][string]$LogPath)
    $hits = @(Select-BRLogLines -LogPath $LogPath -Pattern '(ConflictingInstance|conflicting instance of Global\\UnrealBuildTool_Mutex|is already running, waiting for existing script)' -Max 5)
    return ($hits.Count -gt 0)
}

function Get-BRLiveEditorProcesses {
    # A running editor holds UnrealEditor-<Project>.dll open: the editor target's link
    # step WILL fail, and any Live Coding patch it applied makes every binary suspect
    # (CLAUDE.md law 6 - "never report done on live-coding").
    $out = @()
    foreach ($name in @('UnrealEditor', 'UnrealEditor-Cmd', 'UnrealEditor-Win64-DebugGame')) {
        $procs = @(Get-Process -Name $name -ErrorAction SilentlyContinue)
        foreach ($p in $procs) { $out += ("{0}.exe (pid {1})" -f $name, $p.Id) }
    }
    return $out
}

function Get-BRLiveCodingArtifacts {
    # Live Coding / hot reload leaves patch DLLs next to the real ones. Their presence
    # means some module in memory was patched, not built.
    param([Parameter(Mandatory = $true)][string]$RepoRoot)
    $dir = Join-Path $RepoRoot 'Binaries\Win64'
    if (-not (Test-Path -LiteralPath $dir)) { return @() }
    $items = @(Get-ChildItem -LiteralPath $dir -Filter '*.patch_*.dll' -ErrorAction SilentlyContinue)
    $out = @()
    foreach ($i in $items) { $out += $i.Name }
    return $out
}

# ---- logs ------------------------------------------------------------------------------

function New-BRLogDir {
    <#
      Default Tools\Logs (self-gitignored). Overridable with LOG_DIR in Tools/env.local.

      Why the override matters: UBT watches the project tree and invalidates a target's
      makefile when files appear in it ("Invalidating makefile for X (source file added)"
      was observed on this wrapper's own runs, caused by these very log files). That
      costs a module recompile per run. Harmless for correctness - it makes runs MORE
      likely to really compile, not less - but if you want quiet incremental builds,
      point LOG_DIR outside the repo.
    #>
    param([Parameter(Mandatory = $true)][string]$ToolsDir)
    $dir = Join-Path $ToolsDir 'Logs'
    $envMap = Read-BREnvLocal -ToolsDir $ToolsDir
    if ($null -ne $envMap) {
        if ($envMap.ContainsKey('LOG_DIR')) {
            if ($envMap['LOG_DIR'] -ne '') { $dir = $envMap['LOG_DIR'] }
        }
    }
    if (-not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    return $dir
}

function Get-BRRunStamp {
    return (Get-Date).ToString('yyyyMMdd-HHmmss')
}

# ---- native process invocation (PS 5.1 safe) --------------------------------------------

function Invoke-BRNative {
    <#
      Runs a native exe/bat and captures BOTH streams to $LogPath, via a generated .cmd
      shim. The shim exists for three reasons:
        1. redirection happens inside cmd, so PowerShell never sees native stderr as
           ErrorRecords (which would corrupt $? / $ErrorActionPreference handling);
        2. quoting is exact and inspectable - the shim is kept next to the log as
           evidence of the command that actually ran;
        3. the exit code is propagated explicitly with 'exit /b %ERRORLEVEL%'.

      Returns @{ ExitCode; LogPath; ShimPath; TimedOut; DurationSeconds }.
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [string[]]$Arguments = @(),
        [Parameter(Mandatory = $true)][string]$LogPath,
        [string]$WorkingDirectory = '',
        [int]$TimeoutMinutes = 0
    )

    $shimPath = [System.IO.Path]::ChangeExtension($LogPath, '.invoke.cmd')

    $argText = ''
    foreach ($a in $Arguments) { $argText = $argText + ' ' + $a }

    $lines = @()
    $lines += '@echo off'
    $lines += 'rem Generated by Tools/_BRLadderCommon.ps1 - the exact command that ran.'
    if ($WorkingDirectory -ne '') {
        $lines += ('cd /d "{0}"' -f $WorkingDirectory)
    }
    $lines += ('call "{0}"{1} > "{2}" 2>&1' -f $Executable, $argText, $LogPath)
    $lines += 'exit /b %ERRORLEVEL%'
    Set-Content -LiteralPath $shimPath -Value $lines -Encoding ASCII

    $started = Get-Date
    $proc = Start-Process -FilePath $env:ComSpec -ArgumentList @('/c', ('"' + $shimPath + '"')) `
        -NoNewWindow -PassThru
    # Touching .Handle forces PowerShell to cache the process handle. Without this,
    # Start-Process -PassThru hands back an object whose .ExitCode reads back as $null
    # after the process ends - which silently became "exit code (blank)" and turned a
    # SUCCESSFUL build into a reported FAIL on this wrapper's very first real run.
    # Caught by dogfooding, 31 Jul 2026. An exit code you cannot read is not an exit
    # code, so the reader below refuses to guess 0.
    $null = $proc.Handle
    $timedOut = $false

    if ($TimeoutMinutes -gt 0) {
        $waited = $proc.WaitForExit($TimeoutMinutes * 60 * 1000)
        if (-not $waited) {
            $timedOut = $true
            Write-Host ("!! TIMEOUT after {0} minute(s) - killing process tree (pid {1})" -f $TimeoutMinutes, $proc.Id)
            Start-Process -FilePath 'taskkill.exe' -ArgumentList @('/T', '/F', '/PID', $proc.Id) `
                -NoNewWindow -Wait -ErrorAction SilentlyContinue
            $proc.WaitForExit(30000) | Out-Null
        }
    }
    else {
        $proc.WaitForExit()
    }

    # -999 means "the exit code could not be read", which is NOT the same as 0 and must
    # never be mistaken for it. Every caller treats a non-zero code as failure, so an
    # unreadable code fails loudly instead of passing quietly.
    $exitCode = -999
    try {
        $raw = $proc.ExitCode
        if ($null -ne $raw) { $exitCode = [int]$raw }
    }
    catch {
        $exitCode = -999
    }
    $duration = ((Get-Date) - $started).TotalSeconds

    if (-not (Test-Path -LiteralPath $LogPath)) {
        Set-Content -LiteralPath $LogPath -Value '(no output captured)' -Encoding ASCII
    }

    return @{
        ExitCode        = $exitCode
        LogPath         = $LogPath
        ShimPath        = $shimPath
        TimedOut        = $timedOut
        DurationSeconds = [math]::Round($duration, 1)
    }
}

function Write-BRLogFile {
    <#
      Prints a captured log. Default is the FULL log: on a compile rung the output IS
      the evidence (R19 item 2), and a wrapper that hides it hands the reader an opinion.
    #>
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [int]$TailLines = 0,
        [string]$Title = 'output'
    )
    Write-Host ''
    if (-not (Test-Path -LiteralPath $LogPath)) {
        Write-Host ("---- {0}: NO LOG FILE AT {1} ----" -f $Title, $LogPath)
        return
    }
    $content = @(Get-Content -LiteralPath $LogPath -ErrorAction SilentlyContinue)
    if ($TailLines -gt 0 -and $content.Count -gt $TailLines) {
        Write-Host ("---- {0} (last {1} of {2} lines; full log: {3}) ----" -f $Title, $TailLines, $content.Count, $LogPath)
        $content = $content[($content.Count - $TailLines)..($content.Count - 1)]
    }
    else {
        Write-Host ("---- {0} ({1} lines; log: {2}) ----" -f $Title, $content.Count, $LogPath)
    }
    foreach ($l in $content) { Write-Host $l }
    Write-Host ("---- end {0} ----" -f $Title)
}

function Select-BRLogLines {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [int]$Max = 60
    )
    if (-not (Test-Path -LiteralPath $LogPath)) { return @() }
    $hits = @(Select-String -LiteralPath $LogPath -Pattern $Pattern -AllMatches -ErrorAction SilentlyContinue)
    $out = @()
    $n = 0
    foreach ($h in $hits) {
        if ($n -ge $Max) { break }
        $out += $h.Line.TrimEnd()
        $n = $n + 1
    }
    return $out
}

# ---- blocked / verdict reporting ---------------------------------------------------------

function Write-BRBlocked {
    param(
        [Parameter(Mandatory = $true)][string]$Rung,
        # AllowEmptyString: a blank line inside a BLOCKED explanation is legitimate
        # formatting, and losing the whole report to a parameter-binding error over one
        # is exactly the kind of failure that makes people stop reading the output.
        [Parameter(Mandatory = $true)][AllowEmptyString()][AllowEmptyCollection()][string[]]$Reasons
    )
    Write-BRRule ("{0}: BLOCKED" -f $Rung)
    foreach ($r in $Reasons) { Write-Host ("  BLOCKED: {0}" -f $r) }
    Write-Host ''
    Write-Host "  This is NOT a pass and NOT a failure - the rung did not run."
    Write-Host ("  Exit code {0} = BLOCKED. A CI job that treats this as green is misconfigured." -f $BR_EXIT_BLOCKED)
}
