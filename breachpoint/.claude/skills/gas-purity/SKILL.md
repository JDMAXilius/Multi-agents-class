---
name: gas-purity
description: GAS implementation patterns for BREACHPOINT (UE 5.8, multiplayer, PlayerState ASC). Load for ANY work touching Source/Breachpoint/AbilitySystem — abilities, GameplayEffects, attributes, cues, prediction, TargetData, cooldowns, the damage pipeline, or ability input. The contract (docs/contracts/gas-purity.md) is LAW and wins on any conflict; this skill is HOW the lawful patterns are built.
---

# GAS Purity — the implementation patterns

**Authority note (read first):** `docs/contracts/gas-purity.md` defines what is *forbidden*;
this skill shows how the *lawful* version is built. On any conflict, **the contract wins**
and the conflict is a finding against this skill — fix the skill in the same packet.

## 1. ASC setup (the decisions are already made — implement them exactly)

- ASC lives on **PlayerState** (`BRAbilitySystemComponent`), avatar = the character.
  Server: `InitAbilityActorInfo` on possession. Client: again in `OnRep_PlayerState`
  (and re-run on avatar re-possession — respawn is the bug farm).
- `ReplicationMode::Mixed` — owner receives full GEs, simulated proxies get tags + cues
  only. Correct for player-shaped actors, **bots included** (they sit on PlayerStates).
- `SetShouldTick` never; the ASC is event-driven like everything else (law 4).
- Attribute init via `GE_InitStats` (SetByCaller from `DT_*` rows) at spawn — never
  `SetBaseAttributeValue` calls scattered in code.

## 2. Attributes (`BRAttributeSet`)

- `ATTRIBUTE_ACCESSORS` for every attribute; replicate with
  `GAMEPLAYATTRIBUTE_REPNOTIFY` in each `OnRep_*` (prediction-safe).
- `PreAttributeChange` = **clamp CURRENT values only** (Shields 0..MaxShields, Health
  0..MaxHealth). Never gameplay reactions here — it also runs during prediction replay.
- `PostGameplayEffectExecute` (server-only) is the ONE reaction point:
  `IncomingDamage` (meta attribute, never replicated) → shields-first split → apply
  `GE_RecentDamage` (the 2.5 s regen gate tag) → health ≤ 0 → send `Event.Death`
  gameplay event + broadcast the death delegate. GameMode reacts to the event — the
  attribute set never talks to game flow directly.
- Shield regen: `GE_Regen` = infinite periodic GE (SetByCaller rate from the table),
  its ongoing-tag requirement blocks on `State.Combat.RecentDamage` — regen "starts
  after 2.5 s" falls out of tag removal, zero timer code.

## 3. Generic GameplayEffects (law 7: parameterize, never proliferate)

> **R18: the seven generic GEs are C++ `UGameplayEffect` subclasses, not Content assets.**
> Constructor-authored, referenced by class, living in
> `Source/Breachpoint/AbilitySystem/Effects/`. Read "asset" below as "the one generic GE
> class" — the pattern is identical, only the authoring surface changed. `AddDynamicAssetTag`
> keeps its name (it is engine API for the *spec*, unrelated to `.uasset` files).

- One `GE_Damage` (Instant): magnitude = `SetByCaller.BaseDamage`, execution =
  `BRDamageExecCalc`. EVERY damage source applies this one GE class with its own
  SetByCaller value + dynamic `Damage.*` tags added to the **spec** at apply time
  (`AddDynamicAssetTag` on the spec — the GE itself stays generic).
- `MakeOutgoingSpec` on the SOURCE ASC (correct attribution/context), then
  `ApplyGameplayEffectSpecToTarget` **on the server** (the exec calc is server truth;
  clients see results via replication + cues).
- In 5.8, effect behaviors are **GE components** (target-tags, granted-tags, immunity,
  chance). Under R18 these are constructed in the GE class's constructor
  (`CreateDefaultSubobject` per component, added to `GEComponents`) rather than configured
  in the editor — **this is the riskiest part of R18 and BP02 proves it on one GE before
  writing six.** New content never adds a GE either way; it adds rows and parameters.
- `GE_Cooldown` (Duration, SetByCaller `SetByCaller.CooldownDuration`): the generic-
  cooldown pattern — the ABILITY injects its own cooldown tag into the spec's granted
  tags and overrides `GetCooldownTags()` to return {its tag} ∪ base. One GE class, every
  ability, per-ability durations from the weapon/ability table.

## 4. Abilities (`BRGameplayAbility` subclasses)

- Instancing `InstancedPerActor`; NetExecutionPolicy `LocalPredicted` for everything a
  player feels (fire, sprint, grapple, melee); `ServerInitiated` only when the server
  must originate (respawn grants).
- Activation flow: `CanActivateAbility` (cheap, tag-driven — blocked-by
  `State.Dead`/`State.Shields.Broken` rules live in tags, not branches) →
  `CommitAbility` (cost + cooldown, atomically) → do the work → `EndAbility` ALWAYS
  (every early-return path too; a leaked ability instance is a finding).
- **What may run inside the prediction window:** montages, GameplayCues, tag
  application via predicted GEs, `WaitTargetData`. **What may not:** spawning
  authoritative actors (rocket projectile spawns server-side; the client shows a cue
  ghost), anything reading server-only state.
- Ability ↔ input: granted via `BRAbilitySet` rows carrying `InputTag.*`; the ASC's
  input buffer activates by tag (see the Input layer doc §3.2 in the architecture).
  Press/release must reach the ASC (`AbilityInputTagPressed/Released`) or
  `WaitInputRelease` (sprint end, cook throw) silently never fires.

## 5. The fire pipeline (hitscan, the advanced shape)

```
press → BRGA_FireHitscan activates (LocalPredicted)
  → client traces (camera, spread from DT_Weapons row)
  → builds FGameplayAbilityTargetDataHandle from the hit
  → ServerSetReplicatedTargetData inside the prediction window
  → SERVER re-validates the trace (range, angle, rate) — client data is a CLAIM
  → server applies GE_Damage spec (SetByCaller.BaseDamage, Damage.Kinetic
    [+ Damage.Headshot from the validated hit bone])
  → GameplayCue.Weapon.<X>.Fire fires predictively on the shooter,
    replicates to others via the ASC (Mixed mode handles routing)
```
- Enable **`ServerAbilityRPCBatch`** on the ASC and batch fire abilities: activate +
  TargetData + end in ONE RPC per shot (the GAS-shooter bandwidth optimization; it is
  why fire abilities end same-frame rather than staying active).
- Rear-melee / headshot legitimacy is decided **server-side in the exec calc** from
  captured context — never trusted from the client's tag claim.

## 6. Cues (all FX, no exceptions — law: FX never lives in ability bodies)

- Naming: `GameplayCue.<Domain>.<Thing>.<Event>` — native tags only.
- `Executed` (one-shot: muzzle, impact, hitmarker) vs `Added/Removed` (looping:
  shield-break crackle, regen shimmer). Static cues for stateless one-shots;
  instanced cue actors only when the effect holds state.
- Prediction: cues raised inside a predicted ability play immediately for the
  predictor and are suppressed on the replicated echo (the ASC handles it — do not
  hand-roll "did I already play this" flags).
- MetaSounds hook here: the cue notify triggers the MetaSound; audio parameters
  travel as cue parameters (magnitude → impact volume).

## 7. Death, respawn, and state tags

- Death is data: `GE_Death` (Infinite) grants `State.Dead`; every ability lists
  `State.Dead` in ActivationBlockedTags; movement/aim shutdown reacts to the tag.
- `Event.Death` → GameMode (scoring, respawn timer via `DT_MatchRules`); respawn =
  new avatar, `InitAbilityActorInfo` re-run, ability sets re-granted from loadout
  (grant handles from `BRAbilitySet` make revoke clean), `GE_InitStats` re-applied.
- Per-life state = tags from GEs; per-match state = attributes/PlayerState. Nothing
  gameplay-visible lives in ability member variables across lives.

## 8. The named exceptions (the contract's ledger — memorize, don't rediscover)

**Ammo, movement physics, match meta** do NOT flow through attributes/GEs (ledger
entries in the contract). Ammo lives on the weapon instance (replicated,
server-mutated, cost-checked by the fire ability via CanActivate); the Grappleshot
pull is a **CMC root-motion source** (netcode-owned, predicted) — GAS decides
*whether*, CMC executes *motion*. Sprint shows the split: `BRGA_Sprint` owns the
decision/tag/cancel rules; the CMC reads the tag for speed.

## 9. What the hook and the critic will catch (self-check before handoff)

`TakeDamage`/`ApplyRadialDamage`/`ApplyPointDamage` anywhere · `ConstructorHelpers` ·
unseeded `FMath::RandRange` · a literal gameplay number next to a gameplay noun ·
`SetBaseAttributeValue` outside init · FX played from an ability body instead of a cue
· a new GE class that a SetByCaller parameter could have expressed (or ANY GE authored as
a `.uasset` — R18) · an ability
without `EndAbility` on every path · client-built TargetData applied without server
re-validation · `State.*` set by hand instead of granted by a GE.

Rung honesty (always): prediction bugs are INVISIBLE in single-process PIE — the
floor for "the ability works" is multi-process with a simulated proxy watching.
