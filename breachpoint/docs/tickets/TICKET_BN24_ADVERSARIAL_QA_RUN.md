# TICKET — Compile the adversarial QA probe, run it in PIE, land its report

> STATUS: open — cut by the cloud lead, 28 Aug 2026. Needs one local session with the
> editor: compile, one PIE run of `bn.aqa.start`, copy the JSON report into
> `assignments/09-adversarial-qa/report/`.

Founder directive: Assignment #9 (Adversarial QA Agent) requires a structured report
from **at least one real test run** against BREACHPOINT. The agent code is written
(`Source/BreachpointNext/QA/BNAdversarialAgent.*`) and is currently **WRITTEN, NOT
COMPILED** — honesty ladder rung 0. This ticket climbs it to "ran in PIE" and produces
the report the assignment cannot honestly ship without. The probe is QA-only: dormant
unless `bn.aqa.start` is issued, never spawned by the mode.

**Ordering law:** compile gates the run; the run gates the assignment's README findings
section and zip (`assignments/09-adversarial-qa/verify.sh` FAILS until a real report with
real findings-or-clean-result lands — that failure is the gate working, not a defect).

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: editor-live
- `Source/BreachpointNext/QA/BNAdversarialAgent.cpp` exists and `BreachpointNext.Build.cs`
  lists `"Json"` (both landed with this ticket's cut)
- owner_path: `Source/BreachpointNext/QA/` `assignments/09-adversarial-qa/`

## Steps (in order)

1. **Compile** all three targets (`Tools/run-ubt.ps1`). The probe is one new pair in
   `Source/BreachpointNext/QA/` plus one PrivateDependency line (`Json`) in the Build.cs —
   fix trivial breaks in the QA pair only; anything outside the owner path is a
   `contract_gap`, filed here, STOP.
2. **Run**: PIE as listen server (the usual solo PIE is fine — `MinPlayers=1`, the bot
   fill provides opposition). In the console:
   `bn.aqa.start 300` — five minutes. The probe joins through the same doors a bot does,
   cycles roam → boundary_probe → ledge_dive → grapple_abuse → ability_mash (~12s each),
   and presses on through any death, freeze, or respawn. Watch `LogBNAQA` — every finding
   logs as a Warning the moment it records. It stops and writes by itself; `bn.aqa.stop`
   ends a run early (the report writes on any stop).
3. **Land the report**: copy `Saved/AdversarialQA/aqa_report_*.json` to
   `assignments/09-adversarial-qa/report/` (keep the filename). Commit it.
4. **Fill the README findings**: `assignments/09-adversarial-qa/README.md` has two
   `<!-- FILL AFTER RUN -->` sections — what the agent found (from the real report,
   naming the mechanic/system per finding) and whether it surprised us. Write them from
   the JSON, not from memory. A CLEAN run (zero findings) is a legal result — the README
   then says the seven detector classes held, which is itself the answer.
5. **Verify + package**: `cd assignments/09-adversarial-qa && ./verify.sh` (exit 0), then
   `./make_submission.sh`. Commit the zip like #6–#8.

## Done when

- [ ] All three targets compile with the QA pair in
- [ ] One `bn.aqa.start` run of ≥ 300s completed in PIE; `LogBNAQA` summary line seen
- [ ] `assignments/09-adversarial-qa/report/aqa_report_*.json` committed, parses, schema
      `aqa-report/1`
- [ ] README findings sections filled from the real report; `verify.sh` exits 0
- [ ] `BREACHPOINT-adversarial-qa.zip` built by `make_submission.sh` and committed
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder compiles/fixes the QA pair · verifier runs steps 2–5 · critic reads the
  report before the README is written (a finding that names no mechanic is a finding
  against the report)
- Binary files this ticket OWNS: none
- Out of scope: fixing any bug the probe FINDS (each becomes its own ticket — this
  ticket ships the report, not the repairs); touching the mode, the bots, or the
  adapter; adding detectors (five behaviors + seven detector classes is the assignment's
  scope, deliberately)
- Honesty: after step 1 say "compiles"; after step 2 say "ran in PIE, listen, solo+bots".
  Neither claims multiplayer — the assignment doesn't need rung 3.

## Log
