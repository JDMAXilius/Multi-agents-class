# Contract — GAS Purity (one API for gameplay; exceptions are named or they are bugs)

Status: v1 · Owner: sim-builder (netcode-builder co-signs prediction rules) · Binds every
packet touching combat, abilities, attributes, or anything that changes a fighter's state.
The principle in one line: **gameplay state changes flow through Abilities, Effects,
Attributes, and Tags — or they are findings.** Purity is not aesthetics; it is what makes
prediction, rollback, replication, and testing come for free instead of being hand-built
per feature.

## The Purity Laws

1. **Attributes mutate only through GameplayEffects.** No setter calls, no `+=`, no direct
   writes outside the AttributeSet's own Pre/Post hooks. This is what makes every change
   predictable, rollback-safe, server-authoritative, and visible to the one damage pipeline.
2. **Abilities are the only entry point for actions.** Every player/bot verb (fire, reload,
   swap, grenade, melee, grapple, sprint) is an ability activated by InputTag through the
   ASC. No "quick path" that calls gameplay functions directly from input or UI.
3. **One damage pipeline.** All damage — hitscan, projectile, radial, melee — is `GE_Damage`
   (SetByCaller + `Damage.*` tags) through `BRDamageExecCalc`. **The engine damage API is
   BANNED:** no `AActor::TakeDamage`, no `ApplyDamage/ApplyRadialDamage`, no
   `FDamageEvent`. Radial = our own overlap query → per-target GE application (grenade and
   rocket share it).
4. **Costs and cooldowns are GEs.** Never hand-tracked floats. GAS predicts and rolls them
   back automatically — a rejected ability refunds itself with zero custom code, but ONLY
   if the cost went through the GE path.
5. **Tags are the state language.** Replicated gameplay state reads as tags applied **by
   GEs** (`State.Movement.Sprinting`, `State.Combat.RecentDamage`, `State.Dead`) — never
   `bIsSprinting` booleans, and never loose `AddLooseGameplayTag` for anything gameplay
   depends on (loose tags don't replicate reliably; they are for local-cosmetic use only).
   State *queries* (HasMatchingGameplayTag) may live anywhere; state *mutation* is
   GE-only.
6. **Cues carry all cosmetic consequences.** No FX/audio/camera-shake spawned directly in
   ability or actor code. Predicted presentation = `OnActive/WhileActive` cues (auto
   rolled back); confirmed one-shots = `Executed` cues on the server path. This is the law
   that makes misprediction invisible.
7. **Events over calls at the seams.** Cross-system consequences travel as gameplay events
   (`Event.Death` → GameMode) and delegates — an ability never reaches into GameMode,
   UI, or another system's internals.
8. **Death is a GameplayEffect.** `GE_Death` (infinite) applies `State.Dead`; the ability
   base lists `State.Dead` in ActivationBlockedTags, so death disables every verb through
   ONE mechanism instead of scattered checks. Respawn removes it and re-applies
   `GE_InitStats`. Dying is a state change like any other — it obeys law 1.

## Named Exceptions (deliberate, bounded, and documented — the ledger)

| Exception | Why it is outside GAS | The bound that keeps it honest |
|---|---|---|
| **Ammo** (`BRWeaponInstance` properties, not attributes) | Per-WEAPON state; attributes are per-ASC — modeling two mags as attributes means attribute proliferation per slot | Mutated ONLY inside `BRGA_WeaponFire`/`BRGA_WeaponUtility`; server-authoritative; `COND_OwnerOnly` replication IS the correction path for a mispredicted decrement (Lyra parity) |
| **Movement** (CMC: grapple RMS, jump, and every movement *state*) | Movement prediction/reconciliation is CMC's machinery — re-homing the STATE into attributes would forfeit saved-move replay | GAS supplies the *decision* (abilities + tags: `State.Movement.Sprinting`); CMC applies the *motion* and carries the flags in `FSavedMove_BR`. **Amended 2 Aug 2026 — see the split below.** |
| **Match meta** (phase, timer, team scores, K/D/A) | Match bookkeeping is framework state, not combat simulation — no prediction, no per-fighter effects | Server-only mutation in GameMode; replicated via GameState/PlayerState RepNotify; nothing in the combat sim reads it for gameplay decisions except bots (as observed events) |
| **Projectile Tick** (`ABRProjectile` — added 14 Aug 2026, ruling R42/D-1, BP90) | A projectile needs a per-frame position update; a recursive `SetTimerForNextTick` is the same cost with worse honesty — the no-Tick law gets one declared hole instead of a hidden one | Tick enabled on `ABRProjectile` ONLY; the tick body does position + trace, nothing else — no gameplay decision in Tick; Tick disabled the moment the projectile deactivates. No other class inherits this row |

Anything that wants to join this ledger arrives as a **contract change in its own packet**,
with the rationale and the bound — never as an inline shortcut.

### Amendment, 2 Aug 2026 — movement splits VALUE from STATE

The movement exception previously read as "speed is the CMC's, full stop", which also meant no
GameplayEffect could ever buff or debuff movement. That is too strong. The line is not
*movement vs GAS*; it is **which half a client correction has to replay**.

| Half | Home | Why |
|---|---|---|
| **State** — is this pawn sprinting *this frame* | `bWantsToSprint` in `FSavedMove_BR` | changes several times a second and MUST replay exactly. A correction replays saved moves; a flag inside the move reproduces the original decision, an attribute read during replay returns its value *now* and diverges. |
| **Value** — how fast sprinting is | `MoveSpeedBase`, `SprintSpeedMultiplier` attributes | changes only when a GE lands. A one-frame magnitude discrepancy is invisible; a one-frame on/off discrepancy is the rubber-band. |

**The bound.** Attributes may supply *magnitudes* only. No movement attribute may encode a
per-frame decision, and nothing read by `GetMaxSpeed()` may become the thing that decides
whether a movement state is active — that stays in the saved move. `MoveSpeedBase` overrides
only the walking case and only when non-zero, so `MaxWalkSpeedCrouched` and the non-ground
modes remain `Super::GetMaxSpeed()`'s.

**Zero means unset**, everywhere: an ASC that has not replicated (a joining client, a bot with
no attribute set) falls back to the CMC's configured speed and `CT_Combat.csv`'s curve, so the
pawn never freezes at zero speed because a value has not arrived.

## Enforcement (how the crew catches impurity)

- **Grep gates (verifier, every rung-2 run):** `TakeDamage`, `ApplyRadialDamage`,
  `ApplyPointDamage`, `AddLooseGameplayTag` (outside cosmetic-marked sites), direct
  attribute setter calls outside the AttributeSet — any hit is a reported finding.
- **Spec gates:** `Breachpoint.Sim.*` asserts attribute changes ONLY occur under GE
  application; a deliberate direct-write test proves the grep/who-changed-it detection.
- **Critic REFUTER standing questions** for any combat packet: "what mutates outside a GE?
  what FX spawns outside a cue? what state is a bool that should be a tag? what damage
  path skips the ExecCalc?"
