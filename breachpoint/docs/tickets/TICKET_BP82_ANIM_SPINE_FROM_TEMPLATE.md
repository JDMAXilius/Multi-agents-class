# TICKET — BP82: Read the FPS template's character + AnimInstance, and land the C++ spine

> STATUS: in-progress — mac terminal 8 Aug 2026 (772dade). Editor live, MCP reachable on :8000.

> **AMENDMENT — founder call, 8 Aug 2026: the spine lands in a NEW `Source/Breachpoint/FPS/`,
> not `Source/Breachpoint/Character/`.** Recorded here because a decision that lives only in
> chat is lost. Three consequences, each checkable:
> 1. `owner_path` below changes; `Character/` is no longer written by this ticket at all.
> 2. **The BP82↔BP96 collision is gone.** `GAMEPLAY-REWORK §3.5` budgets `Character/` at 2 units
>    and BP96 owns both; this ticket no longer contends for that folder or that budget.
> 3. R39 (step 4) therefore declares a **new numbered §3.x `FPS` section**, not a `Character/`
>    bump from 2 to 3. `animation.md`'s binding line ("the anim-facing members of
>    `Source/Breachpoint/Character/`") must be re-pointed at `FPS/` in the same commit as the
>    first header — that edit is inside this ticket's owner_path.
>
> No Build.cs change is owed: UE globs every folder under `Source/Breachpoint/`, and
> `PublicIncludePaths` lists only the legacy `Variant_*` shortcuts, which `FPS/` does not use.

> **Founder directive (unchanged):** *"break down the entire character and entire anim instance … transfer
> Founder directive: *"break down the entire character and entire anim instance … transfer
> everything to C++ as much as possible. I understand there's gonna be a couple stuff we might
> not be able to do."* This ticket honours both halves of that sentence — it maximises C++ and
> it names, in advance, exactly what cannot move.

`Content/FPSTemplate/` is a **Lyra animation strip**. The naming is Epic's, not the seller's:
`ALI_ItemAnimLayers`, `ABP_ItemAnimLayersBase`, `ABP_Mannequin_Base/Retarget/CopyPose/TopDown`,
and the `{Unarmed,Pistol,Rifle,Shotgun,Knife}AnimLayers` set with `_Feminine` and `_UE4`
variants. **The architecture is Epic's and it is good.** What is unknown is the quality of the
Blueprint glue the seller wrapped around it, and that is what this ticket reads before deciding
anything.

**The scope is far smaller than the folder.** 2,101 assets, 63 Blueprints — but only about
**five carry logic**. The rest is per-weapon animation *content*, which is Tier 4, legitimately
an asset, and **stays exactly where it is**.

**Ordering law:** step 1 gates every other step. Nothing is ported, designed, or deleted before
the inventory exists — the whole point is to stop estimating and read the thing.

## Kickoff (machine-checkable)

- requires: **editor-live** with the Unreal MCP reachable for step 1 (`bp_extract.py` prints
  BLOCKED and exits 3 if it is not). R21: one editor, one driver. R29/R36: the editor session
  must not overlap a build.
- `python3 mcp-bp/bp_extract.py --list` prints 4 sets, 14 assets — runs anywhere, no editor
- `git lfs pull` has run
- owner_path: `mcp-bp/`, `Source/Breachpoint/FPS/`, `docs/contracts/animation.md`,
  `docs/ANIM-PORT-LEDGER.md`, `docs/tickets/TICKET_BP82_ANIM_SPINE_FROM_TEMPLATE.md`
  — amended 8 Aug 2026. `ANIM-PORT-LEDGER.md` was missing from the original list although step 2
  mandates writing it; added rather than worked around. **`BREACHPOINT-GAMEPLAY-REWORK.md` is
  deliberately NOT here** — step 4's R39 declaration edits it, and law 5 says a blocked write is
  a `contract_gap`, not a widened claim. That gap is filed when step 4 is reached, not now.
- **`animation.md` Amendment A is the design law for this ticket** — read it first; it already
  settles the reference verdicts and the C++/graph boundary.

## Steps (in order)

1. **Extract. Nothing else happens first.**
   ```
   python3 mcp-bp/bp_extract.py --list                 # confirm the 14 targets
   python3 mcp-bp/bp_extract.py --set anim_spine       # the valuable one, do it alone first
   python3 mcp-bp/bp_extract.py                        # then all four sets
   ```
   **Commit `mcp-bp/bp_inventory.json` before writing a line of C++.** It is this ticket's first
   real deliverable: R18 bans Blueprint classes because *"binary assets are invisible to the
   critic — no diff, no merge, no grep."* This file ends that for fourteen of them, permanently,
   whatever happens to the port.
   - **`ABP_Mannequin_Base`'s property list is the prize.** An AnimBlueprint's CDO carries the
     AnimInstance's variables, and that IS the anim state model. Expect it to be most of
     `UBRAnimInstance`'s field list, already named and defaulted by Epic.
   - `ALI_ItemAnimLayers` is an **interface**. Its function list is the layer contract, and it
     becomes a C++ `UINTERFACE` — see step 3.
   - If a layer ABP (`ABP_RifleAnimLayers`) comes back with **zero variables**, that PROVES it
     is pose content, not logic, and it is struck from the port with evidence rather than
     assumption.

2. **Read the inventory and write the port ledger** — files-only, `docs/ANIM-PORT-LEDGER.md`,
   modelled on `BUTTON-MODULE-LEDGER.md`. Three verdicts per asset: **PORT** (logic → C++),
   **KEEP** (content stays an asset), **DROP** (demo, targets, the UE4 skeleton variants if the
   project ships one skeleton). Every verdict cites a property count or a name from the JSON.
   **A verdict with no evidence from the inventory is not a verdict.**

3. **Write the C++.** Per Amendment A §A.2, this is the split, and it is not negotiable per file:

   | Concern | Where | Note |
   |---|---|---|
   | Tag → bool state | **C++** — `FGameplayTagBlueprintPropertyMap` | confirm the include at first compile; unverifiable from a container |
   | Locomotion maths, aim offset, lean, turn-in-place | **C++** `NativeThreadSafeUpdateAnimation` | law 1 — the graph reads fields, never computes |
   | Sway · bob · recoil · spring damping | **C++ custom `FAnimNode_*`** | the graph places ONE node; **this is where we go further than Lyra** |
   | The layer contract | **C++ `UINTERFACE`** replacing `ALI_ItemAnimLayers` | layer *assets* implement it and stay assets |
   | Montage play + notify forwarding | **C++** | law 4, and R17's `Event.*` tags |
   | **State machines, blend spaces, the layer stack** | **ASSET — unavoidable** | R18 names AnimBlueprint graphs as the one thing UE 5.8 has no C++ path for |

   Target shape: **`FPS/BRAnimInstance.h/.cpp`, one pair**, plus the anim nodes. The FP and
   TP instances share it and differ only in what they expose. (Was `Character/` before the
   8 Aug amendment above.)

4. **Declare the unit — R39, and it is not optional.** *"Real `BR*` C++ under `Source/` is either
   a NUMBERED UNIT in §3 or a NAMED EXCLUSION in §4 — silence is not a third option."*
   `BREACHPOINT-GAMEPLAY-REWORK.md` §3.5 budgets `Character/` at **2 units** and this makes it 3.
   The declaration lands **in the same commit as the file**, carrying its form (`X.h/.cpp`), or
   `architect.py` reports finished work as a STUB.

5. **File the tag gaps — R23, before writing code that needs them.** `Core/` is CLOSED for
   `State.*`. Crouch, ADS, grappling and directional hit reaction are all states the spine wants
   and **none exists**. Each is a `contract_gap` filed against `BP93_GAS_SPINE`, not a header
   edit made here.

6. **Compile.** Rung 1 PARTIAL by environment on a launcher install — say PARTIAL, never green.

7. **Prove it on three views.** Law 7: owning client, server, simulated proxy can differ, and
   every anim claim names which it was verified on. **Rung 2 at best from PIE**; the floor for a
   warp or layer-link claim is a networked check (R30: 4b listen is required where the code path
   differs host vs remote).

## Done when

- [ ] `mcp-bp/bp_inventory.json` committed, 14/14 found, **before any C++**
- [ ] `docs/ANIM-PORT-LEDGER.md` gives every asset a PORT/KEEP/DROP verdict **citing the JSON**
- [ ] `FPS/BRAnimInstance.h/.cpp` exists; the header's includes are engine-only
- [ ] The layer contract is a C++ `UINTERFACE`; no C++ hard-refs an AnimInstance class per weapon
- [ ] R39: the unit is declared in §3 with its form, same commit as the file
- [ ] R23: every missing `State.*` tag is a filed `contract_gap`, none added locally
- [ ] Compile reported as **rung 1 PARTIAL by environment**
- [ ] Every anim claim names the view it was verified on (law 7)
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: **anim-builder** owns this (`animation.md` names them owner). `Character/` is shared with
  builder — this packet names anim-builder as the writer and builder as consult, per the
  contract's own owner-path note. Do not name both as writers.
- Binary files this ticket OWNS: **none.** It reads assets and writes C++ and markdown. If a step
  wants to modify an ABP, that is a different packet with a lock.
- **What is out of scope, and why each:**
  - **Porting the 58 non-logic Blueprints.** Per-weapon layers, targets and demo content are
    Tier 4 and stay. Step 2 proves it per asset rather than asserting it.
  - **The animation data itself** — sequences, montages, blend spaces, the skeleton and its
    retargeting. That is the expensive part of the purchase and **100% of it is kept with zero
    porting.** It also closes `animation.md` law 3's long-standing blocker: timings finally have
    a set to be authored against.
  - **Timing and feel numbers.** Law 3 and law 6 — authored to the pack, judged in a build. This
    ticket lands structure, not tuning.
  - **A Blueprint→C++ transpiler.** It does not exist, and a faithful port of unreviewed logic
    preserves whatever was wrong with it. The inventory exists so the port is a re-implementation
    against BREACHPOINT's laws, not a translation.
- **The honest risk, stated once:** a half-ported template is worse than either version — two
  systems with an undocumented seam. If step 2's ledger shows the logic is deeper than the
  five files this ticket scopes, **stop and re-cut** rather than porting halfway.

## Log

(append findings here, dated, newest last)
