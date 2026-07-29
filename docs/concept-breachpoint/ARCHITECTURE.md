# BREACHPOINT — Architecture & File Structure
## The complete C++ framework layout for the vertical slice

**Companion to:** `GDD-VERTICAL-SLICE.md` (what we build) — this is **how
it is laid out on disk and why**.
**Engine:** UE 5.8 · pure native C++ · GAS · Steam listen server behind
`IBRServerLifecycle` · GameLift as a Phase-2 swap
**Doctrine:** house rules apply in full (no-Tick gameplay, server
authority, GAS purity, PlayerState-owned ASC, no dangling state on
cancel, abstraction at migration seams). **Less is more:** every file
below has a stated job; a file without one doesn't get created.

---

## 0. The Five Architecture Laws (read first)

1. **One runtime module.** No plugin farm, no GameFeatures, no per-system
   modules. `Source/Breachpoint/` with **folder-per-discipline** — the
   folders ARE the crew owner_paths, which gives us the boundary
   discipline without the build-system tax. (Lyra's modular plugin
   architecture is correct for Epic's goals and wrong for a 6-week
   slice.)
2. **GAS is the gameplay API.** Anything that changes Shields, Health,
   ammo, or state goes through an Ability, Effect, or Attribute — no
   side-channel damage, no direct attribute writes. Cues carry all FX.
3. **The trust line is the PlayerState ASC.** Clients predict; the server
   owns truth. Every replicated surface is enumerated in §5's table —
   if a property isn't in that table, it doesn't replicate.
4. **Data is not code.** Every tuning number lives in a CSV-backed
   DataTable under `Content/Data/`; every row struct lives in ONE header
   (`BRDataRows.h`). A gameplay literal in C++ is a review finding.
5. **No gameplay Tick.** Timers, delegates, gameplay events, cue
   notifies. (Engine-owned ticks — CMC, StateTree, perception — are the
   engine's business, not a license for ours.)

**Naming:** class prefix **`BR`** (`ABRCharacter`, `UBRAttributeSet`) —
not "BP", which collides with Blueprint vocabulary. Files named after
their class. One class per header except tiny sibling types.

---

## 1. Project Root

```
Breachpoint/
├── Breachpoint.uproject            # modules: Breachpoint (Default, loading phase Default)
├── Config/
│   ├── DefaultEngine.ini           # Steam OSS block, net driver settings, collision
│   │                               #   channels, GAS/StateTree plugin enables
│   ├── DefaultGame.ini             # game mode aliases, map lists, packaging rules
│   ├── DefaultInput.ini            # Enhanced Input config
│   └── DefaultGameplayTags.ini     # tag table registration (tags themselves are native C++)
├── Source/                         # §2–§4 (the heart of this document)
├── Content/                        # §6 — assets only, zero logic
├── Tools/                          # §7 — build, test, deploy scripts
├── docs/                           # crew kit: contracts, tickets, playbook (from crew/)
├── .claude/                        # crew agents + skills (from crew/)
└── .gitignore / .p4ignore          # Saved/, Intermediate/, DerivedDataCache/, Binaries/
```

---

## 2. Build Targets — three, from day one

```
Source/
├── Breachpoint.Target.cs           # Game target (client + listen server) — the slice ships this
├── BreachpointEditor.Target.cs     # Editor target
└── BreachpointServer.Target.cs     # Dedicated server target — COMPILES FROM WEEK 1
```

`BreachpointServer.Target.cs` exists on day one even though the slice
ships listen-server: it costs one file, it keeps every `#if WITH_SERVER_CODE`
/ null-LocalPlayer assumption honest continuously (the verifier compiles
all three targets on every rung-1 run), and it is the entire precondition
for the Phase-2 GameLift swap. **This is the cheapest insurance in the
project.**

---

## 3. The Runtime Module — `Source/Breachpoint/`

```
Source/Breachpoint/
├── Breachpoint.Build.cs            # deps: Core, CoreUObject, Engine, InputCore, EnhancedInput,
│                                   #   GameplayAbilities, GameplayTags, GameplayTasks,
│                                   #   ModularGameplay(no), AIModule, StateTreeModule,
│                                   #   GameplayStateTreeModule, NavigationSystem, UMG, CommonUI,
│                                   #   CommonInput, ModelViewViewModel, OnlineSubsystem,
│                                   #   OnlineSubsystemSteam, HTTP, Json
├── Breachpoint.h / .cpp            # module impl (empty beyond IMPLEMENT_PRIMARY_GAME_MODULE)
│
├── Core/                           # owner: builder
├── AbilitySystem/                  # owner: sim-builder (+ netcode-builder on replication)
├── Character/                      # owner: builder (+ anim-builder when anim lands)
├── Weapons/                        # owner: sim-builder
├── Match/                          # owner: netcode-builder (authority) + builder (flow)
├── AI/                             # owner: ai-builder
├── Online/                         # owner: services-builder
├── UI/                             # owner: ui-builder
├── Telemetry/                      # owner: ai-builder (Spotter) + builder (collection)
├── Data/                           # owner: sim-builder (schemas; VALUES live in Content/Data)
└── Tests/                          # owner: verifier consumes; sim-builder authors
```

### 3.1 `Core/` — 3 files

| File | Job |
|---|---|
| `BRLogChannels.h/.cpp` | `DECLARE_LOG_CATEGORY` per discipline: LogBRCombat, LogBRNet, LogBRAI, LogBROnline, LogBRUI |
| `BRGameplayTags.h/.cpp` | **All native gameplay tags** (`UE_DEFINE_GAMEPLAY_TAG`): `Ability.*`, `State.*` (Shields.Broken, Combat.RecentDamage, Dead), `GameplayCue.*`, `Damage.*` (Kinetic, Explosive, Melee, Headshot, Rear), `Event.*` (Death, Kill) — one authoritative header, no ini-defined tags |
| `BRCollision.h` | `#define` collision channel aliases (Weapon trace, Grapple trace) matching DefaultEngine.ini |

### 3.2 `AbilitySystem/` — the GAS core (12 class-units)

```
AbilitySystem/
├── BRAbilitySystemComponent.h/.cpp     # PORTED from OnSight: input-buffered activation,
│                                       #   prediction-window helpers. Lives on PlayerState.
├── BRAbilitySet.h/.cpp                 # UDataAsset: array of {AbilityClass, Level, InputTag}
│                                       #   + granted effects/attributes; grant/revoke handles.
│                                       #   One asset per loadout & per weapon.
├── BRAttributeSet.h/.cpp               # THE attribute set (one, not many — less is more):
│                                       #   Shields, MaxShields, Health, MaxHealth,
│                                       #   IncomingDamage (meta). PostGameplayEffectExecute:
│                                       #   shields-first application, death detection →
│                                       #   Event.Death gameplay event + delegate. Rep-notified,
│                                       #   COND_None for bars all players can see.
├── BRGameplayAbility.h/.cpp            # base: cost/cooldown conventions, cancel hygiene,
│                                       #   helper accessors (BRCharacter, EquipmentComponent)
├── BRDamageExecCalc.h/.cpp             # THE damage pipeline (single ExecutionCalculation):
│                                       #   captures IncomingDamage, reads Damage.* tags →
│                                       #   headshot ×2 (Magnum tag), rear-melee = lethal,
│                                       #   shields absorb before health. All coefficients
│                                       #   from DT_Combat via CurveTable — zero literals.
├── BRShieldRegenEffect: (NO FILE)      # shield recharge is a GameplayEffect ASSET: infinite
│                                       #   periodic +60/s, activation-blocked by tag
│                                       #   State.Combat.RecentDamage (applied for 2.5 s by the
│                                       #   damage GE). Engine-driven periodic ≠ our Tick.
└── Abilities/
    ├── BRGA_WeaponFire.h/.cpp          # THE FPS-critical ability. Client traces (feel) →
    │                                   #   TargetData via prediction window → SERVER VALIDATES
    │                                   #   (rate vs RPM, ammo, cone-from-muzzle tolerance,
    │                                   #   range) → server applies GE_Damage. Lyra-parity
    │                                   #   trust model; full rewind = Phase 2 (§5.3).
    ├── BRGA_Reload.h/.cpp              # montage + ammo attribute swap; cancel-clean
    ├── BRGA_WeaponSwap.h/.cpp          # 0.4 s; swaps active BRWeaponInstance + input mapping
    ├── BRGA_Grenade.h/.cpp             # cook → spawn predicted projectile actor (server
    │                                   #   authoritative spawn; client ghost for feel)
    ├── BRGA_Melee.h/.cpp               # short trace on notify window; rear-arc check server-side
    └── BRGA_Grapple.h/.cpp             # THE netcode-builder packet. Three modes by hit type
                                        #   (geometry → pull self via root-motion-source;
                                        #   weapon pickup → attract; pawn → reel). Predicted;
                                        #   rejection leaves zero state. Critic REFUTER gate.
```

**GAS purity notes:** abilities are granted ONLY via `UBRAbilitySet`
(loadout set + one set per held weapon, granted/revoked by the equipment
component on equip/unequip). Cooldowns and costs are GEs. Every
visual/audio consequence is a GameplayCue (`GameplayCue.Weapon.Fire.AR`,
`GameplayCue.Shields.Break`, …) — **zero direct FX spawns in ability
code**, which is what makes prediction rollback clean for free.

### 3.3 `Character/` — 2 class-units

| File | Job |
|---|---|
| `BRCharacter.h/.cpp` | First-person pawn (from FPS template, stripped): mesh1P + weapon socket, `IAbilitySystemInterface` **forwarding to PlayerState ASC**, Enhanced Input → tag-based ability activation (`InputTag.Fire` → ASC), death handling (ragdoll + cue), rear-arc helper for melee. **No gameplay logic beyond forwarding.** |
| `BRCharacterMovementComponent.h/.cpp` | CMC subclass: sprint speed handling + **grapple support** (custom movement mode driven by root-motion source from BRGA_Grapple; CMC prediction machinery makes the pull client-predicted "for free"). The only movement code we own. |

### 3.4 `Weapons/` — 3 class-units

| File | Job |
|---|---|
| `BREquipmentComponent.h/.cpp` | On `ABRCharacter`. The **two-slot inventory**: `TArray<UBRWeaponInstance*>[2]` (replicated subobjects), active index (rep-notified), equip/unequip = grant/revoke the weapon's `UBRAbilitySet` + attach mesh. Pickup/drop RPC with server-side reach validation. Dies with the pawn (loadout resets on respawn — correct for arena). |
| `BRWeaponInstance.h/.cpp` | Replicated `UObject`: weapon row handle (`FBRWeaponRow`), ammo-in-mag + reserve (COND_OwnerOnly), spread state. No actor tick, no world presence — the *held* weapon is data + a mesh component on the character. |
| `BRWeaponPickup.h/.cpp` | `ABRWeaponPickup` — world actor for floor weapons: overlap prompt, server-validated take, respawn timer. `ABRPowerWeaponSpawner` (same header, subclass): the **90 s rocket node** — replicated countdown driving the HUD timer + arena-wide cue at spawn. |

### 3.5 `Match/` — 4 class-units

| File | Job |
|---|---|
| `BRGameMode.h/.cpp` | **Server-only.** Match phase machine (`Warmup → Live → SuddenDeath → PostMatch`) on timers + kill events (no Tick). Kill attribution (last-instigator, double-KO both credit), score → win check (25 / 8:00 / 60 s SD + damage tiebreak), **scored respawn selection** (farthest-from-threat over spawn points from the arena manifest), bot slot fill/backfill via `UBRBotManagerComponent`. |
| `BRGameState.h/.cpp` | Replicated match truth: phase, `MatchEndServerTime` (clients render the clock locally — one float, not a ticking rep), team scores, rocket-spawner countdown mirror, killfeed ring buffer (`FBRKillFeedEntry`, RepNotify → UI delegate). |
| `BRPlayerState.h/.cpp` | **Owns the ASC + AttributeSet** (persists across respawn by construction). TeamID, K/D/A (RepNotify → scoreboard VM). Spawns with loadout `UBRAbilitySet`. |
| `BRPlayerController.h/.cpp` | Input mode routing, death-cam target, the **intent boundary for UI** (UI calls PC; PC calls server). Owning-client telemetry relay for coach lines. |

### 3.6 `AI/` — 4 class-units

| File | Job |
|---|---|
| `BRBotController.h/.cpp` | `AAIController` + `UStateTreeComponent` + `UAIPerceptionComponent` (sight 3000uu/FOV 70°, hearing). Seeds its decision jitter ONCE from match seed (determinism law). Presses **the same input path a human uses**: activates abilities by InputTag on its PlayerState ASC — no aimbot hooks; aim error applied from `FBRBotTuningRow.accuracy_pct` before the fire ability ever runs. |
| `BRStateTreeTasks.h/.cpp` | ALL StateTree tasks + conditions in one pair (less is more): `FBRSTTask_MoveToCover`, `FBRSTTask_EngageTarget`, `FBRSTTask_ThrowGrenade`, `FBRSTTask_ContestRocket`, `FBRSTCond_ShieldsBelow`, `FBRSTCond_RocketSpawning`. States themselves are authored in the StateTree asset; logic lives here in C++. |
| `BREnvQueryContexts.h/.cpp` | EQS contexts/tests: `UBREnvQueryContext_Threats`, `UBREnvQueryTest_CoverFromThreat`, `_DistanceToRocket`. The queries (assets) are data; the scoring code is here. |
| `BRBotManagerComponent.h/.cpp` | On GameMode (server-only): fills empty slots at match start, backfills leavers after 10 s grace, applies the difficulty scalar set from `DT_BotTuning`. |

### 3.7 `Online/` — 4 class-units (+1 reserved)

| File | Job |
|---|---|
| `BRSessionsSubsystem.h/.cpp` | **Ported** `OSSessionsSubsystem`: Steam OSS create/find/join/destroy behind `FindAndJoinBestSession`; host + invite flow; delegates the UI consumes. PIE runs Null OSS — claims name their rung. |
| `BRServerLifecycle.h` | **`IBRServerLifecycle`** — the migration seam: `Initialize / OnMatchReady / OnMatchEnded / Shutdown` + health hooks. Callers (GameMode, sessions) never know the backend. |
| `BRListenServerLifecycle.h/.cpp` | The slice implementation: near-trivial (host is the server), which is exactly the point — the *interface* is the deliverable. |
| `BRGameLiftLifecycle.h/.cpp` | **PHASE 2 — file reserved, not created.** Server SDK 5: `InitSDK/ProcessReady/OnStartGameSession/OnProcessTerminate` mapped to the same interface. Listed here so the swap has a named landing site. |

### 3.8 `UI/` — 5 class-units

| File | Job |
|---|---|
| `BRUIManagerSubsystem.h/.cpp` | `ULocalPlayerSubsystem`: CommonUI activatable layer stack — `GameHUD (Input: Game, never deactivated) → Menu → Modal`. Push/pop API; back-action via CommonUI. |
| `BRActivatableWidget.h/.cpp` | `UCommonActivatableWidget` base: input-config-by-enum, the only widget base class in the project. **All logic in C++ bases; Blueprints hold layout/animation only.** |
| `BRVM_Combat.h/.cpp` | MVVM ViewModel (FieldNotify): ShieldsPct, HealthPct, AmmoInMag, AmmoReserve, GrenadeCount, GrappleCooldownPct. Fed by ASC attribute-change delegates + cooldown tag events — **zero polling, zero property bindings**. |
| `BRVM_Match.h/.cpp` | ViewModel: team scores, clock (derived locally from `MatchEndServerTime`), rocket countdown, killfeed entries, medal toasts, carnage report rows. Fed by GameState/PlayerState RepNotify delegates. |
| `BRHUDLayout.h/.cpp` | The GameHUD activatable: binds sub-widgets (BP visuals) to the two ViewModels; killfeed rows from a widget pool. Distinct shield-hit vs flesh-hit marker driven by cue → VM event. |

**CommonUI from C++ — the verdict baked into this design:** yes. All
screen *management*, input routing, and data flow is C++
(subsystem + bases + ViewModels); only visual layout lives in Widget
Blueprints. This is the Lyra-derived production pattern and it satisfies
"no Blueprint runtime logic" — a BP widget subclass with zero graph
nodes is an asset, not logic.

### 3.9 `Telemetry/` — 2 class-units

| File | Job |
|---|---|
| `BRTelemetrySubsystem.h/.cpp` | WorldSubsystem, **authority-only**: folds kill/damage/parry-of-fire events (native delegates from GameMode/AttributeSet) into `FBRMatchTelemetry` per player. Zero hooks inside abilities. |
| `BRSpotterSubsystem.h/.cpp` | GameInstanceSubsystem, **authority-only, never load-bearing**: async `FHttpModule` → Claude Haiku; ≤ 12 event calls + ≤ 8 coach calls; 3 s timeout; `DT_SpotterLines` canned fallback; output = replicated strings via GameState killfeed/carnage entries. Weak-ref callbacks die with the match. API key from server-side config only. |

### 3.10 `Data/` — 1 file (deliberately)

| File | Job |
|---|---|
| `BRDataRows.h` | **Every row struct in one header:** `FBRWeaponRow` (damage, RPM, mag, reserve, spread, damage tags, ability set, meshes/cue refs), `FBRBotTuningRow` (accuracy, reaction_ms ≥ 200 enforced in PostLoad, grenade policy, cover pref, rocket_contest), `FBRMatchRules` (kill target, time, SD cap), `FBRSpotterLineRow`, `FBRKillFeedEntry`, `FBRMatchTelemetry`. One include for any system touching data; the CSVs in `Content/Data/` are the values. |

### 3.11 `Tests/` — 3 files (cpp only)

| File | Pins |
|---|---|
| `BRCombatSpec.cpp` | `Breachpoint.Sim.Combat`: damage exec exact cases (AR TTK vs 100/100, Magnum headshot math, rear-melee lethality, shields-first order) + invariants (health never regens, damage never negative) — values asserted **against the DataTable**, not literals |
| `BRShieldSpec.cpp` | `Breachpoint.Sim.Shields`: recharge gating (2.5 s tag), 60/s rate, break/restore events |
| `BRBotDeterminismSpec.cpp` | `Breachpoint.Bots`: same seed + same tuning row ⇒ identical action trace |

---

## 4. What deliberately does NOT exist (the less-is-more ledger)

| Not created | Because |
|---|---|
| Per-system modules / plugins | One module + owner-path folders gives the same discipline without build tax |
| `UBRAssetManager`, custom engine subclasses | Engine defaults suffice at this scope |
| Inventory system | Two slots is an array on a component, not a system |
| Multiple attribute sets | One set; splitting is Phase-2 refactor *if* attributes grow |
| GameplayMessageRouter (Lyra plugin) | Third-party-adjacent; native delegates + RepNotify cover a slice this size |
| Weapon actor per held weapon | Held weapon = data + mesh component; only *pickups* are actors |
| Server-side rewind / lag compensation | Lyra-parity validation instead (§5.3); rewind is a Phase-2 system with its own packet |
| Save system, settings screens, cosmetics | Not in the GDD's shipped scope |

**Budget: ~44 class-units ≈ 85 source files + 3 targets + 1 module pair.**
Every one owned by a named crew discipline.

---

## 5. Cross-Cutting Contracts

### 5.1 Replication policy (the complete enumerated surface)

| State | Where | Condition | Consumers |
|---|---|---|---|
| Shields/Health (+Max) | PlayerState ASC attributes | COND_None (bars are public) | HUD, bots' perception of winded… n/a — of "shields broken" via cue |
| Ammo in mag / reserve | `BRWeaponInstance` | **COND_OwnerOnly** | Owner HUD only |
| Equipment slots + active index | `BREquipmentComponent` | COND_None (visible weapon) | All (mesh swap), owner HUD |
| Match phase / `MatchEndServerTime` / team scores | GameState | COND_None | HUD, bots |
| K/D/A, TeamID | PlayerState | COND_None | Scoreboard |
| Killfeed ring buffer / Spotter strings | GameState | COND_None | Feed UI |
| Rocket countdown | `ABRPowerWeaponSpawner` | COND_None | HUD timer, bots (`ContestRocket`) |
| Grapple state | via CMC saved moves + ability rep | prediction machinery | — |
| **Never replicated** | exact enemy ammo, bot internals, API key, telemetry aggregates | — | — |

### 5.2 Damage flow (the one pipeline)

```
BRGA_WeaponFire (client): trace → FGameplayAbilityTargetDataHandle
   └─ prediction window → server
BRGA_WeaponFire (server): VALIDATE {rate ≤ RPM, ammo > 0, dir within cone
   of muzzle, range ≤ weapon max}  → reject = drop silently (client whiff)
   └─ apply GE_Damage (SetByCaller: base dmg, tags: Damage.Kinetic[.Headshot])
BRDamageExecCalc (server): headshot/rear multipliers → shields first,
   overflow → health
BRAttributeSet::PostGameplayEffectExecute (server): apply RecentDamage tag
   (kills shield regen 2.5 s) → Health ≤ 0 → Event.Death → GameMode scores
GameplayCues (all clients): impact, shield break, kill toast — cosmetic only
```

### 5.3 The hitscan trust model — stated honestly

Client-side trace + server validation is **Lyra/ShooterGame parity**: the
server never trusts the *claim* (it re-checks rate, ammo, cone, range)
but does trust the client's *aim* within tolerance. A cheater with a
modified client could aim-perfect within that cone; they could not
shoot faster, without ammo, through their budget, or backwards. Full
server-side rewind closes the remaining gap and is a **named Phase-2
packet** — for the slice, this is the professional trade at this scale,
and writing it down is what makes it a decision instead of a hole.

---

## 6. `Content/` — assets only, zero logic

```
Content/
├── Data/                       # CSVs = the tuning truth (reimport scripted)
│   ├── DT_Weapons.csv          ├── DT_Combat.csv (curves)
│   ├── DT_BotTuning.csv        ├── DT_MatchRules.csv
│   └── DT_SpotterLines.csv
├── Maps/
│   ├── BR_Arena01.umap         # from arena_manifest.json (crew blockout)
│   └── BR_Entry.umap           # front-end map
├── AbilitySystem/              # GA/GE/Cue ASSETS (thin BP data containers)
│   ├── Effects/  (GE_Damage, GE_ShieldRegen, GE_Cooldown_*)
│   ├── Cues/     (GC_Weapon_*, GC_Shields_*, GC_Grapple_*)
│   └── Sets/     (AS_Loadout_Default, AS_Weapon_AR/Magnum/Rocket)
├── Characters/                 # sourced meshes + FPS anim sets, ABP assets
├── Weapons/                    # sourced weapon meshes/anims
├── AI/
│   ├── ST_Bot.st               # the StateTree asset (states; logic is C++)
│   └── EQS/                    # query assets
├── UI/                         # WBP_* visual subclasses of C++ bases only
├── Audio/                      # MetaSound sources + attenuations (cue-driven)
└── VFX/                        # sourced Niagara
```

---

## 7. `Tools/` — build, verify, deploy

```
Tools/
├── env.local.example           # ENGINE_ROOT=... (copied to env.local per machine, git-ignored)
├── run-ubt.ps1                 # rung 1: clean UBT, all three targets
├── run-specs.ps1               # rung 2: headless Automation (Breachpoint.Sim.*, .Bots.*)
├── run-gauntlet.ps1            # rung 4: BRGauntlet.SmokeTS2C — server + 2 clients join
│                               #   BR_Arena01, A shoots B, assert-in-threes, then 120ms/5%
├── reimport-tables.ps1         # CSV → DataTable commandlet (never manual)
├── steam/
│   ├── app_build.vdf           # Steam app build script (demo depot)
│   └── depot_build.vdf         # content depot mapping (Windows64)
└── server/                     # PHASE 2 (reserved, empty at slice)
    ├── Dockerfile              #   Linux server image for GameLift Managed Containers
    └── fleet-notes.md          #   SDK5 wiring checklist → IBRServerLifecycle
```

**Deployment paths, both phases:**
- **Slice (ship):** `run-ubt` Game target → package Win64 client →
  `steamcmd +run_app_build app_build.vdf` → demo depot. Sessions over
  Steam OSS listen server. That is the whole pipeline.
- **Phase 2 (post-course):** Server target → `Tools/server/Dockerfile` →
  GameLift Managed Containers fleet; `BRGameLiftLifecycle` drops in
  behind `IBRServerLifecycle`; Steam auth tickets validated server-side.
  No caller changes — that's what the interface bought.

---

## 8. Crew Owner-Path Map (who may touch what)

| owner_path | Agent | Contracts binding |
|---|---|---|
| `Source/Breachpoint/AbilitySystem`, `Weapons`, `Tests`, `Data` | sim-builder | data-and-assets, testing |
| Replicated surface anywhere + `Match/`, `BRGA_Grapple` | netcode-builder | netcode |
| `Source/Breachpoint/AI` | ai-builder | netcode (intent-only), data-and-assets |
| `Source/Breachpoint/Online` | services-builder | online-services |
| `Source/Breachpoint/UI` | ui-builder | (UI doctrine in agent def) |
| `Source/Breachpoint/Character` + anim assets | builder → anim-builder | animation |
| `Content/Data/*.csv` | curators propose → builder lands | data-and-assets |
| `Tools/`, CI, Gauntlet | builder + verifier | testing |

Two standing review triggers: any new `UPROPERTY(Replicated)` or
`UFUNCTION(Server)` anywhere = netcode-builder packet + critic REFUTER;
any literal number next to a gameplay noun = data-contract finding.

---

## 9. Week-1 File Manifest (what exists after the first ticket)

The ladder-bootstrap + foundations ticket creates exactly: the 3 targets,
module pair, `Core/` (3), `BRAbilitySystemComponent`, `BRAttributeSet`,
`BRGameplayAbility`, `BRAbilitySet`, `BRDamageExecCalc`, `BRGA_WeaponFire`,
`BRCharacter`, `BRCharacterMovementComponent`, `BREquipmentComponent`,
`BRWeaponInstance`, `BRPlayerState`, `BRGameMode`, `BRGameState`,
`BRPlayerController`, `BRServerLifecycle.h` + listen impl, `BRDataRows.h`,
`BRCombatSpec`, `BRShieldSpec`, `DT_Weapons.csv`, `DT_Combat.csv`,
Tools scripts. **≈ 24 class-units — half the project — because Week 1 is
the foundation week and everything after it is additive.**
```
Gate check: shields break on a dummy, all three targets compile clean,
Breachpoint.Sim.* red-then-green, Gauntlet skeleton runs.
```
