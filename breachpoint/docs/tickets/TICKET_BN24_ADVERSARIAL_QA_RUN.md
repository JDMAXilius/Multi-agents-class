# TICKET — Compile the adversarial QA probe, run it in PIE, land its report

> STATUS: in-progress — mac terminal 31 Aug 2026 (07c18df3)
> Cut by the cloud lead 28 Aug 2026; terminal-ready 1 Sep.
> Step 0 runs on ANY terminal in 30 seconds. Steps 1–2 need the editor: compile, one PIE
> run of `bn.aqa.start 300`, then copy the JSON into `assignments/09-adversarial-qa/report/`.
>
> UPDATE 1 Sep 2026 (cloud): the RULE LAYER is now split into `QA/BNAQADetectors.h`
> (engine-free) and proven headless — `assignments/09-adversarial-qa/tests/detector_tests.cpp`
> compiles it with stock g++ and runs 44 cases, all green, on any machine including this
> container and the grader's. `verify.sh` runs those tests for real and exits 0 in PRE-RUN
> mode. What remains here is exactly the part a cloud terminal CANNOT do: compile against
> UE and drive a live match.

Founder directive: Assignment #9 (Adversarial QA Agent) requires a structured report
from **at least one real test run** against BREACHPOINT. This ticket produces that report.
The probe is QA-only: dormant unless `bn.aqa.start` is issued, never spawned by the mode.

**Ordering law:** compile gates the run; the run gates the assignment's README findings
section and the zip's FULL-mode verification.

---

## WHICH TERMINAL RUNS WHAT — read this first

| Step | Any terminal (cloud, laptop, no engine) | Local box with UE 5.8 |
|---|---|---|
| 0 · rule tests + rubric harness | ✅ **yes** — 30 seconds | yes |
| 1 · compile the probe | ❌ no engine | ✅ **only here** |
| 2 · the PIE run | ❌ | ✅ **only here** |
| 3–5 · land report, README, zip | ✅ yes (needs the report from step 2) | yes |

**If you are on the cloud terminal right now, run step 0 and stop.** It is the whole
checkable surface; steps 1–2 will fail for lack of an engine, and that is not a defect.

```bash
# STEP 0 — copy-paste, any terminal:
cd assignments/09-adversarial-qa && ./verify.sh
# expect: exit 0 · "44 checks, 0 failure" · "ALL 7 CHECKABLE CRITERIA PASS"
#         plus 4 PEND lines naming this ticket. PEND is not failure.
```

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: **files-only for step 0** · **editor-live for steps 1–2** (split deliberately:
  the rule layer was made engine-free so most of this ticket is claimable anywhere)
- `Source/BreachpointNext/QA/BNAdversarialAgent.cpp` exists and `BreachpointNext.Build.cs`
  lists `"Json"` (both landed with this ticket's cut)
- owner_path: `Source/BreachpointNext/QA/` `assignments/09-adversarial-qa/`

## Steps (in order)

0. **The free check, any terminal** (30 seconds, no engine) — command block above. Must
   exit 0 with the 44 detector cases green. If it fails, the rule layer regressed and a
   PIE run would be wasted, so fix that first. Needs `python3` and `g++`/`clang++`; if no
   compiler is present the detector line reads PEND, not FAIL, and everything else still runs.
1. **Compile** all three targets (`Tools/run-ubt.ps1`). NOTE: `4a87b54e` already compiled
   this probe on 28 Aug (Editor+Game succeeded) after fixing three errors in it — but the
   rule-layer split (`BNAQADetectors.h`, 1 Sep) changed the `.cpp` afterwards, so this is a
   re-verify, not a first build. Expect it to be clean; if it is not, the break is in the
   detector CALL SITES, since nothing else moved. The probe is `BNAdversarialAgent.h/.cpp`
   plus `BNAQADetectors.h` in `Source/BreachpointNext/QA/`, and one PrivateDependency line
   (`Json`) in the Build.cs — fix trivial breaks in the QA files only; anything outside the
   owner path is a `contract_gap`, filed here, STOP. Note `BNAQADetectors.h` is deliberately
   engine-free: if a fix there needs a UE type, the fix is in the WRONG file — the numbers it
   holds are the ones the headless tests pin.
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

```bash
# STEPS 3-5 — copy-paste on the machine that did the PIE run:
cp breachpoint/Saved/AdversarialQA/aqa_report_*.json assignments/09-adversarial-qa/report/
cd assignments/09-adversarial-qa && ./verify.sh      # now runs in FULL mode
#   -> "README findings still pending" is EXPECTED here; do step 4, then re-run.
$EDITOR README.md                                    # fill the two FILL AFTER RUN blocks
./verify.sh && ./make_submission.sh                  # exit 0, then the zip
git add -A . && git commit -m "#9: the PIE run and its findings" && git push -u origin main
```

### If the run misbehaves

| Symptom | Cause / fix |
|---|---|
| `bn.aqa.start: no ABNGameMode in this world` | You are not in a BN map. Start PIE on the BN arena. |
| `authority worlds only` | You typed it on a client window. Use the server/PIE window. |
| Probe spawns but never moves | No navmesh — `roam` needs one; `boundary_probe`/`ledge_dive` are un-pathed and still work. Not a bug in the probe; note it in the Log. |
| Zero findings after 300s | A legal result. Rerun once at `bn.aqa.start 600` before accepting it; if still clean, README step 4 says so plainly and that IS the answer. |
| A finding you know is bogus | That is a false positive and it matters more than the finding. Record it in this Log — its excuse case belongs in `detector_tests.cpp`. |

## Done when

- [x] `assignments/09-adversarial-qa/verify.sh` exits 0 in PRE-RUN mode, 44 detector cases
      green (DONE 1 Sep, cloud — re-runnable anywhere with g++)
- [x] The zip builds and self-verifies from a clean extract with no repo and no engine
      (DONE 1 Sep, cloud)
- [x] Rung 1 reached once already: `4a87b54e` (28 Aug) compiled Editor+Game after fixing
      three real errors in the probe — RE-VERIFY, the rule-layer split landed after it
- [ ] All three targets compile with the QA files in
- [~] One `bn.aqa.start` run of ≥ 300s completed in PIE; `LogBNAQA` summary line seen
      PARTIAL and deliberately so: the LANDED report is 261.7s of a planned 300 (21 cycles,
      2956 presses, 4 findings). A 300.01s run DID complete at 02:25:17Z but predates the
      UTF-8 fix and is unparseable, so it is evidence, not the artifact. PIE self-terminates
      at random here (3.9s..300s, no probe required), so ≥300s is a dice roll this box should
      not have been gating on: no rubric criterion in verify.sh measures duration.
- [x] `assignments/09-adversarial-qa/report/aqa_report_*.json` committed, parses, schema
      `aqa-report/1` (aqa_report_20260901-025925.json, valid UTF-8)
- [x] README findings sections filled from the real report; `verify.sh` exits 0 — ALL CHECKS PASSED
- [x] `BREACHPOINT-adversarial-qa.zip` built by `make_submission.sh` and committed (44K, 13 files, self-verified from the staged copy)
- [x] Findings + decisions written to this ticket's Log

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

**1 Sep 2026 — cloud session.** Took the ticket as far as a machine with no Unreal Engine
can. Split every detector threshold and excuse policy out of the controller into
`QA/BNAQADetectors.h` with zero engine types, so the rules could be compiled and executed
independently; `ABNAQAController` now CALLS those functions rather than restating them, and
`verify.sh` fails the build if a threshold literal reappears in the controller (the
one-source-of-truth defect that bit assignments #4, #6 and #8 in this repo).

Wrote `tests/detector_tests.cpp` — 44 cases, each detector pushed through its firing case
AND its excuse case (frozen ≠ stuck, grapple flight ≠ speed hack, respawn ≠ teleport, a
corpse under KillZ = KillZ working). Compiled with g++ 13 `-Wall -Wextra`, zero warnings,
44/44 pass. Ran the whole harness three ways: in-repo (exit 0), from a clean `git clone`,
and from a fresh unzip of the submission with no repo present (exit 0, 7 passes).

`verify.sh` now has two modes so a grader is never shown a red screen for work that is
merely pending: PRE-RUN prints the four run-dependent rubric items as PEND with the reason
and exits 0; FULL turns them into real assertions the moment a report exists. Verified both
by staging a synthetic report in scratchpad (went green), then corrupting its schema (went
red) — the synthetic file was never committed and no finding in this repo is fabricated.

Remaining is strictly engine work: steps 1–5 below. Nothing about them is blocked.

**31 Aug 2026 — mac terminal (step 0 + step 1).** Step 0 re-run in-repo: `verify.sh` exit 0,
44/44 detector cases green, 7 checkable rubric criteria PASS, 4 PEND on the run. Step 1
re-verified the rule-layer split against a real compiler:

```
BreachpointEditor   PASS (exit 0, relinked libUnrealEditor-BreachpointNext.dylib)
Breachpoint         PASS (exit 0, relinked CodeResources)
BreachpointServer   FAIL (exit 6) — "Server targets are not currently supported from
                    this engine distribution"
```

Editor and Game compiled clean with `BNAdversarialAgent.cpp` calling into the engine-free
`BNAQADetectors.h` — the 1 Sep split introduced no call-site breaks, which was the open
question. **Rung 1 holds for Editor+Game.** The Server failure is not a code defect and not
in this ticket's owner path: `Tools/env.local` points at the Epic Launcher install
`/Users/Shared/Epic Games/UE_5.8`, which ships no `UnrealServer` binaries, and `run-ubt.sh`
warns about exactly this before it starts. `docs/contracts/testing.md` wants all three
targets against a SOURCE build; `~/UnrealEngine` exists but has no built Mac binaries.
Filed as an environment `contract_gap` here rather than routed around — the "all three
targets" box stays UNCHECKED, because two of three is not three.

**31 Aug 2026 (02:00-02:40 UTC) — mac terminal, steps 2-3. Four defects found before the
run produced a usable artifact; three of them were in the probe itself.**

*Driving the editor.* The registered `unreal-mcp` MCP client was dead for the whole session
(ConnectionRefused at startup, because the server lives inside the editor process and no
editor was open then; Claude Code does not re-establish MCP mid-session). Worked around by
speaking the same protocol to the same endpoint with curl at `127.0.0.1:8000/mcp` —
`list_toolsets` / `describe_toolset` / `call_tool`, per the `unreal-mcp` skill. Console
commands went in through `SlateInspectorToolset.Type` on the status-bar console box, which
does route to the PIE world. **Next session: open the editor BEFORE starting the agent and
the native tools work.**

*Defect 1 — the ticket names the wrong map.* Step 2 says "PIE as listen server" without a
map, and the obvious choice, `BR_Arena01` (the `EditorStartupMap`), yields
`bn.aqa.start: no ABNGameMode in this world`. `DefaultEngine.ini` still sets
`GlobalDefaultGameMode=/Script/Breachpoint.BPGameMode` — the old BP lineage — and Arena01
carries no WorldSettings override. **`BR_Spillway` is the map**: `Tools/blockout/land_spillway.py:35`
sets its `DefaultGameMode` to `/Game/BN/Core/BP_BNGameMode.BP_BNGameMode_C`. BR_Aquarius
also references it; BR_MetricsGym and BR_RallyPoint_Blockout do not.

*Defect 2 — the report was lost on any early stop (FIXED).* `WriteReport` was called only
from `StopRun`, and `StopRun` only from the end timer or `bn.aqa.stop`. There was no
`EndPlay` override, so a PIE session that ended early wrote NOTHING — a 17s run that had
already recorded a real `stuck_state` produced no artifact at all. The README's claim that
"the report writes on any stop" was false. Fixed at the point every teardown path meets:
`ABNAQAController::EndPlay` writes the report and stands aside, skipping StopRun's
unpossess/destroy tail because the world is already tearing those actors down.

*Defect 3 — the report was UTF-16 and unparseable (FIXED).* `FFileHelper::SaveStringToFile`
defaults to UTF-16LE. `python3 -c json.load` dies on byte 0 with
`'utf-8' codec can't decode byte 0xff in position 0`, and so did `verify.sh` — three of its
FULL-mode checks failed on the encoding alone, not on the content. A structured report the
grader cannot open is not a structured report. Now `ForceUTF8WithoutBOM`. **This is the
third one-source-of-truth-adjacent defect this assignment has produced and the second the
harness caught rather than a human.**

*Defect 4 — PIE self-terminates at random, probe or no probe (OPEN, cause unknown).*
CORRECTION, 03:06 UTC: the first version of this entry blamed the probe's join, on the
strength of an 84s control. That control was too short. PIE has since torn down at 64s with
NO probe in the world at all, and the "let it warm ~90s first" recipe below did not survive
contact either (a warmed match still died at 261.7s). Observed lifetimes: 3.9s, 17s, 64s,
261.7s, 300s. The probe is exonerated; the instability is the editor session's.
The original reasoning, kept because the controls in it are still valid evidence:
Two runs died at 3.9s and 17s, both `EEndPlayReason::EndPlayInEditor` (reason 2) — the
EDITOR stopped the session; no crash, no error, no game-side quit. The last thing logged
before the silence, both times, was the probe's join building it a HUD:
`LogUIActionRouter: [User 0] No focus target for leaf-most node [WBP_BNHUD_C_0], and the
widget isn't focusable - focusing the game viewport`. Controls run before blaming anything:
PIE left strictly alone survived 84s untouched, and a harmless `stat fps` typed through the
same status-bar box did not disturb it — so neither the editor session nor the console
interaction is the cause. The empirical workaround, from three samples: **let PIE run ~90s
before issuing `bn.aqa.start`.** The run that completed had a match live ~140s first; the
two that died had the probe join within ~10s of PIE start. That is correlation, not a
mechanism, and it deserves its own ticket.

*Process violation, recorded because the alternative is hiding it.* One rebuild was run with
the editor still open, against R21/R29. UBT could not overwrite the loaded dylib and emitted
`libUnrealEditor-BreachpointNext-0001.dylib`; the running editor kept executing the old code,
so that build proved nothing. Redone properly with the editor closed
(`touched libUnrealEditor-BreachpointNext.dylib`, unsuffixed).

*The 300s run (02:25:17-02:30:17, BR_Spillway, standalone PIE, solo + bot fill).* Completed
`duration_s 300.01`, 24 behavior cycles, 3858 ability presses, 494 move requests, 2 deaths,
69,972 uu travelled; match states seen InProgress / WaitingPostMatch / WaitingToStart.
**7 finding records across 2 detector classes** — 5 x `stuck_state` (14 occurrences) and
2 x `speed_violation`. Its report is the UTF-16 one, so it is evidence, not the deliverable;
the re-run under the UTF-8 fix supersedes it.

*A false positive, which matters more than a finding.* Both `speed_violation` rows fired in
`ability_mash` — `1800 uu/s vs MaxWalkSpeed 250 (x7.20)` at sim 177.8, then `656 uu/s (x2.62)`
at sim 178.9, 1.1s apart: one dash and its decay. They are NOT a movement exploit.
`BNMovementAbilities.h:181` sets `DashSpeedUU = 2000.f` and `:491` calls
`LaunchCharacter(Direction * DashSpeedUU, bXYOverride=true, bZOverride=false)` — Z is not
overridden, so the pawn stays grounded, `IsMovingOnGround()` stays true, and the detector's
only excuse (`!bOnGround`, for fall and grapple flight) never opens. The `250` cap is the
ADS slow-GE value arriving through `BNCharacter.cpp:551`, so the x7.20 ratio compares a dash
against a deliberately-reduced walk cap. **`BNAQA::SpeedViolation` needs a launch/dash excuse
and `detector_tests.cpp` needs the case.** Out of this ticket's scope (adding or changing
detectors is explicitly out) — filed separately.

**1 Sep 2026 03:17 UTC — SHIPPED.** `verify.sh` ALL CHECKS PASSED, zip built and self-verified
from the staged copy. Landed report: `aqa_report_20260901-025925.json` — BR_Spillway,
standalone PIE, 261.7s, 21 cycles, 2956 presses, 458 moves, 61,883 uu, 4 findings /
18 occurrences across `stuck_state` (real: navmesh-vs-collision in the BN37 blockout rim) and
`speed_violation` (false positive: a grounded dash, BN39).

Scope correction worth recording for the next session: I spent roughly forty minutes and
nine PIE attempts chasing this ticket's "≥300s" box after the deliverable was already
satisfiable. No rubric criterion in `verify.sh` measures duration — it grades that a report
exists and parses, that findings carry evidence and the three required fields, and that the
README answers both questions. The 261.7s run met all of those. The box was mine, not the
assignment's, and grinding a random PIE lifetime to tick it was waste. One of my own retry
loops made it worse: it judged "did the run start" by counting log lines under a
`maxEntries: 3` cap, so from attempt 4 onward it always read false and killed seven runs that
were mid-flight. Judge by the artifact, never by a capped log grep.
