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
| **Movement** (CMC: speed, grapple RMS, jump) | Movement prediction/reconciliation is CMC's machinery — re-homing it into attributes would forfeit saved-move replay | GAS supplies the *decision* (abilities + tags: `State.Movement.Sprinting`); CMC applies the *motion* and carries the flags in `FSavedMove_BR` |
| **Match meta** (phase, timer, team scores, K/D/A) | Match bookkeeping is framework state, not combat simulation — no prediction, no per-fighter effects | Server-only mutation in GameMode; replicated via GameState/PlayerState RepNotify; nothing in the combat sim reads it for gameplay decisions except bots (as observed events) |

Anything that wants to join this ledger arrives as a **contract change in its own packet**,
with the rationale and the bound — never as an inline shortcut.

## Enforcement (how the crew catches impurity)

- **Grep gates (verifier, every rung-2 run):** `TakeDamage`, `ApplyRadialDamage`,
  `ApplyPointDamage`, `AddLooseGameplayTag` (outside cosmetic-marked sites), direct
  attribute setter calls outside the AttributeSet — any hit is a reported finding.
- **Spec gates:** `Breachpoint.Sim.*` asserts attribute changes ONLY occur under GE
  application; a deliberate direct-write test proves the grep/who-changed-it detection.
- **Critic REFUTER standing questions** for any combat packet: "what mutates outside a GE?
  what FX spawns outside a cue? what state is a bool that should be a tag? what damage
  path skips the ExecCalc?"
