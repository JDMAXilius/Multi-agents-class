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
> **AMENDED 8 Aug after critic finding M8 — it is FOUR units, not three.** The first filing
> listed three and `BRAnimLayerInstance` would have been left declared nowhere and owned by
> nobody, which is the precise silence R39 forbids.
>
> **RE-AMENDED 8 Aug (second expansion, founder call).** It is now **10 units / 15 files**.
> `Source/Breachpoint/FPS/` is a NEW discipline folder needing a numbered §3.x **"FPS — 10"**:
> | Unit | Form | Job |
> |---|---|---|
> | `BRAnimInstance` | `.h/.cpp` | The shared spine. Game-thread snapshot → worker compute. |
> | `BRAnimInstance1P` | `.h/.cpp` | Arms. Zeroes aim offset and lean; own sway scale. |
> | `BRAnimInstance3P` | `.h/.cpp` | Body. Compressed aim pitch; standing threshold. |
> | `BRAnimLayerInstance` | `.h` | C++ base per-weapon layers parent to. |
> | `BRAnimLayerInterface` | `.h` (UINTERFACE) | Code half of the layer contract. |
> | `BRAnimNotify_GameplayEvent` | `.h/.cpp` | Point notify carrying a typed `FGameplayTag`. |
> | `BRAnimNotifyState_GameplayEventWindow` | `.h/.cpp` | Window notify; end guaranteed on interrupt. |
> | `BRAnimTypes` | `.h` (types) | Cardinal enum, snapshot, tag state, spring. |
> | `BRAnimCurveNames` | `.h` (constants) | Curve-name contract. **No reader yet** — see BP82-4. |
> | `BRFPSWeaponAnimTypes` | `.h` (types) | Fire mode, crosshair, sockets. **No reader yet.** |
>
> **Two units have no caller today and must be declared as such, not quietly.** `BRAnimCurveNames`
> exists so the turn-in-place curve (BP82-4) cannot be spelled two ways when someone authors the
> ABP; `BRFPSWeaponAnimTypes` holds the anim-facing half of the weapon vocabulary so BP97
> inherits measured names instead of re-deriving them. Both are scaffolding, landed on an
> explicit founder call, and `architect.py` reporting them as STUBs is **correct behaviour, not a
> defect** — that is the tool doing its job until a consumer exists.
>
> **`Character/` stays at 2 and is untouched** — that is what the founder's FPS-folder call
> bought. Until this lands `architect.py` reports the folder as a STUB, and that report is CORRECT.

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

**Rung V1, run independently by `verifier` (read-only crew, not the author).** It reproduced the
same verdict from scratch — **rung 1 PARTIAL by environment**: `BreachpointEditor` PASS (relinked
the dylib), `Breachpoint` PASS, `BreachpointServer` FAIL on *"Server targets are not currently
supported from this engine distribution."* Its mechanical gates also passed: the folder contains
exactly the four expected files; the six `ABP_`/`/Game/` hits are **all in comments, none in
code**; no banned API (`TakeDamage`, `ApplyRadialDamage`, `FDamageEvent`, loose-tag or attribute
setters) appears anywhere in `FPS/`; `bp_inventory.json` parses with 14 `found=true` and 13
non-empty `properties`.

> **FINDING — the ladder above rung 1 has NO macOS path at all, and this is bigger than BP82.**
> `verifier` reported rungs 2 and 4 BLOCKED for want of a macOS runner. Checked further rather
> than taking it at face value: `pwsh` is **not installed**, and installing it would not help —
> `Tools/run-specs.ps1` is Windows-**hardcoded**, not merely PowerShell-flavoured. It looks for
> `UnrealEditor-Cmd.exe` and `Binaries\Win64\UnrealEditor-Breachpoint.dll`. `run-gauntlet.ps1`
> has no macOS counterpart either; only `run-ubt` was ever ported (`run-ubt.sh`).
>
> So on this machine the honesty ladder **terminates at rung 1**, and every "PIE ≠ multiplayer"
> claim above it is unreachable rather than merely un-run. That is a much sharper statement than
> the 4 Aug HANDOFF's "rung 1 is PARTIAL by environment", which named only the server target.
> `Tools/` is not in this ticket's owner_path — **filed, not fixed.**

### 8 Aug 2026 — second tranche. The rest of the state model, and the seam I had skipped.

The first pass ported roughly **30 of `ABP_Mannequin_Base`'s 96 fields** and stopped. That was
short of the directive, and one row of step 3's own table — *"montage playback + notify
forwarding → C++"* — had been skipped outright rather than deferred with a reason. Both closed.

**New unit: `FPS/BRAnimLayerInstance`.** Before it, `IBRAnimLayer` had **zero implementors** and
was dead code. It splits `ABP_ItemAnimLayersBase`'s 102 properties along the only line that
matters: ~90 pose slots stay on the asset (they are animation assets — porting them means hard
asset refs, law 3), while `disableHandIK`, `enableLeftHandPoseOverride`, `aimOffsetBlendWeight`
and the per-bone aim weights come into C++. **Correction to the first ledger entry:** "KEEP" was
right about ~90 of those properties and wrong about the rest; the ledger now says so.

**Ported this pass:** jump-vs-fall split, `TimeToJumpApex` derived from CMC gravity rather than
an assumed −980, pivot detection (acceleration *opposing* velocity — waiting for velocity to flip
is too late, the plant is already missed), the one-frame transition **edges**, linked-layer
identity asked through the interface so `FPS/` still names no asset, and the additive alphas.

**The montage seam, with the gate that would have been wrong.** R17's four tags in a C++ table.
Montages play on **every** machine, simulated proxies included — forwarding unconditionally
raises a reload-commit event on each observer's copy of a *remote* player, on a machine with no
authority over that pawn. Gated to authority-or-locally-controlled. Invisible in PIE with one
player; wrong the moment there are two.

**Sway pitch defect — found by re-reading, fixed, and stated because it would have shipped
looking fine.** The pitch spring was fed `AimPitch`, an **angle**, while the yaw spring was fed
`YawDeltaSpeed`, a **rate** — and the local was named `PitchRate` while holding an angle. The
symptom is not a wobble but a **permanent tilt**: hold the camera 30° up and the spring settles
to a constant offset and stays there, because a constant angle is a constant target. Sway is a
response to *motion*; a still camera must produce zero sway on both axes. Now fed a true rate.
*(Flagged to the founder as suspected before the critic reported; fixed on the author's own
re-reading rather than waiting. The critic's verdict on it is still owed and will be recorded
here whether it agrees or not.)*

**Second defect from the same re-read: nothing clamped `DeltaSeconds`.** A level-load hitch hands
the anim update a step of hundreds of milliseconds, and semi-implicit Euler at that step
overshoots enormously — the weapon leaves the screen for a frame and snaps back. Added
`MaxIntegrationStep` (0.05 s, config). Elapsed-time accumulators deliberately keep the **real**
delta: they measure wall clock, not motion.

**Not ported, deliberately, with the reason each time:** `groundDistance` and
`left/rightJointTargetLocation` need a trace and an IK chain, and foot placement has no packet
yet; `enableControlRig` / `useFootPlacement` are switches for systems that do not exist;
`bFPSMode` / `bFPSWalkMode` are the pack's own 1P/3P branch, which our two-instance design
replaces; the `basePose*` / `currPose*` / `procApply*` pairs are procedural-pose scratch for a
graph that has not been authored. **None of these is a "couldn't"; each is a "no packet needs it
yet", which is the honest distinction.**

### 8 Aug 2026 — rung V2, `critic` in REFUTER mode. **1 high, 9 medium, 8 low.**

It reached all seven attack items. **H1 blocks a landing and it was right to.** Fixes below are
landed and compiled; the mediums it raised against the ledger are corrected in the ledger itself
rather than argued with.

> **H1 (high) — the montage seam sent every event 2–4× and `Event.Melee.WindowEnd` was
> UNREACHABLE.** Two independent multipliers, both structural, neither visible in a one-player PIE:
>
> 1. **`AnimNotify_PlayMontageNotifyWindow` broadcasts the IDENTICAL `NotifyName` to both the
>    begin and the end delegate.** Both handlers went through **one** name→tag map, so a window's
>    CLOSE re-emitted the tag its OPEN emitted. `Event.Melee.WindowEnd` was not merely un-sent —
>    it was unreachable by any name. `BRGA_Melee` would open the trace window and never be told
>    to close it. **A trace window that never closes is free hits.**
> 2. **Two meshes, one ASC.** `ABRCharacter` owns `FirstPersonMesh` and the inherited 3P `Mesh`,
>    and law 2 gives each its own `UBRAnimInstance`. Both resolve `TryGetPawnOwner()` to the same
>    pawn, so both sent to the same ASC. The netcode gate I was proud of **cannot see this** — it
>    is per-*machine*, and the duplication is per-*mesh*.
>
> **Fixed:** two maps selected by a `bIsEnd` flag (windowed notifies are now named for the
> window, `MeleeWindow`, and the edge picks the tag), plus `IsGameplayEventSource()` — only the
> instance on the third-person mesh speaks, because that mesh exists and plays on every machine
> including a dedicated server. **Consequence, stated so it is not discovered later: a
> gameplay-bearing notify must be authored on the 3P montage.**
>
> `AddDynamic` → `AddUniqueDynamic` as well (L1): it does not de-duplicate, so any re-init would
> have doubled the seam again.

> **M6 (latent, and it would have detonated exactly when BP93 landed).** The worker pass had
> begun *computing* from ASC-callback bools — `bADSStateChanged` is an edge, `UpperBodyAdditiveWeight`
> a product — while the callback writes them from arbitrary game-thread code. Bools do not tear,
> so today's cost is a stale frame; **the latent cost is not bounded.** If the game thread flips
> `bIsADS` between the compare and the store, **the edge is lost permanently** and a state
> machine waiting on it never transitions.
>
> That directly falsified this ticket's own proof-of-design — *"the table gains two lines and
> nothing else in this class changes."* **Fixed:** the callback now writes a private
> `FBRAnimTagState`; the game pass latches it into `Snapshot.Tags`; the worker publishes the
> public bools. Every graph-read field is worker-written again, and the header comment claiming
> so is true again.

> **M3 — `RootYawOffset` had no consumer and stuck at the clamp.** The sign is right (the critic
> tried and could not break it). What it could break: nothing ever *reduced* the offset while
> standing. Pan 200° on the spot → it pins to −120 and **stays** until the player takes a step.
> In Lyra the turn-in-place animation consumes it through a yaw curve; there is no curve, no ABP
> and no packet that authors one. **Landed a standing recovery** as an explicit stand-in, and
> filed the real thing:
>
> > **`contract_gap` BP82-4 → whichever packet authors `ABP_BRMannequin`.** `RootYawOffset` needs
> > a consumer — a turn-in-place yaw curve read back into the spine. Until then the C++ idle
> > recovery is a floor that stops a missing system reading as a broken one, not turn-in-place.

> **M7 — `bUseMultiThreadedAnimationUpdate = true` in the constructor is a no-op, and law 1 is
> enforced by a checkbox in a `.uasset`.** `UAnimInstance` already sets it true, and the **ABP
> compiler overwrites the CDO** — forcing it false if the graph has one non-thread-safe node or a
> `BlueprintUpdateAnimation` event. Every line documented as worker-thread would then run on the
> game thread, and nothing in the repo would notice. Added an `ensureMsgf` that says so out loud.
> The deeper point stands and is uncomfortable: **the guarantee this class is built on lives in a
> binary file**, which is the exact condition R18 exists to prevent.

> **M8 — R39, and I had under-filed my own gap.** `BRAnimLayerInstance` is a real `BR*` `UCLASS`
> named in **no** declaration: `contract_gap BP82-1` listed three units, the Log said four files,
> and the V1 verifier asserted "exactly the four expected files." There are **five**.
> **`contract_gap BP82-1` is amended below to four units.**

> **M5 / M12 — accepted risk, filed not fixed.** The rate terms feeding lean and both sway
> springs are computed from `Snapshot` fields that, **on a simulated proxy**, refresh at the
> actor's net update rate rather than per frame. At 60 fps observing a 20 Hz remote pawn, the
> whole delta lands on one frame in three: `YawDeltaSpeed` reads 270, 0, 0, 270… and the observed
> player's lean **strobes** full-to-none. My pitch fix inherits this; it did not introduce it.
> Law 7 makes the simulated proxy the mandatory view and it has been checked on **none**. Also:
> `Config/DefaultGame.ini` has no `[/Script/Breachpoint.BRAnimInstance]` section, so "tunable
> without a recompile" is currently true of nothing — `Config/` is not in owner_path.
>
> > **`contract_gap` BP82-5 → `Config/DefaultGame.ini`.** Needs a
> > `[/Script/Breachpoint.BRAnimInstance]` section, or the `Config` specifiers are decoration.

**What the critic tried to break and could not** — recorded because a refuter's failures are
evidence too: the netcode gate on the notify seam is **correct on all six views** it walked; the
turn-in-place **sign** is right; the `TagHandles[i]`↔`Bindings[i]` pairing is **unbreakable**
(respawn, PlayerState replacement, stale weak pointer, double-bind all tried); the ASC retry
design is right and `Cast<const IAbilitySystemInterface>` does what its comment claims; declining
`FGameplayTagBlueprintPropertyMap` is justified **in outcome**; and *"no editor module ⇒ no
custom `FAnimNode_`"* is **true for UE 5.8** — it found no path I missed.

It also did the stability maths I could not: k=90, c=14 diverges above **Δt = 0.106 s** and
sign-flips above **0.0714 s**; the 0.05 s clamp gives ζ ≈ 0.74, about 3 % overshoot. **On the
revision that is on `main` without the clamp, a 250 ms hitch produces −45° of weapon rotation in
one frame.** The fix was right and the number is now on the record.

> **PROCESS FINDING, and it is against me, not the code.** I told the critic the artifact was on
> `main`. It was **uncommitted working-tree state, and it changed twice during the review** — its
> first read got the 324-line committed file and it had to re-derive against the 485-line
> on-disk one. A review of uncommitted state **cannot be replayed from a SHA by anyone**. The V1
> verifier's "exactly four files" gate was already stale against disk when it passed.
> **Rule for the next round: commit first, name the SHA in the packet.**

### 8 Aug 2026 — rung V2 round 2, against SHA `b52e43b`. **1 new high, 5 medium.**

The round-1 process finding is closed: this pass reviewed a committed tree and named the SHA.

> **H2 (high) — MY OWN H1 FIX WAS WORSE THAN THE BUG.** I traded a loud double-fire for a
> **silent, nondeterministic no-fire of the entire law-4 seam**, which is precisely the trade I
> asked the critic to check for. It happened.
>
> `IsGameplayEventSource()` gated on `GetOwningComponent() == Character->GetMesh()`, reasoning
> that the 3P mesh exists everywhere. True, and irrelevant — **nothing authored on a montage
> decides which mesh it plays on.** GAS decides, and it does not choose deliberately:
> `UAbilityTask_PlayMontageAndWait::Activate` → `ActorInfo->GetAnimInstance()` →
> `SkeletalMeshComponent->GetAnimInstance()`, where `SkeletalMeshComponent` came from
> `FindComponentByClass<USkeletalMeshComponent>()` — and `AActor::OwnedComponents` is a
> **`TSet`**. Hash order.
>
> `ABRCharacter` owns two skeletal meshes and pins neither. So GAS may hand the reload montage to
> the **1P** instance; the notify fires there; my gate returns false; `Event.Weapon.ReloadCommit`
> is never sent **on any machine, server included**; `WaitGameplayEvent` never fires; **ammo never
> moves.** And which mesh you get can differ between PIE and a packaged build, or shift when an
> unrelated component is added.
>
> **Fixed by asking GAS instead of guessing at it:** the gate is now
> `GetOwningComponent() == ASC->AbilityActorInfo->SkeletalMeshComponent`. Whichever mesh GAS is
> using is the one that speaks — exactly one instance per machine, correct regardless of hash
> order, and it stays correct if the resolution ever changes, because it is no longer an
> assumption. The determinism of *which* mesh is still worth fixing at the source:
>
> > **`contract_gap` BP82-6 → `Source/Breachpoint/Character/` (owner: builder).** `ABRCharacter`
> > should pin the ability actor info's mesh after `InitAbilityActorInfo` —
> > `ASC->AbilityActorInfo->SkeletalMeshComponent = GetMesh();` — so 1P-vs-3P is a decision
> > somebody made rather than a hash-set's iteration order. `Character/` is not in this ticket's
> > owner_path. The `FPS/` fix above is correct without it; this makes it *predictable*.

> **M13 — the M3 recovery gate was structurally always-true for a simulated proxy.** My stated
> worry (a slow deliberate turn tripping it) was unfounded — 0.01°/frame is 0.6°/s and no input
> reaches there. The real defect was the one I did not guess: `Snapshot.WorldRotation` comes from
> the actor, and for a proxy that is a **step function at the net update rate** (CMC smoothing
> smooths the mesh offset, not the actor rotation). At 60 fps against 20 Hz the per-frame yaw
> delta is **exactly 0.0 on two frames in three**, so "has the camera stopped?" answers yes
> two-thirds of the time *while the player is mid-turn*. Recovery ate ~60°/s of a 90°/s turn:
> **any remote player turning slower than ~60°/s never turned in place on an observer's screen**,
> while the same turn worked perfectly for its owner. **Fixed:** the gate now reads
> `Snapshot.bRotationChanged`, set in the game pass, because only the game pass can tell "new
> data arrived" from "nothing moved".

> **M17 — stale tag state outlived the ASC.** `UnbindAbilitySystem` reset the handles, never the
> state. An observing client watching someone **disconnect mid-reload** kept `bReloading` true
> forever: PlayerState destroyed → rebind returns early at the null check → nothing ever writes
> it false. The abandoned pawn reloads for eternity with its upper-body additive pinned to zero.
> Respawn was always clean; ASC *loss* was not. One line, `TagState = FBRAnimTagState{}`.

> **M14 — my `ensureMsgf` asserted law 1 was met in exactly the configuration where it isn't.**
> The ABP compiler copies its flag to the CDO unconditionally, but the engine *also* gates
> threaded updates on `UEngine::bAllowMultiThreadedAnimationUpdate`. Set that false in
> `DefaultEngine.ini` — an ordinary profiling toggle — and the CDO flag stays **true**, my check
> passed happily, and every worker-pass line ran on the game thread anyway. Now checks both.
> Second half unfixed and filed: `ensure` is compiled out of Shipping, which is the build where a
> game-thread anim update actually costs frames.
>
> > **`contract_gap` BP82-8 → `Core/` (owner: builder).** `FPS/` has no log channel, so the
> > Shipping-safe companion to that ensure cannot be written from here (R24/R38).

> **M15 — `UBRAnimLayerInstance` is unreachable from the three layers the ledger says to KEEP.**
> They parent to `ABP_ItemAnimLayersBase`, which parents to `UAnimInstance`, so they do not
> implement `IBRAnimLayer` — `LinkedLayerRow` would be permanently `NAME_None` and the header's
> "the spine knows which layer is up" true of nothing. Fixing it means **reparenting a sourced
> binary asset**, and this ticket owns no binary files by its own Notes.
>
> > **`contract_gap` BP82-7 → a packet that owns `Content/FPSTemplate/`.** Reparent
> > `ABP_ItemAnimLayersBase` to `UBRAnimLayerInstance` (not the three leaves — that would destroy
> > ~90 pose slots and the graph, i.e. the purchase). Also: `MeleeWindow` **must** be authored as
> > `AnimNotify_PlayMontageNotifyWindow`. Authored as the point notify instead, End never fires
> > and the round-1 high returns — a trace window that opens and is never closed — from one wrong
> > dropdown, with no diff, no grep and no assert. The detector I could add only catches the
> > *opposite* mistake, because an absence has no callback.
>
> The critic also caught that `UBRAnimLayerInstance` was about to re-create the very thing this
> packet exists to prevent: a C++ `bDisableHandIK` mirroring the asset's `disableHandIK` (set
> **true** on `ABP_UnarmedAnimLayers`), giving one meaning two values. **Deleted** — the asset's
> is the one the graph reads, and `bOverridesHandPose` is the spine's separate question.

**What round 2 could not break** — the M6 latch (race genuinely closed; publish-before-edge
ordering verified), the two-map split against real engine notify semantics, dedicated-server
behaviour (both meshes exist and tick), **montage interruption** (`FAnimMontageInstance` emits
`BranchingPointNotifyEnd` on terminate, so a cancelled melee still closes its window — law 5
holds), the L3 fix, and it enumerated every accumulator in the class to confirm **no third case**.
It also re-derived ledger corrections C1/C2/C3 and confirmed I corrected them to something *true*
rather than merely different.

**Two findings filed against things I do not own, fixed by nobody today:**
- `run-ubt.sh` warned *"an Unreal editor is running"* on every run **after** the editor was
  closed and confirmed gone. A false positive on a warning about build/editor overlap (R21/R29)
  is corrosive — it trains a reader to ignore the one warning that protects the build lock.
  `Tools/` is not in this ticket's owner_path.
- `mcp.py` still lives in `mcp-ui/gen_ui/` while serving three lanes (UI, materials, and now
  Blueprint extraction), reached by a `sys.path` hop. `bp_extract.py` already carries this as a
  filed-not-fixed comment; a third consumer makes it worth a packet.
