# TICKET — Roadmap 2 Wave 2 assets, through the Unreal MCP
> STATUS: in-progress — mac terminal 13 Aug 2026 (8befcd9). Editor live, MCP on :8000. Runs after DT_BNWEAPONS.

**Cut:** 13 August 2026 by the cloud lead · **For:** the terminal session (editor + MCP in reach)
**Binds to:** the NEXT doc family only · **Owner path:** `Content/BN/`
**Unblocks:** **Checkpoint I** — spawn holding a weapon, see it in the hands, cycle weapons, and
the other window sees the right weapon and pose set.

C++ is landed (`e71f7c5`). This is every asset it announced. **Extends**
[`BREACHPOINT-NEXT-TASK-DT-BNWEAPONS.md`](BREACHPOINT-NEXT-TASK-DT-BNWEAPONS.md) — that ticket owns
the table's row VALUES and how to read them off the template weapons; do not duplicate that work
here, just add the one column below.

**Time test:** roughly six assets plus input wiring — past the couple-of-minutes line, so this is
MCP work rather than founder clicks. If the MCP fights on any single asset, stop and hand that one
back as steps rather than sinking time into it.

## Kickoff

1. **Build first.** Every struct and class below must be loadable, and the module changed at
   `e71f7c5`.
2. Assets that do not exist yet resolve null by design — `FindWeaponRow` returning null is the
   designed miss, not a crash. A red before this ticket runs is this ticket.

## The minimum for Checkpoint I

**Ability sets are NOT required for this checkpoint.** Swap moved to the PlayerState grant at
`e71f7c5`, so the row's `AbilitySet` column may be left **null** and switching still works. The
sets exist for Fire and Reload, which are G4. That is the difference between six assets and four.

### 1 · `DT_BNWeapons` — one column added

Per its own ticket, at `/Game/BN/Data/DT_BNWeapons` on `FBNWeaponRow`, rows named exactly
**`Rifle`** and **`Pistol`** (the ini's `StartupWeaponRows` names them literally). `FBNWeaponRow`
gained **`AbilitySet`** (`TSoftObjectPtr<UBNAbilitySet>`) — **leave it null for now**. Added
columns take defaults, so a table already built to the earlier ticket needs no rebuild.

The one field that matters most for this checkpoint is **`AnimLayerClass`**, which must point at
the matching ABP from §2 — soft **class** ref, `_C` suffix.

### 2 · The per-weapon anim layers

| Asset | Parent |
|---|---|
| `/Game/BN/Animation/ABP_BNWeaponLayers_Rifle` | `/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/LinkedLayers/ABP_ItemAnimLayersBase` |
| `/Game/BN/Animation/ABP_BNWeaponLayers_Pistol` | same |

**The template base IS the BN base** — there is no BN base and none should be made. This inverts
an earlier note; the reason is in `BREACHPOINT-NEXT-TASK-ABP-REPAIR.md` under "Roadmap 2 note —
INVERTED": every layer sharing one main ABP is what makes the cast resolve, and a second main ABP
is what would break it again.

**Cheapest correct route:** duplicate the template's existing per-weapon layer for the equivalent
weapon (the FPSTemplate ships rifle/pistol layers under `.../Animations/LinkedLayers/`) rather
than authoring pose slots from scratch — the ~90 pose slots are content, and duplicating keeps the
FPSTemplate originals untouched. Report which source you duplicated.

### 3 · Input — two actions, two rows, two mappings

| Asset | Type / detail |
|---|---|
| `/Game/BN/Input/IA_BNWeaponNext` | `UInputAction`, value type **Boolean** |
| `/Game/BN/Input/IA_BNWeaponPrevious` | `UInputAction`, value type **Boolean** |
| `/Game/BN/Input/DA_BNInput` | two rows: the actions above ↔ tags `Input.Weapon.Next` and `Input.Weapon.Previous` |
| `/Game/BN/Input/IMC_BNNext` | mouse wheel up → Next, wheel down → Previous (the template's feel) |

No template IA fits — FPSTemplate ships Fire/Reload/Aim/Sprint and no swap action. Both are
new BN-owned assets.

Two traps already paid for on `DA_BNInput`, from `10_input_assets.py`'s Log: `FGameplayTag.TagName`
is read-only in Python, so `import_text('(TagName="…")')` is the only way to set a tag; and
**`UInputMappingContext::Mappings` is DEPRECATED in 5.8** — write `DefaultKeyMappings` and leave
the deprecated array empty, or a PostLoad migration doubles every binding.

### 4 · NOT needed — do not create

- **`BP_BNWeapon`.** The row supplies mesh, both sockets, the anim layer and the ability set, so
  `ABNWeapon` spawns as the C++ class and a defaults-only child would hold nothing. The builder
  refused this one on its own; if a reason for it appears, it costs one `UPROPERTY(Config)` on the
  component and should be asked for explicitly.
- **`DA_BNAbilitySet_Rifle` / `_Pistol`** — not for this checkpoint (see above). They arrive with
  G4, holding Fire and Reload.

### 5 · Wave 3 input — sprint and lean *(added 13 Aug, batched here to save a second trip)*

C++ landed with Wave 3. Three more rows and three more mappings, same two assets as §3.

| Asset | Detail |
|---|---|
| `Input.Sprint` action | **Probably reuse** `/Game/FPSTemplate/Input/Actions/IA_FPST_Sprint`. The handler reads no value — it needs only Started/Completed on a digital action. **Confirm the value type is BOOLEAN in the editor before adding the row**; R1's reuse verdict allows a template IA only when the type matches, and this one is an LFS pointer here so it could not be verified from the repo. If it is not Boolean, create `/Game/BN/Input/IA_BN_Sprint` instead and say so |
| `/Game/BN/Input/IA_BN_LeanLeft` | `UInputAction`, Boolean — new, the template ships no lean action |
| `/Game/BN/Input/IA_BN_LeanRight` | `UInputAction`, Boolean — new |
| `DA_BNInput` rows | the three actions ↔ tags `Input.Sprint`, `Input.Lean.Left`, `Input.Lean.Right` |
| `IMC_BNNext` mappings | **Shift** = sprint, **Q** = lean left, **E** = lean right (the reference's keys, `MyCharacter.cpp:852-855`) |

**Expect lean to produce no visible tilt, and that is not a bug in this ticket.** The lean
*state* is built and replicates; the animation has nowhere to receive it, because the ABP's lean
surface is written only by the procedural component this roadmap defers to R3. Recorded in full
in the Wave 3 commit. The keys should still fire the abilities and flip the tags.

### 6 · Wave 4 — fire and reload *(added 13 Aug, batched here too)*

C++ landed with Wave 4. **These are NOT needed for Checkpoint I** (weapons + switching) — do §1–§5
first, hand that back, and treat this section as the follow-on for Checkpoint K.

| Asset | Detail |
|---|---|
| `/Game/BN/Input/IA_BN_Fire`, `IA_BN_Reload` | `UInputAction`, Boolean. `IA_FPST_Weapon_Fire` / `_Reload` exist in the template — reuse if the value type matches, same rule as sprint |
| `DA_BNInput` rows | ↔ tags `Input.Weapon.Fire`, `Input.Weapon.Reload` |
| `IMC_BNNext` mappings | LMB = fire, R = reload |
| **`DA_BNAbilitySet_Rifle` / `_Pistol`** | now they have content: list `UBNGA_Fire` (tag `Input.Weapon.Fire`) and `UBNGA_Reload` (tag `Input.Weapon.Reload`). Parent `UBNAbilitySet`, referenced from the row's `AbilitySet` column. **Swap is NOT in here** — it is PlayerState-granted, and putting it back would restore the self-revoking bug Wave 2 fixed |
| `FireMontage` / `ReloadMontage` on the rows | the template's per-weapon montages, soft refs |
| Niagara: muzzle · impact · tracer | referenced by the C++ cue classes. **The tracer system MUST expose a user Vector parameter named `BeamEnd`** — the beam's far end; the cue writes it. Cues degrade silently when unset, so a missing system is invisible rather than fatal |

## Read back and report

Fresh-load every asset and print an intent-vs-actual table: each asset's path and parent/type, the
two `DT_BNWeapons` rows with their `AnimLayerClass` actual values, the two `DA_BNInput` rows with
their tags, and the `IMC_BNNext` mappings with the deprecated array confirmed empty. **The
read-back is the proof; that a call returned is not.**

## Done means

All of §1–§3 exist, saved and committed, the read-back pasted into the Log below, and anything
unresolved named. Then Checkpoint I is the founder's: spawn holding the rifle, cycle to the pistol
and back, and confirm the **other window** shows the right weapon and the right pose set.

## Rules that govern this ticket

- **No scripts.** Drive the MCP directly; hand back clicks if it fights.
- **Announce, do not assume.** If this needs a C++ change, say so and stop — do not edit
  `Source/BreachpointNext/`.
- **The MCP client may fail to attach at startup and does not re-bind mid-session.** Fallback is
  JSON-RPC over its HTTP transport (`.mcp.json` → `http://127.0.0.1:8000/mcp`).
- **Factory modals block the game thread.** `DataTableFactory` needs its `RowStruct` set *before*
  creation, and `BlueprintTools.create`'s `asset_type` is the PARENT class — either one wrong
  raises a picker that wedges the editor on an unattended call.

## Log

_(terminal session: append the read-back table, what was duplicated, anything handed back)_
