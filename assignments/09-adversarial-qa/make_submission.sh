#!/usr/bin/env bash
# Build the Assignment #9 zip.
#
# Stages this folder plus the deliverables that live in the game tree — the agent's C++
# (into game-code/, so the zip carries the Agent Code deliverable standalone) and the
# run ticket (provenance for how the report was produced). Then it runs ./verify.sh
# from the staged copy and refuses to ship a package that fails its own verification —
# which also means it refuses to build before TICKET_BN24's PIE run has landed a report.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME="$HERE/../../breachpoint"
NAME="BREACHPOINT-adversarial-qa"
OUT="$HERE/$NAME.zip"
STAGE="$(mktemp -d)/$NAME"

mkdir -p "$STAGE/report" "$STAGE/game-code"
cp "$HERE/README.md" "$HERE/verify.sh" "$STAGE/"
cp "$HERE"/report/aqa_report_*.json "$STAGE/report/" 2>/dev/null \
  || { echo "error: no report/aqa_report_*.json — run TICKET_BN24 first" >&2; exit 1; }
cp "$GAME/Source/BreachpointNext/QA/BNAdversarialAgent.h" \
   "$GAME/Source/BreachpointNext/QA/BNAdversarialAgent.cpp" "$STAGE/game-code/"
cp "$GAME/docs/tickets/TICKET_BN24_ADVERSARIAL_QA_RUN.md" "$STAGE/game-code/"

echo "  running verify.sh from the staged copy…"
if ( cd "$STAGE" && bash verify.sh >/tmp/stage9.$$.log 2>&1 ); then
  grep -E "ALL CHECKS" /tmp/stage9.$$.log | sed 's/^/    /'
  rm -f /tmp/stage9.$$.log
else
  echo "error: staged package failed ./verify.sh — not zipping" >&2
  sed 's/^/    /' /tmp/stage9.$$.log >&2; rm -f /tmp/stage9.$$.log; exit 1
fi

rm -f "$OUT"
( cd "$(dirname "$STAGE")" && zip -qr "$OUT" "$(basename "$STAGE")" )
echo "built $OUT ($(du -h "$OUT" | cut -f1), $(unzip -l "$OUT" | tail -1 | awk '{print $2}') files) — verified from the staged copy"
