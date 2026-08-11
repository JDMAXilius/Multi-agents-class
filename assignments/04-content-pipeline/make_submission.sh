#!/usr/bin/env bash
# Build the standalone Assignment #4 zip.
#
# The knowledge base is the LIVE game repo (breachpoint/BREACHPOINT-GDD-*.md and
# Content/Data/*.csv) — that is the point of the assignment, and it is also what
# breaks in a zip, which has no parent repo. So this mirrors the eight KB sources
# into ./kb/, which rag.py falls back to. Citations keep their repo-relative
# paths in both layouts, so a line reference means the same thing either way.
#
# gaps.py additionally greps Source/Breachpoint/*.cpp, which cannot be mirrored
# sensibly. In the zip layout the pipeline says plainly that it did NOT re-prove
# the gaps and reads output/gap_report.json instead.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
OUT="$HERE/BREACHPOINT-content-pipeline.zip"
STAGE="$(mktemp -d)/BREACHPOINT-content-pipeline"

mkdir -p "$STAGE/kb" "$STAGE/output"
cp "$HERE"/run_pipeline.py "$HERE"/rag.py "$HERE"/gaps.py "$HERE"/README.md \
   "$HERE"/recording.json "$HERE"/recording_naive.json "$STAGE/"
cp -r "$HERE"/output/. "$STAGE/output/"

# Mirror the knowledge base, reading the source list from rag.py so the zip can
# never fall out of sync with what the pipeline actually indexes.
python3 - "$REPO" "$STAGE/kb" <<'PY'
import shutil, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
repo, dest = Path(sys.argv[1]), Path(sys.argv[2])
sys.path.insert(0, str(repo / "assignments" / "04-content-pipeline"))
import rag
for rel, _canon, _w in rag.KB_SOURCES:
    src = repo / rel
    if not src.exists():
        sys.exit(f"error: KB source missing from the repo: {rel}")
    shutil.copy2(src, dest / Path(rel).name)
print(f"  mirrored {len(rag.KB_SOURCES)} knowledge-base sources into kb/")
PY

# Prove the package runs before shipping it: replay from the staged copy with the
# game repo unreachable, exactly as a grader sees it. Deleting the outputs first
# means "it ran" cannot be satisfied by the files we just copied in.
( cd "$STAGE" \
  && rm -f output/DT_SpotterLines_Additions.csv output/DT_CoachLines.csv \
           output/DT_BotCallsigns.csv \
  && python3 run_pipeline.py >/dev/null 2>&1 \
  && [ -s output/DT_SpotterLines_Additions.csv ] \
  && [ -s output/DT_CoachLines.csv ] \
  && [ -s output/DT_BotCallsigns.csv ] ) \
  || { echo "error: staged package failed its own replay — not zipping" >&2; exit 1; }

rm -f "$OUT"
( cd "$(dirname "$STAGE")" && zip -qr "$OUT" "$(basename "$STAGE")" -x '*__pycache__*' )
echo "built $OUT ($(du -h "$OUT" | cut -f1)) — replay verified from the staged copy"
