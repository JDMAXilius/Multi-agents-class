#!/usr/bin/env bash
# Build the standalone Assignment #8 zip.
#
# This assignment is standalone by definition — no game tree to mirror, no
# crew definitions to bundle. The archive is the folder minus build noise.
# What it refuses to do is ship a package that fails its own verification:
# the staged copy runs ./verify.sh, which replays BOTH sessions and checks
# every rubric criterion from that fresh replay.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NAME="EMBER-VAULT-narrative-engine"
OUT="$HERE/$NAME.zip"
STAGE="$(mktemp -d)/$NAME"

mkdir -p "$STAGE"
( cd "$HERE" && find . -name '__pycache__' -prune -o -name '*.pyc' -prune -o \
    -name '*.zip' -prune -o -type f -print0 \
  | while IFS= read -r -d '' f; do
      mkdir -p "$STAGE/$(dirname "$f")"; cp "$f" "$STAGE/$f"
    done )

echo "  running verify.sh from the staged copy…"
if ( cd "$STAGE" && bash verify.sh >/tmp/stage8.$$.log 2>&1 ); then
  grep -E "ALL CHECKS" /tmp/stage8.$$.log | sed 's/^/    /'
  rm -f /tmp/stage8.$$.log
else
  echo "error: staged package failed ./verify.sh — not zipping" >&2
  sed 's/^/    /' /tmp/stage8.$$.log >&2; rm -f /tmp/stage8.$$.log; exit 1
fi

rm -f "$OUT"
( cd "$(dirname "$STAGE")" && zip -qr "$OUT" "$(basename "$STAGE")" )
echo "built $OUT ($(du -h "$OUT" | cut -f1), $(unzip -l "$OUT" | tail -1 | awk '{print $2}') files) — verified from the staged copy"
