#!/usr/bin/env bash
# =====================================================================================
# BREACHPOINT - Tools/run-specs.sh   (macOS counterpart of run-specs.ps1)
#
# RUNG 2 - the automation specs. Same exit contract as run-ubt.sh:
#   EXIT 0 PASS | 1 FAIL | 2 INCONCLUSIVE | 3 BLOCKED   (only 0 is green)
#
# A spec run that finds NO tests is INCONCLUSIVE, never PASS. That is the failure this
# script exists to catch: a filter typo, a stale build, or a module whose Tests/ files
# were compiled out reports "0 tests, 0 failures" and looks exactly like success.
#
# Default filter is the BreachpointNext suites. Pass one argument to narrow it:
#   Tools/run-specs.sh BreachpointNext.Sim.BotBrain
# =====================================================================================
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_DIR="$REPO_ROOT/Tools"
LOG_DIR="$TOOLS_DIR/Logs"
EXIT_PASS=0; EXIT_FAIL=1; EXIT_INCONCLUSIVE=2; EXIT_BLOCKED=3

FILTER="${1:-BreachpointNext}"

blocked() { echo; echo "== BLOCKED - RUNG 2 =="; for r in "$@"; do echo "  $r"; done; exit $EXIT_BLOCKED; }

echo "== BREACHPOINT run-specs.sh - RUNG 2 (automation specs) =="
echo "repo   : $REPO_ROOT"
echo "filter : $FILTER"

# --- engine root: from Tools/env.local, never hardcoded (run-ubt.sh's rule) -----------
[[ -f "$TOOLS_DIR/env.local" ]] || blocked "Tools/env.local missing. Copy env.local.example and set ENGINE_ROOT."
ENGINE_ROOT="$(grep -E '^ENGINE_ROOT=' "$TOOLS_DIR/env.local" | tail -1 | cut -d= -f2- | tr -d '"' | xargs)"
[[ -n "$ENGINE_ROOT" && -d "$ENGINE_ROOT" ]] || blocked "ENGINE_ROOT unset or not a directory: '${ENGINE_ROOT:-}'"

EDITOR_CMD="$ENGINE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
UPROJECT="$REPO_ROOT/Breachpoint.uproject"
[[ -x "$EDITOR_CMD" ]] || blocked "UnrealEditor-Cmd not found/executable: $EDITOR_CMD"
[[ -f "$UPROJECT" ]] || blocked "Breachpoint.uproject not found at $UPROJECT"

# --- R21's rule, for the same reason: editor state is global -------------------------
if pgrep -f "UnrealEditor" >/dev/null 2>&1; then
  blocked "An UnrealEditor process is already running." \
          "Automation runs headless against the same project state; close the editor first."
fi

mkdir -p "$LOG_DIR"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG="$LOG_DIR/specs-$STAMP.log"

echo "log    : $LOG"
echo

# -nullrhi: no window, no GPU. The specs are simulation, not rendering.
"$EDITOR_CMD" "$UPROJECT" \
  -ExecCmds="Automation RunTests $FILTER; Quit" \
  -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput \
  2>&1 | tee "$LOG"

# --- the verdict, read from the log rather than from the exit code -------------------
# UnrealEditor-Cmd exits 0 on a run that had failing tests, so its code proves only that
# the process ended. The automation controller's own summary line is the truth.
RESULT_LINE="$(grep -E "Automation Test (Succeeded|Failed)|Total Tests|Test Completed" "$LOG" | tail -20)"
RAN="$(grep -cE "Test Started" "$LOG" || true)"
FAILED="$(grep -cE "Automation Test Failed|Error: Test Failed" "$LOG" || true)"

echo
echo "== RUNG 2 RESULT =="
[[ -n "$RESULT_LINE" ]] && echo "$RESULT_LINE"

if [[ "${RAN:-0}" -eq 0 ]]; then
  echo "  NO TESTS RAN for filter '$FILTER'."
  echo "  This is INCONCLUSIVE, not a pass: a filter typo, a stale build, or specs compiled"
  echo "  out (WITH_DEV_AUTOMATION_TESTS off in this target) all look like this."
  exit $EXIT_INCONCLUSIVE
fi

if [[ "${FAILED:-0}" -gt 0 ]]; then
  echo "  $FAILED failing test(s) across $RAN started. See $LOG"
  exit $EXIT_FAIL
fi

echo "  $RAN test(s) started, 0 failures. Log: $LOG"
exit $EXIT_PASS
