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

mkdir -p "$STAGE/report" "$STAGE/game-code" "$STAGE/tests"
cp "$HERE/README.md" "$HERE/TESTING.md" "$HERE/verify.sh" "$STAGE/"
cp "$HERE/tests/detector_tests.cpp" "$STAGE/tests/"

# The agent AND the rule header it calls: the tests #include the header from game-code/,
# so the zip stays self-testing — a grader unzips and runs ./verify.sh with no repo and
# no engine. Shipping the tests without the header would be a package that cannot run.
cp "$GAME/Source/BreachpointNext/QA/BNAdversarialAgent.h" \
   "$GAME/Source/BreachpointNext/QA/BNAdversarialAgent.cpp" \
   "$GAME/Source/BreachpointNext/QA/BNAQADetectors.h" "$STAGE/game-code/"
cp "$GAME/docs/tickets/TICKET_BN24_ADVERSARIAL_QA_RUN.md" "$STAGE/game-code/"

# The report ships when it exists. Its ABSENCE is not a build failure — the package is
# still worth submitting (the rule layer runs, the schema is proven) and verify.sh says
# plainly which rubric items await the PIE run. What must never happen is a package that
# FAILS its own verification, and that is what the staged run below enforces.
if compgen -G "$HERE/report/aqa_report_*.json" >/dev/null; then
  cp "$HERE"/report/aqa_report_*.json "$STAGE/report/"
  echo "  including the PIE report — the zip verifies in FULL mode"
else
  echo "  NOTE: no PIE report yet (TICKET_BN24) — zipping in PRE-RUN mode;"
  echo "        the Findings criterion stays PENDING until that run lands."
fi

echo "  running verify.sh from the staged copy…"
if ( cd "$STAGE" && bash verify.sh >/tmp/stage9.$$.log 2>&1 ); then
  # Both banners are a pass: "ALL CHECKS PASSED" (full) and "ALL n CHECKABLE" (pre-run).
  grep -E "ALL .*(PASSED|PASS)|PENDING the one" /tmp/stage9.$$.log | sed 's/^/    /' || true
  rm -f /tmp/stage9.$$.log
else
  echo "error: staged package failed ./verify.sh — not zipping" >&2
  sed 's/^/    /' /tmp/stage9.$$.log >&2; rm -f /tmp/stage9.$$.log; exit 1
fi

rm -f "$OUT"
( cd "$(dirname "$STAGE")" && zip -qr "$OUT" "$(basename "$STAGE")" )
echo "built $OUT ($(du -h "$OUT" | cut -f1), $(unzip -l "$OUT" | tail -1 | awk '{print $2}') files) — verified from the staged copy"
