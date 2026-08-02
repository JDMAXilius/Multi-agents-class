#!/usr/bin/env bash
# =====================================================================================
# BREACHPOINT - Tools/run-ubt.sh   (macOS counterpart of run-ubt.ps1)
#
# RUNG 1 - clean compile. Same contract as the PowerShell wrapper:
#   EXIT 0 PASS | 1 FAIL | 2 INCONCLUSIVE | 3 BLOCKED   (only 0 is green)
#
# R19 evidence per target, every run: start time, full output, exit code, binary
# mtime, and an explicit "mtime > start" assertion. A target that UBT calls
# up-to-date relinked nothing, so it is INCONCLUSIVE (2) - never a silent PASS.
#
# READ THIS BEFORE TRUSTING A GREEN RUN ON A MAC:
#   docs/contracts/testing.md requires all three targets against a SOURCE build.
#   An Epic Launcher install ships no UnrealServer binaries, so BreachpointServer
#   cannot link here. On a launcher install this script reports PARTIAL and exits
#   non-zero - that is correct, not a bug to route around.
# =====================================================================================
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_DIR="$REPO_ROOT/Tools"
ALL_TARGETS=(BreachpointEditor Breachpoint BreachpointServer)
EXIT_PASS=0; EXIT_FAIL=1; EXIT_INCONCLUSIVE=2; EXIT_BLOCKED=3

blocked() { echo; echo "== BLOCKED - RUNG 1 =="; for r in "$@"; do echo "  $r"; done; exit $EXIT_BLOCKED; }

TARGETS=("${ALL_TARGETS[@]}")
[[ $# -gt 0 ]] && TARGETS=("$@")

echo "== BREACHPOINT run-ubt.sh - RUNG 1 (clean compile) =="
echo "repo: $REPO_ROOT"

# --- engine root: from Tools/env.local, never hardcoded (same rule as the ps1) -------
[[ -f "$TOOLS_DIR/env.local" ]] || blocked "Tools/env.local missing. Copy env.local.example and set ENGINE_ROOT."
ENGINE_ROOT="$(grep -E '^ENGINE_ROOT=' "$TOOLS_DIR/env.local" | tail -1 | cut -d= -f2- | tr -d '"' | xargs)"
[[ -n "$ENGINE_ROOT" && -d "$ENGINE_ROOT" ]] || blocked "ENGINE_ROOT unset or not a directory: '${ENGINE_ROOT:-}'"

BUILD_SH="$ENGINE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"
UPROJECT="$REPO_ROOT/Breachpoint.uproject"
[[ -x "$BUILD_SH" ]] || blocked "Build.sh not found/executable: $BUILD_SH"
[[ -f "$UPROJECT" ]] || blocked "Breachpoint.uproject not found at $UPROJECT"

echo "ENGINE_ROOT : $ENGINE_ROOT"
echo "UBT wrapper : $BUILD_SH"

# --- every named target must exist, or the run lies about its own scope --------------
missing=()
for t in "${TARGETS[@]}"; do
  [[ -f "$REPO_ROOT/Source/$t.Target.cs" ]] || missing+=("$t (no Source/$t.Target.cs)")
done
[[ ${#missing[@]} -eq 0 ]] || blocked "${missing[@]}"

# --- R21: one build at a time. Refuse, do not queue. ---------------------------------
if pgrep -f "UnrealBuildTool.dll" >/dev/null 2>&1; then
  blocked "Another UBT is already running. UE build state is GLOBAL (R21)." \
          "Whatever a second build reports is not about YOUR targets." \
          "Wait for it, or kill that process tree deliberately, then re-run."
fi
echo "  build lock: FREE"

# --- warnings that change how a green run must be read -------------------------------
partial=0
if [[ ! -f "$ENGINE_ROOT/Engine/Build/SourceDistribution.txt" ]]; then
  echo "!! WARNING: LAUNCHER INSTALL (no SourceDistribution.txt). It ships no server"
  echo "!! binaries - BreachpointServer is EXPECTED to fail. See Tools/env.local.example."
fi
if pgrep -f "UnrealEditor" >/dev/null 2>&1; then
  echo "!! WARNING: an Unreal editor is running. It holds"
  echo "!! libUnrealEditor-Breachpoint.dylib open; the editor target link may fail."
fi
for t in "${ALL_TARGETS[@]}"; do
  printf '%s\n' "${TARGETS[@]}" | grep -qx "$t" || partial=1
done
if [[ $partial -eq 1 ]]; then
  echo "!! PARTIAL RUN: testing.md requires all three targets. This is NOT a rung-1 result."
fi

# --- build ---------------------------------------------------------------------------
mkdir -p "$TOOLS_DIR/Logs"
overall=$EXIT_PASS
declare -a SUMMARY

for t in "${TARGETS[@]}"; do
  log="$TOOLS_DIR/Logs/ubt-$t.log"
  # R19 needs "binary newer than start". BSD find has no -newermt @epoch (GNU only,
  # and an interactive shell may shadow find with a GNU-compatible wrapper - do not
  # trust a by-hand test of this line). A reference file with -newer is portable.
  ref="$(mktemp -t brubt)"
  echo
  echo "--- $t : started $(date '+%Y-%m-%d %H:%M:%S') ---"

  # -NoUBA: UBA's local executor deadlocks the link step on macOS/arm64 (UE 5.8) -
  # the detoured linker sleeps forever on its shared-memory IPC and UBT never times out.
  "$BUILD_SH" "$t" Mac Development -project="$UPROJECT" -NoUBA -waitmutex >"$log" 2>&1
  rc=$?
  tail -5 "$log"

  # R19: a build that reports success but touched no binary produced nothing.
  newest=$(find "$REPO_ROOT/Binaries/Mac" -type f -newer "$ref" 2>/dev/null | head -1)
  rm -f "$ref"
  if [[ $rc -ne 0 ]]; then
    SUMMARY+=("FAIL    $t (exit $rc) - $log")
    overall=$EXIT_FAIL
  elif [[ -z "$newest" ]]; then
    # UBT succeeded but nothing was relinked: an up-to-date incremental build. That
    # is not evidence the code compiles NOW, so it is not a pass - but it is not a
    # failure either. Rung 1 wants a clean compile; re-run with -Rebuild to get one.
    SUMMARY+=("INCONC  $t (exit 0, already up to date - no binary relinked; use -Rebuild)")
    [[ $overall -eq $EXIT_PASS ]] && overall=$EXIT_INCONCLUSIVE
  else
    SUMMARY+=("PASS    $t (exit 0, touched $(basename "$newest"))")
  fi
done

echo
echo "== RUNG 1 SUMMARY =="
printf '  %s\n' "${SUMMARY[@]}"
[[ $partial -eq 1 ]] && { echo "  PARTIAL - fewer than three targets. Report as PARTIAL, not rung 1."; overall=$EXIT_FAIL; }
echo "exit $overall"
exit $overall
