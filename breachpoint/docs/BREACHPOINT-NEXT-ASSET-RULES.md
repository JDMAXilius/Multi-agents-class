# BREACHPOINT NEXT — asset rules, and the terminal's scope

**Cut:** 13 August 2026, founder's ruling · **Binds:** every session, every ticket, every packet.
**Read this before any editor work.** It is short on purpose.

---

## 1. Reuse first. Never author what already exists.

This project is the FPS template's *content* under BN's *architecture*. The template already
ships the effects, sounds, weapons, meshes, montages and animations. **Use them.**

- **Do NOT author a new Niagara system, sound, montage, mesh or material** when the template has
  one that fits. Find it and point at it.
- Creating from scratch is allowed only when the founder asks for it, or when a search proves
  nothing suitable exists — and then say so explicitly in the Log, naming what you searched.
- Initiative that produces a redundant asset is not initiative; it is a second source of truth.

## 2. Where a new asset goes — and when it should not move

| Case | Rule |
|---|---|
| **We create it from scratch** | It is born in `/Game/BN/<Domain>/`, mirroring `Source/BreachpointNext/<Domain>/`. Never anywhere else. |
| **It already exists elsewhere** (FPSTemplate, engine content) | **Leave it where it is** and reference it. Do NOT copy it into `/Game/BN/`. |
| **We must diverge from an existing asset** | Duplicate into `/Game/BN/`, and say in the Log what diverged and why. This is the exception, not the habit. |

Copying an asset we did not change costs a second copy to keep in sync, and the sync is what
rots. A reference costs nothing.

## 3. What is C++ and therefore has NO asset

Do not go looking for these in the Content Browser and do not create assets for them:

- **Gameplay abilities** (`UBNGA_*`) — C++ classes. Visible in the Class Viewer, never the Content Browser.
- **Attributes** (`UBNAttributeSet`) — a C++ class.
- **GameplayEffects** (`UBNGE_*`) — C++ classes.
- **GameplayCues** (`UBNGameplayCue_*`) — C++ classes; the FX/sound they *play* are assets, pointed at by config.
- **Data row structs** (`FBNWeaponRow`) — C++; only the DataTable built on it is an asset.

The only ability-shaped assets are the **AbilitySets** (`DA_BNAbilitySet_*`), which say *which*
C++ ability to grant with *which* input tag.

## 4. Inputs

Every input goes through the established path: an `IA_*` **InputAction** asset → a row in
`DA_BNInput` binding it to a tag → a mapping in `IMC_BNNext`. Reuse a template `IA_*` when its
value type matches; create a BN-owned one otherwise. No input is ever hard-bound in C++.

## 5. THE TERMINAL'S SCOPE — do what the ticket says, and stop

Observed problem: the terminal does substantially more than it was asked. That costs review time
and puts unrequested changes in the tree. The rules:

- **Do only what the ticket lists.** If the ticket has six items, do those six.
- **Finding extra work is a REPORT, not a licence.** Write it in the Log, name it, and stop. The
  lead decides whether it becomes a ticket.
- **Never touch `Source/`.** If a ticket cannot be completed without a C++ change, say so and
  stop — that is a hand-back, not a blocker to route around.
- **Never edit an asset the ticket did not name.** An accidental editor dirty-save is a real
  change: revert it and say so (this has already happened once, on
  `BPC_FPST_Lyra_FireEffectComp`).
- **The read-back is the deliverable.** Intent vs actual, per item. "The call returned" proves
  nothing.
- **When something cannot be done as written, stop and hand it back.** Do not improvise a
  substitute. The tracer parameter mismatch was handled exactly right: it stopped, recorded why,
  and left the field unset rather than inventing a system.

## 6. Current BN asset inventory (13 Aug 2026)

All 16 live under `/Game/BN/`. Nothing BN-owned sits outside it.

```
BN/Animation/   ABP_BNMannequin (DEAD — see below) · ABP_BNWeaponLayers_Rifle · _Pistol
BN/Characters/  BP_BNCharacter
BN/Core/        BP_BNGameMode · BP_BNPlayerController · BP_BNPlayerState
BN/Data/        DT_BNWeapons · DA_BNAbilitySet_Rifle · _Pistol
BN/Input/       DA_BNInput · IMC_BNNext · IA_BNWeaponNext · IA_BNWeaponPrevious
                IA_BN_LeanLeft · IA_BN_LeanRight
```

**`ABP_BNMannequin` is dead** — the shipped reparent route made it unused (open item E2). It
should be deleted so it cannot rot into a second source of truth.
