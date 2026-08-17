#!/usr/bin/env bash
# Freeze the agent's target project.
#
# The agent must not read or write the live BREACHPOINT tree. That tree changes
# every day, and an agent whose inputs move under it produces a result nobody
# can reproduce — the run that graded well on Tuesday scans a different codebase
# on Wednesday. So the target is a pinned copy, committed inside this
# assignment, and `agent.py` is hard-wired to refuse any path outside it.
#
# What gets copied, and why exactly this:
#   GDD.md      the real vertical-slice GDD — the features the agent reads
#   Source/     every BR*.h — the DECLARATION surface, which is what decides
#               whether a unit exists. Bodies (.cpp) are not needed to answer
#               "is this built?", so they are not copied.
#   Source/     ...plus the four BR*Subsystem.{h,cpp} pairs, which ARE needed:
#               they are the style exemplars the generator is shown, and a
#               generator with no exemplar writes generic Unreal.
#
# Re-run this to re-pin against a newer commit. It rewrites PROVENANCE.md so the
# pin is always a fact on disk rather than something remembered.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
SRC="$REPO/breachpoint"
DEST="$HERE/project"

[ -d "$SRC/Source/Breachpoint" ] || { echo "error: no game tree at $SRC" >&2; exit 1; }

rm -rf "$DEST"
mkdir -p "$DEST/Source/Breachpoint"

cp "$SRC/BREACHPOINT-GDD-VERTICAL-SLICE.md" "$DEST/GDD.md"

# The tuning tables come too. They are small, and they are how the agent answers
# "are this feature's inputs already on disk?" — the term that stops it selecting
# a unit it would immediately stall on for want of data. That is the exact
# failure the previous attempt at this assignment hit.
mkdir -p "$DEST/Content/Data"
cp "$SRC"/Content/Data/*.csv "$DEST/Content/Data/"

# Preserve the folder layout — the agent reports gaps by folder, and a flattened
# copy would make every unit look like it lives in the same place.
( cd "$SRC/Source/Breachpoint" \
  && find . \( -name 'BR*.h' -o -name 'BR*Subsystem.cpp' \) -print0 \
  | while IFS= read -r -d '' f; do
      mkdir -p "$DEST/Source/Breachpoint/$(dirname "$f")"
      cp "$f" "$DEST/Source/Breachpoint/$f"
    done )

COMMIT="$(git -C "$REPO" rev-parse HEAD)"
SHORT="$(git -C "$REPO" rev-parse --short HEAD)"
WHEN="$(git -C "$REPO" log -1 --format=%cI HEAD)"
HEADERS="$(find "$DEST/Source" -name '*.h' | wc -l | tr -d ' ')"
TABLES="$(find "$DEST/Content" -name '*.csv' | wc -l | tr -d ' ')"
BODIES="$(find "$DEST/Source" -name '*.cpp' | wc -l | tr -d ' ')"
SIZE="$(du -sh "$DEST" | cut -f1)"

cat > "$DEST/PROVENANCE.md" <<EOF
# Provenance — this is a frozen copy, not the live project

Everything in this folder was copied from the BREACHPOINT game tree by
\`../freeze_project.sh\`. **Nothing here is authored for the assignment**, and
nothing here is edited by hand.

| | |
|---|---|
| Source | \`breachpoint/\` in this repo |
| Pinned commit | \`$COMMIT\` (\`$SHORT\`) |
| Commit date | $WHEN |
| Contents | \`GDD.md\` + $HEADERS headers + $BODIES subsystem bodies + $TABLES data tables |
| Size | $SIZE |

## Why a freeze at all

The live tree changes daily. An agent that scans it would give a different
answer every run, and the README's claims would rot within a day of being
written. Pinning the target makes the run reproducible: \`agent.py\` on this
folder gives the same perception, the same gaps and the same ranking today as
it did when the README was written.

## What this copy deliberately does NOT contain

- **\`.cpp\` bodies**, except the four \`BR*Subsystem\` pairs. Whether a unit
  exists is decided by its declaration; bodies would triple the size and change
  no answer.
- **Non-\`BR\` sources** — engine template leftovers (\`Variant_*\`,
  \`breachpoint*\`) are not units the architecture declares.
- **Any write path back to the live tree.** Code the agent generates lands in
  \`project/Source/\`, here, in the frozen copy. Porting it into the real game is
  a separate, manual step under a ticket — see the README.

## Re-pinning

\`\`\`
./freeze_project.sh      # re-copies from breachpoint/ and rewrites this file
\`\`\`

Re-pinning invalidates the committed run: \`output/\` and \`recording.json\`
describe the codebase as it was at the pin above.
EOF

echo "froze $HEADERS headers + $BODIES bodies ($SIZE) at $SHORT -> project/"
