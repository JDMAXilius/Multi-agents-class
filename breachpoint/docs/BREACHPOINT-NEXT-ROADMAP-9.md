# ROADMAP 9 — THE BOTS FEEL LIKE PEOPLE

**Cut:** 23 August 2026 by the cloud lead · **Status:** LANDED (written, not compiled)
**Numbering:** R8 was TEAMS, landed and then **reverted** at the founder's call — it changed
what solo PIE looks like while the HUD was being verified. Its history is intact at `a284ef5`;
reverting the revert restores it. R9 is the next thing, not a renumber.

## The one-line goal

Four faults found by reading the bot stack end to end after the founder's build. Three are
behaviour a player can see; one is a limitation R5 wrote down and R9 closes.

## 9.1 — a fleeing bot walked back toward the thing it was fleeing

`HasFreshLastKnownLocation()` disqualified itself when `GetCurrentTarget() != nullptr` — but
**Survive deliberately blanks `GetCurrentTarget()`** (R6 G2 2.2, how the brain steers the tree
without editing it). So the test could not tell *"no target because I lost them"* from *"no
target because I am running"*, and a hurt bot with a fresh memory entered **Search** and walked
to where its attacker was.

Roam already had this right: it flips to the point of interest FARTHEST from the threat when
Surviving. Search simply never got told. One condition, at the top of the freshness test.

## 9.2 — bots could not hear

A bot shot in the back re-scored its ambition and **did nothing else**. You could empty a
magazine into one and it would never turn around.

The attacker was already recorded: `FBNLastDamage` is captured at the one reaction point every
damage passes through, on the authority — which is where bots live. `OnRecentDamageTagChanged`
now reads it and stamps the attacker's position as the last-known threat, so **Search** sends the
bot to look, exactly as losing sight of a seen enemy does.

**A memory, not a target**, and that is the whole design: being hit should make a bot go and
hunt, not acquire a perfect lock on someone it has never seen. Perception still has to do the
seeing, and the reaction window still applies when it does.

`ClearCurrentTarget` now writes that memory through the same `RememberThreatAt` — one writer for
one fact.

## 9.3 — bots stood perfectly still while firing

The single loudest tell that a thing is not a person. `FBNStrafeTask` sidesteps during **Shoot**:
a companion task that returns Running forever beside Face Target, so **Fire Burst still decides
when the state ends**.

- Perpendicular to the line of fire only. Stepping toward or away would be closing or retreating,
  and those are Close's and Survive's jobs.
- **Safe only because `ABNCharacter` aims with the CONTROLLER** (`bUseControllerRotationYaw`
  true, `bOrientRotationToMovement` false): the body moves sideways while the aim stays on the
  target. On a character that orients to movement this task would spin the bot mid-burst — that
  is written on the task itself.
- Deterministic (§5): the opening side is seeded off the controller's identity and flipped each
  step, never drawn from the global stream. A failed step flips the side too — a bot with its
  back to a wall turns around rather than pressing into it.

**THIS ONE NEEDS THE EDITOR.** A new StateTree node does not appear in a compiled tree by
existing: `ST_BNBot` must be rebuilt by `Tools/bn/62_bot_assets.py` (the probe list there gained
`FBNStrafeTask`, so a stale build now stops the script instead of silently authoring a tree
without it). Until that rebuild, everything else in R9 works and bots simply do not strafe.

## 9.4 — R5's fill limitation, closed

`EnsureBotFill` returned early on any state but warmup, so a human joining after the start left
the lobby one wide for the whole match. Warmup still both fills and yields; a **live match may
only yield** — a bot materialising beside you mid-fight is worse than a seat over, and Lyra draws
the same line (remove on join, backfill between matches).

## 9.5 — the jump, as a verb bots can spend (founder's ask)

Bots could not jump at all. `UBNGA_Jump` existed and `Input.Jump` was granted to them with every
other default — nothing ever pressed it.

`ABNBotController::TryJump()` presses that same tag, so a bot's jump IS a human's jump: the same
ability, the same `State.Movement.Jumping` tag, the same landing handling. Not
`ACharacter::Jump()`, which would have been a second movement path with no ability and no tag —
the exact split this controller exists to avoid.

**The cooldown is the whole difference between "uses jumps" and "is a rabbit".** Refused while
already airborne (BN gives humans no double jump either) and until `JumpCooldownSeconds` has
passed. The controller decides only whether a jump is ALLOWED; the tasks decide when it is worth
one:

| Where | When | What it buys |
|---|---|---|
| **Closing on an enemy** (`BN Move To Target`) | wedged at HALF the give-up window, one attempt | A path exists, path following says Moving, the bot gets nowhere: a lip, a crate, a step. That is the shape a jump clears, so it is spent BEFORE the target is written off, not after |
| **Roaming** (`BN Move To Point Of Interest`) | move reports done while short of the point, one attempt per leg | The "get up there" and "get out of here" case. Before R9.5 stopping short silently counted as arriving, so the bot quietly gave up on unreachable ground forever |
| **Firefight** (`BN Strafe`) | every Nth sidestep, and immediately when a step is refused | The juke. A bot that never leaves the ground can be led by aim alone; one that jumps every step cannot shoot. A refused step means cornered, and a jump is the one move left that changes the picture |

**One attempt per wedge, per leg** — a bot that jumps repeatedly at a wall it cannot pass reads as
stuck *and* stupid, where giving up and going elsewhere reads as a decision.

**What this is NOT:** gap-jumping across a chasm on purpose. That needs NavLinkProxies placed in
the level so the navmesh knows a jump connects two islands — level work, not code. This is the
without-nav-data version: jumps that clear what is *in the way*, spent at the moments the bot
already knows something has gone wrong.

## 9.6 — `Tests/` stops being empty

Every claim in this project was verified by a person looking at a screen. Three specs now assert
in code, and each one locks a bug that actually happened or a rule that is invisible until it
breaks in front of a player.

| Suite | What it holds down |
|---|---|
| `BreachpointNext.Sim.BotBrain` | The utility table, the commit window, and **the interrupt regression**: Survive's break-out was written as a utility RATIO the shipped weights could never satisfy, which left a bot at 5% health firing through its whole commit. Nothing caught it but a human reading arithmetic. The rows come from `UBNBotBrain::DefaultRow`, not from literals, so the spec cannot pass while the shipped decision drifts |
| `BreachpointNext.Sim.Damage` | The door, entered **through the door**: init numbers, the drain, the floor at zero, R7.3's cause-of-death `SourceName` *and its blanking*, and R7.4's grenade cost with the BASE clamp. A spec that poked attributes directly would prove the attribute set works and nothing about whether the game can reach it. **Rewritten once** — see the note below |
| `BreachpointNext.Sim.KillfeedView` | Dedupe by sequence (the ring replicates WHOLE — a joiner receives every entry again), the empty-line "seen but not shown" path the join-age filter depends on, and the 5-row cap that must match the pool the WBP builds |

`UBNBotBrain` is testable at all only because it is headless by contract — no world, no clock, no
actors. That contract was written for R8's determinism harness; this is the first thing to cash it.

**The world scaffolding is transcribed, not invented**: `BuildWorld` / `SpawnFighter` come from the
old module's `BRShieldSpec`, which compiled and ran against this engine.

**THE DAMAGE SPEC WAS REWRITTEN, and the reason is the point of the file.** Its first draft used
three things this project has never compiled: `UAbilitySystemComponent::CanApplyAttributeModifiers`
(which appeared nowhere in the repo except a comment *I* had written about it), the four-argument
`FHitResult` constructor (BN has only ever default-constructed one), and an assertion on
`UGameplayEffect::Modifiers` from outside the class — a member this project writes only from
inside its own constructors, which proves it exists and proves nothing about its access.

All three were written from memory. **A spec is the worst place in the codebase to guess at an
API**: it compiles into the editor target, so a spec that does not build takes the game down with
it — while claiming to be the thing that protects the game. The rewrite uses only what the old
module's compiled spec used, plus what BreachpointNext itself used in the founder's last
successful build. A mechanical sweep over all three spec files now reports **zero engine symbols
that nothing in this repo has compiled**.

The coverage that went with those APIs is NAMED IN THE FILE rather than quietly dropped: GAS's
refusal at zero grenades, the cost GE's shape, and the head-hit multiplier are all listed as gaps
at the bottom of `BNDamageSpec.cpp`, each with what it would have taken to keep them. The refusal
and the multiplier are PIE read-backs in `TEST-MATCH` instead.

`Tools/run-specs.sh` is the macOS runner, and it treats **zero tests as INCONCLUSIVE, never PASS** —
a filter typo, a stale build, or specs compiled out all report "0 failures" and look like success.

## What is still weak, and still true

- **No cover.** Close is a straight line at the enemy. Real cover wants EQS or tagged cover
  points, and it is the next real step in bot quality.
- **Sight only.** 9.2 gives bots a reaction to being HIT; they still cannot hear a firefight
  they are not part of, or footsteps. A hearing sense is a perception config change plus one
  handler, and it reuses 9.2's memory.
- **No squad sense.** Nothing coordinates two bots; each fights alone. Teams (R8, reverted) is
  the prerequisite for any of that.

---

# R10 — WHAT HALO'S BOTS HAVE THAT OURS DID NOT

**Cut:** 23 August 2026 · **Status:** LANDED (written, not compiled)

Three gaps, picked off the Halo Infinite feature audit by value per line: difficulty tiers,
hearing, and cover. What Infinite still has and BN does not is at the bottom.

## 10.1 — four tiers, because a tier is not a skill slider

Every bot in BN fought identically. Infinite's Recruit / Marine / ODST / Spartan move reaction,
aim, awareness and movement **together** — a Recruit that only aims worse reads as a broken
Spartan rather than a rookie, which is the whole reason the row has nine numbers instead of one.

`FBNBotTuningRow` keyed by tier name, `BotTier` on the controller, resolved once in `OnPossess`
(so a GameMode may later hand different tiers to different bots for a mixed lobby).

**The numbers already existed** — scattered as per-controller Config keys. They moved onto the row
and the keys are gone, because a difficulty setting that cannot change them is not one, and
keeping both would be two sources of truth for one number.

**MARINE IS THE FOUNDER'S ARENA TUNING, kept exactly.** Sight 1200/1500 was lowered from the
engine defaults for a measured reason: at 2500/3000 a bot could see most of `BR_Arena01` from
where it stood, so every bot always had a target, the tree never left Engage, Search was
unreachable and nobody roamed. **Every tier is scaled around those numbers, not around the
defaults they replaced** — my first draft had Spartan at 4000, which would have recreated that
exact bug and called it difficulty.

Aim and footwork reach the tasks through an OVERRIDE rule: a negative authored parameter means
"ask the tier", zero still means hitscan-perfect, and a positive value is a deliberate per-state
pin the tree keeps. That is what lets a tier change how a bot fights without re-authoring the
StateTree.

Spartan's aim error is deliberately **not zero**. A bot that never misses is not hard, it is
unfair; what makes a Spartan hard is the reaction window and the footwork.

## 10.2 — ears

Bots were deaf. R9.2 gave them a reaction to being *hit*; a firefight ten metres away was silent,
and you could clear a room next door without anyone looking up.

A hearing sense beside the sight one, with **sight kept dominant** (seeing beats hearing when both
report the same actor), and the two things worth hearing now report themselves: **every shot**
(from `ApplyCost`, which runs once per trigger pull on the authority for humans and bots alike —
the same place ammo is spent, so a shotgun's pellets are one noise) and **every grenade blast**.

**A noise is a PLACE, never a target.** It stamps the last-known position and Search walks the bot
over to look. A bot that acquired you through a wall because you fired would be omniscient, and it
would skip the reaction window that makes a firefight readable.

`HearingRange` is a tier number and is deliberately **longer than sight** — you hear a fight
through a wall you cannot see through, and that asymmetry is most of what makes a level feel
occupied. Zero deafens a tier, which is part of what makes Recruit flankable.

## 10.3 — cover

The behaviour Infinite gets free from its shield economy: hurt, under fire, so stop standing in
the open. BN's shields are off by ruling, so the trigger is said out loud instead — **health below
60% AND `State.Combat.RecentDamage` AND off cooldown.** All three, because a bot chipped once ten
seconds ago diving for cover mid-fight reads as cowardice, not tactics.

**No EQS, deliberately.** A rosette of eight navmesh-projected samples, each traced back at the
threat on the **weapon channel**, answers the only question cover asks — *can this spot be shot
from where they are standing* — using the same geometry the bullets use. EQS would ask it more
expensively and no more truthfully. The day BN wants *scored* cover (flanking angles, distance
bands, height) EQS earns its place; picking a wall does not need it.

- **Closest blocking spot wins**, not the best one: a bot that crosses the arena to a better wall
  spends the trip being shot in the back.
- **Failing is a real answer.** An open arena has no cover, and dropping through to Close/Shoot is
  correct when the only option left is to fight.
- The cooldown lives on the CONTROLLER and is spent on the **attempt**, not the arrival — a state
  cooldown resets every time the tree re-selects, and a bot that re-enters cover the instant it
  leaves never shoots back.
- The hold at the end is what makes it read as cover rather than a pathing twitch — and it is the
  window a shield would use, the day shields come back on.

## 10.4 — get out of the way of a grenade

The single most-cited missing behaviour in every review of Halo: Campaign Evolved was **Elites no
longer dodging grenades**. BN had none of it at all: we threw grenades and nothing reacted to one
landing at its feet — including the bot that threw it.

**PUSHED, not polled** (law 4). `ABNProjectile` arms a second timer `BotWarnLeadSeconds` before its
fuse, does ONE overlap at the blast's own radius, and tells only the bots actually inside it. A
poll would have had every bot in the level asking every evaluation whether anything was about to
explode. The warning carries a place and a deadline and nothing else — **no thrower, no target**:
this is a place to not be standing, and the bot learns nothing about who put it there.

**`Evade` sits ABOVE `Engage` in the tree**, and that ordering is the whole design. It needs no
health condition and no target condition, because a grenade at your feet outranks having a target,
being hurt, and everything else — none of them matter in a second.

- **Straight away, flat.** The move a player makes without thinking, and the only bearing that is
  right regardless of geometry: every other way out of a circle is longer.
- **Past the edge, not to it** — the falloff is linear to zero AT the radius, so standing exactly
  on the line still hurts.
- **A jump on the way out.** What the cooldown is for, ground the walk does not cover, and the
  read a player recognises instantly as *it saw that coming*.
- **Clear is enough.** A bot that only had to take two steps returns to the fight immediately
  rather than running the whole leg — the difference between reacting and fleeing.
- **Cornered fails**, and the tree goes back to what it was doing. There is no better answer to
  being cornered by a grenade than carrying on.
- **The soonest warning wins**: a second grenade must not push the deadline out from under a bot
  already running from the first.
- **Recruit does not dodge** (`bEvadesBlasts`). Halo's own shape — the low tiers are the ones you
  can catch with a grenade, and taking that away takes away the tier.

## Still missing, against Infinite

Weapon pickups and power weapons (no pickups exist in BN at all) · per-weapon range preference ·
target leading for projectiles · shield-break → headshot discipline (no shields yet) · crouch
(`UBNGA_Crouch` exists; nothing presses it) · clamber and nav-link gap jumps (level work —
NavLinkProxies) · vehicles · objective play (BN has one mode) · voice callouts · a bot taking over
an abandoned slot mid-match (R9.4 only yields seats, it does not fill them).

**NEEDS THE EDITOR:** FOUR new StateTree nodes and a new table. `Tools/bn/62_bot_assets.py` must be
re-run — it now probes for `FBNStrafeTask`, `FBNShouldTakeCoverCondition`, `FBNTakeCoverTask`,
`FBNIncomingBlastCondition` and `FBNEvadeBlastTask`,
so a stale build stops the script instead of authoring a tree without them, and it builds
`DT_BNBotTuning` alongside the tree and the ambitions.

---

## Pre-build audit — 23 August 2026

Six packets stacked up unbuilt (R7.6, R7.7, R9, R9.5, R9.6, R10) while the founder was away from
their machine. R5/R6 ran a sweep like this and it caught a real error; R7's did NOT catch the
`FBNKillfeedEntry` / `UBNKillfeedEntry` engine-name collision, which then blocked the terminal for
a whole session. This is that sweep, run mechanically rather than by reading.

| Check | Result |
|---|---|
| UHT engine-name collisions (the R7 bug's exact class) across 142 module types | **0** |
| Declarations without definitions / definitions without declarations | **0** (4 hits were namespace calls and a UHT `_Implementation`) |
| Symbols used without their include, across every new API | **0** |
| `FFieldNotificationClassDescriptor::X` ids referenced by widgets vs `FieldNotify` properties | **29 referenced, 29 resolve** |
| Members the new specs touch — all must be public, a spec is nobody's friend | **all public** |
| StateTree `FInstanceDataType`s that are real USTRUCTs; the three new nodes registered | **19/19, all three present** |
| `DefaultGame.ini` keys with no matching UPROPERTY | **0** |
| **HARD `BindWidget`s vs the terminal's read-back of the built WBPs** | **9/9 classes satisfied** |

**ONE REAL BUG, found and fixed.** `BNFindCoverPoint` passed a `const UWorld*` to
`FNavigationSystem::GetCurrent<>`, which takes a non-const `UWorld*` — it would not have compiled.
The proven call four hundred lines above it in the same file (`ReportMoveFailure`) had the right
shape and I re-derived instead of copying. That is the second time in this project that
transcribing beat inventing, and the first time a sweep caught it before the founder did.

Also renamed `BotTuningTable_Soft` → `BotTuningTablePath` before it reached an ini a designer reads.

**What this sweep CANNOT tell you:** whether the engine's API surface matches what I transcribed
(template resolution, overload sets, UHT's opinion of a specifier). It checks the module against
ITSELF. `run-ubt.sh` is still the only thing that can say "compiles", and the specs are the highest
risk in the stack — a spec that fails to build takes the whole editor target with it.

**The BindWidget row is the most useful line in this table.** A name mismatch there fails at ASSET
LOAD, not at build: the HUD comes up empty and nothing in the compiler log says why. All nine
widget classes' required binds exist in the assets the terminal actually built.
