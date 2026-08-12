# BREACHPOINT NEXT — Roadmap 1: The Character Spine

**Cut:** 12 August 2026 · **Domain:** Gameplay (trunk) + minimum Animation + multiplayer-correct
throughout · **Self-contained:** this roadmap and the NEXT docs
([STRUCTURE](BREACHPOINT-NEXT-STRUCTURE.md) · [RESEARCH](BREACHPOINT-NEXT-RESEARCH.md) ·
[DOMAINS](BREACHPOINT-NEXT-DOMAINS.md)) are the only documents in force for this work. Older
project documents do not bind it.

---

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
- **Founder tests; the crew builds.** Testing protocol at the bottom; three checkpoints, each
  a thing you can run.

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
| 6.1 | `BRAnimInstance`: `NativeInitializeAnimation` caches character + ASC; thread-safe update computes Speed/Direction; **no logic remains in the ABP event graph** |
| 6.2 | Booleans the template ABP exposed become tag-driven state: the instance registers ASC tag-change delegates for `State.Movement.*` and exposes the results to the graph |
| 6.3 | Reparent the FPS template ABP to `UBRAnimInstance`; delete its event graph; AnimGraph + linked layers untouched this pass (layer C++ migration = the Animation roadmap) |
| 6.4 | `BP_BRCharacter`: 1P arms owner-only, 3P mannequin hidden-to-owner — both meshes assigned, ABP on the 3P (1P layer wiring stays minimal this pass) |

**Objective (Checkpoint C):** the mannequin animates from tag-driven states — idle/walk/jump/crouch — in standalone and in both PIE windows.

---

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
