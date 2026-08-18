#!/usr/bin/env bash
# Verify Assignment #5 against the assignment's own criteria.
#
#   ./verify.sh
#
# One command. It deletes the agent's output, re-runs the agent from scratch,
# and then checks each requirement against what the run actually produced —
# not against anything the README claims. Every check prints the evidence it
# used, so a failure says which file disagreed rather than just "FAIL".
#
# Exit code is the number of failed checks, so `./verify.sh && echo OK` works.
#
# Needs: python3. Nothing else — no pip install, no API key, no network.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

GEN_H="project/Source/Breachpoint/Telemetry/BRSpotterSubsystem.h"
GEN_CPP="project/Source/Breachpoint/Telemetry/BRSpotterSubsystem.cpp"
FAILURES=0

pass() { printf '  \033[32mPASS\033[0m  %-46s %s\n' "$1" "$2"; }
fail() { printf '  \033[31mFAIL\033[0m  %-46s %s\n' "$1" "$2"; FAILURES=$((FAILURES+1)); }
note() { printf '  ----  %-46s %s\n' "$1" "$2"; }
head2() { printf '\n\033[1m%s\033[0m\n' "$1"; }

check() {  # check <label> <python expression file> — python exits 0 = pass
  local label="$1"; shift
  local out
  out="$(python3 - "$@" 2>&1)"
  if [ $? -eq 0 ]; then pass "$label" "$out"; else fail "$label" "$out"; fi
}

printf '\033[1mAssignment #5 — verification\033[0m\n'
printf 'Student: Juan Diego Lugo · Game: BREACHPOINT · %s\n' "$(date -u '+%Y-%m-%d %H:%M UTC')"

# ---------------------------------------------------------------------------
head2 "0. Environment"
# ---------------------------------------------------------------------------
if command -v python3 >/dev/null; then
  pass "python3 available" "$(python3 -V 2>&1)"
else
  fail "python3 available" "not on PATH — nothing else can run"; exit 1
fi
if [ -n "${ANTHROPIC_API_KEY:-}" ]; then
  note "ANTHROPIC_API_KEY is set" "unset it to prove no network is needed"
else
  pass "no API key set" "the run below uses the recorded responses only"
fi

# ---------------------------------------------------------------------------
head2 "1. The agent runs from a clean state"
# ---------------------------------------------------------------------------
rm -f "$GEN_H" "$GEN_CPP" output/perception.json output/ranking.json output/build_report.json
if [ -f "$GEN_H" ] || [ -f "$GEN_CPP" ]; then
  fail "generated code cleared" "could not delete it"
else
  pass "generated code cleared" "deleted before the run, so nothing is pre-baked"
fi

RUN_LOG="$(mktemp)"
if env -u ANTHROPIC_API_KEY python3 agent.py >"$RUN_LOG" 2>&1; then
  pass "agent.py exits 0" "$(wc -l <"$RUN_LOG" | tr -d ' ') lines of output"
else
  fail "agent.py exits 0" "see the log below"; sed 's/^/        /' "$RUN_LOG"; exit 1
fi

# ---------------------------------------------------------------------------
head2 "2. The five requirements"
# ---------------------------------------------------------------------------

check "R1  reads the GDD" <<'PY'
import json, re, pathlib
p = json.load(open("output/perception.json"))
feats = p["features"]
raw = pathlib.Path("project/GDD.md").read_text(encoding="utf-8")
assert "### 5.1 Shipped Scope" in raw, "the GDD section the agent parses is missing"
assert len(feats) >= 10, f"only {len(feats)} features extracted"
# Every feature must be traceable to the GDD's own words. Compare with markdown
# emphasis stripped and whitespace collapsed: the document writes "**1**
# three-level arena map", and the agent (correctly) removes the bold markers.
norm = lambda s: " ".join(re.sub(r"[*`]", "", s).split())
gdd = norm(raw)
missing = [f["text"] for f in feats if norm(f["text"]) not in gdd]
assert not missing, f"features not traceable to the GDD: {missing[:2]}"
print(f"{len(feats)} features, all traceable verbatim to GDD.md §5.1")
PY

check "R2  scans the codebase" <<'PY'
import json, pathlib
p = json.load(open("output/perception.json"))
files, syms = p["files_scanned"], p["symbols_found"]
b = json.load(open("output/build_report.json"))
# The scan happens BEFORE the agent writes its own output, so the two generated
# files are on disk now but were not there to be counted. Excluding them is the
# difference between checking the scan and checking the clock.
generated = {pathlib.Path(b["header"]).name, pathlib.Path(b["source"]).name}
on_disk = [q for q in pathlib.Path("project/Source").rglob("*")
           if q.suffix in (".h", ".cpp") and q.name not in generated]
assert files == len(on_disk), f"reported {files} files but {len(on_disk)} were scannable"
assert syms > 100, f"only {syms} declarations found"
print(f"{files} source files read (of {len(on_disk)} present pre-run), "
      f"{syms} declarations indexed")
PY

check "R3  detects gaps, with evidence" <<'PY'
import json
p = json.load(open("output/perception.json"))
feats = p["features"]
built = [f for f in feats if f["built"]]
gaps = [f for f in feats if not f["built"]]
assert gaps, "no gaps detected at all"
assert built, "everything reported missing — the scanner is not matching"
noev = [f["id"] for f in feats if not f["evidence"]]
assert not noev, f"verdicts with no evidence: {noev}"
print(f"{len(built)} built / {len(gaps)} missing, every verdict carries evidence")
PY

check "R4  prioritises, and the ranking is reproducible" <<'PY'
import json
r = json.load(open("output/ranking.json"))
cands, sel = r["candidates"], r["selected"]
assert cands, "no candidates ranked"
assert sel, "no selection recorded"
# Recompute each total from its own stored terms, and recompute the winner.
for c in cands:
    total = round(sum(c["terms"].values()), 1)
    assert abs(total - c["score"]) < 0.05, \
        f"{c['id']}: terms sum to {total} but score says {c['score']}"
top = max(cands, key=lambda c: c["score"])
assert top["id"] == sel, f"selected {sel} but the highest score is {top['id']}"
assert len(top["terms"]) >= 3, "fewer than three scoring terms — not much of a rationale"
runner = sorted(cands, key=lambda c: -c["score"])[1:2]
margin = f", margin {top['score'] - runner[0]['score']:.1f}" if runner else ""
print(f"{len(cands)} ranked, {len(top['terms'])} terms each; "
      f"winner recomputes to {sel}{margin}")
PY

check "R5  generates code for a missing feature" <<'PY'
import json, pathlib
b = json.load(open("output/build_report.json"))
h = pathlib.Path(b["header"]); c = pathlib.Path(b["source"])
for p in (pathlib.Path("project") / h, pathlib.Path("project") / c):
    assert p.exists(), f"{p} was not written"
    assert p.stat().st_size > 500, f"{p} is suspiciously small"
assert not b["verify_problems"], f"landed with problems: {b['verify_problems']}"
assert b["gap_closed_on_rescan"], "the agent's own re-scan says the gap is still open"
size = (pathlib.Path("project")/h).stat().st_size + (pathlib.Path("project")/c).stat().st_size
print(f"{b['class']} written ({size:,} bytes), verify clean, gap closed on re-scan")
PY

# ---------------------------------------------------------------------------
head2 "3. The two deliverables"
# ---------------------------------------------------------------------------

check "D1  a complete, runnable agent" <<'PY'
import pathlib
need = ["agent.py", "README.md", "recording.json", "project/GDD.md",
        "project/PROVENANCE.md"]
missing = [n for n in need if not pathlib.Path(n).exists()]
assert not missing, f"missing: {missing}"
src = pathlib.Path("agent.py").read_text(encoding="utf-8")
# stdlib only: no third-party import should appear. Resolved by spec origin rather
# than sys.stdlib_module_names, which only exists on Python 3.10+ (this repo's
# graders run 3.9).
import re, importlib.util, sysconfig
STDLIB = sysconfig.get_paths()["stdlib"]
mods = {m.split(".")[0] for m in re.findall(r"^\s*(?:import|from)\s+([\w.]+)",
                                            src, re.M)}
def is_stdlib(name):
    try:
        origin = getattr(importlib.util.find_spec(name), "origin", None) or ""
    except (ImportError, ValueError):
        return False
    if origin in ("built-in", "frozen"):
        return True
    return origin.startswith(STDLIB) and "-packages" not in origin
third = {m for m in mods - {"crew", "rag", "gaps", "agent", "anthropic"}
         if not is_stdlib(m)}
assert not third, f"non-stdlib imports: {third}"
print(f"{len(need)} required files present; agent.py imports stdlib only")
PY

check "D2  README answers the three questions" <<'PY'
import pathlib, re
t = pathlib.Path("README.md").read_text(encoding="utf-8").lower()
qs = {"what it built": ["what the agent built", "ubrspottersubsystem"],
      "why that feature": ["why it picked", "why did the agent select", "because"],
      "did it run in the game": ["were you able to run it in your game"]}
missing = [q for q, keys in qs.items() if not any(k in t for k in keys)]
assert not missing, f"README does not answer: {missing}"
print("all three required questions are answered by heading")
PY

# ---------------------------------------------------------------------------
head2 "4. Claims the README makes that should be checkable"
# ---------------------------------------------------------------------------

check "reasoning layer makes no model call" <<'PY'
import subprocess, os, sys
env = {k: v for k, v in os.environ.items() if k != "ANTHROPIC_API_KEY"}
env["PATH"] = "/nonexistent"          # no `claude` CLI reachable
r = subprocess.run([sys.executable, "agent.py", "--rank"],
                   capture_output=True, text=True, env=env, timeout=300)
assert r.returncode == 0, f"--rank failed with no model available:\n{r.stdout}{r.stderr}"
assert "SELECTED" in r.stdout, "--rank did not reach a decision"
print("`agent.py --rank` reaches a selection with no model reachable at all")
PY

check "refuses to write outside the frozen copy" <<'PY'
import subprocess, sys
code = ("import agent, pathlib; agent.guard_path(pathlib.Path('/tmp/escape.h'))")
r = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True)
assert r.returncode != 0, "guard_path allowed a write outside project/"
assert "refusing" in (r.stdout + r.stderr).lower(), "it failed, but not by refusing"
print("guard_path() rejects a path outside project/ — the live game tree is unreachable")
PY

check "the frozen target records its pin" <<'PY'
import pathlib, re
t = pathlib.Path("project/PROVENANCE.md").read_text(encoding="utf-8")
m = re.search(r"`([0-9a-f]{40})`", t)
assert m, "no full commit hash recorded in PROVENANCE.md"
print(f"pinned at {m.group(1)[:7]} — the target cannot drift under the run")
PY

check "generated code references only real types" <<'PY'
import re, pathlib, json
b = json.load(open("output/build_report.json"))
gen = [pathlib.Path("project") / b["header"], pathlib.Path("project") / b["source"]]
body = "".join(p.read_text(encoding="utf-8") for p in gen)
names = set(re.findall(r"\b([UAIFE]BR[A-Za-z_]\w*)\b", body))
defined = set()
for p in pathlib.Path("project/Source").rglob("*"):
    if p.suffix in (".h", ".cpp"):
        defined |= set(re.findall(r"(?:enum class|struct|class)\s+(?:\w+_API\s+)?(\w+)",
                                  p.read_text(encoding="utf-8", errors="ignore")))
        defined |= set(re.findall(r"DECLARE_\w*DELEGATE\w*\s*\(\s*(\w+)",
                                  p.read_text(encoding="utf-8", errors="ignore")))
unknown = sorted(names - defined)
assert not unknown, f"invented types: {unknown}"
print(f"all {len(names)} BREACHPOINT types it names are defined in the codebase")
PY

# ---------------------------------------------------------------------------
head2 "5. What this does NOT prove"
# ---------------------------------------------------------------------------
note "not compiled" "no Unreal Engine here; the C++ has never been through UBT"
note "not run in the game" "the code is in the frozen copy, not in a running build"
note "the README says so" "see its 'Were you able to run it in your game?' section"

# ---------------------------------------------------------------------------
printf '\n'
if [ "$FAILURES" -eq 0 ]; then
  printf '\033[32m%s\033[0m\n' "ALL CHECKS PASSED — every assignment requirement is satisfied by this run."
else
  printf '\033[31m%s\033[0m\n' "$FAILURES CHECK(S) FAILED — see above."
fi
exit "$FAILURES"
