# AIB21 — the Halo-fidelity audit, and what it found

> STATUS: in-progress — **ALL NINE FINDINGS CLOSED.** H3 and L3 closed 29 Aug; M1 written 1 Sep 2026
> (cloud lead) — **WRITTEN, NOT COMPILED**, with a headless spec that pins it. The
> ticket closes when the terminal runs rung 1 and `AIBot.Sim.AvatarBelt`.

`aib-critic` audited the whole module against the Halo-Infinite-1:1 bar. Verdict:
containment PASS, server-only PASS, F1/F4/F8 PASS, arbitration sound. The stuck-state
surface failed.

## Closed (28 Aug)

| # | Finding | Fix |
|---|---|---|
| **H1** | The reload gate is a trap: its early return also excluded melee, swap, grenade, fire and ADS, so a bot whose reload was refused went silent for the branch's life | Root cause was upstream — a weapon with an unresolved row read as EMPTY (`ammo 0/0 reserve 90`), and the ability's own gate is `CurrentAmmo < MagazineSize`, so `0 < 0` refused forever. Now reports FULL. Plus a 6s give-up that stands the bot back up and falls through |
| **H2** | The defend band was a ONE-WAY DOOR: stand-down killed the path request and nothing re-issued it, so a bot froze ~1.5s in the open on every re-entry | Re-issue `MoveToNavPoint` on the falling edge. **Measured 19 re-entries in one match** — this fired constantly |
| **M2** | Retreat's ADS band `[500,900]` overlapped the sprint the flee mover holds below 700, so Aim was pressed and refused every 0.75s forever | `AimRangeUU = DEFEND_RANGE_UU` |
| **M3** | A reloading bot would not swing at a rusher inside knife range — and the early return skipped `ShouldMelee` entirely, breaking that policy's "stepped every tick" contract | Melee read hoisted above the reload gate; a warranted swing now outranks the magazine |
| **M4** | Law 1's own certifying grep was no longer empty — three comments of mine named host symbols | Reworded; the grep is empty again |
| **L1** | `FightRangeUU = 3000` was inert (every tier's `LoseSightRadius` is 1500) and hid a hole where neither mover owned the legs | `AIB::EngageFadeEndUU` |
| **L2** | A corpse kept the reload crouch | Released in the death block |

## Open

- ~~**H3 — grenade evasion does not exist.**~~ **CLOSED 29 Aug — see the Log.**
- **H3 (as filed):** The whole chain (perceivability trace → fuse
  noise → `CanEvadeBlast` → reaction clock → facts) terminates in a dead fact:
  `BlastCenterRelative` has **zero readers**, and `BlastSecondsToDetonation` is read only
  as a commit-breaking edge that explicitly does not pick a winner. Throw a grenade at a
  bot's feet and it finishes its strafe leg and dies. In Halo Infinite the scatter is the
  single most legible bot behaviour there is, and FAIRPLAY F2 is written as though this
  works. The critic's proposed fix needs no new node: a `BlastSecondsToDetonation`
  consideration on Retreat, and `FleeFromBelief` using the blast centre as its threat
  point when `bIncomingBlast`.
- ~~**M1 — the unpossess belt releases Fire only.**~~ **CLOSED 1 Sep — see the Log.**
- **M1 (as filed):** Its comment asserts Fire is the only held
  verb; since Phase 4 the module also holds Sprint, Aim and a crouch toggle. The host
  backstops it today, which is exactly the dependency the containment law forbids.
- **L3 — a zero-score ambition can win.** `SelectionScore > BestSelectionScore` starting at
  0 makes the first registered spec (Engage) Best unconditionally. Unreachable today
  (Roam is a 0.2 floor); certain on the code path. One guard: `> 0.f`.

## Log

**28 Aug** — H1's root cause was found by instrumentation, not by reading: two earlier
hypotheses (degenerate `MagSize`, a weapon-lookup mismatch) were both wrong and were ruled
out with measurements before the third was tested. The critic independently reached the same
trap from the other end and named a different suspect for the refusal (a leaked
`State.Weapon.Reloading` GE); the measured fix — refusals 65–1222 → 2 — says the unresolved
row was the live cause, but the critic's leak theory is worth keeping for the 2 that remain.

Measured after all six fixes, one match: reload REFUSED 5, crouch presses 3, DEFEND
stand-downs 26, **defend-band re-entries 19**, melee swings 3, eliminations 9.


## Log — 29 Aug: the scatter

H3 closed, but **not** by the fix the critic proposed, and the difference is the same trap
`ShieldNorm` fell into.

The proposal was "one consideration on Retreat — `BlastSecondsToDetonation`, falling — so an
imminent blast lifts Retreat over Engage." Considerations MULTIPLY, and Retreat's `Hurt` is
exactly 0 above 0.8 vitality. A healthy bot's Retreat is 0.000, and no factor lifts zero. The
bot winning a fight at full health is precisely the one that needs to move, so the proposed
term would have dodged only for bots already hurt.

**EVADE is its own want.** `AIBot.Ambition.Evade`, BaseUtility 4.0 (a dodge that loses to a
fight is not a dodge), CommitSeconds 1.5 (shorter than the fuse — a bot must not spend two
seconds running from a crater), one consideration on the fuse falling from 1.0 to 0.2 across
the interrupt window, and `ValueWhenUnknown = 0.0` so the want is INVISIBLE on any host
without a blast seam.

The movement half needed no new node: `FAIBFleeFromBeliefTask` now prefers the blast centre
over the enemy as its threat point when `bIncomingBlast`. `BlastCenterRelative` was published
for exactly this and had zero readers. The Evade branch is `Sentinel + Flee` only — no
FaceBelief, no FireWhenAble, because a Spartan diving from a grenade is not also lining up a
shot, and a second rotation owner on a running body is a bug waiting. Retreat's defend
stand-down is now also blocked while `bIncomingBlast`, or the band would stand a dodging bot
up one tick after it started.

Measured, one match: 12 grenades thrown, 26 blasts perceived, **14 Evade wins, 22 scatter
moves**, at 0.2–0.8s from detonation. Bots visibly break off and run.

Known and accepted: the want is driven by the FUSE only, not by proximity. The source gating
is already right — `WarnNearbyBots` overlaps at the blast's own 500uu radius, so a bot only
ever learns about a blast that could hurt it — but a bot that has already escaped keeps
dodging until its 1.5s commit ends. Cheap to refine later with a distance selector; not worth
a new fact today.

Eliminations 9 -> 6 in the sampled match. Single-sample and inside this project's known
variance, but directionally expected: bots that dodge grenades die less to grenades.


## Log — 29 Aug: the utility model itself

Founder asked whether the goal/utility AI is applied *properly*. Reviewed against the
Infinite Axis Utility System, which this design otherwise follows closely: normalised
considerations, arbitrary curves, an explicit veto at 0, commit windows, a switch cost for
inertia, and per-want hysteresis. One canonical piece was missing.

**The compensation (make-up value) was absent.** Multiplying normalised considerations
punishes a want for being well described: four terms at a healthy 0.8 left Engage at 0.410
of its base while a two-term want kept 0.640. Engage is the most carefully modelled want in
the module and was therefore the most demoted — 59% of its base gone before any fact was
even bad. Authors learn to add fewer considerations, which is exactly backwards.

Now each term is lifted toward 1 in proportion to how many terms share the product. The two
endpoints are why it is safe here, and both are pinned:

- value `0` → `0` — a veto is still a veto (suppression, unknown-must-not-act, Evade's
  silence without a blast, Seek's dormancy all depend on this)
- value `1` → `1` — an all-perfect want scores exactly its base, never more

**It immediately broke a real thing, which is the point.** Seek's dormancy was a NUMERIC
COINCIDENCE — `ValueWhenUnknown = 0.1` with the comment "1.4 x 0.1 = 0.14 < Roam's 0.2
floor", a value tuned against another want's base. Compensation lifted it to 0.23 and a bot
wanted to Seek with nowhere to go: the exact SeekWeapon trap this ambition was rewritten to
escape. Dormancy is now an explicit `0` — an unsatisfiable want must be VETOED, not merely
outbid, and 0 says that at any base and under any compensation.

**L3 closed alongside it.** Selection started at "no best yet", so on an all-zero scoreboard
the FIRST REGISTERED spec won by default — a bot wanting Engage at 0.00, entering a branch
that fails on the belief it does not have. Electing nothing is the honest answer; the
ungated Fallback catches it and says so.

Measured, one match against the run before it — Engage's share of all selections:

| | before | after |
|---|---|---|
| Engage | 100 (31.8%) | **158 (41.0%)** |
| Roam | 95 | 132 |
| Search | 41 | 30 |
| Mode.Rally | 40 | 31 |
| Retreat | 24 | 21 |
| Evade | 14 | 13 |

Bots fight more. Retreat's small drop is the same correction seen from the other side —
it had been winning partly because Engage was structurally depressed. Evade is unchanged, as
expected: one consideration, so it is uncompensated by construction. Zero unserved wants.

**1 Sep — M1 closed. The fix is a list, because the bug was a comment.**

Confirmed the leak is real before writing anything. Every press crosses the door into
`UBNAIBAvatarAdapter::PressVerb`, which calls `ASC->AbilityInputTagPressed(InputTag)` on
an ASC the adapter itself resolves as *"pawn -> PlayerState -> the one ASC that outlives
the body"*. So a held verb at unpossession is not untidy — it is state parked on an
object that survives into the bot's next life. `SetSprint`'s own comment already said so
(*"a leaked hold rides the persistent ASC into the next life"*) about the sprint it was
careful to balance; the belt underneath it was not.

Held verbs, counted rather than remembered — every `PressVerb` site in
`AIBStateTreeTasks.cpp` whose `ReleaseVerb` is NOT the adjacent line:

| Verb | Site | Shape |
|---|---|---|
| Fire | 930 | held, released at 912/923/951 |
| Sprint | 261 | held, `SetSprint` rising/falling edge |
| Aim | 882 | held, re-asserted (release-then-press) |

Everything else — Jump, Crouch, Melee, Grenade, Reload, WeaponNext, Grapple, Dash — is a
press and a release on adjacent lines. **A tap cannot leak.** Crouch is the interesting
one: it is rented, but it is a TOGGLE, so releasing it gives nothing back.

**The fix.** `AIBTags::HeldVerbs()` is now a real list next to the tags, and
`AIB::ReleaseHeldVerbs()` (new `Interfaces/AIBAvatarInterface.cpp`) walks it and then
taps the crouch back up if — and only if — the avatar's own `IsCrouched()` says it is
down. Both belts, `OnUnPossess` and `EndPlay`, call that one function and nothing else.
Adding a held verb is now one line in one place, and the belt cannot go stale behind a
comment again, which is the actual defect: the code was correct when written and the
comment kept asserting it afterwards.

**The proof, headless:** `AIBot.Sim.AvatarBelt` (`Tests/AIBAvatarBeltSpec.cpp`), six
pins on a fake avatar that journals every press and release —
(1) the list names Fire/Sprint/Aim and excludes every tap, Crouch included;
(2) the belt releases every verb in the list exactly once, whatever is added later;
(3) a hard-coded Fire AND Sprint AND Aim pin, so shrinking the list back to Fire fails
here rather than in a match;
(4) a standing bot's crouch is not touched;
(5) a squatting bot is tapped up exactly once, and the trigger comes back FIRST;
(6) idempotent, because unpossess-then-EndPlay is a normal sequence and both belts run.

**Rung: WRITTEN, NOT COMPILED.** The cloud has no engine. Terminal owes rung 1 on all
three targets plus `AIBot.Sim.AvatarBelt`. One thing worth an eye during the run: the
fake avatar implements the interface on a plain struct, which relies on
`GENERATED_BODY()`'s I-class destructor being reachable from a derived type — correct as
read, unverified by a compiler.
