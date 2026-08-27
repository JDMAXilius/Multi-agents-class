#!/usr/bin/env bash
# Build the standalone Assignment #6 zip.
#
# Everything the pipeline needs at runtime is already in this folder — the
# recording replays the run and the evaluator is pure Python — so the archive is
# the folder minus build noise. What it refuses to do is ship a package that
# does not run: the staged copy has output/ deleted and the pipeline re-run from
# inside the staging directory, which is a grader's situation after unzipping.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NAME="BREACHPOINT-ger-pipeline"
OUT="$HERE/$NAME.zip"
STAGE="$(mktemp -d)/$NAME"

mkdir -p "$STAGE/game"

# The two game files the pipeline reads: the GDD whose rule it enforces, and the
# shipped table it takes the house voice from. A zip has no game tree, so they
# travel with it. Copied, never edited — the repo stays the one place they live.
GAME="$HERE/../../breachpoint"
[ -d "$GAME" ] || { echo "error: no game tree at $GAME" >&2; exit 1; }
cp "$GAME/BREACHPOINT-GDD-VERTICAL-SLICE.md" "$STAGE/game/"
cp "$GAME/Content/Data/DT_SpotterLines.csv" "$STAGE/game/"

( cd "$HERE" && find . -name '__pycache__' -prune -o -name '*.pyc' -prune -o \
    -name '*.zip' -prune -o -type f -print0 \
  | while IFS= read -r -d '' f; do
      mkdir -p "$STAGE/$(dirname "$f")"; cp "$f" "$STAGE/$f"
    done )

( cd "$STAGE" && rm -rf output && python3 ger.py >/dev/null 2>&1 \
  && [ -s output/DT_SpotterLines_TeamEvents.csv ] ) \
  || { echo "error: staged package failed its own replay — not zipping" >&2; exit 1; }

# "It ran" is not the same as "it ran correctly": confirm the escalations the
# ReadMe describes actually happened, and that no accepted row fails the rules.
python3 - "$STAGE" <<'PY'
import json, sys, csv, pathlib
sys.path.insert(0, sys.argv[1])
from ger import evaluate
s = pathlib.Path(sys.argv[1])
r = json.loads((s / "output" / "run_report.json").read_text(encoding="utf-8"))
if r["escalated"] < 1:
    sys.exit("error: staged replay escalated nothing — the breaker never fired")
rows = list(csv.DictReader((s / "output" / "DT_SpotterLines_TeamEvents.csv")
                           .open(encoding="utf-8")))
bad = [x["RowName"] for x in rows if evaluate(x["Text"])]
if bad:
    sys.exit(f"error: landed rows fail their own evaluator: {bad}")
print(f"  staged replay: {len(rows)} accepted, {r['escalated']} escalated, "
      f"0 rows failing the rules")
PY

rm -f "$OUT"
( cd "$(dirname "$STAGE")" && zip -qr "$OUT" "$(basename "$STAGE")" )
echo "built $OUT ($(du -h "$OUT" | cut -f1), $(unzip -l "$OUT" | tail -1 | awk '{print $2}') files) — replay verified"
