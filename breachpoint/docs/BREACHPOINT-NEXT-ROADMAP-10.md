# ROADMAP 10 — TIERS, EARS, COVER, AND GETTING OUT OF THE WAY

**Cut:** 23 August 2026 by the cloud lead · **Written up:** 24 August 2026, mac terminal
**Status:** C++ **LANDED and COMPILING**. Editor half **NOT LANDED** — see the blocker below,
which is the most important thing on this page.
**Numbering:** R8 was TEAMS, landed then reverted at the founder's call; its history is intact at
`a284ef5`. R9 was THE BOTS FEEL LIKE PEOPLE (`docs/BREACHPOINT-NEXT-ROADMAP-9.md`).

## Why this document exists at all

**It did not, until now.** R1–R7 and R9 each have a roadmap file; R10 and R10.4 existed ONLY as
commit bodies (`69f61b4`, `5af22b8`, `cefbf40`). A session that read `docs/` — which is what
`CLAUDE.md` tells every agent to do — would not have known that bots have four difficulty tiers,
can hear, take cover, or dodge grenades. Reconstructed here from those commits so the next
session does not have to run `git log` to find the design.

## THE BLOCKER — R10 and R10.4 are not in the tree the bots actually run

`Content/BN/AI/ST_BNBot.uasset` is dated **22 Aug 18:38**, which is BEFORE R10 landed. Read back
from the asset, the compiled tree contains:

- tasks: `BNFireBurstTask`, `BNMoveToTargetTask` — **and nothing else**
- states: `Root, Engage, Rearm, Nade, Knife, Close, Shoot, Search, Roam`

Missing against what `UBNBotAuthoring::Author` now builds: the **`Cover`** state (10.3), the
**`Evade`** state (10.4), and `FBNStrafeTask` — R9's footwork, which `BNBotAuthoring.cpp` adds to
`Shoot`. So today a bot cannot take cover, cannot dodge a grenade, and does not sidestep while
firing, no matter what the C++ says. Measured 24 Aug: of 20 samples of a moving bot, 19 were
FORWARD, which is consistent with a `Shoot` state that has no strafe task in it.

**The fix is one command against a live editor**, and it is not a code change:

```
python Tools/bn/62_bot_assets.py probe    # is the running build current?
python Tools/bn/62_bot_assets.py build    # author + compile + read back
```

`probe` exists precisely so a stale build stops the script instead of authoring a tree without
the new nodes. Requires `editor-live`. **This is the highest-value single action outstanding in
the BN track** — four packets' worth of behaviour is written, compiled, and switched off.

**It is now a ticket on the board: `docs/tickets/TICKET_BN10_BOT_ASSETS.md`.** It was found by
reading `docs/`, which is not a process — see the 24 Aug entry in `docs/tickets/HANDOFF.md`
for why BN tickets now live where `/tickets list` can actually see them.

## 10.1 — four tiers

Every bot fought identically. **Recruit / Marine / ODST / Spartan** move reaction, aim, awareness
and movement TOGETHER — a Recruit that only aims worse reads as a broken Spartan, which is why the
row carries nine numbers rather than one. The numbers already existed as scattered per-controller
Config keys; they moved onto `DT_BNBotTuning` and the keys were deleted, because a difficulty
setting that cannot change them is not one, and keeping both is two sources of truth.

**Marine is the founder's arena tuning, kept exactly.** Sight 1200/1500 was lowered from engine
defaults for a measured reason: at 2500/3000 a bot could see most of `BR_Arena01` from where it
stood, so every bot always had a target, the tree never left Engage, Search was unreachable and
nobody roamed. Every tier is scaled around those numbers. The first draft put Spartan at 4000,
which would have recreated that exact bug and called it difficulty.

Aim and footwork reach the tasks by an override rule: **negative = ask the tier, zero = still
hitscan-perfect, positive = a per-state pin the tree keeps.** Spartan's aim error is deliberately
not zero — a bot that never misses is not hard, it is unfair.

## 10.2 — ears

Bots were deaf. R9.2 gave them a reaction to being HIT, but a firefight ten metres away was
silent. A hearing sense sits beside the sight one with **sight kept dominant**, and the two things
worth hearing report themselves: every shot (from `ApplyCost`, which runs once per trigger pull on
the authority for humans and bots alike, so a shotgun's pellets are ONE noise) and every grenade
blast.

**A noise is a place, never a target.** It stamps the last-known position and Search walks the bot
over to look. Acquiring through a wall because someone fired would be omniscience, and would skip
the reaction window. Hearing range is a tier number and is LONGER than sight — you hear a fight
through a wall you cannot see through, and that asymmetry is most of what makes a level feel
occupied.

## 10.3 — cover

What Halo Infinite gets free from its shield economy: hurt, under fire, stop standing in the open.
BN's shields are off, so the trigger is said out loud — health below 60% **AND**
`State.Combat.RecentDamage` **AND** off cooldown, all three, because a bot chipped ten seconds ago
diving mid-fight reads as cowardice rather than tactics.

**No EQS, deliberately.** A rosette of eight navmesh-projected samples, each traced back at the
threat on the WEAPON channel, answers the only question cover asks — *can this spot be shot from
where they are standing* — using the same geometry the bullets use. EQS would ask it more
expensively and no more truthfully. **Closest blocking spot wins, not the best one:** crossing the
arena to a better wall means spending the trip being shot in the back. Failing is a real answer —
an open arena has no cover, and fighting is then correct. The cooldown lives on the CONTROLLER and
is spent on the ATTEMPT, because a state cooldown resets on every re-selection and a bot that
re-enters cover the instant it leaves never shoots back.

## 10.4 — bots get out of the way of a grenade

The most-cited missing behaviour in every review of *Halo: Campaign Evolved* was Elites no longer
dodging grenades. BN had none of it — we threw grenades and nothing reacted, including the bot
that threw it.

**Pushed, not polled (law 4).** `ABNProjectile` arms a second timer `BotWarnLeadSeconds` before
its fuse, does ONE overlap at the blast's own radius, and tells only the bots inside it. A poll
would have every bot in the level asking every evaluation whether anything was about to explode.
The warning carries a place and a deadline and nothing else — no thrower, no target: it is a place
not to be standing, and the bot learns nothing about who put it there.

**Evade sits ABOVE Engage in the tree, and that ordering is the design.** No health condition, no
target condition: a grenade at your feet outranks having a target, being hurt, and everything
else, because none of them matter in a second.

Straight away and flat — the move a player makes without thinking, and the only bearing that is
right regardless of geometry, since every other way out of a circle is longer. **Past the edge
rather than to it**, because falloff is linear to zero AT the radius and the line still hurts. A
jump on the way out, which is what the cooldown is for. **Clear is enough:** a bot that took two
steps goes straight back to the fight rather than running the whole leg — the difference between
reacting and fleeing. Cornered FAILS and the tree carries on, because there is no better answer to
being cornered by a grenade. The soonest warning wins, so a second grenade cannot push the
deadline out from under a bot already running from the first. **Recruit does not dodge at all**
(`bEvadesBlasts`) — Halo's own shape, where the low tiers are the ones you can catch with one.

## The pre-build audit (`5af22b8`) — worth keeping as a method

Six packets stacked up unbuilt while the founder was away. R5/R6 ran a sweep like this and it
caught a real error; R7's did not catch the `FBNKillfeedEntry`/`UBNKillfeedEntry` engine-name
collision, which blocked the terminal for a whole session.

**The bug it caught:** `BNFindCoverPoint` passed a `const UWorld*` to
`FNavigationSystem::GetCurrent<>`, which takes non-const. It would not have compiled. The proven
call four hundred lines above it in the same file had the right shape and was re-derived instead
of copied — *"the second time in this project that transcribing beat inventing."*

Everything else came back clean: 0 engine-name collisions across 142 module types, 0 decl/def
mismatches, all 29 FieldNotify ids resolve, all 19 StateTree instance data types are real
USTRUCTs, every ini key maps to a real UPROPERTY. The most useful line: all nine widget classes'
HARD `BindWidget`s exist in the WBPs the terminal built — that class of mismatch fails at ASSET
LOAD, not at build, i.e. an empty HUD with nothing in the compiler log to explain it.

**What the sweep cannot say:** whether the ENGINE's API matches what was transcribed. It checks
the module against itself.

## Ladder state

- **Rung 1: PASS for `BreachpointEditor`** (24 Aug, mac terminal) — R9 and R10 compile. Still
  PARTIAL against `contracts/testing.md`, which wants all three targets; a launcher install ships
  no server binaries so `BreachpointServer` cannot link on this machine.
- **Rung 2: PASS** — `BreachpointNext.Sim`, 30 tests, 0 failures.
- **Editor half: NOT DONE.** See the blocker at the top.
- Nobody has watched a bot take cover or dodge a grenade, because on this build no bot can.
