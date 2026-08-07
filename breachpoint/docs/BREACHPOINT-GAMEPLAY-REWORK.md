# BREACHPOINT — Gameplay Layer Rework (v3)
## The from-scratch gameplay architecture, and the road to it

**Scope:** gameplay programming ONLY — Core, Data, Input, AbilitySystem, Character,
Equipment, Match. **Out of scope:** UI (`UI/`), Online (`Online/`), Telemetry, AI brain
(`AI/`), Tools. Those keep their current shape and meet this layer at named seams.

**Cut:** 7 August 2026 · **Supersedes:** `BREACHPOINT-ARCHITECTURE.md` §3.1–§3.6 and
§3.11 for the folders it names. That document remains authoritative for UI, AI, Online,
Telemetry, Tests, and for §5/§6 (GAS coverage + replication surface), which this rework
does not change.

**Binding:** `docs/contracts/gas-purity.md` (all eight laws) · `docs/contracts/netcode.md` ·
`docs/contracts/data-and-assets.md` · `docs/contracts/testing.md` (the ladder).

---

## 0. Why a rework, and what it is not

This is **not** a rebuild of the framework. The framework is sound — the reference audit
(three shipped/complete UE shooters read end to end, 33k lines) confirmed that every one of
them pays for a law Breachpoint already has:

| Breachpoint law | What the references show costs when it's missing |
|---|---|
| One damage pipeline | ZoransResistance has **five** damage entry points, each re-implementing spec construction; one bug (`FMath::Rand()` where `FRand()` was meant) is copy-pasted into two of them; its `DamageExecutionCalculation` reads the magnitude and does nothing with it |
| Target Actors rejected | Zorans built them anyway: `AZoransTargetActor` + two custom ability tasks + three hand-written `NetSerialize` structs ≈ 600 lines to move a hit client→server. TargetData in a prediction window is ~30 |
| No gameplay Tick | ShooterCore ticks per frame for crosshair spread, enemy detection, two timelines, and a full grenade-spline rebuild |
| State = GE-applied tags | ShooterCore tracks weapon state in five bools + two modulo counters; the function keeping them consistent is 180 lines of nested switch |

What the rework fixes is narrower and real:

1. **The gameplay layer accreted around a deleted plan.** The ticket board that built it was
   removed (46 tickets, founder direction — see `tickets/HANDOFF.md`). What survives on disk
   no longer matches `BREACHPOINT-ARCHITECTURE.md` §3, and no scanner checks it any more.
2. **The UE template is still compiling inside the runtime module.** `Variant_Horror/` (4
   pairs), `Variant_Shooter/` (14 pairs — a complete parallel shooter with its own weapon,
   projectile, AI controller and character), plus `breachpointCharacter/GameMode/
   PlayerController`. **36 files. Nothing in the `BR` tree references any of them** — the
   only couplings are self-referential plus six `PublicIncludePaths` entries in
   `Breachpoint.Build.cs`, and `Slate` sits in the dependency list solely to keep them
   linking.
3. **Four genuine gaps** the references expose, listed in §5.

## 1. The governing ideas (five)

Everything below is a consequence of these. If a proposed file does not serve one of them,
it does not get built.

1. **The ASC on PlayerState is the trust line.** It outlives the pawn, so respawn is free.
2. **Abilities are the only verb.** Input produces a *tag*, never a call. Bots press the
   same tags through the same ASC — there is no privileged path and none can be added.
3. **Damage has exactly one door, and it is a door, not a convention.** No ability, actor,
   or library builds a damage spec itself.
4. **Movement state lives in tags; motion lives in CMC.** Prediction and reconciliation are
   engine-tier and are never re-implemented.
5. **C++ knows tags and row handles. It never knows an asset.**

---

## 2. File layout — 29 units, 53 files

```
Source/Breachpoint/
│
├── Core/ ──────────────────────────────────────────────── 2 units
│   ├── BRGameplayTags.h/.cpp        every native tag, one file
│   └── BRCore.h/.cpp                log channels · collision channels · team attitude
│
├── Data/ ──────────────────────────────────────────────── 2 units
│   ├── BRDataRows.h                 every row struct (header-only, no .cpp)
│   └── BRGameData.h/.cpp            GameInstance subsystem: owns tables, resolves soft refs
│
├── Input/ ─────────────────────────────────────────────── 1 unit
│   └── BRInput.h/.cpp               UBRInputConfig (DataAsset) + UBRInputComponent
│
├── AbilitySystem/ ─────────────────────────────────────── 8 units
│   ├── BRAbilitySystemComponent.h/.cpp   input buffer · tag activation · batched RPC
│   ├── BRAttributeSet.h/.cpp             Health · Shields · IncomingDamage · move magnitudes
│   ├── BRAbilitySet.h/.cpp               DataAsset: what to grant, handles to revoke
│   ├── BRGameplayAbility.h/.cpp          base: activation policy · death gate · accessors
│   ├── BRGameplayEffects.h/.cpp          the 8 generic GEs, one header
│   ├── BRGameplayCues.h/.cpp             cue handler classes
│   ├── BRDamage.h/.cpp             ★    THE damage door — point + radial, nothing else
│   └── BRDamageExecCalc.h/.cpp           the one execution: mods → shields → health
│
│   └── Abilities/ ─────────────────────────────────────── 6 units
│       ├── BRGA_WeaponFire.h/.cpp        hitscan OR projectile, chosen by row
│       ├── BRGA_WeaponUtility.h/.cpp     Reload + Swap (two tiny siblings)
│       ├── BRGA_Movement.h/.cpp          Sprint + Jump (two tiny siblings)
│       ├── BRGA_Melee.h/.cpp             notify-window trace, rear arc server-checked
│       ├── BRGA_Grenade.h/.cpp           cook → predicted ghost + authoritative projectile
│       └── BRGA_Grapple.h/.cpp           root-motion source through CMC
│
├── Character/ ─────────────────────────────────────────── 2 units
│   ├── BRCharacter.h/.cpp                the body: meshes, GAS wiring, death consequence
│   └── BRCharacterMovementComponent.h/.cpp  saved-move flags + RMS grapple
│
├── Equipment/ ─────────────────────────────────────────── 4 units   (was Weapons/ — D-2)
│   ├── BREquipmentComponent.h/.cpp       slots · equip grants an AbilitySet
│   ├── BRWeaponInstance.h/.cpp           replicated UObject: row handle + ammo
│   ├── BRProjectile.h/.cpp               time-parameterized, owner-predicted
│   └── BRWeaponPickup.h/.cpp             pickup + power-weapon spawner
│
└── Match/ ─────────────────────────────────────────────── 4 units
    ├── BRGameMode.h/.cpp                 phases · respawn · kill attribution
    ├── BRGameState.h/.cpp                phase · scores · killfeed ring
    ├── BRPlayerState.h/.cpp        ★    hosts the ASC + AttributeSet · team · K/D/A
    └── BRPlayerController.h/.cpp         input relay · death cam · UI intent boundary
```

---

## 3. What each unit does

### 3.1 Core — 2

| Unit | Job |
|---|---|
| `BRGameplayTags` | Every `UE_DEFINE_GAMEPLAY_TAG`. Four families: `InputTag.*`, `Ability.*`, `State.*` (Dead, Movement.Sprinting, Shields.Broken, Combat.RecentDamage), and the payload family `Damage.*` / `SetByCaller.*` / `Event.*`. If a tag is not declared here it does not exist. Extension rule for the montage→gameplay seam stays `Event.<Verb>.<Moment>` (R17). |
| `BRCore` | Log channels (`LogBRCombat/Net/AI`), collision channel aliases matching `DefaultEngine.ini`, and one free function: `BRTeams::GetAttitude(const AActor*, const AActor*)` wrapping UE's native `IGenericTeamAgentInterface`. **This is the entire team system.** 4v4 with two fixed sides needs an id and an attitude function; AI perception reads the same interface for free. |

### 3.2 Data — 2

| Unit | Job |
|---|---|
| `BRDataRows.h` | Every row struct, one header, no `.cpp`. `FBRWeaponRow` carries numbers (damage, RPM, mag, spread, range), shaping (`FRuntimeFloatCurve DamageFalloff`, `TMap<FName,float> BodySectionMods`), an `EBRFireMode` (Hitscan / Projectile), and **soft** refs to mesh, cues, and ability set. Plus `FBRMatchRules`, `FBRLoadoutRow`. |
| `BRGameData` | GameInstance subsystem. Owns every `UDataTable`/`UCurveTable` handle, exposes typed lookups (`GetWeaponRow(FName)`, `GetCombatCurve(FName)`), and is **the only place** a `TSoftObjectPtr` is resolved through the streamable manager. Nothing else in the codebase holds a table pointer, calls `ConstructorHelpers`, or starts an async load. |

### 3.3 Input — 1

One unit, two classes, always used together.

```
IMC_Default → IA_Fire → UBRInputComponent → InputTag.Weapon.Fire
                                          → ABRPlayerController::InputTagPressed(Tag)
                                          → ASC->AbilityInputTagPressed(Tag)
                                          → activates the AbilitySet entry carrying that tag
```

`UBRInputConfig` is a `UDataAsset` of `{TSoftObjectPtr<UInputAction>, FGameplayTag}` in two
lists — native (Move, Look) and ability-driven (everything else). **A new ability is a data
row, not a line of binding code.** Because press *and* release both reach the ASC,
`WaitInputRelease` / hold / toggle tasks work without special-casing.

### 3.4 AbilitySystem — 8 + 6 abilities

**`BRDamage` is the architectural centrepiece.** A static function library with exactly two
public entries:

```cpp
namespace BRDamage
{
    void ApplyPoint (const FBRDamageRequest&, const FHitResult&);
    void ApplyRadial(const FBRDamageRequest&, const FVector& Origin, float Radius);
}
```

`FBRDamageRequest` carries `{Instigator, EffectCauser, BaseDamage, DamageTags, WeaponRow}`.
Both build the same `GE_Damage` spec with the same SetByCaller magnitude and hand it to the
target ASC. **Abilities never call `MakeOutgoingSpec` for damage.** This makes purity law 3
*structural* rather than aspirational — Zorans grew a fifth damage path precisely because
nothing prevented a second. `ApplyRadial` is also the sanctioned replacement for the banned
`ApplyRadialDamage`: sphere overlap → per-body-section visibility checks → falloff curve →
one GE per victim.

| Unit | Job |
|---|---|
| `BRAbilitySystemComponent` | Input-buffered tag activation — the buffer lives here, at the one choke point. `ReplicationMode::Mixed` for every fighter (humans and bots alike; `Minimal` is for non-player-shaped AI and ours are player-shaped). `ServerAbilityRPCBatch` enabled so a shot is ONE packet: activate + TargetData + end. |
| `BRAttributeSet` | Health/MaxHealth, Shields/MaxShields, `IncomingDamage` (meta), plus the movement *magnitudes* the gas-purity amendment of 2 Aug allows (`MoveSpeedBase`, `SprintSpeedMultiplier` — magnitudes only, never per-frame decisions). `PreAttributeChange` clamps. `PostGameplayEffectExecute` is the single transition point: applies `GE_RecentDamage`, flips `GE_ShieldsBroken`, and on zero Health broadcasts `Event.Death`. Zero means unset, everywhere. |
| `BRAbilitySet` | `UDataAsset` listing abilities (each with its InputTag), effects, and attribute sets. `GiveToAbilitySystem()` returns a handle struct so `TakeFromAbilitySystem()` is exact — unequip never guesses. |
| `BRGameplayAbility` | Base class: activation-policy enum (OnPressed / WhileHeld / Toggle), `State.Dead` in `ActivationBlockedTags` so **death disables every verb through one mechanism**, cost/cooldown wired to the generic GEs, cancel hygiene, typed accessors. |
| `BRGameplayEffects` | Eight constructor-authored `UGameplayEffect` subclasses in ONE header (R18 — GEs are C++ classes, not Content assets): `Damage`, `Regen`, `Cooldown`, `InitStats`, `RecentDamage`, `Death`, `ShieldsBroken`, `AbilityCost`. All parameterized by SetByCaller + dynamic tags. New content is new *rows*, never new effects. |
| `BRGameplayCues` | Cue handler classes — the **only** thing in the codebase that spawns FX, plays sound, or shakes the camera. Predicted presentation via `OnActive`/`WhileActive` (auto rolled back); confirmed one-shots (kill toast, shield break) via `Executed` on the server path. |
| `BRDamageExecCalc` | The one execution and the only place damage *math* happens: read `SetByCaller.BaseDamage` → `Damage.*` tag multipliers from `CT_Combat` → body-section modifier → absorb into Shields → overflow to Health. Weak point is *derived* (any section modifier > 1.0), never declared. |

**The six abilities**

| Ability | The load-bearing part |
|---|---|
| `BRGA_WeaponFire` | Reads `EBRFireMode` off the row and branches hitscan/projectile — one ability serves every weapon. Hitscan: predicted cue + recoil + ammo decrement → `FScopedPredictionWindow` → client trace → `FGameplayAbilityTargetData_SingleTargetHit` → batched RPC → **server revalidates** (rate ≤ RPM + tolerance, ammo > 0, direction within cone of server-known muzzle, range ≤ row max) → `BRDamage::ApplyPoint`. Rejection is a silent drop: the client sees a whiff, a cheater sees nothing. |
| `BRGA_WeaponUtility` | `UBRGA_Reload` + `UBRGA_Swap`. Both commit on montage notifies (`Event.Weapon.ReloadCommit` / `SwapCommit`), never on timers — the animation *is* the timing, which is what keeps 1P and 3P honest. |
| `BRGA_Movement` | `UBRGA_Sprint` (WhileHeld; grants `State.Movement.Sprinting` via ActivationOwnedTags; **no cost**) + `UBRGA_Jump`. Fire/Melee/Grenade list Sprint in `CancelAbilitiesWithTags` — firing ends sprint with **zero code inside the sprint ability**. |
| `BRGA_Melee` | Trace window opens on `Event.Melee.WindowBegin`, closes on `…WindowEnd`. Rear arc re-checked server-side. Routes to `BRDamage::ApplyPoint` with `Damage.Melee[.Rear]`. |
| `BRGA_Grenade` | Client spawns a cosmetic ghost immediately (feel); server spawns the authoritative `ABRProjectile`. Correlated by a `LocalIndex` the ASC mints — see §3.6. Cost is `GE_AbilityCost`, never a hand-decremented counter. |
| `BRGA_Grapple` | **The netcode packet.** Server validates the hit, then asks the CMC to apply a **root-motion source** for the pull. RMS rides CMC's saved-move pipeline, so the grapple is client-predicted and server-reconciled with no bespoke reconciliation code. Rejection leaves zero state. Critic REFUTER gate; rungs 4a AND 4b. |

### 3.5 Character — 2

| Unit | Job |
|---|---|
| `BRCharacter` | The pawn is a body, not a brain. Dual mesh (`Mesh1P` OnlyOwnerSee + no shadow; `Mesh3P` OwnerNoSee + casts shadow). Implements `IAbilitySystemInterface` by **forwarding to PlayerState**. Calls `InitAbilityActorInfo(PS, this)` in `PossessedBy` (server) **and** `OnRep_PlayerState` (client). Death is a *consequence*: play the cue, ragdoll `Mesh3P`, disable collision and input, wait for GameMode. **No health, no scoring, no weapon logic on the pawn, ever.** One combat helper only: the server-side rear-arc check for melee. |
| `BRCharacterMovementComponent` | Subclass, never a rewrite. `FSavedMove_BR` + `FNetworkPredictionData_Client_BR` carry compressed flags for sprint and grapple so state replays correctly through corrections. Owns grapple detach rules (arrival, jump-cancel); the *decision* to grapple stays in the ability. `GetMaxSpeed()` reads `MoveSpeedBase` for the walking case only and only when non-zero. Halo-feel numbers are CMC defaults in config, not code. |

### 3.6 Equipment — 4

| Unit | Job |
|---|---|
| `BREquipmentComponent` | Two weapon slots + grenade. Equip = grant the weapon's `BRAbilitySet` and async-load its soft mesh via `BRGameData`. Replicated subobject list. Pickup/drop are server-validated RPCs. |
| `BRWeaponInstance` | Replicated `UObject`, not an actor: row handle + ammo (`COND_OwnerOnly`). An actor per weapon is three extra replicated actors per player for nothing. Ammo is a named gas-purity exception — mutated only inside `BRGA_WeaponFire`/`WeaponUtility`, and `COND_OwnerOnly` replication IS the correction path for a mispredicted decrement. |
| `BRProjectile` | **Time-parameterized, not physics-simulated.** Position = `f(spawnPoint, launchDir, elapsed, gravity)`. `IsNetRelevantFor` returns **false for the instigator** — the owner never receives the replicated copy and runs its own local one instead. Server hits route back to the owner by `LocalIndex` (minted by the ASC). Client/server divergence is **lerped away over a capped sync window**, never snapped. |
| `BRWeaponPickup` | `ABRWeaponPickup` + `ABRPowerWeaponSpawner` (90 s node). The spawner replicates a server *timestamp* and `OnRep`s it; clients derive the countdown locally. No replicated ticker. |

### 3.7 Match — 4

| Unit | Job |
|---|---|
| `BRPlayerState` | **Hosts the ASC and the AttributeSet.** Team as `FGenericTeamId` (+ `IGenericTeamAgentInterface`). K/D/A. `NetUpdateFrequency` raised — the 1 Hz default is unusable for an FPS scoreboard and unacceptable for an ASC host. |
| `BRGameMode` | Server-only, never replicates. Phase machine on timers and events. Respawn re-possesses and re-points `InitAbilityActorInfo` — attributes and granted abilities survive by construction; per-life state is cleared by removing `GE_Death` and re-applying `GE_InitStats`. Kill attribution including double-KO. Bot fill calls into `AI/`'s manager across the existing seam. |
| `BRGameState` | Phase, `MatchEndServerTime` (clients derive the countdown — no replicated clock), team scores, killfeed ring buffer. |
| `BRPlayerController` | Input tag relay → ASC. Death cam. The boundary UI is allowed to talk to; UI never reaches past it into abilities or attributes. |

---

## 4. The four flows

**Fire — one packet, predicted, server-validated**

```
CLIENT                                          SERVER
  IA_Fire → InputTag.Weapon.Fire
  ASC->AbilityInputTagPressed
  BRGA_WeaponFire::Activate
    ├─ predicted: cue · recoil · ammo−1     (GAS rolls all three back on reject)
    ├─ FScopedPredictionWindow
    │    └─ trace → TargetData_SingleTargetHit
    └─ batched RPC ────────────────────────────► validate: rate · ammo · cone · range
                                                   ├─ reject → silent drop
                                                   └─ accept → BRDamage::ApplyPoint
                                                        └─ GE_Damage → BRDamageExecCalc
                                                             → shields → health
                                                             → PostGEExecute:
                                                                 GE_RecentDamage
                                                                 Event.Death → GameMode
                                                        └─ GameplayCue → all clients
```

**Damage — one door, no exceptions**

```
BRGA_WeaponFire ─┐
BRGA_Melee ──────┤
BRGA_Grenade ────┼──► BRDamage::ApplyPoint / ApplyRadial ──► GE_Damage ──► BRDamageExecCalc
BRProjectile ────┤          (the ONLY builder of a                          (the ONLY math)
future hazards ──┘           damage spec — enforced by
                             grep gate + critic REFUTER)
```

**Life cycle — why respawn is free**

```
              ┌──────────────── BRPlayerState (persists) ─────────────────┐
              │  ASC · AttributeSet · granted abilities · TeamId · K/D/A  │
              └────────────────────────┬─────────────────────────────────┘
                                       │ InitAbilityActorInfo(PS, pawn)
   spawn ──► BRCharacter #1 ──► death ─┴─► BRCharacter #2 ──► death ──► …
             (avatar only)                 (avatar only)

  death   = GE_Death applies State.Dead → every ability blocked by ONE tag
  respawn = new pawn · re-point avatar · remove GE_Death · re-apply GE_InitStats
```

**Input — humans and bots are literally one API**

```
human:  hardware → IA → InputTag ─┐
                                  ├──► ASC->AbilityInputTagPressed(Tag) ──► ability
bot:    brain decision ───────────┘         (no privileged path exists,
                                             and none can be added)
```

---

## 5. The four gaps this rework closes

Named because the current architecture doc does not cover them, and the reference audit
showed each is a real cost:

1. **No "everything is ready" barrier.** ASC, equipment, input config, and UI all initialise
   asynchronously across two paths (`PossessedBy` / `OnRep_PlayerState`) with no rendezvous.
   ZoransResistance solves this with convergent init: each component reports ready, the
   character fires `On_CharacterReady()` only when all have. **Adopted** —
   `BRCharacter::CheckReady()` in BP95.
2. **Client-ghost projectile correlation was unspecified.** `BRGA_Grenade` said "client ghost
   for feel" without saying how the ghost learns the server's outcome. **Adopted** — ASC-minted
   `LocalIndex`, `IsNetRelevantFor` false for instigator, hit routed back by index, divergence
   lerped over a capped window (BP98).
3. **Damage shaping was code, not data.** "headshot ×2, rear-melee lethal" as literals in an
   execution violates law 4. **Adopted** — per-weapon `DamageFalloff` curve + `BodySectionMods`
   map + bone→section map on the character; weak point derived from the modifier (BP93).
4. **The widget↔pawn↔PlayerState binding race.** Out of scope for this rework (it is a `UI/`
   concern) but **recorded here** because the seam is ours: `BRPlayerController` must expose a
   stable "combat surface ready" delegate for `UI/` to bind, rather than UI polling for a pawn.
   Filed to `DECISIONS-OWED.md`, not built in this roadmap.

---

## 6. The "less is more" ledger

| Cut | Instead |
|---|---|
| Team subsystem / team objects (Zorans: 5 files) | `FGenericTeamId` + one attitude function in `BRCore`. AI perception gets it free |
| `AGameplayAbilityTargetActor` + send/listen tasks (Zorans: ~600 lines) | TargetData built inline in a prediction window (~30 lines) |
| Weapon actors | `BRWeaponInstance` as a replicated `UObject` — saves 3 replicated actors per player |
| `BRExplosion` as a class | `BRDamage::ApplyRadial` — an explosion is a damage *shape*, not a thing |
| Separate `BRGA_Reload` / `Swap` / `Sprint` / `Jump` | Two sibling pairs (`WeaponUtility`, `Movement`): 4 units → 2 |
| Separate `BRInputConfig` / `BRInputComponent` | One `BRInput` pair — always used together |
| Per-ability cost/cooldown effects | One `GE_Cooldown` + one `GE_AbilityCost`, SetByCaller-driven |
| Scattered `FDataTableRowHandle` members, `ConstructorHelpers` | `BRGameData` is the only table/asset resolver |
| `BRCombatCurves` as its own unit | Folded into `BRGameData` |

**Net: 29 units / 53 files** for gameplay, versus 25 units budgeted in
`BREACHPOINT-ARCHITECTURE.md` §3 for the same scope. The four extra are the four gaps:
`BRDamage` (structural enforcement of law 3), `BRGameData` (soft-ref choke point),
`BRProjectile` (absent entirely), `BRGameplayCues` (implied but unbudgeted).

---

## 7. Decisions this roadmap needs (defaults stated; founder may overrule)

| # | Decision | Default taken | Consequence if overruled |
|---|---|---|---|
| **D-1** | `ABRProjectile` needs a per-frame position update. Zorans dodged the no-Tick law with a recursive `SetTimerForNextTick`, which is Tick wearing a hat. | **Declare a named exception** in `gas-purity.md`'s ledger and enable Tick on `ABRProjectile` **only**, honestly. Bound: tick does position + trace, nothing else; no gameplay decision in Tick; disabled the moment the projectile deactivates. | If refused: recursive next-tick timer, same cost, worse honesty. Do not ship a hidden Tick. |
| **D-2** | `Weapons/` → `Equipment/` (it holds weapons, grenades, projectiles, pickups). | **Rename.** BP90 updates the crew owner-path map and `.claude/` config in the same commit. | Keep `Weapons/`; strike the rename step from BP90 and use the old path in BP96–BP99. |
| **D-3** | Is the AI brain (`AI/`, 6 units) in scope? | **Out of scope.** It stays as-is behind the input-tag seam, which this design preserves for free. `BRBotController` keeps pressing InputTags on its own ASC. | If in scope: a BP103 packet after BP101, owned by ai-builder, re-fitting `BRBotFacts` to the new attribute names. |

Each decision is recorded in the ticket that first depends on it. A session that disagrees
files a `contract_gap` and **stops** — it does not decide unilaterally.

---

## 8. The road — 12 tickets, 5 phases

Ordering law: **a phase does not start until the previous phase's tickets are all DONE.**
Within a phase, tickets may run in parallel where their owner paths do not overlap.

### Phase 0 — Demolition (1 ticket)
> Nothing new is built on top of the template.

| Ticket | Covers | Owner |
|---|---|---|
| `BP90_DEMOLITION` | Delete `Variant_Horror/`, `Variant_Shooter/`, `breachpoint{Character,GameMode,PlayerController}`. Strip the six `PublicIncludePaths` and re-test whether `Slate` is still needed. Resolve D-1/D-2/D-3. Update the owner-path map. Archive the superseded §3 sections. | builder |

### Phase 1 — Foundation (2 tickets, parallel)
> Vocabulary and data before behaviour. Nothing here activates.

| Ticket | Covers | Owner |
|---|---|---|
| `BP91_FOUNDATION` | `BRGameplayTags`, `BRCore` (incl. `BRTeams::GetAttitude`), `BRDataRows.h`, `BRGameData` subsystem, `DT_Weapons.csv` + `CT_Combat.csv` schemas | builder + sim-builder |
| `BP92_INPUT` | `BRInput` (config DataAsset + component), `IMC_Default` + `IA_*` generated by committed script | builder |

### Phase 2 — The GAS spine (2 tickets, sequential)
> The trust line. Nothing above this phase is safe until it is right.

| Ticket | Covers | Owner |
|---|---|---|
| `BP93_GAS_SPINE` | `BRAbilitySystemComponent`, `BRAttributeSet`, `BRAbilitySet`, `BRGameplayAbility` base, `BRGameplayEffects` (all 8) | sim-builder + netcode-builder |
| `BP94_DAMAGE` | `BRDamage` (the door), `BRDamageExecCalc`, `BRGameplayCues`; the grep gate that proves nothing else builds a damage spec | sim-builder + critic |

### Phase 3 — The living pawn (2 tickets, sequential)
> First rung-4 evidence. Two clients can see each other move.

| Ticket | Covers | Owner |
|---|---|---|
| `BP95_MATCH_HOST` | `BRPlayerState` (ASC host, TeamId, NetUpdateFrequency), `BRPlayerController` (tag relay, ready delegate) | netcode-builder |
| `BP96_CHARACTER` | `BRCharacter` (dual mesh, GAS init dance, `CheckReady()`, death consequence), `BRCharacterMovementComponent` (`FSavedMove_BR`) | builder + netcode-builder |

### Phase 4 — The golden triangle (3 tickets, sequential)
> **The gate.** At the end of BP98 the game is playable: shoot, kill, respawn.

| Ticket | Covers | Owner |
|---|---|---|
| `BP97_EQUIPMENT` | `BREquipmentComponent`, `BRWeaponInstance`, `BRWeaponPickup` | sim-builder |
| `BP98_FIRE` | `BRGA_WeaponFire` (hitscan path), `BRGA_WeaponUtility`, `DT_Weapons` rows, pinned combat specs, cheat tests | sim-builder + netcode-builder + critic |
| `BP99_MATCH_FRAME` | `BRGameMode` phases/respawn/attribution, `BRGameState` scores + killfeed | netcode-builder + builder |

### Phase 5 — The rest of the verb set (3 tickets)
> Everything after the game is already fun.

| Ticket | Covers | Owner |
|---|---|---|
| `BP100_MOVEMENT_MELEE` | `BRGA_Movement` (Sprint + Jump), `BRGA_Melee` | sim-builder |
| `BP101_PROJECTILE` | `BRProjectile` (D-1 applies), `BRGA_Grenade` + client ghost correlation | sim-builder + netcode-builder |
| `BP102_GRAPPLE` | `BRGA_Grapple` — root-motion source through CMC. **Rungs 4a AND 4b. Critic REFUTER mandatory.** | netcode-builder + critic |

---

## 9. Migration — what happens to the code that exists

| Existing | Disposition |
|---|---|
| `Variant_Horror/`, `Variant_Shooter/`, `breachpoint*` (36 files) | **DELETED** in BP90. Verified: no `BR` source references any of them |
| `Core/`, `Input/`, `Data/` | **REWRITTEN** in place (BP91/BP92) — same paths, new contents |
| `AbilitySystem/` (15 h / 15 cpp) | **REWRITTEN** to the 8+6 shape. Anything not in §2 is deleted, not orphaned |
| `Character/` (3 pairs incl. template) | Template pair deleted; the two `BR` pairs rewritten (BP96) |
| `Weapons/` (5 pairs) | Renamed `Equipment/` (D-2), reduced to 4 units. `BRExplosion` deleted — folded into `BRDamage::ApplyRadial` |
| `Match/` (6 pairs incl. 2 template) | Template pairs deleted; 4 `BR` pairs rewritten (BP95/BP101) |
| `Camera/BRPlayerCameraManager` | **KEPT as-is.** Not in the gameplay budget; unaffected by this rework |
| `UI/` (50 h / 46 cpp), `AI/`, `Online/`, `Telemetry/` | **UNTOUCHED.** They meet this layer at named seams only |
| `Tests/` (3 specs) | `BRCombatSpec` + `BRShieldSpec` rewritten against the new attribute names (BP93/BP94). `BRSettingsSpec` is a `UI/` concern — untouched |

**Honesty note on the ladder.** Rung 1 is PARTIAL on the current machine and structurally
cannot be more: this is an Epic Launcher install with no server binaries, so
`BreachpointServer` cannot link (`tickets/HANDOFF.md`). Every ticket below reports rung 1 as
PARTIAL-by-environment, never as green. A source-built engine is what changes that, and it is
the single highest-leverage environment fix available.

**Honesty note on the guard.** `.claude/hooks/guard_laws.py` hooks `Edit|Write|MultiEdit`
only — a shell `rm`/`mv`/`>` bypasses it entirely. BP90 is a deletion packet executed through
the shell, so **its owner-path confinement is advisory, not enforced.** Write the claim file
anyway; the banned-API greps still fire on every write.
