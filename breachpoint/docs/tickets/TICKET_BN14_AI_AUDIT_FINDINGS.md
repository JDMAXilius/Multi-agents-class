# TICKET — BN14: what the three-agent AI audit found, and what is still open

> STATUS: open — cut 25 Aug 2026 by the lead from a three-agent crew audit
> (bn-critic netcode, critic GAS-purity/laws, ai-builder architecture). The fixes marked
> LANDED are in `53f806c`..HEAD; everything else is open work with an owner named.

## What was fixed the same session

- **R11 (200 ms reaction floor) — was BREACHED IN SHIPPING CONFIG.** Spartan ran 0.08–0.16 s and
  ODST 0.14–0.28 s, and `BotTier=Spartan` was live. Now clamped at the single draw point
  (`ABNBotController::DrawReactionSeconds`) so no tier, table or ini can breach it again.
- **R11 for explosives.** `Evade` carried no `FBNReactedCondition` while Nade/Knife/Shoot all do,
  so a bot dodged 0 ms after a warning that carries no line-of-sight test — through a wall, at a
  grenade it had never seen. `HasReactedToBlast()` now gates it on the same drawn reaction.
- **The never-idle fallback was a wallhack.** `FindNearestValidEnemy` iterated every pawn in the
  world with no range and no LOS test, making `SightRadius` decorative. Now bounded by
  `LoseSightRadius` AND `LineOfSightTo`.
- **Search died below 33 % health, permanently.** Survive wins on utility below ~33 % with no
  target, `HasFreshLastKnownLocation` returned false for Survive, and BN has no health regen — so
  a bot under a third health could never hunt a last-known again for the rest of its life. Scoped
  to `GetThreat() != nullptr`: fleeing requires something to flee from.
- **A roaming bot ignored an enemy in front of it.** `FBNHasTargetCondition` is an ENTER
  condition and `BNBotAuthoring` authors no OnTick and no event transitions, so Roam finished its
  leg and its dwell first. `FBNMoveToPointOfInterestTask::Tick` now succeeds on a live target.

## HIGH, still open — needs a founder ruling, not a packet

**R28 vs R10.1.** R28 is a CLOSED ruling: *"`sight_radius_m` (35.0) and `sight_fov_deg` (90.0)
are identical across all three tiers and stay that way."* R10.1 shipped per-tier sight
(900/1200/1500/1800) and FOV (55/70/85/100) without amending it. Law 8 says rulings are judged
against, never re-litigated inside a packet — so either R10.1 is in violation, or R28 needs a
dated amendment. **The mechanical half is the expensive part:** `RescoreBrain` normalises
`DistToTargetNorm` by `GetTuning().SightRadius`, which is exactly the consequence R28 predicts —
two bots at the same 900 uu from the same enemy produce 1.0 and 0.5, so any `DistanceWeight` a
designer ever enables means four different things. `DistanceWeight` is currently 0.0 everywhere,
so nothing is broken TODAY; it is a trap armed for whoever turns it on.

## HIGH, still open — law 3

**Bot tuning lives in THIRTEEN places** and the runtime winner is a binary generated from C++.
`BNBotAuthoring` calls `Table->EmptyTable()` and re-mints every row from
`ABNBotController::DefaultTuning` / `UBNBotBrain::DefaultRow`, while `ResolveTuning` makes the
table win at runtime. There is no BN CSV. `Tools/bn/60_dt_bot_ambitions.py` is a third mirror and
**has already drifted** — it still carries Fight `commitSeconds` 3.0 and Survive
`interruptBelowHealthNorm` 0.35 against the founder's 8.0 / 0.15, so running its documented
`apply` path silently reverts two explicit tuning decisions with no diff and no compile error.
Fix is one direction of flow, not a patch.

## MEDIUM — owners named, all outside AI/

- **Ammo replicates to everyone** (`Weapons/BNWeapon.cpp:44-46`, unconditional `DOREPLIFETIME`).
  `docs/contracts/netcode.md:59` names ammo `COND_OwnerOnly`. A modified client reads every
  enemy's magazine and reserve; bots are the dominant emitter. **netcode-builder.**
- **A bot despawned mid-match vanishes.** `BNGameMode.cpp:454-465` evaluates the yield branch
  before the live-match guard, so a fifth human joining destroys a bot mid-firefight with no death
  ability, no cue, no killfeed, and its scoreboard row and kills disappear. The comment above it
  reasons carefully about the mirror case. **builder / match owner.**
- **The tree runs on a corpse for 3 s.** `StopLogic` only fires in `OnUnPossess`, which
  `RespawnPlayer` does not reach for `RespawnDelay`. Bounded only by luck:
  `GiveUpAfterNoProgressSeconds` (12) currently exceeds it, so nothing blacklists a killer — raise
  `RespawnDelay` past 12 and every bot ignores the player who killed it for 6 s after respawn.
- **Law 4 was routed around, not obeyed.** The controller's tick is off; the per-frame work moved
  into StateTree task ticks. `Shoot` runs three ticking tasks, two of which call
  `LineOfSightTo` (up to 7 traces each) EVERY tick, with `TargetPlayers=8`. The quality bar is
  30 Hz with 8 fighters and nobody has measured it. Needs a ledger row bounding what may run in a
  task tick, plus a memoised LOS.
- **The determinism claim is false.** Seeds are `GetTypeHash(this)` (a pointer hash) and
  `GFrameCounter`. The header concedes "stable within a run, not across one" while the class
  comment claims §5 determinism. `Breachpoint.Bots.*` — the suite R11 and DoD #9 both name as the
  pin — **does not exist**.

## The architecture verdict, recorded because it is worth arguing with

The ambition layer is *"a two-branch `if` wearing a GOAP costume"*: with the shipped rows, Survive
out-scores Fight only across a 1.7-percentage-point band of health, and the interrupt bypasses the
scoring anyway. `DistanceWeight` is read by nothing. Net behaviour is
`if (h < 0.15 && target) Survive; else if (target) Fight; else Roam`. **The recommendation is not
to delete it** — it is the seam where target priority, coordination and personality plug in — but
the "GOAP" claim does not survive scrutiny and should not be made in a portfolio.

Also flagged as shallow: cover is a pathing twitch that fired 5 times in a whole match and never
shoots from cover; every bot in the lobby is the same tier because `SpawnBot` never sets one; and
**bots never press Sprint**, so a sprinting human outruns every bot in the game, always.

## Ranked next work (from the ai-builder roadmap)

1. Cache `HasLineOfSightToTarget` behind a ~0.1 s stamp (kills the per-frame traces).
2. Bots press `Input.Sprint` when out of LOS or beyond ~1.5× acceptance radius.
3. Seed from identity, not pointers — then build `BreachpointNext.Bots.*` and pin R11 and R28.
4. Target SELECTION instead of a first-perceived slot that is never re-evaluated.
5. Reload discipline: order `Cover` above `Rearm` so a bot ducks before it reloads in the open.
6. Per-bot tiers from the GameMode + move behavioural numbers onto the tuning row.
7. Cover that fights: reload, peek, burst, step back.
