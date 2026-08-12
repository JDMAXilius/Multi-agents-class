# BREACHPOINT NEXT — the folder architecture

**Cut:** 12 August 2026 · **Status:** structure only — no code, no build wiring.
**Location:** `Source/BreachpointNext/` (sibling of the existing `Source/Breachpoint/` module).

This document is the **visual structure** of the reworked framework: where every future file
goes, and — more importantly — what may *not* go there. It is the map, not the build. Nothing
in `Breachpoint.uproject`, `Breachpoint.Build.cs`, or the three `*.Target.cs` files was touched;
`BreachpointNext/` contains only directories and `.gitkeep` markers, so UBT does not see it and
the existing module compiles exactly as before.

**Why a parallel tree and not an in-place cleanup.** The current module carries 272 source files
across 34 folders, including 36 UE-template files (`Variant_Horror/`, `Variant_Shooter/`, the three
`breachpoint*` files) that nothing in the `BR` tree references, plus two folders that are the same
discipline spelled twice (`anim/` and `Animation/`, `Weapons/` and `FPS/`). A parallel tree means
every file that lands in it landed on purpose: nothing arrives by inheritance.

---

## The three rules that produced this shape

1. **One home per concept.** If a file could plausibly live in two folders, the structure is
   wrong. Projectiles live in `Actors/Projectiles/` and are *referenced* by `Weapons/` — they are
   not duplicated there.
2. **A folder is an ownership boundary.** Every top-level folder maps to a discipline in
   `docs/method/ENGINEERING-DISCIPLINES.md`, so a packet's `owner_path` is always a real path and
   `guard_laws.py` has something to enforce.
3. **The folder shape encodes the laws.** `AbilitySystem/Effects/Damage/` being the *only*
   damage folder is CLAUDE.md law 2 made structural. `Data/Rows/` being the only place a
   `FTableRowBase` lives is law 3. There is no `Tick/` anything, because of law 4.

---

## The visual structure

```
Source/BreachpointNext/
│
├── Core/ ─────────────────── the vocabulary everything else speaks
│   ├── Tags/                 native GameplayTag declarations — ONE source of truth
│   ├── Types/                shared enums + plain structs, zero logic
│   ├── Logging/              log categories
│   ├── Collision/            channels, profiles, trace queries
│   └── Settings/             UDeveloperSettings — config-backed, never gameplay numbers
│
├── Data/ ─────────────────── law 3: data is not code
│   ├── Rows/                 every FTableRowBase struct
│   ├── Assets/               UPrimaryDataAsset definitions
│   └── Providers/            table ownership + soft-ref resolution (TSoftObjectPtr only)
│
├── Input/ ────────────────── intent in, tags out
│   ├── Config/               InputConfig DataAsset: InputAction → InputTag
│   └── Components/           the owned InputComponent; produces a TAG, never a call
│
├── AbilitySystem/ ────────── law 2: abilities are the only verb
│   ├── Abilities/
│   │   ├── Combat/           fire, reload, melee, grenade
│   │   ├── Movement/         sprint, jump, grapple, crouch
│   │   └── Equipment/        equip, swap, drop, pickup
│   ├── Effects/
│   │   ├── Calculations/     ExecCalcs + ModifierMagnitudeCalculations
│   │   └── Damage/           THE one damage door — the only spec builder in the codebase
│   ├── Attributes/           AttributeSets + clamping
│   ├── Components/           the ASC subclass, ability-granting components
│   ├── Cues/                 GameplayCue handlers as C++ classes (law 2: all FX via cues)
│   ├── Tasks/                AbilityTasks
│   ├── TargetData/           client→server hit transport inside the prediction window
│   └── Sets/                 AbilitySet DataAssets: what to grant, handles to revoke
│
├── Characters/ ───────────── the pawn is a body, not a brain
│   ├── Player/               the player pawn
│   ├── Bot/                  the bot pawn — same pawn contract, no privileged path
│   ├── Components/           per-pawn components (health display, team, footsteps)
│   ├── Movement/             the CMC subclass + FSavedMove
│   └── Camera/               camera modes, view target, spectator
│
├── Actors/ ───────────────── things that exist in the world
│   ├── Pickups/              weapon/ammo/powerup pickups
│   ├── Projectiles/          the ONE projectile home (weapons reference, never redefine)
│   ├── Volumes/              kill volumes, capture zones, out-of-bounds
│   ├── Interactables/        doors, terminals, switches
│   └── Spawning/             spawn points and spawn-selection actors
│
├── Weapons/ ──────────────── the equipment layer (rework decision D-2 lives here)
│   ├── Equipment/            slots, the quickbar, the manager component
│   ├── Instances/            per-weapon runtime instance objects
│   ├── Firing/               hitscan trace, spread, recoil, fire modes
│   ├── Ammo/                 reserves, magazines, the named GAS-purity exception
│   └── Attachments/          scopes, barrels — Phase 2, folder reserved
│
├── Animation/ ────────────── law: the graph reads fields, never computes
│   ├── Instances/            AnimInstances (thread-safe update only)
│   ├── Layers/               linked anim layers + IBRAnimLayer
│   ├── Notifies/             AnimNotify / AnimNotifyState (melee windows, footsteps)
│   ├── Nodes/                custom FAnimNode_* (blocked on an editor module — see ledger)
│   └── Data/                 anim DataAssets, montage tables
│
├── AI/ ───────────────────── three-layer brain: ambitions → StateTree → GAS hand
│   ├── StateTree/            the spine
│   │   ├── Tasks/
│   │   ├── Conditions/
│   │   └── Evaluators/
│   ├── BehaviorTree/         where BT is the better fit — tasks/decorators/services
│   │   ├── Tasks/
│   │   ├── Decorators/
│   │   └── Services/
│   ├── EQS/                  EnvQuery generators, tests, contexts
│   ├── Perception/           sight/hearing config, the bot's world model
│   ├── Controllers/          AIController, blackboard wiring
│   └── Ambitions/            the GOAP-style ambition scorer (deterministic, no LLM)
│
├── UI/ ───────────────────── CommonUI + MVVM; UI never mutates authoritative state
│   ├── Components/           reusable widgets (buttons, bars, tiles)
│   ├── Screens/              full activatable screens
│   ├── HUD/                  in-match layer
│   ├── ViewModels/           the ONLY thing widgets bind to — zero polling, zero Tick
│   ├── Styles/               style assets/classes, tokens
│   └── Layers/               the CommonUI activatable stack + layer registration
│
├── Match/ ────────────────── the match frame
│   ├── GameModes/            server-only rules authority
│   ├── GameStates/           replicated match truth
│   ├── Host/                 PlayerState + PlayerController — the ASC host pair
│   ├── Phases/               warmup → live → post
│   ├── Scoring/              score, kill attribution, killfeed, medals
│   └── Respawn/              death, respawn timing, spawn selection
│
├── Subsystems/ ───────────── lifetime-scoped services
│   ├── GameInstance/         survives level travel
│   ├── World/                per-world
│   └── LocalPlayer/          per-local-player (settings, input mapping, UI routing)
│
├── Online/ ───────────────── D4: sessions, lifecycle, platform
│   ├── Session/              create/find/join
│   ├── Lifecycle/            IBRServerLifecycle — listen now, dedicated later
│   └── Platform/             Steam identity; never trusted without validation
│
├── Audio/ ────────────────── D6
│   ├── Components/           audio components, occlusion, mix control
│   └── Data/                 sound tables, MetaSound parameter contracts
│
├── Telemetry/ ────────────── measurement, never gameplay
│   ├── Events/               event structs
│   └── Sinks/                where they go
│
├── Interfaces/ ───────────── cross-discipline UINTERFACEs, so folders talk without including
│
├── Utilities/ ────────────── helpers only; nothing here owns state
│   ├── Libraries/            UBlueprintFunctionLibrary statics
│   ├── Math/                 pure functions
│   └── Debug/                cheat manager, debug draw, console commands
│
├── Tests/ ────────────────── D7
│   ├── Specs/                automation specs (Breachpoint.Sim.*, Breachpoint.Bots.*)
│   └── Gauntlet/             the networked rung — server + 2 clients, assert in threes
│
└── Python/ ──────────────── editor-side automation (NOT compiled; UBT ignores non-C++)
    ├── editor/               commandlets, Remote Control, editor entry points
    ├── generators/           generated-scripts-over-live-editing: blockouts, input, WBPs
    └── audits/               read-back audits that prove an asset matches its spec
```

**19 top-level folders · 82 leaf directories.**

---

## Folder ownership — who writes where

| Folder | Discipline | Owning agent |
|---|---|---|
| `Core/`, `Data/`, `Utilities/`, `Interfaces/` | shared vocabulary | `builder` (single-writer; changes are contract-level) |
| `AbilitySystem/`, `Weapons/`, `Characters/`, `Actors/` | D1 gameplay math | `sim-builder` |
| `Match/`, `Subsystems/` | D1 + D2 | `sim-builder` / `netcode-builder` |
| replication surfaces anywhere | D2 authority | `netcode-builder` (packet + critic REFUTER) |
| `Animation/` | D3 | `anim-builder` |
| `Online/` | D4 | `services-builder` |
| `UI/` | D5 | `ui-builder` |
| `Audio/` | D6 | `builder` |
| `Tests/` | D7 | `builder` / `verifier` runs it |
| `AI/` | D8 | `ai-builder` |
| `Input/` | intent layer | `builder` |
| `Python/` | tooling | any builder, per the `ue-editor` skill |

---

## Deliberate omissions — folders that will NOT be created

| Not here | Why |
|---|---|
| `Variant_*/` | the template is not the architecture; it was the whole reason for the rework |
| a second animation folder | `anim/` and `Animation/` in the old tree are one discipline spelled twice |
| `FPS/` | first-person is a camera and a mesh, not a discipline — it lives in `Characters/Camera/` |
| `Blueprints/` | R18: zero Blueprint classes; the R26 exception is a defaults-only child and lives in `Content/`, not `Source/` |
| `Managers/` | a manager is a subsystem or a component; the name hides which |
| `Common/`, `Misc/`, `Shared/` | the folders where architecture goes to die |
| `Damage/` at top level | there is one damage door and it is inside `AbilitySystem/Effects/Damage/` |

---

## What is NOT decided yet

These are open, and naming them here is cheaper than discovering them mid-packet:

1. **Second module vs replacement.** Does `BreachpointNext` become a second compiled module
   (its own `.Build.cs` + a `Modules` entry in the `.uproject`), or does it get renamed over
   `Source/Breachpoint/` once populated? Nothing here forecloses either.
2. **Public/Private split.** The reference project (`OnSight`) uses `Public/` + `Private/`;
   the current Breachpoint module does not. This tree is flat — flipping to the split is a
   mechanical move if you want it, but it doubles every path.
3. **Migration order.** Which files move first, and whether anything moves at all versus being
   rewritten against the new shape.
4. **Relationship to `docs/BREACHPOINT-GAMEPLAY-REWORK.md`.** That document's 29 units / 53
   files fit inside this tree, but it targets `Source/Breachpoint/` by path. One of the two
   needs to be repointed before BP90–BP102 are executed.
