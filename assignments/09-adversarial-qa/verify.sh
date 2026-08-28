#!/usr/bin/env bash
# Verify Assignment #9 against its rubric.
#
#   ./verify.sh
#
# Checks each rubric criterion against what is actually committed: the agent code (in
# the game tree, or in game-code/ inside the extracted zip), the agent's own JSON
# report from the PIE run, and the README's two answers. Every check prints its
# evidence. Exit code = number of failed checks.
#
# Until TICKET_BN24's PIE run lands a report in report/, the Findings checks FAIL —
# that failure is the gate working: this assignment cannot honestly ship without a run.
#
# Needs: python3. No pip install, no API key, no network, no Unreal Engine.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; cd "$HERE"
FAILURES=0
pass() { printf '  \033[32mPASS\033[0m  %-46s %s\n' "$1" "$2"; }
fail() { printf '  \033[31mFAIL\033[0m  %-46s %s\n' "$1" "$2"; FAILURES=$((FAILURES+1)); }
note() { printf '  ----  %-46s %s\n' "$1" "$2"; }
head2() { printf '\n\033[1m%s\033[0m\n' "$1"; }
check() { local l="$1"; shift; local o; o="$(python3 - "$@" 2>&1)"; \
  if [ $? -eq 0 ]; then pass "$l" "$o"; else fail "$l" "$o"; fi; }

printf '\033[1mAssignment #9 — verification against the rubric\033[0m\n'
printf 'Student: Juan Diego Lugo · %s\n' "$(date -u '+%Y-%m-%d %H:%M UTC')"

# Where is the agent code? The repo layout or the extracted-zip layout.
CODE=""
for c in "$HERE/../../breachpoint/Source/BreachpointNext/QA" "$HERE/game-code"; do
  [ -f "$c/BNAdversarialAgent.cpp" ] && CODE="$c" && break
done
export AQA_CODE="$CODE"
REPORT="$(ls report/aqa_report_*.json 2>/dev/null | head -1 || true)"
export AQA_REPORT="$REPORT"

# ---------------------------------------------------------------------------
head2 "1. Agent Logic /3.0 — a continuous loop that TRIES TO BREAK, with 'broken' defined"
# ---------------------------------------------------------------------------

check "agent code present, loop cycles 5 behaviors" <<'PY'
import os, re, sys
code = os.environ["AQA_CODE"]
assert code, "agent code not found (game tree or game-code/)"
src = open(f"{code}/BNAdversarialAgent.cpp", encoding="utf-8").read()
hdr = open(f"{code}/BNAdversarialAgent.h", encoding="utf-8").read()
behaviors = ["roam", "boundary_probe", "ledge_dive", "grapple_abuse", "ability_mash"]
missing = [b for b in behaviors if f'TEXT("{b}")' not in src]
assert not missing, f"behaviors missing from the loop: {missing}"
assert "AdvanceBehavior" in src and "SetTimer(BehaviorTimer" in src, "no behavior cycle timer"
assert "bn.aqa.start" in src, "no console entry point"
assert "Tick" not in re.findall(r"virtual void (\w+)", hdr), "gameplay Tick (banned by project law)"
print(f"{len(behaviors)} behaviors cycled on a timer; console-driven; no Tick")
PY

check "'broken' is defined: 7 detector classes in code" <<'PY'
import os
code = os.environ["AQA_CODE"]
assert code, "agent code not found"
src = open(f"{code}/BNAdversarialAgent.cpp", encoding="utf-8").read()
detectors = ["fell_out_of_world_alive", "escaped_playable_space", "stuck_state",
             "speed_violation", "attribute_anomaly", "teleport_discontinuity",
             "acted_while_dead", "input_during_freeze"]
missing = [d for d in detectors if f'TEXT("{d}")' not in src]
assert not missing, f"detector classes missing: {missing}"
print(f"{len(detectors)} error types, each recorded with evidence and location")
PY

# ---------------------------------------------------------------------------
head2 "2. Findings /4.0 — a real run, real defects, mechanic named"
# ---------------------------------------------------------------------------

check "a report from a real run exists and parses" <<'PY'
import json, os
p = os.environ["AQA_REPORT"]
assert p, "no report/aqa_report_*.json — run TICKET_BN24 (PIE: bn.aqa.start 300) first"
r = json.load(open(p))
assert r.get("schema") == "aqa-report/1", f"unexpected schema {r.get('schema')}"
s = r["stats"]
assert r["duration_s"] >= 60, f"run too short: {r['duration_s']}s"
assert r["behavior_cycles"] >= 5, f"loop barely cycled: {r['behavior_cycles']}"
assert s["ability_presses"] > 0 and s["move_requests"] > 0, "the agent never acted"
print(f"{r['duration_s']:.0f}s on {r['map']}; {r['behavior_cycles']} behavior cycles, "
      f"{s['ability_presses']} presses, {s['move_requests']} moves, {s['deaths']} deaths")
PY

check "at least one finding, each naming its evidence" <<'PY'
import json, os
p = os.environ["AQA_REPORT"]
assert p, "no report yet"
f = json.load(open(p))["findings"]
if not f:
    raise SystemExit("ZERO findings — a clean run is honest but scores no Findings "
                     "points; rerun longer (bn.aqa.start 600) before accepting it")
bad = [x["error_type"] for x in f if not x.get("evidence") or not x.get("nearest_anchor")]
assert not bad, f"findings without evidence/anchor: {bad}"
kinds = sorted({x["error_type"] for x in f})
print(f"{len(f)} finding(s), {len(kinds)} class(es): {', '.join(kinds)}")
PY

# ---------------------------------------------------------------------------
head2 "3. Structured Report /2.0 — location, error type, game context on every row"
# ---------------------------------------------------------------------------

check "every finding carries the three required fields" <<'PY'
import json, os
p = os.environ["AQA_REPORT"]
assert p, "no report yet"
r = json.load(open(p))
for i, f in enumerate(r["findings"]):
    loc = f["location"]
    assert all(isinstance(loc[k], (int, float)) for k in ("x", "y", "z")), f"finding {i}: bad location"
    assert f["error_type"], f"finding {i}: no error_type"
    ctx = f["game_context"]
    for k in ("behavior", "match_state", "sim_time_s", "alive", "health_norm", "speed_uu_s"):
        assert k in ctx, f"finding {i}: game_context missing {k}"
assert r["map"] and "started_utc" in r, "top-level context missing"
n = len(r["findings"])
print(f"{n} finding(s): location xyz + nearest_anchor, error_type, 6-field game_context")
PY

# ---------------------------------------------------------------------------
head2 "4. ReadMe /1.0 — what did it find, and were you surprised"
# ---------------------------------------------------------------------------

check "README answers both questions from the real run" <<'PY'
t = open("README.md", encoding="utf-8").read()
assert "FILL AFTER RUN" not in t, "README findings still pending the PIE run (TICKET_BN24 step 4)"
low = t.lower()
assert "## what the agent found" in low, "findings section missing"
assert "surprised" in low, "the surprise answer is missing"
print("both questions answered; no placeholder markers remain")
PY

# ---------------------------------------------------------------------------
head2 "5. What this does NOT prove"
# ---------------------------------------------------------------------------
note "PIE, not packaged/multiplayer" "the report names its net mode; honesty ladder holds"
note "bounds detector is heuristic" "PlayerStart hull + margin; each finding says so"

printf '\n'
if [ "$FAILURES" -eq 0 ]; then
  printf '\033[32m%s\033[0m\n' "ALL CHECKS PASSED — every rubric criterion is satisfied."
else
  printf '\033[31m%s\033[0m\n' "$FAILURES CHECK(S) FAILED — see above."
fi
exit "$FAILURES"
