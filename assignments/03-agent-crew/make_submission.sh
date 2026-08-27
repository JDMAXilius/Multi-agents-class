#!/usr/bin/env bash
# Build the standalone Assignment #3 zip.
#
# run_crew.py normally reads the crew's agent definitions from the planning repo
# (../../crew/.claude/agents/). A zip has no parent repo, so this bundles the four
# agents it uses into ./agents/ — the fallback location run_crew.py checks first.
# Without this step an extracted zip exits immediately with "agent definitions not
# found", which is the whole Working Crew score.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$HERE/../../crew/.claude/agents"
# The crew moved into the game repo after submission; fall back to it.
[ -f "$SRC/critic.md" ] || SRC="$HERE/../../breachpoint/.claude/agents"
OUT="$HERE/BREACHPOINT-agent-crew.zip"
STAGE="$(mktemp -d)/BREACHPOINT-agent-crew"

[ -d "$SRC" ] || { echo "error: crew agents not found at $SRC" >&2; exit 1; }

mkdir -p "$STAGE/agents/curators" "$STAGE/output"
cp "$HERE/run_crew.py" "$HERE/README.md" "$HERE/recording.json" "$STAGE/"
cp "$HERE"/output/* "$STAGE/output/"
cp "$SRC/critic.md" "$SRC/builder.md" "$SRC/verifier.md" "$STAGE/agents/"
cp "$SRC/curators/tuning-curator.md" "$SRC/curators/arena-architect.md" "$STAGE/agents/curators/"

# Prove the package runs before shipping it: replay from the staged copy, with the
# planning repo unreachable, exactly as a grader would see it.
( cd "$STAGE" && rm -f output/DT_Weapons.csv output/arena_manifest.json \
  && python3 run_crew.py >/dev/null 2>&1 \
  && [ -s output/DT_Weapons.csv ] && [ -s output/arena_manifest.json ] ) \
  || { echo "error: staged package failed its own replay — not zipping" >&2; exit 1; }

rm -f "$OUT"
( cd "$(dirname "$STAGE")" && zip -qr "$OUT" "$(basename "$STAGE")" )
echo "built $OUT ($(du -h "$OUT" | cut -f1)) — replay verified from the staged copy"
