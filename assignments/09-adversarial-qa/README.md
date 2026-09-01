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

The run: **`BR_Spillway`, standalone PIE, solo + bot fill, 1 Sep 2026 02:59:25Z.** 21 behavior
cycles, 2,956 ability presses, 458 move requests, 61,883 uu travelled, match state
`InProgress` throughout. **Four finding records across two of the seven detector classes**,
18 occurrences in total. The run stopped at 261.7s of a planned 300 — see the honesty note
at the end of this section, because that is part of the answer.

**`stuck_state` — 2 records, 10 occurrences — the level, not the code.** The probe issued a
move request the navigation system *accepted*, stayed alive and unfrozen, and then sat at
0.0 uu/s for over three seconds. Both sites are on the outer rim of the Spillway blockout:
`(4366, 3966, 98)` during `ledge_dive`, nine times, 2,473 uu from the nearest PlayerStart,
and `(4366, 1939, 42)` during `boundary_probe`. The mechanic is **navmesh-vs-collision
disagreement in the BN37 blockout geometry**: a path exists across ground the pawn's capsule
cannot actually traverse, so the character commits to a route and wedges. This is the
failure a player finds by walking into a corner and never getting out, and it is worth
noting the detector's excuse policy did its job here — a *rejected* move request is not a
finding, and a frozen pawn is not a finding; this fired only where a route was granted and
then betrayed.

**`speed_violation` — 2 records, 8 occurrences — a grounded grapple pull, and the detector's
excuse policy is wrong about it.** Both fired during `ability_mash`, both reading
`ground speed 1800 uu/s vs MaxWalkSpeed 250 (x7.20)`, one second apart at sim 109s and 110s.
The convicting number is `1800` exactly, and that is not a coincidence:
`BNCharacterMovementComponent.h:89` sets `GrapplePullSpeedUU = 1800.f`. (It is *not* the dash,
which is `DashSpeedUU = 2000.f` — and the probe never presses `Input.Dash` at all. It does
press `Input.Grapple`, in both `grapple_abuse` and `ability_mash`.)

The mechanism is in `UBNCharacterMovementComponent::StartGrapplePull`, which drops the
character out of walking **only at the moment the pull begins**:

```cpp
// Off the ground first: a grounded MoveToForce fights floor snapping every frame.
if (IsMovingOnGround()) { SetMovementMode(MOVE_Falling); }
```

If the character lands while the root-motion pull is still running, nothing puts it back in
the air — the pull keeps driving it *across the ground* at full pull speed. And this was not
a one-sample blip on landing: the two records sit 1,905 uu apart, one second apart, both at
1800 uu/s, so the pawn really did cross the floor at 7.2x its walk cap for about a second.

`BNAQA::SpeedViolation`'s only excuse is `!bOnGround`, written on the belief that "the
grapple's regime is flight" and therefore never grounded. That belief is false, so the rule
convicts a legal grapple. **The detector is wrong here, not the movement code** — but the
finding is still the most valuable thing in this report, because it is the one place the run
proved my model of the game was wrong rather than merely confirming it. Filed as
`TICKET_BN39_AQA_DASH_EXCUSE`; the fix is to excuse an active grapple root-motion source
(`UBNCharacterMovementComponent::IsGrapplePullActive()` already exists) rather than to excuse
"recently launched", and certainly not to raise the tolerance until the number stops
appearing.

One honest caveat I am not qualified to close from a QA probe: whether a grounded 1800 uu/s
slide is *intended* is a design question, not a detector question. If it is not intended,
the excuse above would hide a real ground-skimming exploit, and the right fix is in the
movement component instead. BN39 names that fork rather than assuming the answer.

**The other five detector classes did not fire.** No `fell_out_of_world_alive`, no
`escaped_playable_space`, no `attribute_anomaly`, no `teleport_discontinuity`, no
`acted_while_dead` / `input_during_freeze` — across 2,956 presses that deliberately included
mis-combined and dead-state input. The state gates held.

**Two defects in the probe itself, found by running it.** Neither is a finding *about the
game*, and both would have silently degraded this report, so they belong here:
`WriteReport` was reachable only from the end timer or `bn.aqa.stop`, so a PIE session that
ended early wrote **nothing at all** — an earlier 17s run that had already recorded a real
`stuck_state` produced no artifact; fixed by writing from `EndPlay`, where every teardown
path meets. And `FFileHelper::SaveStringToFile` defaults to UTF-16LE, so the first complete
report was a "structured report" that `json.load` refused at byte 0 — and so did this
folder's own `verify.sh`; fixed with `ForceUTF8WithoutBOM`.

**Why 261.7s and not 300.** PIE in this editor session self-terminates at random —
`EEndPlayReason::EndPlayInEditor`, no crash, no error — at observed lifetimes of 3.9s, 17s,
64s, 71s, 190s, 261.7s and once a clean 300.01s. It does so **with no probe in the world at
all**, which two controls established: PIE left strictly alone died at 64s, and a session
the probe never joined died the same way. An earlier draft of my notes blamed the probe's
join; that was wrong, and the control that would have caught it sooner was simply too short.
The 300.01s run happened, and its numbers agree with this one (24 cycles, same two detector
classes, the same grounded-grapple speed finding) — but it is the UTF-16 one, written before the
encoding fix, so it is evidence rather than the deliverable. This report is the honest
artifact: a real run, 87% of its planned length, with the shortfall named rather than
rounded away.

## Was I surprised by the findings?

**By the game, no. By my own agent, twice.**

The `stuck_state` cluster is not a surprise: `BR_Spillway` is a four-day-old blockout
(BN37), its geometry has never been walked by anything but bots, and "navmesh says yes,
capsule says no" is the most ordinary defect a new level has. If anything the surprise is
how *localised* it was — nine of the ten occurrences at one rim corner, which is why the
report deduplicates by type and 2,000uu grid cell rather than emitting 600 rows at 10 Hz.
One bad corner reads as one bad corner.

What genuinely surprised me was the shape of my own errors. I built the detectors expecting
to argue about thresholds, and both real problems were about **excuse policy** instead — the
question of what *legitimately* looks like a defect. `SpeedViolation` knew that falling and
grapple flight are legal ways to exceed walk speed, and never considered that **grapple
flight is not always flight** — that a pull which begins in the air can finish along the
ground, at full speed, with the pawn's feet down the whole way. That is not a tuning error, it is a gap in the
model of the game, and no amount of adjusting 1.75 would have found it. Writing 44 headless
cases with both a firing and an excuse case for every rule made me feel thorough; the live
run found the excuse I had not thought to write. That is the argument for running the thing
against the real game rather than against my own imagination, and it is the assignment's
actual lesson.

The second surprise was worse and more useful: **the agent lost its own evidence.** For the
first hour the probe could run, detect correctly, log a real finding as a warning — and then
write no report at all, because the only path to `WriteReport` was a timer that an early PIE
stop never reached. The README you are reading claimed "the report writes on any stop"
before I checked whether that sentence was true. A QA agent that cannot survive its own
environment ending is not a QA agent, and I would not have discovered it if the environment
had been stable. The instability that cost me six runs is the reason the tool is now
correct.

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
