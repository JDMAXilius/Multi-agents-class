# BREACHPOINT NEXT — the framework, file by file

**Revised:** 12 August 2026 (v3 — research-checked) · **Status:** structure and manifest only. No code.
**Location:** `Source/BreachpointNext/` — a sibling of `Source/Breachpoint/`, not wired to any build.

**63 units · 121 files · 15 top-level folders · 8 subfolders.**
The module it replaces has **272 files across 34 folders**, and that comparison is the point.

**Provenance.** This manifest is derived from what the game needs, not from what the old tree
has. Every unit traces to one of: a project law, or a finding in
[`BREACHPOINT-NEXT-RESEARCH.md`](BREACHPOINT-NEXT-RESEARCH.md) — six reference projects read on
disk (Lyra 482 files · OnSight 395 · ZoransResistance 250 · NewMoons 87 · ShooterCore 42 ·
UE5_Multiplayer_FPS 38) plus the Epic/AWS docs for EOS + Steam + GameLift. Where a name here
matches an old-tree name, that is convergence on the same engine concept (a GameMode is a
GameMode), not inheritance.

> **Not set in stone.** This is the target shape, and it is expected to move once code is
> written against it. When a better approach shows up, the structure changes — that is the
> rule, not an exception to it. What this document buys is that every change is then a
> *decision* rather than an accretion.

---

## The brief this shape answers

UE 5.8 · native C++ only · multiplayer-native (server-authoritative from the first line) ·
**GAS purity** · **CommonUI purity** · **less is more**.

Five principles decided every call below. Where they conflicted, the one higher on this list won:

1. **Fewer files, straight to the point.** A file exists because something needs a name. A class
   that is one function is a function. A folder that holds one file is a naming problem, not a
   structure. UE lets several `UCLASS`es share a header — that is used deliberately here
   (`BRMovementAbilities`, `BRGameplayEffects`, `BRAnimNotifies`) wherever the classes are small,
   related, and always change together.
2. **Modularity where reuse is real.** Reuse when the second caller exists — not in anticipation
   of one. Every "generic" layer in the old tree that had exactly one user is gone.
3. **One home per concept.** If a file could plausibly live in two folders, the structure is wrong.
4. **Group by domain, never by base class.** This is why there is no `Subsystems/`, no
   `Managers/`, no `Components/` at the root. A subsystem lives with the thing it serves.
5. **The shape encodes the laws.** One damage folder (law 2). One row-struct header (law 3).
   No `Tick/` anything (law 4). No `Blueprints/` (R18). Bots and players share one pawn, because
   "there is no privileged path" is an architecture statement, not a comment.

---

## The complete tree

```
Source/BreachpointNext/
│
├── Core/ ─────────────────────────────────────────────────────────── 3 units · 5 files
│   ├── BRGameplayTags.h/.cpp        every native tag, declared once
│   ├── BRTypes.h                    enums + POD structs (ETeam, EMatchPhase) — header-only
│   └── BRCore.h/.cpp                log categories · collision channels · team attitude solver
│
├── Data/ ─────────────────────────────────────────────────────────── 3 units · 5 files
│   ├── BRDataRows.h                 EVERY FTableRowBase, one header — header-only
│   ├── BRGameData.h/.cpp            GameInstance subsystem: owns the tables, resolves soft refs
│   └── BRAssetSettings.h/.cpp       UDeveloperSettings: soft refs to tables + ability sets
│
├── Input/ ────────────────────────────────────────────────────────── 2 units · 3 files
│   ├── BRInputConfig.h/.cpp         DataAsset: InputAction → InputTag. No gameplay knowledge
│   └── BRInputComponent.h           templated tag binding — header-only, no .cpp to write
│
├── AbilitySystem/ ────────────────────────────────────────────────── 16 units · 32 files
│   ├── BRAbilitySet.h/.cpp          DataAsset: what to grant, handles to revoke on unequip
│   ├── BRGameplayAbility.h/.cpp     the base: activation policy, input tag, cost/cooldown
│   ├── BRGameplayCues.h/.cpp        every cue handler as C++ — several UCLASS, one file
│   │
│   ├── Abilities/ ───────────────────────────────────────────────── 8 units · 16 files
│   │   ├── BRGA_Fire.h/.cpp         predicted fire; hitscan OR projectile from the weapon row
│   │   ├── BRGA_Reload.h/.cpp
│   │   ├── BRGA_Melee.h/.cpp        notify-window trace
│   │   ├── BRGA_Grenade.h/.cpp
│   │   ├── BRGA_Grapple.h/.cpp      predicted pull via a CMC root-motion source
│   │   ├── BRGA_Equip.h/.cpp        equip · swap · drop — one verb family, one class
│   │   ├── BRGA_Death.h/.cpp
│   │   └── BRMovementAbilities.h/.cpp   Sprint · Jump · Crouch — 3 UCLASS, one file
│   │
│   ├── Attributes/ ──────────────────────────────────────────────── 1 unit · 2 files
│   │   └── BRAttributeSet.h/.cpp    Health · Shield · IncomingDamage · move magnitudes. ONE set
│   │
│   ├── Components/ ──────────────────────────────────────────────── 1 unit · 2 files
│   │   └── BRAbilitySystemComponent.h/.cpp   input buffer · tag activation · batched RPC
│   │
│   └── Effects/ ─────────────────────────────────────────────────── 3 units · 6 files
│       ├── BRGameplayEffects.h/.cpp     the generic GE set (cost, cooldown, damage, state
│       │                                tags) — SetByCaller-driven, several UCLASS, one file
│       ├── BRDamage.h/.cpp              THE one damage door. The only spec builder that exists
│       └── BRDamageExecution.h/.cpp     shields→health, headshot, friendly fire
│
├── Characters/ ───────────────────────────────────────────────────── 4 units · 8 files
│   ├── BRCharacter.h/.cpp                    ONE pawn. Players and bots both possess it
│   ├── BRCharacterMovementComponent.h/.cpp   CMC subclass + FSavedMove_BR in the same header
│   ├── BRHealthComponent.h/.cpp              attribute → gameplay bridge; owns death detection
│   └── BRCameraComponent.h/.cpp              first-person view, recoil/kick offsets
│
├── Actors/ ───────────────────────────────────────────────────────── 3 units · 6 files
│   ├── BRProjectile.h/.cpp          ONE projectile, row-driven (grenade, rocket, plasma)
│   ├── BRPickup.h/.cpp              ONE pickup, row-driven (weapon, ammo, powerup)
│   └── BRPlayerStart.h/.cpp         team-tagged spawn; the SCORING lives in the GameMode
│
├── Weapons/ ──────────────────────────────────────────────────────── 2 units · 4 files
│   ├── BRWeapon.h/.cpp              the weapon actor: mesh + replicated state. No firing logic
│   └── BREquipmentComponent.h/.cpp  slots, swap, grant/revoke the weapon's AbilitySet
│
├── Animation/ ────────────────────────────────────────────────────── 3 units · 6 files
│   ├── BRAnimInstance.h/.cpp        thread-safe update. The graph READS fields, never computes
│   ├── BRAnimLayer.h/.cpp           IBRAnimLayer + the linked-layer base; class from a soft row
│   └── BRAnimNotifies.h/.cpp        melee window · footstep · fire — several UCLASS, one file
│
├── AI/ ───────────────────────────────────────────────────────────── 4 units · 8 files
│   ├── BRAIController.h/.cpp        possession, perception config, StateTree host
│   ├── BRAmbitionScorer.h/.cpp      the ambition layer: deterministic utility, no LLM, no Tick
│   └── StateTree/ ──────────────────────────────────────────────── 2 units · 4 files
│       ├── BRStateTreeTasks.h/.cpp        several FStateTreeTaskCommonBase, one file
│       └── BRStateTreeConditions.h/.cpp   several FStateTreeConditionCommonBase, one file
│
├── UI/ ───────────────────────────────────────────────────────────── 11 units · 22 files
│   ├── BRUISubsystem.h/.cpp         LocalPlayer subsystem: the ONLY thing that pushes a screen
│   ├── BRPrimaryLayout.h/.cpp       the CommonUI activatable stack + layer tags
│   ├── BRActivatableWidget.h/.cpp   the screen base: input mode, visibility, back handling
│   │
│   ├── Components/ ──────────────────────────────────────────────── 3 units · 6 files
│   │   ├── BRButton.h/.cpp          UCommonButtonBase subclass — the whole button module
│   │   ├── BRProgressBar.h/.cpp     health · shield · reload — one bar, driven by a ViewModel
│   │   └── BRReticle.h/.cpp         crosshair + hitmarker
│   │
│   ├── ViewModels/ ─────────────────────────────────────────────── 2 units · 4 files
│   │   ├── BRPlayerViewModel.h/.cpp   health, shield, ammo, equipped weapon
│   │   └── BRMatchViewModel.h/.cpp    phase, timer, team scores, killfeed
│   │
│   └── Screens/ ────────────────────────────────────────────────── 3 units · 6 files
│       ├── BRHUDScreen.h/.cpp
│       ├── BRScoreboardScreen.h/.cpp
│       └── BRPauseScreen.h/.cpp
│
├── Match/ ────────────────────────────────────────────────────────── 5 units · 10 files
│   ├── BRGameMode.h/.cpp            server-only: phases, spawn selection, scoring rules
│   ├── BRGameState.h/.cpp           replicated match truth: phase, timer, team scores
│   ├── BRPlayerState.h/.cpp         the ASC host — outlives the pawn, so respawn is free
│   ├── BRPlayerController.h/.cpp    input owner · UI owner · client RPC target
│   └── BRMatchMessages.h/.cpp       verb messages ("X killed Y with Z") + replicated broadcast
│                                    — gameplay→UI decoupling via GameplayMessageRouter
│                                    (Lyra's Messages/ pattern; see RESEARCH §2)
│
├── Online/ ───────────────────────────────────────────────────────── 2 units · 4 files
│   ├── BRServerLifecycle.h/.cpp     IBRServerLifecycle — listen now, dedicated behind the seam
│   └── BRSessionSubsystem.h/.cpp    create · find · join
│
├── Interfaces/ ───────────────────────────────────────────────────── 1 unit · 2 files
│   └── BRInterfaces.h/.cpp          IBRTeamAgent, IBRInteractable — so folders talk w/o including
│
├── Utilities/ ────────────────────────────────────────────────────── 2 units · 4 files
│   ├── BRStatics.h/.cpp             pure helpers only. Nothing here owns state
│   └── BRCheatManager.h/.cpp        give weapon · set health · spawn bot — the testing lever
│
└── Tests/ ────────────────────────────────────────────────────────── 2 units · 2 files
    ├── BRSimSpec.cpp                damage, attributes, equipment — .cpp only, no header
    └── BRBotSpec.cpp                bot determinism
```

---

## Where the file count went — the decisions worth arguing with

| Decision | Files saved | Why it holds |
|---|---|---|
| **One pawn, not player + bot** | ~6 | Law: bots press the same tags through the same ASC. Two pawn classes create the privileged path the law forbids. |
| **One projectile, one pickup, row-driven** | ~10 | Grenade vs rocket is a data row, not a subclass. The moment it isn't, subclass — not before. |
| **One AttributeSet** | ~4 | Splitting into Health/Combat/Movement sets buys nothing at 4v4; it costs an extra `GetSet` at every callsite. |
| **Firing lives in `BRGA_Fire`, not a `Firing/` layer** | ~8 | Spread, recoil and the trace are the ability's job. A weapon that computes its own fire is a second damage path waiting to happen. |
| **Several `UCLASS` per file where they always change together** | ~12 | Sprint/Jump/Crouch are 40 lines each. Three files, three headers, three includes for that is ceremony. |
| **No `Phases/`, `Scoring/`, `Respawn/`** | ~8 | These are methods on `BRGameMode`/`BRGameState`. A folder implies a class; a class implies a lifetime; they have neither. |
| **CommonUI does the layer stack** | ~6 | `BRPrimaryLayout` + layer tags replaces a hand-rolled screen manager. |
| **No custom AbilityTasks or TargetData yet** | ~6 | `WaitTargetData`, `WaitGameplayEvent`, `PlayMontageAndWait` and `FGameplayAbilityTargetData_SingleTargetHit` cover the slice. Write one when the engine's actually falls short. |
| **`Online/` stays at 2 units despite EOS + Steam + GameLift** | ~10 | EOS+Steam coexistence is `OnlineSubsystemEOSPlus` **configuration**, not per-service classes; the game codes only against the abstract interfaces (Lyra's CommonSession pattern, NewMoons' one-file proof). GameLift is Phase 2: implementation #2 of `IBRServerLifecycle`, whose interface is GameLift-shaped (session-activated · admission · health · terminate) from day one. |

---

## Four folders from v1 that are not here

I removed these when the file-level pass gave each of them zero files. Each is one `mkdir` away.

| Removed | Why |
|---|---|
| **`Subsystems/`** | Grouping by base class is the same mistake as `Managers/`. `BRGameData` is a subsystem and belongs in `Data/`; `BRUISubsystem` belongs in `UI/`. The folder can only ever hold "things that happen to inherit from the same thing." |
| **`Audio/`** | Law 2 routes all FX — sound included — through GameplayCues, which are already C++ classes in `AbilitySystem/BRGameplayCues`. The one real gap (unwired UI sounds) is a `CommonButtonStyle` field, not a C++ file. |
| **`Telemetry/`** | Nothing in the vertical slice writes an event. Add it with its first file. |
| **`Python/`** | Your Python already lives in `Tools/` (`gen_ui`, `gen_input`, `blockout`, `reimport_tables`). A second Python home inside `Source/` splits the tooling for no gain. |

Also gone from v1: `Actors/Volumes`, `Actors/Interactables`, `Weapons/Attachments`, `Animation/Nodes`
(blocked on an editor module the project doesn't have), `AI/EQS`, `AI/Perception`, `AI/Ambitions`,
`UI/Styles`, `UI/Layers`, `UI/HUD` — all zero-file at slice scope.

---

## Three open decisions

1. **`BehaviorTree/` — StateTree or both?** You named both. Running both means two brains and two
   places to look for one bug. `BREACHPOINT-AI-BOTS.md` already specifies a StateTree spine, and
   UE 5.8's direction is StateTree, so the tree above is **StateTree-only**. Say the word if a
   specific behaviour wants BT and it comes back.
2. **Second module, or rename over `Source/Breachpoint/`?** Nothing here forecloses either. A
   second module means `BreachpointNext.Build.cs` + a `Modules` entry in the `.uproject`.
3. **Flat, or `Public/`+`Private/`?** This tree is flat. `OnSight` (the reference) uses the split.
   Flipping is mechanical while there are no files; after, it is a 119-file move.

---

## What is deliberately not created

`Variant_*` · a second animation folder · `FPS/` (first-person is a camera and a mesh, not a
discipline) · `Blueprints/` (R18; the R26 defaults-only child lives in `Content/`) · `Managers/` ·
`Common/`, `Misc/`, `Shared/` · a top-level `Damage/` — there is one door and it is
`AbilitySystem/Effects/BRDamage`.
