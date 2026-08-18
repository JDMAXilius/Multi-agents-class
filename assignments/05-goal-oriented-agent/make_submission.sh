#!/usr/bin/env bash
# Build the standalone Assignment #5 zip.
#
# Unlike #3 and #4, this one has nothing to go and fetch: the agent's target is
# already committed inside the folder (`project/`, pinned — see
# project/PROVENANCE.md), and the two agent modules import nothing outside the
# standard library. So the zip is the folder, minus build noise.
#
# What it still does is refuse to ship a package that does not run. The staged
# copy has its generated C++ deleted and the agent re-run from inside the
# staging directory, with the repo unreachable from there — which is exactly the
# situation a grader is in after unzipping. If the agent cannot reproduce
# BRSpotterSubsystem from that state, no archive is written.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NAME="BREACHPOINT-goal-oriented-agent"
OUT="$HERE/$NAME.zip"
STAGE="$(mktemp -d)/$NAME"

mkdir -p "$STAGE"

# Copy everything except build noise and any previous archive.
( cd "$HERE" && find . \
    -name '__pycache__' -prune -o \
    -name '*.pyc' -prune -o \
    -name '*.zip' -prune -o \
    -type f -print0 \
  | while IFS= read -r -d '' f; do
      mkdir -p "$STAGE/$(dirname "$f")"
      cp "$f" "$STAGE/$f"
    done )

GENERATED="Source/Breachpoint/Telemetry/BRSpotterSubsystem"
[ -f "$STAGE/project/$GENERATED.h" ] || {
  echo "error: the generated feature is missing from the source folder — run" >&2
  echo "       'python3 agent.py' before packaging" >&2; exit 1; }

# Prove the package runs, from the staged copy, with its outputs removed first
# so "it ran" cannot be satisfied by the files we just copied in.
( cd "$STAGE" \
  && rm -f "project/$GENERATED.h" "project/$GENERATED.cpp" \
  && python3 agent.py >/dev/null 2>&1 \
  && [ -s "project/$GENERATED.h" ] \
  && [ -s "project/$GENERATED.cpp" ] ) \
  || { echo "error: staged package failed its own replay — not zipping" >&2; exit 1; }

# The re-run also rewrites output/; confirm the run reported a clean verify
# rather than silently landing a file with problems.
python3 - "$STAGE" <<'PY'
import json, sys, pathlib
report = json.loads((pathlib.Path(sys.argv[1]) / "output" / "build_report.json")
                    .read_text(encoding="utf-8"))
if report.get("verify_problems"):
    sys.exit(f"error: staged replay landed with problems: {report['verify_problems']}")
if not report.get("gap_closed_on_rescan"):
    sys.exit("error: staged replay did not close the gap it selected")
print(f"  staged replay: {report['class']} written, verify clean, gap closed")
PY

rm -f "$OUT"
( cd "$(dirname "$STAGE")" && zip -qr "$OUT" "$(basename "$STAGE")" )
echo "built $OUT ($(du -h "$OUT" | cut -f1), $(unzip -l "$OUT" | tail -1 | awk '{print $2}') files) — replay verified from the staged copy"
