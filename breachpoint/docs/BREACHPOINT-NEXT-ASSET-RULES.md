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

### 1b. Where to search, in order

**Founder, 13 Aug 2026: "all assets are in the FPSTemplate folder — remember that."**
**And, same day: "we have the MigrateLyra as well… we are using a lot of stuff from Lyra."**

**CORRECTED 13 Aug, same day** — the first version of this section said everything outside
FPSTemplate was "not ours", full stop. The founder corrected it: *"we have the MigrateLyra as well…
we are using all the animation assets from Lyra, and we are using a lot of stuff from Lyra."* The
rule is a **search ORDER**, not a single folder.

| # | Search | Notes |
|---|---|---|
| 1 | `/Game/FPSTemplate/…` | The primary source. 2,102 assets. Look here first, always. |
| 2 | `/Game/MigrateLyra/…` | The Lyra migration — 826 assets, mostly mannequin animation and material functions. Legitimate and in use. |
| 3 | The rest of `Content/` | Legitimate but check WHY a thing is only there. `/Game/UI/` (756) is a real HUD library and will matter; `/Game/Characters/` (887) is largely a duplicate of FPSTemplate's mannequin set. |
| 4 | `/Game/BN/…` | Ours, and only for what steps 1–3 genuinely do not have. |

**The duplicate hazard stands, and it is the reason the order exists.** `ABP_Mannequin_Base` exists
in three places, `AM_MM_Rifle_Melee` in three, `IA_Fire` in three. When a search returns several
matches, **take the FPSTemplate one** — the others are the same asset at a different point in
history, and picking one produces a bug that reads as the asset being broken. Order beats
proximity: a match in step 3 does not win just because you found it first.

*Written because a ticket got it wrong twice over. The melee/grenade handoff pointed at
`/Game/Input/Actions/IA_Melee`, which is not FPSTemplate's — and then the correction over-reached
and declared the whole rest of `Content/` off limits, which threw away Lyra.*

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
- **GameplayCues** (`UBNGameplayCue_*`) — C++ classes; the FX/sound they *play* are assets.
- **Data row structs** (`FBNWeaponRow`) — C++; only the DataTable built on it is an asset.

**AMENDED by §7.** The LOGIC of all of the above stays C++ — that has not changed and is not
negotiable. What changed is that each now also gets a thin **Blueprint child holding its asset
references**, set in the editor, so a swap is a click rather than a rebuild. Read this section as
*"do not reimplement these as Blueprints"*, not *"these have no editor presence"*.

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

## 5b. THE DUPLICATION LAW — the mistake that cost R3, in rules that prevent it

**Founder's ruling, 14 Aug 2026, after duplicated anim layers were identified as the frozen-aim
root cause.** These are not preferences. A violation is a `high` finding and gets reverted.

### The law

1. **NEVER duplicate an animation asset.** Anim Blueprints, anim layers, ABP children, linked
   layer classes, montages: link, reference and reuse the FPSTemplate original. This is the
   one category where duplication has now caused a multi-week bug.
2. **Never duplicate ANY asset to "make a BN version" of it.** Ownership is not a reason.
   BN-owned copies exist to hold DIVERGENT VALUES, and only when the divergence exists today.
3. **A duplicate needs a written reason BEFORE it exists.** In the ticket, in advance, naming
   the specific value that must differ and why the original cannot carry it. No reason written
   = do not duplicate. "The lead can decide later" means: hand it back, do not copy.
4. **A duplicate you did not create is not a precedent.** Finding `ABP_BNWeaponLayers_Rifle`
   in a row is not permission to make `_Shotgun`. Copying an undocumented divergence is how a
   habit forms — that is exactly how this bug spread from one row to two.
5. **Anything already duplicated without a written reason is presumed WRONG** and gets
   repointed at the original on sight, in a ticket, never silently.

### Why — the mechanism, so it is understood and not just obeyed

A duplicated anim layer keeps **property-access bindings compiled against the main AnimBP's
layout on the day it was copied**. When the main ABP is reparented — as ours was, moving
`Pitch` / `PitchRotator` / `bFPSMode` into C++ — the original layers get recompiled against
the new layout and keep working. **The duplicates do not.** They resolve to zero, with no
error, no warning, and no compile failure. The pose simply stops moving, and every upstream
value looks perfect while you hunt for weeks.

That is the trap: **duplication of an animation asset does not fail loudly. It fails silently
and permanently.**

### What the terminal does instead

| Situation | Action |
|---|---|
| A ticket names a layer/ABP asset | Link the FPSTemplate original by path |
| The original "does not work" | **Hand it back** with what you observed. Do not duplicate to route around it |
| A row already points at a `/Game/BN/` anim asset | Report it in the Log as a suspected stale duplicate |
| A ticket explicitly orders a duplicate WITH a written reason | Make it, and copy that reason into the asset's ticket Log |

### The tell, in the log, at runtime

C++ now announces this automatically on every link (§below and `BNLayerCheck`). A line reading
`[BN DUPLICATE]` is a defect report, not a status line. If you see it after a pass of yours,
you introduced it — fix it before closing the ticket.

## 5c. THE LOGGING LAW — nothing communicates silently

**Founder's standing order, 14 Aug 2026:** *"in every stuff that we have a communication for the
animation, the anim instance, and the adding layers, we need to have logs."*

Any code that hands a value across a boundary it cannot verify — into a Blueprint, an anim
instance, a linked layer, a component function, or an ability — **must announce the outcome of
that handoff, both ways.** Concretely:

- **Whether the connection exists at all** (linked / not linked, function found / not found).
- **WHICH one it is, by full path or class name.** Never "a layer" — name it.
- **Whether each value landed or was rejected**, by property name.
- **On the EDGE, never per frame.** Announce on change, on first sight, and on failure. A log
  that spams is a log that gets ignored, which is the same as no log.
- **A failure is `Warning` or `Error`, never silence.** Silence is what made every bug in R3
  expensive.

Live traces (per-frame streams) are allowed only behind an explicit console toggle that
defaults OFF — `BNAimLog 1` is the pattern.

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

---

## 7. THE STANDING RULE: soft in C++, set in the editor, wired by the terminal

**Founder, 13 Aug 2026, and it governs everything from here:**

> *"Whatever you do — if you are making a gameplay cue, or you are using the assets inside the
> gameplay ability, or if you are using it inside the character, or whenever you're using it — we
> want to have it as a soft reference, and then you need to task the terminal for him to use the
> Unreal MCP so he can set the correct assets directly from the editor in the Blueprint class. And
> that goes for everything."*

Three parts, and all three are required. Two out of three is the bug.

| # | Rule | Why |
|---|---|---|
| 1 | **C++ declares the reference SOFT** — `TSoftObjectPtr` / `TSoftClassPtr`, never a hard pointer, never `ConstructorHelpers`, never a `/Game/` literal in a `.cpp` | Law 3. A hard ref drags the asset into memory with the class and makes the module depend on content |
| 2 | **The VALUE is set in the editor**, on a Blueprint child of the C++ class | So a swap is a click, not a rebuild. This is the half that was missing — values lived in `DefaultGame.ini` |
| 3 | **The terminal sets it through the Unreal MCP**, from a ticket naming exact asset paths | The lead cannot open the editor; the founder should not have to click through a list |

### What this supersedes

`Config/DefaultGame.ini` was carrying the values (the R2-W2 "Config ruling"). That was the right
answer when no Blueprint children existed. It is now the **fallback**, not the destination: a BP
child that leaves a field empty still reads the ini line, which is what makes the migration
reversible one asset at a time.

**Do not delete an ini line until its Blueprint has been confirmed working in a playtest.**

### The trap that comes with it — read before making a cue Blueprint

`UBNGameplayCueRegistrar` registers every class derived from `UBNGameplayCue_Base`. A BP child
inherits `GetHandledCueTag()`, so it claims its **parent's tag**, and `UGameplayCueSet` replaces on
a duplicate — the surviving handler would depend on class-iteration order.

Fixed in C++ ahead of the migration: the registrar now keeps only the **most-derived** class per tag
and logs `BNCues: <tag> -> <class>` for each. With no BP children it selects exactly what it did
before. **If a cue Blueprint's assets do not appear, read that log line first** — it names which
class actually answered.

### The order of operations, every time

1. C++ gets the soft property (lead)
2. C++ ships with the ini fallback set, so nothing is silent in the meantime (lead)
3. Ticket naming the BP class, the property, and the exact asset path (lead)
4. Terminal creates/edits the BP child through the MCP and sets it (terminal)
5. Founder playtests; only then does the ini line go (lead)
