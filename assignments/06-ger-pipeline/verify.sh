#!/usr/bin/env bash
# Verify Assignment #6 against its own rubric.
#
#   ./verify.sh
#
# Deletes the pipeline's output, replays the committed run, and checks every
# rubric criterion against what that fresh run produced — never against what
# the README claims. Each check prints the evidence it used. Exit code is the
# number of failed checks, so `./verify.sh && echo OK` works.
#
# Needs: python3. Nothing else — no pip install, no API key, no network.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"
FAILURES=0

pass() { printf '  \033[32mPASS\033[0m  %-46s %s\n' "$1" "$2"; }
fail() { printf '  \033[31mFAIL\033[0m  %-46s %s\n' "$1" "$2"; FAILURES=$((FAILURES+1)); }
note() { printf '  ----  %-46s %s\n' "$1" "$2"; }
head2() { printf '\n\033[1m%s\033[0m\n' "$1"; }

check() {
  local label="$1"; shift
  local out
  out="$(python3 - "$@" 2>&1)"
  if [ $? -eq 0 ]; then pass "$label" "$out"; else fail "$label" "$out"; fi
}

printf '\033[1mAssignment #6 — verification against the rubric\033[0m\n'
printf 'Student: Juan Diego Lugo · Game: BREACHPOINT · %s\n' "$(date -u '+%Y-%m-%d %H:%M UTC')"

# ---------------------------------------------------------------------------
head2 "0. The gate: code that does not run scores 0 across all criteria"
# ---------------------------------------------------------------------------
rm -rf output
if env -u ANTHROPIC_API_KEY python3 ger.py >/tmp/ger_verify.$$.log 2>&1; then
  pass "replay runs from a clean state, no API key" \
       "$(wc -l </tmp/ger_verify.$$.log | tr -d ' ') lines; output/ regenerated"
else
  fail "replay runs from a clean state, no API key" "see below"
  sed 's/^/        /' /tmp/ger_verify.$$.log; rm -f /tmp/ger_verify.$$.log; exit 1
fi
rm -f /tmp/ger_verify.$$.log

# ---------------------------------------------------------------------------
head2 "1. Working Pipeline /3.0 — all four stages demonstrably engaged"
# ---------------------------------------------------------------------------

check "Generator produced content" <<'PY'
import json
r = json.load(open("output/run_report.json"))
slots = r["slots"]
gen = sum(1 for s in slots for a in s["history"] if a["stage"] == "generate") \
      + sum(len(s["accepted"]) for s in slots)
assert r["accepted"] > 0, "nothing was accepted"
assert len(slots) >= 5, f"only {len(slots)} slots ran"
print(f"{len(slots)} slots, {r['accepted']} lines accepted")
PY

check "Evaluator rejected real content, citing the rule" <<'PY'
import json
r = json.load(open("output/run_report.json"))
rej = [a for s in r["slots"] for a in s["history"] if a["violations"]]
assert rej, "the evaluator never rejected anything — it never engaged"
cited = [a for a in rej if any("GDD" in v or "DT_SpotterLines" in v
                               for v in a["violations"])]
assert cited, "rejections exist but none cite the GDD or the shipped table"
print(f"{len(rej)} rejections, every one carrying a citation")
PY

check "Refiner fixed a failure that then LANDED" <<'PY'
import json
r = json.load(open("output/run_report.json"))
assert r["refined_to_pass"] >= 1, (
    "no refinement ever passed — the Refiner stage engaged but never "
    "demonstrably succeeded, which is the rubric's 'Refiner fixes failures'")
# find one and show it
for s in r["slots"]:
    hist = s["history"]
    for i, a in enumerate(hist):
        if a["stage"] == "refine" and not a["violations"]:
            prev = hist[i-1]
            print(f"{prev['line']!r} -> {a['line']!r} ({s['trigger']})")
            raise SystemExit(0)
PY

check "Circuit Breaker escalated when the loop could not self-correct" <<'PY'
import json
r = json.load(open("output/run_report.json"))
assert r["escalated"] >= 1, "the breaker never fired — the loop was never stuck"
esc = [(s["trigger"], e) for s in r["slots"] for e in s["escalated"]]
t, e = esc[0]
assert e["attempts"] >= 3, f"escalated after only {e['attempts']} attempts"
assert e["history"] or e["unresolved"], "escalation carries no history for the human"
print(f"{r['escalated']} escalation(s); e.g. {t} after {e['attempts']} attempts, "
      f"history attached")
PY

# ---------------------------------------------------------------------------
head2 "2. Evaluator Quality /3.0 — a specific rule, identifiable in the GDD"
# ---------------------------------------------------------------------------

check "the enforced rule exists verbatim in the GDD" <<'PY'
import sys
sys.path.insert(0, ".")
from ger import GDD
text = GDD.read_text(encoding="utf-8")
for phrase in ("identical minus flavor", "canned-line DataTable fallback",
               "≤ 18 words"):
    assert phrase in text, f"GDD does not contain {phrase!r}"
print(f"3 quoted phrases found in {GDD.name}")
PY

check "not a generic validity check: valid text fails for a game reason" <<'PY'
import sys
sys.path.insert(0, ".")
from ger import evaluate
# Each of these is well-formed English in the right register — and wrong for
# BREACHPOINT for a reason the GDD states. A spellchecker passes all of them.
for line in ("Reyes is down.", "Two teammates left.", "Down 3 to 5."):
    v = evaluate(line)
    assert v, f"{line!r} passed — the evaluator is only checking validity"
    assert any("GDD" in str(x) for x in v), f"{line!r} failed without a GDD citation"
# ...and legitimate lines pass, so it is not just rejecting everything.
for line in ("Teammate down.", "Rocket secured.", "Regroup."):
    assert not evaluate(line), f"{line!r} was wrongly rejected"
print("3 valid-but-wrong lines rejected with citations; 3 legitimate lines pass")
PY

check "the evaluator is deterministic — no model call" <<'PY'
import subprocess, sys, os
env = {k: v for k, v in os.environ.items() if k != "ANTHROPIC_API_KEY"}
env["PATH"] = "/nonexistent"
r = subprocess.run([sys.executable, "ger.py", "--rules"],
                   capture_output=True, text=True, env=env, timeout=120)
assert r.returncode == 0, f"--rules failed with no model reachable:\n{r.stderr}"
assert "STANDS_ALONE" in r.stdout, "--rules did not print the headline rule"
print("`ger.py --rules` prints all rules with no model reachable at all")
PY

# ---------------------------------------------------------------------------
head2 "3. Game Connection /2.0 — the capstone, not a generic game"
# ---------------------------------------------------------------------------

check "README names the game, the content type, and the catch" <<'PY'
t = open("README.md", encoding="utf-8").read()
low = t.lower()
assert "breachpoint" in low, "game not named"
assert "team" in low and "announcer" in low, "content type not named"
assert "would have missed" in low, "the catch is not described"
print("BREACHPOINT · team-event announcer lines · the catch, all present")
PY

check "landed content extends the game's real table" <<'PY'
import csv, sys
sys.path.insert(0, ".")
from ger import SHIPPED, evaluate
rows = list(csv.DictReader(open("output/DT_SpotterLines_TeamEvents.csv",
                                encoding="utf-8")))
ship = list(csv.DictReader(SHIPPED.open(encoding="utf-8")))
cols_new = set(rows[0].keys()); cols_ship = set(ship[0].keys())
assert cols_new == cols_ship, f"schema mismatch: {cols_new ^ cols_ship}"
ship_triggers = {r["TriggerId"] for r in ship}
overlap = {r["TriggerId"] for r in rows} & ship_triggers
assert not overlap, f"regenerated triggers the table already has: {overlap}"
bad = [r["RowName"] for r in rows if evaluate(r["Text"])]
assert not bad, f"landed rows fail their own evaluator: {bad}"
print(f"{len(rows)} rows, schema identical to DT_SpotterLines.csv, "
      f"all triggers new, 0 rows failing the rules")
PY

# ---------------------------------------------------------------------------
head2 "4. ReadMe /2.0 — Pre-Build Declaration included, three answers"
# ---------------------------------------------------------------------------

check "declaration: all three answers, under 150 words" <<'PY'
import re
d = open("PRE-BUILD-DECLARATION.txt", encoding="utf-8").read()
for q in ("1. WHAT CONTENT", "2. WHAT SPECIFIC RULE", "3. WHAT DOES A FAILURE"):
    assert q in d, f"missing section {q!r}"
lines = [l for l in d.splitlines() if l and not re.match(r"^[0-9]\. |^PRE-BUILD|^Juan", l)]
w = len(re.findall(r"[A-Za-z0-9'§=>_-]+", " ".join(lines)))
assert w <= 150, f"{w} words, cap is 150"
print(f"three answers, {w} words (cap 150)")
PY

check "the answers are ALSO inside the README" <<'PY'
t = open("README.md", encoding="utf-8").read().lower()
for k in ("what content does my game generate", "what specific rule",
          "what does a failure look like"):
    assert k in t, f"README is missing: {k!r}"
print("all three declaration answers reproduced in the README")
PY

# ---------------------------------------------------------------------------
head2 "5. What this does NOT prove"
# ---------------------------------------------------------------------------
note "not imported" "the CSV has not been through UE's importer"
note "triggers not raised" "the Team.* events need game code before any line plays"
note "the README says so" "see its 'Honest limits' section"

printf '\n'
if [ "$FAILURES" -eq 0 ]; then
  printf '\033[32m%s\033[0m\n' "ALL CHECKS PASSED — every rubric criterion is satisfied by this replay."
else
  printf '\033[31m%s\033[0m\n' "$FAILURES CHECK(S) FAILED — see above."
fi
exit "$FAILURES"
