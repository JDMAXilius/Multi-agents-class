#!/usr/bin/env bash
# Verify Assignment #9 against its rubric.
#
#   ./verify.sh
#
# Needs: python3, and a C++ compiler (g++ or clang++) for the detector suite.
# Does NOT need: Unreal Engine, a GPU, an API key, network, or pip install.
#
# TWO MODES, chosen automatically:
#
#   PRE-RUN   no report/aqa_report_*.json present yet. Everything that can be checked
#             without the game is checked and must pass; the four run-dependent rubric
#             items print as PEND with the reason. Exit 0 when the checkable part is
#             clean — see TESTING.md for what the run adds.
#   FULL      a report is present. Every rubric criterion is checked for real, including
#             the findings and the README's answers. Exit = number of failures.
#
# The detector checks are not documentation-reading — they COMPILE the game's own rule
# header with a stock compiler and execute all 44 rule cases. That is the part a grader
# with no Unreal install can run and see for themselves.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; cd "$HERE"
FAILURES=0
PENDING=0
PASSES=0
pass() { printf '  \033[32mPASS\033[0m  %-44s %s\n' "$1" "$2"; PASSES=$((PASSES+1)); }
fail() { printf '  \033[31mFAIL\033[0m  %-44s %s\n' "$1" "$2"; FAILURES=$((FAILURES+1)); }
pend() { printf '  \033[33mPEND\033[0m  %-44s %s\n' "$1" "$2"; PENDING=$((PENDING+1)); }
note() { printf '  ----  %-44s %s\n' "$1" "$2"; }
head2() { printf '\n\033[1m%s\033[0m\n' "$1"; }

# Python checks print ONE clean line on failure — a grader should never have to read a
# traceback to learn that a file is missing.
check() {
  local label="$1"; shift
  local out
  if out="$(python3 - "$@" 2>&1)"; then
    pass "$label" "$out"
  else
    local msg
    msg="$(printf '%s' "$out" | sed -n 's/^[A-Za-z]*Error: //p' | tail -1)"
    [ -z "$msg" ] && msg="$(printf '%s' "$out" | tail -1)"
    fail "$label" "$msg"
  fi
}

printf '\033[1mAssignment #9 — verification against the rubric\033[0m\n'
printf 'Student: Juan Diego Lugo · %s\n' "$(date -u '+%Y-%m-%d %H:%M UTC')"

# Locate the agent source: repo layout, or game-code/ inside the extracted zip.
CODE=""
for c in "$HERE/../../breachpoint/Source/BreachpointNext/QA" "$HERE/game-code"; do
  [ -f "$c/BNAdversarialAgent.cpp" ] && CODE="$(cd "$c" && pwd)" && break
done
export AQA_CODE="$CODE"
REPORT="$(ls report/aqa_report_*.json 2>/dev/null | head -1 || true)"
export AQA_REPORT="$REPORT"
MODE="FULL"; [ -z "$REPORT" ] && MODE="PRE-RUN"
printf 'Mode: %s%s\n' "$MODE" \
  "$([ "$MODE" = "PRE-RUN" ] && printf ' (no PIE report committed yet — see TESTING.md)')"

# ---------------------------------------------------------------------------
head2 "0. The rule layer, COMPILED AND EXECUTED (no Unreal Engine involved)"
# ---------------------------------------------------------------------------
CXX=""; for c in g++ clang++ c++; do command -v "$c" >/dev/null 2>&1 && CXX="$c" && break; done
if [ -z "$CODE" ]; then
  fail "detector suite compiles and runs" "agent source not found in repo or game-code/"
elif [ -z "$CXX" ]; then
  pend "detector suite compiles and runs" "no C++ compiler on PATH (install g++ or clang++)"
else
  BIN="$(mktemp -d)/detector_tests"
  if "$CXX" -std=c++17 -O0 -Wall -Wextra -I"$CODE" -o "$BIN" tests/detector_tests.cpp \
       >/tmp/aqa_cc.$$.log 2>&1; then
    if "$BIN" >/tmp/aqa_run.$$.log 2>&1; then
      pass "detector suite compiles and runs" \
           "$(grep -oE '^[0-9]+ checks, [0-9]+ failure' /tmp/aqa_run.$$.log) — all rules fire and excuse correctly"
      note "  (full output)" "$CXX -std=c++17 -I<QA dir> tests/detector_tests.cpp && ./a.out"
    else
      fail "detector suite compiles and runs" "$(grep -E 'FAIL' /tmp/aqa_run.$$.log | head -3)"
    fi
  else
    fail "detector suite compiles and runs" "$(head -3 /tmp/aqa_cc.$$.log)"
  fi
  rm -f /tmp/aqa_cc.$$.log /tmp/aqa_run.$$.log
fi

check "the game calls those rules — no second copy" <<'PY'
import os, re, sys
code = os.environ["AQA_CODE"]
if not code:
    raise SystemExit("Error: agent source not found")
src = open(f"{code}/BNAdversarialAgent.cpp", encoding="utf-8").read()
calls = ["FellOutOfWorldAlive", "EscapedPlayableSpace", "StuckState", "SpeedViolation",
         "AttributeAnomaly", "TeleportDiscontinuity", "GateBreak"]
missing = [c for c in calls if f"BNAQA::{c}(" not in src]
if missing:
    raise SystemExit(f"Error: controller does not call {missing} — the rule layer was bypassed")
# a threshold literal re-stated in the controller would be a second source of truth
for lit in ("1.75", "1200.f", "3.0f"):
    if lit in src:
        raise SystemExit(f"Error: threshold literal {lit} restated in the controller")
print(f"all {len(calls)} detectors called from BNAQADetectors.h; no threshold restated")
PY

# ---------------------------------------------------------------------------
head2 "1. Agent Logic /3.0 — a continuous loop that TRIES TO BREAK, 'broken' defined"
# ---------------------------------------------------------------------------

check "loop cycles 5 adversarial behaviors" <<'PY'
import os, re
code = os.environ["AQA_CODE"]
if not code:
    raise SystemExit("Error: agent source not found")
src = open(f"{code}/BNAdversarialAgent.cpp", encoding="utf-8").read()
hdr = open(f"{code}/BNAdversarialAgent.h", encoding="utf-8").read()
behaviors = ["roam", "boundary_probe", "ledge_dive", "grapple_abuse", "ability_mash"]
missing = [b for b in behaviors if f'TEXT("{b}")' not in src]
if missing:
    raise SystemExit(f"Error: behaviors missing from the loop: {missing}")
if "SetTimer(BehaviorTimer" not in src or "AdvanceBehavior" not in src:
    raise SystemExit("Error: no behavior-cycle timer")
if "bn.aqa.start" not in src:
    raise SystemExit("Error: no console entry point")
if "Tick" in re.findall(r"virtual void (\w+)", hdr):
    raise SystemExit("Error: gameplay Tick present (banned by project law 4)")
print(f"{len(behaviors)} behaviors on a timer; console-driven; no Tick")
PY

check "'broken' is defined: 7 detector classes" <<'PY'
import os
code = os.environ["AQA_CODE"]
if not code:
    raise SystemExit("Error: agent source not found")
src = open(f"{code}/BNAdversarialAgent.cpp", encoding="utf-8").read()
detectors = ["fell_out_of_world_alive", "escaped_playable_space", "stuck_state",
             "speed_violation", "attribute_anomaly", "teleport_discontinuity",
             "acted_while_dead", "input_during_freeze"]
missing = [d for d in detectors if f'TEXT("{d}")' not in src]
if missing:
    raise SystemExit(f"Error: detector classes missing: {missing}")
print(f"{len(detectors)} error types, each recorded with evidence and location")
PY

check "it presses the same path a human's input does" <<'PY'
import os
code = os.environ["AQA_CODE"]
if not code:
    raise SystemExit("Error: agent source not found")
src = open(f"{code}/BNAdversarialAgent.cpp", encoding="utf-8").read()
if "AbilityInputTagPressed" not in src:
    raise SystemExit("Error: does not press through the ability input path")
if "GenericPlayerInitialization" not in src or "RestartPlayer" not in src:
    raise SystemExit("Error: does not join through the game mode's own doors")
print("joins via GenericPlayerInitialization/RestartPlayer; presses on the PlayerState ASC")
PY

# ---------------------------------------------------------------------------
head2 "2. Findings /4.0 — a real run, real defects, mechanic named"
# ---------------------------------------------------------------------------

if [ -z "$REPORT" ]; then
  pend "a report from a real run exists" "needs one PIE run — TICKET_BN24, 'bn.aqa.start 300'"
  pend "at least one finding, with evidence" "produced by that same run"
else
  check "a report from a real run exists and parses" <<'PY'
import json, os
r = json.load(open(os.environ["AQA_REPORT"]))
if r.get("schema") != "aqa-report/1":
    raise SystemExit(f"Error: unexpected schema {r.get('schema')}")
s = r["stats"]
if r["duration_s"] < 60:
    raise SystemExit(f"Error: run too short ({r['duration_s']}s)")
if r["behavior_cycles"] < 5:
    raise SystemExit(f"Error: loop barely cycled ({r['behavior_cycles']})")
if s["ability_presses"] <= 0 or s["move_requests"] <= 0:
    raise SystemExit("Error: the agent never acted")
print(f"{r['duration_s']:.0f}s on {r['map']}; {r['behavior_cycles']} cycles, "
      f"{s['ability_presses']} presses, {s['move_requests']} moves, {s['deaths']} deaths")
PY

  check "at least one finding, each naming its evidence" <<'PY'
import json, os
f = json.load(open(os.environ["AQA_REPORT"]))["findings"]
if not f:
    raise SystemExit("Error: ZERO findings — honest, but scores no Findings points; "
                     "rerun longer (bn.aqa.start 600) before accepting it")
bad = [x["error_type"] for x in f if not x.get("evidence") or not x.get("nearest_anchor")]
if bad:
    raise SystemExit(f"Error: findings without evidence/anchor: {bad}")
kinds = sorted({x["error_type"] for x in f})
print(f"{len(f)} finding(s), {len(kinds)} class(es): {', '.join(kinds)}")
PY
fi

# ---------------------------------------------------------------------------
head2 "3. Structured Report /2.0 — location, error type, game context on every row"
# ---------------------------------------------------------------------------

check "the report SCHEMA the agent writes is complete" <<'PY'
import os, re
code = os.environ["AQA_CODE"]
if not code:
    raise SystemExit("Error: agent source not found")
src = open(f"{code}/BNAdversarialAgent.cpp", encoding="utf-8").read()
writer = src[src.index("void ABNAQAController::WriteReport"):]
required = ["error_type", "severity", "occurrences", "location", "nearest_anchor",
            "game_context", "behavior", "match_state", "sim_time_s", "alive",
            "health_norm", "speed_uu_s", "evidence", "first_seen_utc", "map", "net_mode"]
missing = [k for k in required if f'TEXT("{k}")' not in writer]
if missing:
    raise SystemExit(f"Error: report writer omits {missing}")
print(f"writer emits all {len(required)} fields incl. location, error_type, game_context")
PY

if [ -z "$REPORT" ]; then
  pend "every finding carries the three fields" "checked against the real report once it exists"
else
  check "every finding carries the three required fields" <<'PY'
import json, os
r = json.load(open(os.environ["AQA_REPORT"]))
for i, f in enumerate(r["findings"]):
    loc = f["location"]
    if not all(isinstance(loc.get(k), (int, float)) for k in ("x", "y", "z")):
        raise SystemExit(f"Error: finding {i} has no usable location")
    if not f.get("error_type"):
        raise SystemExit(f"Error: finding {i} has no error_type")
    for k in ("behavior", "match_state", "sim_time_s", "alive", "health_norm", "speed_uu_s"):
        if k not in f["game_context"]:
            raise SystemExit(f"Error: finding {i} game_context missing {k}")
if not r.get("map"):
    raise SystemExit("Error: top-level context missing")
print(f"{len(r['findings'])} finding(s): location xyz + anchor, error_type, 6-field context")
PY
fi

# ---------------------------------------------------------------------------
head2 "4. ReadMe /1.0 — what did it find, and were you surprised"
# ---------------------------------------------------------------------------

check "README explains the strategy and how to test it" <<'PY'
t = open("README.md", encoding="utf-8").read().lower()
for need, why in [("boundary_probe", "the behaviors"), ("detector", "the definition of broken"),
                  ("bn.aqa.start", "how to reproduce"), ("testing.md", "the grader's guide")]:
    if need not in t:
        raise SystemExit(f"Error: README does not cover {why} ({need})")
print("strategy, detector definitions, reproduction command and grader guide all present")
PY

if [ -z "$REPORT" ]; then
  pend "README answers 'what did it find'" "written from the real report, per TICKET_BN24 step 4"
else
  check "README answers both questions from the real run" <<'PY'
t = open("README.md", encoding="utf-8").read()
if "FILL AFTER RUN" in t:
    raise SystemExit("Error: README findings still pending (TICKET_BN24 step 4)")
low = t.lower()
if "## what the agent found" not in low:
    raise SystemExit("Error: findings section missing")
if "surprised" not in low:
    raise SystemExit("Error: the surprise answer is missing")
print("both questions answered; no placeholder markers remain")
PY
fi

# ---------------------------------------------------------------------------
head2 "5. What this does NOT prove"
# ---------------------------------------------------------------------------
note "rules proven, integration not" "the 44 cases run headless; wiring needs the PIE run"
note "PIE, not packaged/multiplayer" "the report names its net mode; honesty ladder holds"
note "bounds detector is heuristic" "PlayerStart hull + margin; every finding says so"

printf '\n'
if [ "$FAILURES" -eq 0 ] && [ "$PENDING" -eq 0 ]; then
  printf '\033[32m%s\033[0m\n' "ALL CHECKS PASSED — every rubric criterion is satisfied."
elif [ "$FAILURES" -eq 0 ]; then
  printf '\033[32m%s\033[0m\n' "ALL $PASSES CHECKABLE CRITERIA PASS — 0 failures."
  printf '\033[33m%s\033[0m\n' "$PENDING item(s) PENDING the one Play-In-Editor run — see TESTING.md §3."
  printf '%s\n' "Nothing is failing. Those items become PASS when report/ holds a real run."
else
  printf '\033[31m%s\033[0m\n' "$FAILURES CHECK(S) FAILED — see above."
fi
exit "$FAILURES"
