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

- [x] `mcp-bp/bp_inventory.json` committed, 14/14 found, **before any C++**
- [x] `docs/ANIM-PORT-LEDGER.md` gives every asset a PORT/KEEP/DROP verdict **citing the JSON**
- [x] `FPS/BRAnimInstance.h/.cpp` exists; the header's includes are engine-only — with one
      deviation stated rather than hidden: it also includes `FPS/BRAnimTypes.h`, its own folder's
      types header, which itself includes only `CoreMinimal.h`. The rule's purpose (no gameplay
      or sim headers reachable from the anim header) holds; the literal wording does not.
- [x] The layer contract is a C++ `UINTERFACE`; no C++ hard-refs an AnimInstance class per weapon
      — verified by grep: every `ABP_` in `FPS/` is inside a comment, zero in code.
- [ ] R39: the unit is declared in §3 with its form, same commit as the file
      — **BLOCKED, `contract_gap` BP82-1.** `BREACHPOINT-GAMEPLAY-REWORK.md` is not in this
      ticket's owner_path and law 5 forbids widening the claim to reach it.
- [x] R23: every missing `State.*` tag is a filed `contract_gap`, none added locally
- [x] Compile reported as **rung 1 PARTIAL by environment**
- [ ] Every anim claim names the view it was verified on (law 7)
      — **NOT DONE, and no anim claim is made.** No ABP is parented to `UBRAnimInstance` yet, so
      not one pose has been evaluated on any view. The rung is "compiles".
- [x] Findings + decisions written to this ticket's Log

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

### 8 Aug 2026 — mac terminal. Steps 1–3, 5, 6 done; step 4 blocked; step 7 not claimable.

**Step 1 — the inventory, and the reader that lied.** 14/14 Blueprints extracted to
`mcp-bp/bp_inventory.json` (553,831 bytes, 13/14 fully valued).

The first extraction **looked complete and carried nothing.** `list_properties` returns a JSON
**object** keyed by property name, not the array `bp_extract.py` assumed. `json.loads` produced a
dict, `len()` on it gave the *correct* count, and that dict was then passed to `get_properties`
as its `properties` argument — where the server rejected it and returned nothing. Every record
came back with an accurate `property_count` and an **empty** `properties` map. Caught by the file
size: 2,779 bytes for 14 assets averaging 100+ properties.

This is the exact failure mode `mcp-ui/gen_ui/mcp.py` names in its own docstring for the WRITE
path — *"a wrong name fails silently, which is why every write goes through `write_verified` and
is read back and compared"* — and the READ path had no equivalent. Fixed in the reader, not
worked around: `schema` (name → type + description) is now persisted separately so a values
failure can never cost the whole read, and an empty `properties` against a non-empty key list
appends a `HOLLOW:` note carrying the server's reply.

**Step 2 — `docs/ANIM-PORT-LEDGER.md`, every verdict citing the JSON.** The headline finding
contradicts the ticket's own estimate and it is the good direction: the ticket guessed "about
five carry logic"; the inventory says **one**.

- `ABP_Mannequin_Base` (96 props) — **PORT.** The state model, as predicted.
- The three layer ABPs — **KEEP**, with *stronger* evidence than the ticket anticipated. It
  predicted "zero variables proves it is content"; what came back is **schema diff 0** against
  `ABP_ItemAnimLayersBase`, three times over. They add nothing; all 34–42 differences are
  animation asset slots. They are three rows of data that UE requires be shaped as classes.
- `ABP_Mannequin_Retarget` / `_CopyPose` (11 each) — **KEEP.** All 11 are stock `UAnimInstance`
  members. Zero custom variables ⇒ graph only.
- `BP_FPST_Character` (137, of which 42 Blueprint-added) — **DROP in full.** Walk speeds are law-3
  numbers; `availableWeapons`/`currentWeaponIndex` is an inventory on the pawn (BP97, and
  §3.5's "the pawn is a body, not a brain"); the axis values are BP92's; the `onTake*Damage`
  delegates say it routes through the engine damage API, which law 2 bans.
- The 5 weapon BPs — **DROP**, and BP97 gets the evidence: schema diff **0** across all four
  subclasses, differing in 6–20 defaults. They are rows, not classes.

**Step 3 — the C++, in `Source/Breachpoint/FPS/` per the 8 Aug amendment.** `BRAnimTypes.h`,
`BRAnimLayerInterface.h`, `BRAnimInstance.h/.cpp`. Law 1 is the shape of the whole class: a
game-thread pass fills `FBRAnimSnapshot` from UObjects, a worker pass computes every graph-read
field from the snapshot and nothing else.

**Amendment A's open question is closed:** `FGameplayTagBlueprintPropertyMap` is in
`GameplayEffectTypes.h:1480`. **Finding filed against Amendment A's wording** (contract updated,
original left intact as the dated record): the mechanism is used, the container declined. Its
`PropertyMappings` array is `protected` + `EditAnywhere`, so the tag→bool table would be authored
**on the ABP asset** — no diff, no grep, precisely what R18 exists to prevent. BP82 binds the same
engine event one layer down (`RegisterGameplayTagEvent`, `AbilitySystemComponent.h:720`) from a
C++ table. Amendment A's stated intent is met in full and the result is reviewable besides.

Two netcode-shaped details that PIE would never have caught, handled in code:
- **The ASC bind retries.** `BRCharacter` forwards `GetAbilitySystemComponent()` to the
  PlayerState, which arrives by *replication* on a client — `OnRep_PlayerState` can land many
  frames after the mesh initialises. Binding once in `NativeInitializeAnimation` works on a
  listen server and leaves every remote client tag-blind.
- **Bindings seed from the current tag count.** A callback fires on *change*; binding to an ASC
  that is already sprinting would leave the bool false until the player stops.

**Step 4 — R39 declaration: BLOCKED. `contract_gap` filed, not routed around.**
The unit must be declared in `BREACHPOINT-GAMEPLAY-REWORK.md` §3, and that file is **not** in
this ticket's owner_path. Law 5 is explicit — *"Blocked? File a `contract_gap` and STOP — never
edit shared code to unblock"* — and widening my own claim to reach it would be the same thing
wearing a different hat.

> **`contract_gap` BP82-1 → `BREACHPOINT-GAMEPLAY-REWORK.md` §3 (owner: game-lead).**
> `Source/Breachpoint/FPS/` is a NEW discipline folder with three units:
> `BRAnimInstance` (`.h/.cpp`), `BRAnimLayerInterface` (`.h`, UINTERFACE), `BRAnimTypes` (`.h`).
> Needs a numbered §3.x "FPS — 3". **`Character/` stays at 2 and is untouched** — that is what
> the founder's FPS-folder call bought. Until this lands `architect.py` will report the folder as
> a STUB, and that report is CORRECT.

**Step 5 — R23 tag gaps: filed against BP93, none added locally.** `Core/` is CLOSED; the spine
binds only tags that exist (`State.Movement.Sprinting`, `.Grappling`, `State.Weapon.Reloading`,
`.Swapping`, `State.Combat.Meleeing`, `.ThrowingGrenade`, `State.Dead`).

> **`contract_gap` BP82-2 → `BP93_GAS_SPINE`.** Two `State.*` tags the anim spine needs and
> `BRGameplayTags.h` does not declare: **`State.Weapon.ADS`** and **`State.Weapon.Firing`**.
> The template had both (`gameplayTag_IsADS`, `gameplayTag_IsFiring`). `bIsADS` and `bIsFiring`
> exist on `UBRAnimInstance` and are **deliberately bound to nothing and left false** rather than
> set from a second source of truth. When BP93 declares them the binding table gains two lines
> and nothing else changes — which is the test of whether the seam was drawn correctly.
>
> **Crouch is NOT in this gap and that is deliberate:** `ACharacter::bIsCrouched` is already
> engine-replicated, so a `State.Movement.Crouching` tag would be a second authority for
> something UE already owns. Directional hit reaction is also not filed — no packet needs it yet
> and R23 is for what the code needs *now*.

**Step 5b — a second gap, found while writing the sway springs.**

> **`contract_gap` BP82-3 → `Breachpoint.uproject` + `Source/` (owner: game-lead).** A custom
> `FAnimNode_*` needs a `UAnimGraphNode_*` in an **editor module** to be placeable in an
> AnimGraph, and the project declares exactly one module (`Breachpoint`, Runtime). So Amendment
> A §A.2's "sway · bob · recoil in a custom `FAnimNode_*`" is **not reachable today**. The
> *computation* landed where the law requires (worker thread, C++, springs in
> `BRAnimInstance.cpp`); it publishes `SwayRotation`/`SwayLocation` and the graph applies them
> with one stock Transform Bone node. The law — "the graph reads fields, it never computes" —
> holds. The node is an upgrade with a named blocker, not a missing piece pretending to be done.

**Step 6 — Rung 1: PARTIAL by environment.** Not green, and it structurally cannot be here.

| Target | Result |
|---|---|
| `BreachpointEditor` | **PASS** — compiled and relinked `libUnrealEditor-Breachpoint.dylib` |
| `Breachpoint` | **PASS** — compiled, touched `CodeResources` |
| `BreachpointServer` | **FAIL** — *"Server targets are not currently supported from this engine distribution."* |

The server failure is the **Epic Launcher install**, not this code — it ships no server binaries,
exactly as `run-ubt.sh`'s own header and the 4 Aug HANDOFF both predicted. A source build is what
changes it. Two real defects were caught by the compiler on the way and both were fixed as
design corrections rather than suppressions:
1. UHT runs with `-WarningsAsErrors`. A pure-virtual `BlueprintCallable` interface function is a
   UHT warning — and it was also *wrong*, because the implementor of a layer contract is an
   AnimBlueprint. Now `BlueprintNativeEvent` with C++ defaults.
2. `Content/Data/*.csv` inside a block comment is a literal `/*`, which is `-Werror,-Wcomment`.

**Step 7 — NOT CLAIMABLE, and no claim is made.** Law 7 wants owning client, server and
simulated proxy named. **This code has run on none of them.** There is no ABP parented to
`UBRAnimInstance` yet, so nothing has evaluated a single pose — the honest rung is **"compiles",
one rung below "works"**, and every rung above it is owed by whichever packet authors that ABP.
Specifically unverified: the worker-thread update actually running off the game thread, the
turn-in-place sign, the sway spring under a frame spike, and the ASC rebind on respawn.

**Two findings filed against things I do not own, fixed by nobody today:**
- `run-ubt.sh` warned *"an Unreal editor is running"* on every run **after** the editor was
  closed and confirmed gone. A false positive on a warning about build/editor overlap (R21/R29)
  is corrosive — it trains a reader to ignore the one warning that protects the build lock.
  `Tools/` is not in this ticket's owner_path.
- `mcp.py` still lives in `mcp-ui/gen_ui/` while serving three lanes (UI, materials, and now
  Blueprint extraction), reached by a `sys.path` hop. `bp_extract.py` already carries this as a
  filed-not-fixed comment; a third consumer makes it worth a packet.
