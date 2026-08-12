# BREACHPOINT NEXT — Roadmap 1: The Character Spine

**Cut:** 12 August 2026 · **Domain:** Gameplay (trunk) + minimum Animation + multiplayer-correct
throughout · **Self-contained:** this roadmap and the NEXT docs
([STRUCTURE](BREACHPOINT-NEXT-STRUCTURE.md) · [RESEARCH](BREACHPOINT-NEXT-RESEARCH.md) ·
[DOMAINS](BREACHPOINT-NEXT-DOMAINS.md)) are the only documents in force for this work. Older
project documents do not bind it.

---

## Status — 12 August 2026

**Goals 1–5 DONE, founder-verified at Stage B** (Checkpoint A: walk/look standalone ✓ ·
Checkpoint B: jump + crouch on both windows, client+server ✓). Goal 6 C++ written and
review-complete (one blocking anim-thread race found and fixed; all graph-facing properties
now published only from the thread-safe update). Goal 7 scripts in progress. Next:
**Checkpoint C** — run the `Tools/bn/` scripts, then the mannequin animates in both windows.

### Log — 12 August 2026, G4 reworked onto real input assets

G4 shipped with the controller calling `NewObject<UInputAction>` in `SetupInputComponent`:
four transient InputActions and a transient IMC, fabricated per run. It played at Checkpoint A,
but there was no asset to open, nothing for the founder to change without a rebuild, and
`BNInputConfig`/`BNInputComponent` — built by G4.1 — were dead code the controller never called.
Reworked to the shape G4.2 actually specifies:

- `ABNPlayerController` is `Config=Game` and holds exactly two references, both
  `TSoftObjectPtr`: `InputConfig` and `MappingContexts[]` (added at priority = index).
  Set in `DefaultGame.ini`, so no BP child of the controller has to exist (law 7 prefers the ini).
- The controller creates its own `UBNInputComponent` before `Super::SetupInputComponent()` —
  the engine's sanctioned override point. It does NOT ride the project-wide
  `DefaultInputComponentClass`, which names the OLD module's `BRInputComponent`; NEXT depends
  on nothing there.
- `BindActionByTag` now returns bool and every miss is logged. A tag with no InputAction used
  to bind nothing, silently — that is how a dead control ships.
- `Tools/bn/10_input_assets.py` creates `IMC_BNNext` + `DA_BNInput`, reusing the FPSTemplate IAs
  per the reuse verdict but ONLY when the asset's value type matches what the handler reads;
  otherwise it creates a BN-owned IA and says so in the audit. It audits the
  `ABNPlayerController` CDO, not just the assets — the CDO rows prove the ini actually resolved.
- Mappings replicate the founder-verified Checkpoint A feel exactly (swizzle+negate on Move,
  negate-Y on Look). Gamepad is deliberately NOT added: reproduce verified behaviour in assets
  first, add new behaviour as its own change.

Also found and fixed, unrelated to the rework: **`BreachpointNext.Build.cs` never had
`PublicIncludePaths.Add("BreachpointNext")`.** The module has no Public/Private split, so its
own root was not on the include path and every `.cpp` failed to open its own header. The module
had never been compiled on the founder's machine — it landed from the cloud branch. The old
module carries the equivalent line; this one was missing since creation.

Character, same pass: crouch moved the camera by the capsule's shrink only, leaving the view
floating above the crouched head — `OnStartCrouch`/`OnEndCrouch` now offset it by
`HalfHeightAdjust`. The `MoveSpeed` attribute delegate was never unregistered, and the ASC
outlives the pawn (it is the PlayerState's), so every respawn left a dead binding on it;
removed in `EndPlay`. `GetMesh()->SetOwnerNoSee(true)` moved into the C++ constructor so the
class is correct without the BP.

**Editor run — DONE, 21/21.** `10_input_assets.py` ran through `UnrealEditor-Cmd
-run=pythonscript` (the MCP bridge lives in the editor process and was not reachable this
session). It created `/Game/BN/Input/IMC_BNNext` and `/Game/BN/Input/DA_BNInput`, reused all
four `IA_FPST_*` (every value type already matched — Move/Look AXIS2D, Jump/Crouch BOOLEAN, so
no BN-owned IA was needed), and audited 21 rows clean. Re-run: still 21/21, idempotent. The two
rows that matter are the last two — the `ABNPlayerController` CDO resolves both assets out of
`DefaultGame.ini`, which is the claim "input is wired" actually rests on.

Four UE-5.8 Python surfaces the script had to be corrected against, all found by probing the
live editor rather than by guessing:
- `FKey`'s python constructor takes no arguments; `key_name` is set via `set_editor_property`.
- `FGameplayTag.TagName` is Read-Only and python has no `RequestGameplayTag` — the struct's
  `import_text('(TagName="…")')` is the only way in.
- `UInputModifierNegate`'s properties are `x/y/z`, not `bX/bY/bZ`.
- **`UInputMappingContext::Mappings` is deprecated in 5.8** — the live property is
  `DefaultKeyMappings`. The script writes that and empties the deprecated array, so a PostLoad
  migration of leftovers cannot double every binding. An audit row asserts it stays empty.

Two C++ changes fell out of the editor work, both about scriptability, neither changing
runtime behaviour: `FBNInputBinding` needed `BlueprintType` (UE generates no python bindings
for unexposed structs, so no committed script could build a row), and its properties plus
`UBNInputConfig::Bindings` moved from `EditDefaultsOnly` to `EditAnywhere` — a DataAsset on
disk IS an instance, so `EditDefaultsOnly` sets `CPF_DisableEditOnInstance` and python refuses
to write with "cannot be edited on instances". Same details panel either way.

**Rung 1: PARTIAL.** All 15 `BreachpointNext` compile actions pass, zero errors. The target
still FAILS on `Source/Breachpoint/` — BP82's in-flight folder rename (`Animation/` →
`Animations/`) moved the files but not the `#include "Animation/..."` lines, and
`BRMannequinAnimInstance.h` declares both `IsCrouching` and `isCrouching`, which UHT rejects as
one name. Untouched here: that is BP82's packet. Checkpoint A/B cannot be re-run, and
`10_input_assets.py` cannot run, until it compiles — the script correctly refuses when
`/Script/BreachpointNext.BNInputConfig` will not load.

## The one-line goal

A first-person character on the mannequin skeleton, possessed and playable — ASC living on the
PlayerState, all input through the PlayerController, jump and crouch as tag-activated gameplay
abilities, and an animation instance that reads **gameplay tags, not booleans** — correct in
standalone AND in client+server from the first build.

## Operating rules for this roadmap (the founder's, recorded once)

- **Tight code.** Comments: rare, 1–2 lines max, only where the code cannot say it. Logs:
  one category, minimal, temporary ones deleted before a checkpoint is offered for testing.
- **C++ everything** except what UE cannot express in C++: the AnimGraph, meshes/anim assets,
  input assets, and one thin defaults-only BP child per class that needs asset defaults.
- **Multiplayer from line one.** No local-only state that pretends. Old code that isn't
  multiplayer-correct is not a reference.
- **Founder tests; the crew builds — including the editor work.** Every roadmap ships its
  asset setup as committed Python scripts run through the editor (Unreal MCP / Editor Script
  Plugin), followed by a read-back audit proving the result. The founder's hands touch only
  the play button. (Standing rule for ALL roadmaps, not just this one.)
- **Variables: only the necessary ones. Asset references: soft (`TSoftObjectPtr`/
  `TSoftClassPtr`) wherever UE allows it.**

## Reuse verdict (you asked — here it is)

| What exists | Verdict |
|---|---|
| Old module C++ (`Source/Breachpoint/` — incl. its 28 animation files, `BRFPSCharacter`) | **Not reused.** NEXT code is written fresh; nothing in `BreachpointNext` includes the old module. It stays compiling, untouched, and we may *mine it for reference* in the later animation roadmap — never copy it in. |
| `Content/FPSTemplate/` — SK_Mannequin, animation packs, the FPS template ABP | **Reused as assets.** The ABP is *reparented* to our C++ `UBRAnimInstance`; its event-graph logic and variables migrate to C++ (this roadmap: initial migration only — the tag-driven states). AnimGraph + linked-layer graphs stay as-is this pass. |
| Existing input assets (`IA_FPST_Look`, `IA_FPST_Jump`, …) | **IA assets reused; new `IMC_BRNext` mapping context** so we inherit zero stale mappings. |
| Old BP characters (`BP_FPSCharacter`, `BP_FPST_Character`, `BP_BRcharacter`) | **Visual reference only.** Their logic is not multiplayer-correct and is never an implementation reference. New thin `BP_BRCharacter` child holds mesh/ABP defaults only. |

---

## Files this roadmap creates

15 units — the R1 subset of the STRUCTURE manifest, plus module boilerplate:

```
Source/BreachpointNext/
├── BreachpointNext.Build.cs · BreachpointNext.h/.cpp     module + the one log category
├── Core/       BRGameplayTags.h/.cpp
├── Input/      BRInputConfig.h/.cpp · BRInputComponent.h
├── AbilitySystem/
│   ├── BRAbilitySystemComponent.h/.cpp
│   ├── BRGameplayAbility.h/.cpp
│   ├── BRAbilitySet.h/.cpp
│   ├── Attributes/BRAttributeSet.h/.cpp
│   ├── Abilities/BRMovementAbilities.h/.cpp              BRGA_Jump · BRGA_Crouch
│   └── Effects/BRGameplayEffects.h/.cpp                  passive attribute-init GE
├── Characters/ BRCharacter.h/.cpp
├── Animation/  BRAnimInstance.h/.cpp
└── Match/      BRPlayerState.h/.cpp · BRPlayerController.h/.cpp · BRGameMode.h/.cpp
```

Editor-side (thin, defaults only): `BP_BRCharacter` (meshes + ABP class) · reparented FPS ABP ·
`IMC_BRNext` · a test map whose WorldSettings points at `BRGameMode`.

Where each of those lands, and the complete editor-artifact inventory with its blockers:
**[`BREACHPOINT-NEXT-CONTENT-LAYOUT.md`](BREACHPOINT-NEXT-CONTENT-LAYOUT.md)** — `/Game/BN/<Domain>`
mirrors `Source/BreachpointNext/<Domain>`, and R1 authors exactly four assets.

---

## Goal 1 — The module exists

*The framework becomes real: a second runtime module compiling next to the old one.*

| # | Task |
|---|---|
| 1.1 | `BreachpointNext.Build.cs` — deps: Core, CoreUObject, Engine, InputCore, EnhancedInput, GameplayAbilities, GameplayTags, GameplayTasks, NetCore, AnimGraphRuntime |
| 1.2 | Module entry in `Breachpoint.uproject`; `BreachpointNext.h/.cpp` with `LogBRNext` |
| 1.3 | Old module untouched — zero edits to `Source/Breachpoint/` |

**Objective:** editor builds with both modules; old game runs exactly as before.

## Goal 2 — The tag vocabulary

*Tag management from the get-go: every name the spine speaks, declared once, natively.*

| # | Task |
|---|---|
| 2.1 | `BRGameplayTags.h/.cpp` — native tags, one file: `Input.Move` · `Input.Look` · `Input.Jump` · `Input.Crouch` |
| 2.2 | State tags: `State.Movement.Jumping` · `State.Movement.InAir` · `State.Movement.Crouching` (`Landing` only if the ABP proves it needs it — not speculative) |
| 2.3 | No string-built tags anywhere; everything references these symbols |

**Objective:** tags appear in the editor's tag manager; grep finds zero `RequestGameplayTag("...")` string literals outside this file.

## Goal 3 — The GAS spine on the PlayerState

*The ASC lives where multiplayer says it must; the character is a body that bridges to it.*

| # | Task |
|---|---|
| 3.1 | `BRPlayerState`: owns `BRAbilitySystemComponent` + `BRAttributeSet` (Health, Shield, MoveSpeed). Mixed replication mode, sensible NetUpdateFrequency |
| 3.2 | `BRCharacter` implements `IAbilitySystemInterface`, forwards to the PlayerState's ASC |
| 3.3 | The bridge, both net roles: `InitAbilityActorInfo` in `PossessedBy` (server) **and** `OnRep_PlayerState` (client) — the client-side init is the step single-player-minded code always misses |
| 3.4 | `BRGameplayEffects`: one passive infinite GE initializing attributes; applied on grant |
| 3.5 | `BRAbilitySet` (DataAsset): the grant list — abilities + passive effects, handles kept; granted server-side on possess |

**Objective:** in PIE, `showdebug abilitysystem` shows the attributes and granted abilities on the server **and** on a client.

## Goal 4 — Input through the PlayerController

*All input and action mapping in one place: the controller. The pawn owns none of it.*

| # | Task |
|---|---|
| 4.1 | `BRInputConfig` (DataAsset): InputAction → InputTag pairs; `BRInputComponent`: templated tag binds (header-only) |
| 4.2 | `BRPlayerController`: adds `IMC_BRNext`, binds everything. Move + Look drive the pawn directly (no ability ceremony for axis input) |
| 4.3 | Jump + Crouch binds call the ASC **by tag** — press → activate by `Input.Jump`/`Input.Crouch`, release → input-released event. Hard C++ path, no BP layer |

**Objective (Checkpoint A, standalone):** WASD + mouse + camera respond; the pawn walks the test map.

## Goal 5 — Movement abilities

*Jump and crouch are abilities. State is tags. Nothing tracks a boolean.*

| # | Task |
|---|---|
| 5.1 | `BRGameplayAbility` base: input tag, activation-owned tags, instancing + net execution policy defaults |
| 5.2 | `BRGA_Jump`: activates by tag, drives `Character::Jump`, owns `State.Movement.Jumping`/`InAir` through the ASC; ends on landing |
| 5.3 | `BRGA_Crouch`: press/release through the ability; drives the engine crouch (already replicated); mirrors `State.Movement.Crouching` as an ASC tag |
| 5.4 | State tags applied/removed **through the ASC only**, so they exist on server and all clients — never `SetBool` anywhere |

**Objective (Checkpoint B, standalone → then client+server):** jump and crouch work; on the client+server run, *the other window's character* visibly jumps and crouches; tags flip in `showdebug abilitysystem` on both.

## Goal 6 — The animation instance on tags

*The ABP is reused; its brain moves to C++; its states come from GAS.*

| # | Task |
|---|---|
| 6.1 | `BRAnimInstance`: `NativeInitializeAnimation` caches character + ASC; thread-safe update computes Speed/Direction; **no logic remains in the ABP event graph**. Variables: only what the graph actually reads — nothing exposed "just in case" |
| 6.2 | Booleans the template ABP exposed become tag-driven state: the instance registers ASC tag-change delegates for `State.Movement.*` and exposes the results to the graph |
| 6.3 | Reparent the FPS template ABP to `UBRAnimInstance`; delete its event graph; AnimGraph + linked-layer *graphs* untouched this pass (layer C++ migration = the Animation roadmap) |
| 6.4 | **Linked layers hooked up correctly, unarmed by default:** `BRCharacter` exposes the current-weapon seam (returns none — no weapons exist yet); at anim init the layer class resolves from it — none → the **Unarmed** layer class, held as a `TSoftClassPtr` and linked via `LinkAnimClassLayers`. When weapons arrive, the layer swaps through this seam with zero animation edits |
| 6.5 | `BP_BRCharacter`: 1P arms owner-only, 3P mannequin hidden-to-owner — both meshes assigned, ABP on the 3P (1P layer wiring stays minimal this pass) |

**Objective (Checkpoint C):** the mannequin animates from tag-driven states — idle/walk/jump/crouch — through the linked unarmed layer, in standalone and in both PIE windows.

## Goal 7 — Asset wiring by automation (the founder touches nothing)

*Every editor-side artifact this roadmap needs is created by committed scripts through the
editor (Unreal MCP / Python Editor Script Plugin) — the same generated-scripts method the UI
work proved — then read back and audited.*

| # | Task |
|---|---|
| 7.1 | Script: create `BP_BRCharacter` (child of `BRCharacter`, defaults only) — assign 1P/3P meshes, ABP class, visibility flags; all asset paths as soft references |
| 7.2 | Script: reparent the FPS template ABP to `UBRAnimInstance`; clear its event graph; verify the unarmed layer link resolves |
| 7.3 | Script: create `IMC_BRNext` with the four mappings (reusing the existing IA assets); create/point the test map's WorldSettings at `BRGameMode` |
| 7.4 | Read-back audit script: every property set in 7.1–7.3 is read back from the live editor and diffed against the intended values — the audit output is the proof, not "the script ran" |

**Objective:** from a fresh pull, the founder runs the scripts (or asks the crew to), opens the test map, presses Play. No manual editor setup steps exist anywhere in this roadmap.

---

## How the crew runs this roadmap

Executed by the NEXT crew ([`BREACHPOINT-NEXT-CREW.md`](BREACHPOINT-NEXT-CREW.md) — 3 agents)
in four waves: **W1** G1+G2 (done, pre-crew) · **W2** G3+G4 → `bn-builder`, reviewed by
`bn-critic`, ends at Checkpoint A · **W3** G5 → same loop, Checkpoint B · **W4** G6
(`bn-builder`) + G7 (`bn-editor`), Checkpoint C. Every wave: build → one-lens multiplayer
review → merge → checkpoint. The founder's Stage B pass is the only DONE.

## The founder's test protocol (the definition of done)

**Stage A — standalone PIE.** Walk, look, jump, crouch. Everything responds; no log spam.

**Stage B — multiplayer in editor.** Net mode: *Play as Listen Server*, 2 players, separate
windows. Pass means:
- both windows: own arms visible (1P), own mannequin not blocking the camera
- each window sees the **other** character's full body, animating — walk, jump, crouch
- `showdebug abilitysystem` on a client shows the same attribute values and state tags as the server
- no errors on join, no divergent state after 60 seconds of movement

A goal is DONE when its objective passes **Stage B**, not Stage A. Checkpoints A/B/C are the
three moments to grab the build and play.

## Explicitly out of scope for Roadmap 1

Fire/damage/health changes · sprint · any UI beyond the debug HUD · AI · sessions/online ·
anim linked-layer C++ migration · procedural anim (sway/recoil) · pickups/weapons. Each is a
later roadmap; nothing here pre-builds for them.
