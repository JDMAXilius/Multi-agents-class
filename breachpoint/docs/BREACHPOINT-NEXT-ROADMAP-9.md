# ROADMAP 9 — THE BOTS FEEL LIKE PEOPLE

**Cut:** 23 August 2026 by the cloud lead · **Status:** LANDED and **COMPILED** (24 Aug 2026,
mac terminal — rung 1 PASS for `BreachpointEditor`, rung 2 PASS 30/30). The header said
"written, not compiled" until 24 Aug; it was true when written and is not any more.
**Caveat:** R9's `FBNStrafeTask` is NOT in the compiled `ST_BNBot` — see
`BREACHPOINT-NEXT-ROADMAP-10.md`, "THE BLOCKER".
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

# R10 — MOVED

R10 (tiers, ears, cover) and R10.4 (grenade evasion) were written into THIS file because they
began as an extension of R9's reaction work. That was wrong: a reader looking for R10 looks for
`ROADMAP-10`, and the terminal had to reconstruct the design from commit bodies on 24 Aug to
write one.

**The page is `docs/BREACHPOINT-NEXT-ROADMAP-10.md`.** It is the single source for R10, it
carries the ladder state, and it carries the blocker: the C++ compiles and the behaviour is
SWITCHED OFF until `ST_BNBot` is rebuilt (`TICKET_BN10_BOT_ASSETS`). The duplicate that stood
here has been removed rather than left to drift out of step with it.

## Pre-build audit — 23 August 2026 (kept in ROADMAP-10)

Six packets stacked up unbuilt while the founder was away, and a mechanical sweep over all of
them caught one real compile error — a `const UWorld*` passed to `FNavigationSystem::GetCurrent<>`
in the cover search, re-derived instead of copied from the proven call in the same file.

**The full table lives in `docs/BREACHPOINT-NEXT-ROADMAP-10.md`**, under "The pre-build audit —
worth keeping as a method". One copy, and it is the one the terminal maintains.
