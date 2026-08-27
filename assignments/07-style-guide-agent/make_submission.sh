#!/usr/bin/env bash
# Build the standalone Assignment #7 zip.
#
# Everything the pipeline needs at runtime is in this folder — the recording
# replays the run, the evaluator is pure Python — so the archive is the folder
# minus build noise. What it refuses to do is ship a package that does not run:
# the staged copy has output/ deleted and the pipeline re-run from inside the
# staging directory, which is a grader's situation after unzipping.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NAME="BREACHPOINT-style-guide-agent"
OUT="$HERE/$NAME.zip"
STAGE="$(mktemp -d)/$NAME"

mkdir -p "$STAGE/game"

# The three game files the pipeline reads: the manifest whose landmarks are the
# slots (and rule 1's vocabulary), the GDD the canon rules cite, and the shipped
# table the voice is measured from. Copied, never edited — the repo stays the
# one place they live.
GAME="$HERE/../../breachpoint"
[ -d "$GAME" ] || { echo "error: no game tree at $GAME" >&2; exit 1; }
cp "$GAME/Content/Data/arena_manifest.json" "$STAGE/game/"
cp "$GAME/BREACHPOINT-GDD-VERTICAL-SLICE.md" "$STAGE/game/"
cp "$GAME/Content/Data/DT_SpotterLines.csv" "$STAGE/game/"

( cd "$HERE" && find . -name '__pycache__' -prune -o -name '*.pyc' -prune -o \
    -name '*.zip' -prune -o -type f -print0 \
  | while IFS= read -r -d '' f; do
      mkdir -p "$STAGE/$(dirname "$f")"; cp "$f" "$STAGE/$f"
    done )

# The staged copy must pass its own self-test (every rule fires + the breaker
# trips on a refiner that never complies) and its own replay.
( cd "$STAGE" && python3 style_agent.py --selftest >/dev/null 2>&1 ) \
  || { echo "error: staged package failed its own self-test — not zipping" >&2; exit 1; }
( cd "$STAGE" && rm -rf output && python3 style_agent.py >/dev/null 2>&1 \
  && [ -s output/ArenaIntelCards.csv ] ) \
  || { echo "error: staged package failed its own replay — not zipping" >&2; exit 1; }

# "It ran" is not "it ran correctly": the committed run must show the loop
# actually correcting (rejections that were then fixed), every landmark must
# have landed a card, and no accepted card may fail the very rules it shipped
# under. (The breaker is proven by the self-test above — this run's model
# happened to fix everything within budget, and the ReadMe says so.)
python3 - "$STAGE" <<'PY'
import json, sys, csv, pathlib
sys.path.insert(0, sys.argv[1])
from style_agent import evaluate
s = pathlib.Path(sys.argv[1])
r = json.loads((s / "output" / "run_report.json").read_text(encoding="utf-8"))
if r["rejections"] < 1 or r["refined_to_pass"] < 1:
    sys.exit("error: staged replay shows no correction — the loop demonstrated nothing")
rows = list(csv.DictReader((s / "output" / "ArenaIntelCards.csv")
                           .open(encoding="utf-8")))
if len(rows) != 7 - r["escalated"]:
    sys.exit(f"error: {len(rows)} cards for {7 - r['escalated']} non-escalated landmarks")
bad = [x["RowName"] for x in rows if evaluate(x["Card"])]
if bad:
    sys.exit(f"error: landed cards fail their own style guide: {bad}")
print(f"  staged replay: {len(rows)} cards, {r['rejections']} rejections, "
      f"{r['refined_to_pass']} refined to pass, 0 cards failing the rules")
PY

rm -f "$OUT"
( cd "$(dirname "$STAGE")" && zip -qr "$OUT" "$(basename "$STAGE")" )
echo "built $OUT ($(du -h "$OUT" | cut -f1), $(unzip -l "$OUT" | tail -1 | awk '{print $2}') files) — self-test + replay verified"
