# Assignment #9 — Adversarial QA Agent

**Course:** Multi-Agent AI for Game Development · **Student:** Juan Diego Lugo
**Target:** BREACHPOINT — my capstone, a UE 5.8 native-C++ 4v4 arena FPS. Unlike #8,
this assignment *requires* running against the capstone, so the agent code lives in the
game's own source tree and the report comes from a real Play-In-Editor run.

```bash
./verify.sh        # ← START HERE: checks every rubric criterion — agent code, report
                   #   schema, loop evidence, README answers. Exit 0 = all pass.
```

## Where everything is

| Piece | Location |
|---|---|
| Agent code (the deliverable) | `breachpoint/Source/BreachpointNext/QA/BNAdversarialAgent.h/.cpp` (a copy ships in this folder's zip under `game-code/`) |
| Structured report (JSON) | `report/aqa_report_*.json` — written by the agent itself during the run |
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

Per the project's law: the code in this folder's zip is the code that ran; the report
names its net mode (`standalone PIE` / `listen server`). This is a PIE claim, not a
packaged-build or multiplayer claim — the assignment needs one real test run, and that
is exactly what the report is.

## Files

```
README.md            this file
verify.sh            one check per rubric criterion, against the committed report
make_submission.sh   builds the zip; refuses if verify.sh fails from the staged copy
report/              the agent's own JSON output from the PIE run
game-code/           (zip only) the C++ copied from breachpoint/Source/BreachpointNext/QA/
```
