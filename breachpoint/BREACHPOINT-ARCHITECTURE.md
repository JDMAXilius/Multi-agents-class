# BREACHPOINT — Architecture & File Structure (v2)
## The complete C++ framework layout for the vertical slice

**Companion to:** `BREACHPOINT-GDD-VERTICAL-SLICE.md` (what we build) — this is **how
it is laid out on disk and why**.
**Engine:** UE 5.8 · pure native C++ · GAS · Steam listen server behind
`IBRServerLifecycle` · GameLift as a Phase-2 swap
**v2 changes:** necessity audit applied (§4 ledger), **modularity laws**
added (soft references, reusable parameterized GEs), **advanced-GAS
coverage checklist** (§5.4), **owned input layer** (Enhanced Input →
InputTag → ASC), telemetry/testing kept with explicit justification.

---

## 0. The Seven Architecture Laws

1. **One runtime module.** No plugin farm, no GameFeatures. Folder-per-
   discipline — the folders ARE the crew owner_paths.
2. **GAS is the gameplay API.** Anything that changes Shields, Health,
   ammo, or state goes through an Ability, Effect, or Attribute. Cues
   carry all FX.
3. **The trust line is the PlayerState ASC.** Clients predict; the server
   owns truth. The replicated surface is enumerated in §6.1 — if a
   property isn't in the table, it doesn't replicate.
4. **Data is not code.** Every tuning number in CSV-backed DataTables;
   every row struct in one header. A gameplay literal in C++ is a finding.
5. **No gameplay Tick.** Timers, delegates, gameplay events, cue notifies.
6. **Soft references only, at the data boundary.** C++ classes reference
   **tags and table rows**, never assets. Every asset reference in data
   (meshes, cues, MetaSounds, ability sets, input actions) is a
   `TSoftObjectPtr`/`TSoftClassPtr`, resolved through the streamable
   manager at load points (match load, equip). A hard `UPROPERTY` asset
   reference in a C++ class is a finding — it welds cook chains together
   and kills reuse.
7. **Effects are parameterized, not proliferated.** GameplayEffects are
   **generic templates driven by SetByCaller + dynamic tags** — one
   damage GE for every damage source in the game, one regen GE for every
   regenerating attribute, one cooldown GE for every ability. New content
   = new *rows and parameters*, not new effect assets.

**Naming:** class prefix **`BR`**; files named after their class; one
class per header except tiny siblings.

---

## 1. Project Root

```
Breachpoint/
├── Breachpoint.uproject
├── Config/
│   ├── DefaultEngine.ini           # Steam OSS, net driver, collision channels,
│   │                               #   GAS/StateTree/MVVM plugin enables
│   ├── DefaultGame.ini             # mode aliases, map list, packaging
│   ├── DefaultInput.ini            # Enhanced Input runtime config
│   └── DefaultGameplayTags.ini     # registration only (tags are native C++)
├── Source/                         # §2–§3
├── Content/                        # §7 — assets only, zero logic
├── Tools/                          # §8 — build, verify, deploy
├── docs/                           # crew kit: contracts, tickets, playbook
├── .claude/                        # crew agents + skills
└── .gitignore / .p4ignore
```

## 2. Build Targets — three, from day one

```
Source/
├── Breachpoint.Target.cs           # Game (client + listen server) — the slice ships this
├── BreachpointEditor.Target.cs
└── BreachpointServer.Target.cs     # dedicated — COMPILES FROM WEEK 1 (Phase-2 insurance;
                                    #   verifier builds all three on every rung-1 run)
```

---

## 3. The Runtime Module — `Source/Breachpoint/` (44 class-units)

```
Source/Breachpoint/
├── Breachpoint.Build.cs            # Core, CoreUObject, Engine, InputCore, EnhancedInput,
│                                   #   GameplayAbilities, GameplayTags, GameplayTasks,
│                                   #   AIModule, StateTreeModule, GameplayStateTreeModule,
│                                   #   NavigationSystem, UMG, CommonUI, CommonInput,
│                                   #   ModelViewViewModel, OnlineSubsystem(+Steam), HTTP, Json
├── Breachpoint.h / .cpp
│
├── Core/          (2)   builder
├── Input/         (2)   builder (+ sim-builder at the ASC seam)
├── AbilitySystem/ (11)  sim-builder (+ netcode-builder on replication)
├── Character/     (2)   builder → anim-builder
├── Weapons/       (3)   sim-builder
├── Match/         (4)   netcode-builder (authority) + builder (flow)
├── AI/            (4)   ai-builder
├── Online/        (3)   services-builder
├── UI/            (4)   ui-builder
├── Telemetry/     (2)   ai-builder (Spotter) + builder (collection)
├── Data/          (1)   sim-builder (schemas; values live in Content/Data)
└── Tests/         (3)   sim-builder authors, verifier runs
```

### 3.1 `Core/` — 2

| File | Job |
|---|---|
| `BRGameplayTags.h/.cpp` | ALL native tags (`UE_DEFINE_GAMEPLAY_TAG`): `Ability.*`, `InputTag.*` (Move, Look, Jump, Crouch, Sprint, Fire, Reload, Swap, Grenade, Melee, Grapple), `State.*` (Shields.Broken, Combat.RecentDamage, Movement.Sprinting, Dead), `GameplayCue.*`, `Damage.*` (Kinetic, Explosive, Melee, Headshot, Rear), `SetByCaller.*` (BaseDamage, RegenRate, CooldownDuration), `Event.*` (Death, Kill). One authoritative header. |
| `BRCore.h/.cpp` | Log channels (LogBRCombat/Net/AI/Online/UI) + collision channel aliases matching DefaultEngine.ini. *(v2: merged from two files — same audience, one include.)* |

### 3.2 `Input/` — 2 *(v2: new — the owned input layer)*

The advanced Enhanced Input → GAS bridge, authored ours (Lyra-pattern,
zero Lyra code):

| File | Job |
|---|---|
| `BRInputConfig.h/.cpp` | `UDataAsset`: rows of `{TSoftObjectPtr<UInputAction>, FGameplayTag InputTag}` in two lists — native actions (Move/Look/Jump/Crouch) and **ability actions** (Sprint, Fire, Reload, Swap, Grenade, Melee, Grapple). Loadout-agnostic: the config maps hardware to tags, never to abilities. |
| `BRInputComponent.h/.cpp` | `UEnhancedInputComponent` subclass with `BindNativeAction(...)` and `BindAbilityActions(config, this, &Pressed, &Released)` templates. Every ability action routes to exactly two functions carrying the tag. |

**The input flow (the whole design in five arrows):**

```
IMC_Default → UInputAction → BRInputComponent → InputTag.Fire
  → BRPlayerController::AbilityInputTagPressed(Tag)
  → BRAbilitySystemComponent::AbilityInputTagPressed(Tag)   ← input buffer lives here
  → activates granted abilities whose BRAbilitySet entry carries that InputTag
```

Why this is the advanced answer: **abilities are bound by tag, not by
binding code** — a new ability is a DataAsset row (ability class ×
InputTag), no input code changes; press/release reach the ASC so
`WaitInputRelease`/hold/toggle tasks work; the buffer (ported, fighting-
game grade) sits at the one choke point; and bots inject the *same tags*
into the same ASC path — human and bot input are literally one API.

### 3.3 `AbilitySystem/` — 11

| File | Job |
|---|---|
| `BRAbilitySystemComponent.h/.cpp` | PORTED: input-buffered tag activation, prediction-window helpers, `ReplicationMode::Mixed` (owner gets full GEs; others get tags/cues — the correct player-ASC setting, bots included since they are player-shaped). Enables `ServerAbilityRPCBatch` for single-RPC fire (activate + TargetData + end in one packet — the GAS-shooter optimization). |
| `BRAbilitySet.h/.cpp` | `UDataAsset`: `{TSoftClassPtr<GameplayAbility>, Level, InputTag}` + granted effects/attributes; returns grant handles for clean revoke. One per loadout, one per weapon. |
| `BRAttributeSet.h/.cpp` | THE set: Shields/MaxShields, Health/MaxHealth, IncomingDamage (meta). `PreAttributeChange` clamps; `PostGameplayEffectExecute`: shields-first application, RecentDamage tag application, death → `Event.Death` + delegate. |
| `BRGameplayAbility.h/.cpp` | Base: activation-policy enum (OnPressed / WhileHeld / Toggle), cost/cooldown via the generic GEs, `State.Dead` in ActivationBlockedTags (death disables every verb through one mechanism), cancel hygiene, typed accessors. |
| `BRDamageExecCalc.h/.cpp` | ONE execution for all damage: reads `SetByCaller.BaseDamage` + `Damage.*` tags → headshot ×2, rear-melee lethal, shields absorb → overflow to health. Coefficients from `CT_Combat` curve table. |
| `Abilities/BRGA_WeaponFire.h/.cpp` | Client trace → `FGameplayAbilityTargetDataHandle` in a scoped prediction window → batched RPC → **server validates** (rate/ammo/cone/range) → applies the one damage GE. |
| `Abilities/BRGA_WeaponUtility.h/.cpp` | *(v2: merged)* `UBRGA_Reload` + `UBRGA_WeaponSwap` — two tiny sibling abilities, one pair. |
| `Abilities/BRGA_Grenade.h/.cpp` | Cook → server-authoritative projectile spawn (client ghost for feel); explosion applies the same damage GE with `Damage.Explosive`. |
| `Abilities/BRGA_Melee.h/.cpp` | Notify-window trace; rear-arc validated server-side; same damage GE with `Damage.Melee[.Rear]`. |
| `Abilities/BRGA_Grapple.h/.cpp` | THE netcode packet: three modes by hit; self-pull via **root-motion source through CMC** (predicted by CMC machinery); rejection leaves zero state. Critic REFUTER gate. |
| `Abilities/BRGA_Sprint.h/.cpp` | The movement-state ability, and the pattern-prover for **WhileHeld** activation: hold `InputTag.Sprint` → activate; release → end. Grants `State.Movement.Sprinting` via ActivationOwnedTags (predicted + replicated by GAS); the CMC reads it into the `FSavedMove_BR` sprint flag and applies the speed multiplier from `CT_Combat`. **No cost** (Halo sprint is free); `BRGA_WeaponFire`/`Melee`/`Grenade` list it in CancelAbilitiesWithTags — firing ends the sprint, Halo-style, with zero code in the sprint ability itself. |

**The generic-effect library (Content assets, 6 total — law #7 made real; purity law: `crew/docs/contracts/gas-purity.md`):**

| GE asset | Parameterized by | Reused by |
|---|---|---|
| `GE_Damage` | `SetByCaller.BaseDamage` + dynamic `Damage.*` tags | **every** damage source: AR, Magnum, Rocket, grenade, melee, (Phase-2 hazards) |
| `GE_Regen` | attribute (curve row) + `SetByCaller.RegenRate`, activation-blocked by a tag | shield recharge now; any future regen (health pickups, overshield) |
| `GE_Cooldown` | `SetByCaller.CooldownDuration` + per-ability cooldown tag | **every** ability cooldown (grapple, swap, magic-slot Phase 2) |
| `GE_InitStats` | curve table row per archetype | attribute init on spawn |
| `GE_RecentDamage` | 2.5 s tag application | shield-regen gate; (Phase-2: "in combat" logic) |
| `GE_Death` | infinite; applies `State.Dead` | the ONE death mechanism: ability base blocks activation on `State.Dead`; respawn removes it + re-applies `GE_InitStats` |

### 3.4 `Character/` — 2

**`BRCharacter.h/.cpp` — the pawn is a body, not a brain.** Rebased from
the FPS template with its gameplay deleted. What it owns:

- **Dual-mesh setup (the pro-FPS standard):** `Mesh1P` (arms + weapon,
  `OnlyOwnerSee`, no shadow) for the owning client; `Mesh3P` (full body,
  `OwnerNoSee`, casts shadow) — what everyone else and the death cam see.
  Weapon mesh attaches to both via socket. anim-builder's two ABPs drive
  one mesh each; remote aim pitch reads the replicated `RemoteViewPitch`.
- **The GAS init dance:** ASC lives on PlayerState; the pawn implements
  `IAbilitySystemInterface` by forwarding, and calls
  `InitAbilityActorInfo(PS, this)` in `PossessedBy` (server) **and**
  `OnRep_PlayerState` (client) — the canonical respawn-safe wiring.
- Input wiring (`BRInputComponent` from `DA_InputConfig`): native
  move/look/jump/crouch handled by CMC; ability tags forwarded to the ASC.
- Death is a **consequence, not a decision**: on `Event.Death` it plays
  the cue, ragdolls `Mesh3P`, disables collision/input, and waits for
  GameMode's respawn — no scoring, no health, no weapon logic on the pawn,
  ever (attributes/PlayerState/equipment own those).
- One combat helper: the server-side rear-arc check for melee.

**`BRCharacterMovementComponent.h/.cpp` — subclass, NOT from scratch.**
Rewriting CMC means rewriting the most battle-tested networked prediction
code in the engine (saved moves, corrections, smoothing) — that is
engine-tier, like the renderer; "100% our gameplay code" does not include
re-implementing engine subsystems. (Epic's replacement, **Mover**, is
still experimental — rejected on the same grounds as MassAI.) What the
subclass owns:

- **`FSavedMove_BR` + `FNetworkPredictionData_Client_BR`** (same files):
  compressed flags for sprint/grapple so our state replays correctly
  through CMC's own prediction/reconciliation — the standard advanced
  CMC extension pattern.
- **Grapple execution:** `BRGA_Grapple` (server-validated target) asks
  this component to apply a **root-motion source** for the pull; RMS
  rides CMC's saved-move pipeline, so the grapple is client-predicted
  and server-reconciled with zero bespoke reconciliation code. Detach
  rules (arrival, jump-cancel) live here; the *decision* to grapple
  stays in the ability.
- Sprint: the CMC applies the `CT_Combat` speed multiplier while
  `State.Movement.Sprinting` is present (set by `BRGA_Sprint`, carried in
  the saved-move flag so corrections replay it). Halo-feel numbers (high
  air control, jump velocity, gravity scale) are **config on CMC
  defaults, not code**.

### 3.5 `Weapons/` — 3

`BREquipmentComponent.h/.cpp` (two-slot replicated subobjects; equip =
grant weapon's `BRAbilitySet` + async-load soft mesh; pickup/drop RPC
server-validated) · `BRWeaponInstance.h/.cpp` (replicated UObject: row
handle + ammo `COND_OwnerOnly`) · `BRWeaponPickup.h/.cpp`
(`ABRWeaponPickup` + `ABRPowerWeaponSpawner` 90 s node).

### 3.6 `Match/` — 4

`BRGameMode.h/.cpp` (phase machine on timers/events, kill attribution +
double-KO, win/SD checks, scored respawns, bot fill via AI's manager) ·
`BRGameState.h/.cpp` (phase, `MatchEndServerTime`, team scores, rocket
mirror, killfeed ring buffer) · `BRPlayerState.h/.cpp` (**ASC + set
live here**; TeamID, K/D/A) · `BRPlayerController.h/.cpp` (input→ASC
relay, death cam, UI intent boundary).

### 3.7 `AI/` — 6

Full design: **`BREACHPOINT-AI-BOTS.md`** (three layers, one brain — GOAP-style
ambitions over a StateTree spine over GAS; rulings R8–R12).

`BRBotBrain.h/.cpp` (**layer 1** — pure, headless: `DT_BotAmbitions` utility
scoring, hysteresis, bounded ≤3-step plans with ASC-query preconditions) ·
`BRBotFacts.h` (the typed fact struct) ·
`BRBotController.h/.cpp` (glue: events → brain; plan → StateTree params; presses
InputTags on its ASC — aim error applied *before* the fire ability; no
privileged paths) · `BRStateTreeTasks.h/.cpp` (**layer 2** — all tasks/conditions;
Engage internals are a BT-shaped priority selector; no second BehaviorTree asset) ·
`BREnvQueryContexts.h/.cpp` (cover/threat/rocket/perch scoring over the arena
manifest's authored vocabulary) ·
`BRBotManagerComponent.h/.cpp` (fill/backfill on GameMode).
*(v2: was 4 units — the GOAP layer adds `BRBotBrain` + `BRBotFacts`, both
headless-testable; `DT_BotAmbitions.csv` joins `Content/Data/`.)*

### 3.8 `Online/` — 3 (+1 reserved)

`BRSessionsSubsystem.h/.cpp` (ported; Steam OSS behind
`FindAndJoinBestSession`) · `BRServerLifecycle.h` (**`IBRServerLifecycle`**
— the seam) · `BRListenServerLifecycle.h/.cpp` (slice impl) ·
*(reserved, Phase 2: `BRGameLiftLifecycle` — SDK5 `InitSDK/ProcessReady/
OnStartGameSession` behind the same interface).*

### 3.9 `UI/` — 4

`BRUIManagerSubsystem.h/.cpp` (CommonUI layer stack: GameHUD/Menu/Modal) ·
`BRActivatableWidget.h/.cpp` (the one widget base) ·
`BRViewModels.h/.cpp` *(v2: merged — `UBRVM_Combat` + `UBRVM_Match` in
one pair; both are FieldNotify ViewModels fed by ASC delegates and
RepNotify events — zero polling)* · `BRHUDLayout.h/.cpp` (binds BP visual
subclasses to the ViewModels; killfeed widget pool; shield-vs-flesh hit
markers).

### 3.10 `Telemetry/` — 2 *(kept — verdict below)*

`BRTelemetrySubsystem.h/.cpp` (WorldSubsystem, authority-only; folds
existing delegates into `FBRMatchTelemetry`) · `BRSpotterSubsystem.h/.cpp`
(GameInstance, authority-only, async HTTP, caps + canned fallback,
replicated strings only).

**Why telemetry/tests stay (the owner's question, answered):** you never
write these — **the crew does**. The 3 spec files are what the verifier's
ladder *runs*; without them "works" is an opinion and the whole
separation-of-powers loop is theater. Telemetry is 2 files feeding 3
consumers (Spotter, tuning-curator balance bands, QA soak baselines). Both are
ticketed to Claude terminal from day one; your cost is reading green/red,
not authoring.

### 3.11 `Data/` — 1

`BRDataRows.h` — every row struct, one header: `FBRWeaponRow` (numbers +
**soft refs**: mesh, cues, ability set, MetaSounds), `FBRBotTuningRow`
(`reaction_ms ≥ 200` enforced in `OnPostDataImport`), `FBRMatchRules`,
`FBRSpotterLineRow`, `FBRKillFeedEntry`, `FBRMatchTelemetry`.

### 3.12 `Tests/` — 3 (cpp only)

`BRCombatSpec.cpp` (`Breachpoint.Sim.Combat`: TTKs, headshot math,
rear-melee, shields-first — asserted **against the DataTable**) ·
`BRShieldSpec.cpp` (recharge gate/rate/events) ·
`BRBotDeterminismSpec.cpp` (same seed + row ⇒ identical action trace).

---

## 4. The Necessity Audit (v2 ledger — every file challenged)

| v1 item | Verdict | Why |
|---|---|---|
| `BRLogChannels` + `BRCollision` | **MERGED** → `BRCore` | Same audience, one include |
| `BRGA_Reload` + `BRGA_WeaponSwap` | **MERGED** → `BRGA_WeaponUtility` | Two tiny siblings |
| `BRVM_Combat` + `BRVM_Match` | **MERGED** → `BRViewModels` | One include for the HUD |
| Input layer (2 units) | **ADDED** | The owned Enhanced-Input→tag→ASC bridge (§3.2) |
| `BRPlayerController` | **KEPT** | Input relay + UI intent boundary + death cam — three jobs, one class |
| `BRBotManagerComponent` | **KEPT** separate from GameMode | It's ai-builder's owner_path; folding it into Match/ would cross crew boundaries |
| `BRTelemetry` / `BRSpotter` as two files | **KEPT** | Different trust profiles (collector vs. HTTP client); merging couples an always-on system to an optional one |
| Everything in the v1 "does NOT exist" ledger | **STILL OUT** | Per-system modules, asset manager, inventory system, message router, per-weapon actors, rewind, save/settings — reasons unchanged |
| Target Actors (`AGameplayAbilityTargetActor`) | **REJECTED** | Built for confirm/cancel targeting flows; wrong for hitscan — we build TargetData from the client trace in a prediction window (Lyra parity) |
| Sprint ability | **ADDED** (`BRGA_Sprint`) | Sprint is gameplay *state*, not just speed — GAS owns the decision (tag, cancel rules), the CMC owns the motion; it doubles as the WhileHeld-policy prover |

**Budget: 44 class-units ≈ 84 source files + 3 targets.** (v1 was 44; v2's
consolidation took it to 42 — five merged/cut, two added for input, one for
sprint — and the GOAP goal layer then added `BRBotBrain` + `BRBotFacts`,
both headless-testable. Same total, different shape: fewer glue classes,
more provable ones.)

---

## 5. GAS — Advanced Multiplayer Coverage (the complete checklist)

Every advanced topic, with its decision — nothing left implicit:

### 5.1 Prediction
- **`LocalPredicted`** NetExecutionPolicy on all player combat abilities;
  scoped prediction windows (`FScopedPredictionWindow`) around TargetData
  production so client and server share a key.
- **Costs/cooldowns predicted and rolled back** by GAS's own machinery —
  a rejected fire refunds ammo and cooldown with zero custom code
  (this is *why* costs must be GEs, law #2).
- **`ServerAbilityRPCBatch`** on fire: activate + TargetData + end in one
  RPC — the single-shot optimization that halves fire-path packets.
- **Cue discipline:** predicted cues via `OnActive/WhileActive` (rolled
  back automatically); confirmed-only one-shots (kill toast, shield
  break) fire from `Executed` on the server path. No FX ever spawned
  directly by ability code.

### 5.2 TargetData (and why not Target Actors)
- Fire builds `FGameplayAbilityTargetData_SingleTargetHit` from the
  client trace; melee/rear uses the same handle type with its arc check
  re-run server-side; grapple sends the hit + surface classification.
- Server-side validation list per packet (fire): rate ≤ RPM + tolerance,
  ammo > 0, direction within cone of server-known muzzle, range ≤ table
  max. Reject = silent drop (client sees a whiff; a cheater sees nothing).

### 5.3 Rewind & smoothing — the honest positions
- **Lag compensation / server rewind: Phase 2, by name.** The slice
  ships Lyra-parity validation (above). Rewind is a self-contained
  netcode-builder packet later; the seam (server validation function) is
  where it plugs in.
- **Movement smoothing:** simulated proxies use CMC's built-in mesh
  smoothing (`NetworkSmoothingMode = Exponential`); the grapple pull is
  a **root-motion source inside CMC**, so it predicts and reconciles
  through the same saved-move machinery as walking — no bespoke
  reconciliation code to get wrong.
- **Attribute presentation smoothing** (shield bar lerp) is a ViewModel
  concern — visual interpolation only, never simulation.

### 5.4 Replication settings that are decisions, not defaults
- ASC on PlayerState, **`ReplicationMode::Mixed`** for every fighter
  (humans and bots): full GE replication to owner, tags + cues to
  everyone else. (`Minimal` is for non-player-shaped AI; our bots are
  player-shaped by design.)
- PlayerState **NetUpdateFrequency raised** (default is 1 Hz — far too
  slow for an FPS scoreboard/ASC host).
- Ammo `COND_OwnerOnly`; equipment visible to all; killfeed via ring
  buffer RepNotify; never-replicated list in §6.1.
- Death/respawn: ASC stays on PlayerState; on possess,
  `InitAbilityActorInfo` re-points the avatar — attributes and granted
  abilities survive by construction, per-life state (RecentDamage tag)
  cleared by an explicit respawn GE.

---

## 6. Cross-Cutting Contracts

### 6.1 Replication surface (complete)

| State | Where | Condition |
|---|---|---|
| Shields/Health (+Max) | PlayerState ASC | COND_None (public bars) |
| Ammo in mag / reserve | `BRWeaponInstance` | **COND_OwnerOnly** |
| Equipment slots + active index | `BREquipmentComponent` | COND_None |
| Phase / `MatchEndServerTime` / team scores | GameState | COND_None |
| K/D/A, TeamID | PlayerState | COND_None |
| Killfeed + Spotter strings | GameState ring buffer | COND_None |
| Rocket countdown | `ABRPowerWeaponSpawner` | COND_None |
| Grapple/movement | CMC saved moves + ability replication | prediction machinery |
| **Never** | exact enemy ammo, bot internals, API key, telemetry aggregates | — |

### 6.2 The one damage pipeline

```
Ability (any) ─ SetByCaller.BaseDamage + Damage.* tags ─► GE_Damage
  ─► BRDamageExecCalc (server): multipliers → shields first → health
  ─► PostGameplayEffectExecute: RecentDamage tag · death event → GameMode
  ─► GameplayCues (cosmetic, all clients)
```
One GE, one ExecCalc, every source. Adding a weapon adds a **row**, not a
pipeline.

---

## 7. `Content/` — assets only

```
Content/
├── Data/            DT_Weapons.csv · CT_Combat.csv · DT_BotTuning.csv ·
│                    DT_MatchRules.csv · DT_SpotterLines.csv
├── Maps/            BR_Arena01 · BR_Entry
├── AbilitySystem/   Effects/ (the 6 generic GEs) · Cues/ · Sets/ (AS_Loadout, AS_Weapon_*)
├── Input/           IMC_Default · IA_* actions (incl. IA_Sprint) · DA_InputConfig
├── Characters/      sourced meshes + FPS anim sets + ABP assets
├── Weapons/         sourced meshes/anims
├── AI/              ST_Bot.st · EQS query assets
├── UI/              WBP_* visual subclasses of C++ bases only
├── Audio/           MetaSound sources + attenuation (cue-driven)
└── VFX/             sourced Niagara
```

## 8. `Tools/` — build, verify, deploy

```
Tools/
├── env.local.example · run-ubt.ps1 · run-specs.ps1 · run-gauntlet.ps1
│     (rung 4 scenario: BRGauntlet.SmokeTS2C — server + 2 clients,
│      A shoots B, assert-in-threes, then 120 ms / 5% emulation)
├── reimport-tables.ps1
├── steam/    app_build.vdf · depot_build.vdf        ← the slice's entire deploy
└── server/   Dockerfile · fleet-notes.md            ← PHASE 2 reserved (GameLift
                                                        Managed Containers behind
                                                        IBRServerLifecycle)
```

## 9. Crew Owner-Path Map

| owner_path | Agent |
|---|---|
| `AbilitySystem/`, `Weapons/`, `Data/`, `Tests/` | sim-builder |
| Any replicated surface + `Match/` + `BRGA_Grapple` | netcode-builder |
| `AI/` | ai-builder |
| `Online/` | services-builder |
| `UI/` | ui-builder |
| `Core/`, `Input/`, `Character/`, `Tools/` | builder (anim-builder joins at anim) |
| `Content/Data/*.csv` | curators propose → builder lands |

Standing triggers: any new `Replicated`/`Server` symbol → netcode packet +
critic REFUTER; any gameplay literal → data-contract finding.

---

## 10. Execution — the ticket system

Work is divided by **tickets** (`docs/tickets/` in the game repo, crew
kit format): each names its owner agents, binding contracts, ordered
steps, and observable done-when. Claude terminal picks them up with
`/tickets <name>` and the board is the shared memory between sessions.
The Week-1/2 set (cut and committed alongside this doc):

| Ticket | Covers | Primary owners |
|---|---|---|
| `TICKET_BP00_LADDER` | targets, wrappers, Gauntlet skeleton, first specs red→green | builder, sim-builder, verifier, critic |
| `TICKET_BP01_SKELETON_INPUT` | module, Core/, Input/ layer, FPS-template strip, character shell | builder |
| `TICKET_BP02_GAS_CORE` | ASC port, attributes, ability base/set, generic GE library, damage exec | sim-builder, netcode-builder |
| `TICKET_BP03_WEAPONS_FIRE` | equipment, weapon instance, fire/utility abilities, DT_Weapons, cheat tests | sim-builder, netcode-builder, critic |
| `TICKET_BP04_MATCH_FRAME` | GameMode/State/PlayerState/PC, scoring, respawn, killfeed | netcode-builder, builder |

Later weeks cut tickets the same way (grapple, bots, UI, sessions,
Steam) — one ticket ≈ one packet chain ≈ one owner path.
```
The gate stays the GDD's: Week 2 = golden-triangle fun test.
```
