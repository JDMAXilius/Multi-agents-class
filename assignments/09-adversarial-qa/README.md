# Assignment #9 — Adversarial QA Agent

**Course:** Multi-Agent AI for Game Development · **Student:** Juan Diego Lugo
**Target:** BREACHPOINT — my capstone, a UE 5.8 native-C++ 4v4 arena FPS. Unlike #8,
this assignment *requires* running against the capstone, so the agent code lives in the
game's own source tree and the report comes from a real Play-In-Editor run.

```bash
./verify.sh        # ← START HERE. Compiles and RUNS the agent's rule layer (44 cases,
                   #   no Unreal needed), then checks every rubric criterion.
```

> **Grading this without Unreal Engine?** Read **[`TESTING.md`](TESTING.md)** — it is the
> two-minute guide. Short version: `verify.sh` needs only Python 3 and a C++ compiler, and
> it *executes* the detector rules rather than just reading the code at you.

## Where everything is

| Piece | Location |
|---|---|
| Agent code (the deliverable) | `breachpoint/Source/BreachpointNext/QA/BNAdversarialAgent.h/.cpp` (a copy ships in this folder's zip under `game-code/`) |
| The rule layer — engine-free, executable | `breachpoint/Source/BreachpointNext/QA/BNAQADetectors.h` |
| Rule tests you can run right now | [`tests/detector_tests.cpp`](tests/detector_tests.cpp) — 44 cases, stock `g++` |
| Structured report (JSON) | `report/aqa_report_*.json` — written by the agent itself during the run |
| Grader's guide | [`TESTING.md`](TESTING.md) |
| The run instructions | `breachpoint/docs/tickets/TICKET_BN24_ADVERSARIAL_QA_RUN.md` |

**To reproduce with the editor:** open the project, PIE, and in the console:
`bn.aqa.start 300`. The probe spawns itself into the match, runs five minutes, and
writes `Saved/AdversarialQA/aqa_report_*.json`. `bn.aqa.stop` ends early — the report
writes on any stop. Without the editor, `./verify.sh` audits the committed report.

## The strategy — what the agent does, and what "broken" means

The agent is `ABNAQAController`, an AIController that joins the match **through the same
doors a bot does** (the mode's `GenericPlayerInitialization` → `RestartPlayer`, ability
presses on the PlayerState ASC via the same input-tag path a human's keyboard reaches).
It runs on the authority and presses server-gated paths — so anything it breaks, a
player could break.

**The loop** (a behavior switch every ~12s, acting at 4 Hz, all timer-driven — the
project bans gameplay Tick):

| Behavior | What it's trying to break |
|---|---|
| `roam` | baseline pathed wander — calibrates the detectors |
| `boundary_probe` | un-pathed walks into walls/edges on 8 compass headings, jumping at them |
| `ledge_dive` | sprints straight off the arena's farthest edge — tries to LEAVE the map |
| `grapple_abuse` | grapples the sky, the floor, and nothing; jumps mid-flight |
| `ability_mash` | every input tag, rapid, deliberately mis-combined (ADS+sprint+fire, crouch strobe, weapon-cycle mid-reload) |

Plus two **opportunistic modes** that outrank the cycle: whenever the pawn is dead or
match-frozen, it keeps pressing everything — leaning on the `State.Dead` and
`State.Match.Frozen` gates that are supposed to refuse every activation.

**"Broken" is defined precisely — seven detector classes**, each with its excuse policy
stated in code:

1. `fell_out_of_world_alive` — below KillZ and still alive past a 1s grace
2. `escaped_playable_space` — standing on ground outside the PlayerStart hull + margin
3. `stuck_state` — an accepted move request, alive, unfrozen, yet ~0 speed for 3s
4. `speed_violation` — ground speed > 1.75× what the movement component allows
5. `attribute_anomaly` — health/shield NaN, negative, or above its own max
6. `teleport_discontinuity` — more displacement in one 100ms sample than any legal mover
7. `acted_while_dead` / `input_during_freeze` — an ability activation landing through a
   state gate that must refuse it

**Every rule has an excuse policy, and both halves are tested.** The rules live in
`BNAQADetectors.h` with no engine types in them, so `tests/detector_tests.cpp` compiles
that exact header with a stock compiler and pushes all seven through the defect they catch
*and* the legitimate situation that looks identical: a frozen pawn is not "stuck" (the
match freeze is supposed to pin it), a grappling player over walk speed is not
speed-hacking (flight has its own envelope), a respawn is not a teleport, a corpse below
the kill plane is the kill plane *working*. 44 cases, and the controller calls those same
functions — `verify.sh` fails if any threshold is restated in the game code, because a
validator that drifts from the thing it validates is the defect that bit assignments #4,
#6 and #8 in this same repo.

Every finding records **location** (x/y/z + nearest PlayerStart as a human-readable
anchor), **error_type**, **game_context** (behavior, match state, sim time, alive,
health/shield, speed), one-sentence **evidence with the convicting numbers**, and an
occurrence count (findings dedup by type + 2000uu grid cell, so one bad ledge is one
record with a count, not 600 rows at 10 Hz). The report also carries run **stats**
(presses, move requests, deaths, distance travelled, behavior cycles) — proof the loop
actually exercised the game rather than idling.

## What the agent found

<!-- FILL AFTER RUN — from report/aqa_report_*.json, per TICKET_BN24 step 4: one short
paragraph per finding class, naming the mechanic or system it occurred in. A clean run
is a legal result: then this section says the seven detector classes held for the full
run, with the stats that make that claim mean something. -->

## Was I surprised by the findings?

<!-- FILL AFTER RUN — the honest answer, written against the real report. -->

## Honesty ladder

The project I am building has a written rule — *compiles ≠ works · PIE ≠ multiplayer ·
listen ≠ dedicated · editor ≠ packaged* — and it applies to my own submissions. Where
this one actually stands, rung by rung:

| Rung | Claim | Status |
|---|---|---|
| 0 | The rule layer **executes** — 44 cases, both firing and excuse | ✅ proven, and you can re-run it |
| 1 | The agent **compiles** into the game (Editor + Game targets) | ✅ done 28 Aug (`4a87b54e`), which found 3 real errors in my code — see below |
| 2 | The agent **runs** in a live match and reports | ⏳ the one remaining step (TICKET_BN24) |
| 3 | Multiplayer / packaged claims | ❌ not claimed, not needed here |

**Rung 1 cost me three real bugs, and that is worth saying plainly.** A build session
found that `StopRun` took `const FString&` while being bound as a *timer payload*
(payload delegates bind by value), and that two of my `TActorIterator` loops `return`
on the first hit, leaving the increment unreachable and failing `-Werror`. All three were
invisible to me because I wrote this in a container with no Unreal Engine. That is exactly
the gap the honesty ladder exists to name — I could not have found them by re-reading my
own code, and I did not pretend otherwise.

One consequence to be straight about: the rule-layer split described above landed *after*
that compile, so the current `.cpp` wants one more pass through the compiler before the PIE
run. TICKET_BN24 step 1 says so.

## Files

```
README.md            this file
verify.sh            one check per rubric criterion, against the committed report
make_submission.sh   builds the zip; refuses if verify.sh fails from the staged copy
report/              the agent's own JSON output from the PIE run
game-code/           (zip only) the C++ copied from breachpoint/Source/BreachpointNext/QA/
```
