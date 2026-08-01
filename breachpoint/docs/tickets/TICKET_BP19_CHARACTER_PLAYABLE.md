# TICKET — BP19: The character, playable. C++ owns it; the editor only points at things.

> STATUS: open — cut 1 Aug 2026 by the founder, for the **MCP-connected terminal**. This is the
> "make the character actually work" packet: press W and move, press Space and jump, press LMB
> and the ability fires, in `BR_Arena01`, with our GameMode/Controller/PlayerState.

**Read this whole file before the first tool call.** It is written to be executed without asking
questions — every asset path, class name and value is stated. Where a real decision is left open
it is marked **DECIDE** with the options and a recommendation, and it goes in the Log.

## Founder directive

*"We already have all the code. This is the editor part. We are not making any Blueprint
whatsoever — we're just filling out some stuff from the editor. We might use an AnimGraph, but
in reality what we want is as much as possible from the code. If there's anything we'd need to
create in Blueprint, instead of creating it from Blueprint we move it to C++ and add it back and
forth between the editor and the C++."*

That last sentence is the operating instruction for this entire ticket, and it has a mechanical
consequence: **this packet alternates between two lock modes and cannot be done in one window.**
Write the C++ with the editor CLOSED, build, open the editor, point assets at it, close, build
again if the editor revealed a missing property. Plan for **three windows** (R36).

## The three laws that decide every judgement call here

1. **Law 7 + R18 + R26.** Zero new Blueprint classes. The only Blueprint assets that may exist
   are the five R26 default-value containers **that already exist** — empty graphs, no new
   members, no gameplay numbers. **You may not create a sixth.** If a step seems to need one,
   it needs C++ instead: that is the founder's instruction, not a workaround.
2. **R37.** The MCP may execute an asset step, but **a committed plan specifies it first and a
   committed receipt records every call and its result.** The critic cannot diff a `.uasset`;
   the plan and the receipt are what it reviews. An MCP call with no committed plan behind it
   is hand-placing with a different hand and is a `high` finding.
3. **Law 3.** No gameplay number is set in an asset. Damage, speeds, cooldowns, ranges live in
   `Content/Data/*.csv`. A value you type into a details panel must be a **reference** (a mesh,
   a config asset, a class) or a **presentation** value (a socket name, a camera offset) —
   never a number the sim reads.

**Ordering law:** **A0 gates A1** (an AnimInstance reading a tag from an ungranted ASC reads
false forever, and B3's graph then looks broken for a reason that is not in the graph). Phase A
gates Phase B gates Phase C. Within Phase B, B0 gates B1 gates B2–B4. Do not reorder to "get something visible sooner" — B3's AnimGraph
can only bind to properties A1 creates, and creating them afterwards means rebuilding the graph.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: **mixed.** Phase A is `engine-installed` (editor CLOSED — it builds). Phase B and C
  are `editor-live`. **R36: the editor must be closed for every build in this packet**, and
  `build-input.ps1` / `rename-r26.ps1` refuse to launch while any editor process is live.
- **Satisfy the gate in this order, or it is unsatisfiable** (the BP16 lesson): editor CLOSED →
  run the build proof → *then* start Phase A.
- `Tools/run-ubt.ps1 -Targets BreachpointEditor Breachpoint BreachpointServer` exits 0 with an
  R19 timestamp proof at current HEAD. **This has never been run at HEAD** (`DECISIONS-OWED.md`
  verification addendum) — if it fails, that failure IS this packet's step A0 and everything
  else waits.
- `Content/Maps/BR_Arena01.umap` exists (BP18 landed it) and opens in the editor.
- `Content/Input/DA_InputConfig.uasset`, `IMC_Default.uasset` and the 8 `IA_*` exist (BP18).
- owner_path:
  `Source/Breachpoint/Character/`, `Source/Breachpoint/Animation/`,
  `Source/Breachpoint/Match/BRGameMode.cpp`, `Source/Breachpoint/Match/BRGameMode.h`,
  `Content/Characters/`, `Content/AbilitySets/`, `Content/Animation/`, `Content/Core/`,
  `Content/Maps/BR_Arena01.umap`, `Content/Data/DT_Weapons.csv`,
  `Config/DefaultEngine.ini`, `Config/DefaultGame.ini`,
  `Tools/bp19/` (the plans and receipts R37 requires)

---

# PHASE A — C++, editor CLOSED

## A0. Nothing grants a startup set and nothing applies `GE_InitStats` on spawn

**This is the step that decides whether "playable" means anything**, and it was found by running
PIE, not by reading code — filed by T2 after `bc5cf8f`:

> *"NOTHING GRANTS A STARTUP/BASE SET AND NOTHING APPLIES `GE_InitStats` ON SPAWN.
> `BRAttributeSet`'s constructor sets every attribute to zero and calls it 'uninitialized';
> `CheckForDeath` refuses to kill a pawn with `MaxHealth` 0. So the character moves and binds
> input but has **no health, no shields and no Sprint**."*

`UBRAbilitySystemComponent::ApplyInitStats()` **already exists** — `BRShieldSpec.cpp:395` and
`:439` call it, and the specs pin `GE_InitStats`'s modifier order (capacities before current
values). **The function is written and the game never calls it.**

What to add, on the **server only**, in `ABRPlayerState`'s or `ABRCharacter`'s possession path
(wherever the ASC's owner+avatar are both valid — `InitializeAbilitySystem` is the existing
seam and is already idempotent):

1. Grant the startup ability set — `TSoftObjectPtr<UBRAbilitySet>` **`EditDefaultsOnly`**, set
   from `Config/DefaultGame.ini` or the R26 container, pointing at
   `/Game/Abilities/DA_AbilitySet_Core`. **Soft, never a hard ref** (law 3).
2. Call `ApplyInitStats()`.
3. Guard both against re-entry — `PossessedBy` and `OnRep_PlayerState` both fire, and a respawn
   fires them again. Re-granting an ability set duplicates every grant.

**Do this before A1.** An AnimInstance reading `bIsSprinting` from an ASC that was never granted
Sprint will read `false` forever, and B3's graph will look broken for a reason that is not in
the graph.

Owner: **builder** (**netcode-builder** consults — this is a server-authoritative spawn path).
Contracts: `gas-purity.md`, `netcode.md`.

## A1. `UBRAnimInstance` — the class that does not exist, and is the reason the ABPs can't be built

**The finding this step is built on.** `grep -rn "AnimInstance" Source/Breachpoint/` returns
**only template files** (`breachpointCharacter.cpp`, `Variant_Shooter/*`). There is no
`UBRAnimInstance`. Meanwhile `BREACHPOINT-AUTHORING-MATRIX.md` Tier 1 says:

> **AnimInstance C++ base + thread-safe update + custom `FAnimNode_*`** — ALL animation *state
> and math* lives here.

and Tier 4 permits the AnimGraph asset only for *"node wiring + state-machine transitions
**reading C++ properties only**."* **A graph cannot read properties that do not exist.** So
building the ABP first would force every value into the graph — exactly the thing law 7 forbids
— and that is why this is step A1 and not a Phase-B afterthought.

Create `Source/Breachpoint/Animation/BRAnimInstance.{h,cpp}`:

- `UCLASS()` `UBRAnimInstance : public UAnimInstance`.
- `NativeInitializeAnimation()` — cache the owning `ABRCharacter` and its
  `UBRCharacterMovementComponent`. Both may be null on a spectator or a default preview; every
  read below must survive that.
- **`NativeThreadSafeUpdateAnimation(float DeltaSeconds)`** — this is where the math goes, NOT
  `NativeUpdateAnimation`. It runs on the worker thread, which is the whole point of the Tier-1
  rule. Mark the class `BlueprintThreadSafe` where UE 5.8 requires it for the graph to call it.
- `UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Anim")` for **every** value the graph
  reads. Minimum set, all computed here:

  | Property | Type | Source |
  |---|---|---|
  | `GroundSpeed` | `float` | horizontal `Velocity.Size2D()` |
  | `MovementDirection` | `float` | `CalculateDirection(Velocity, BaseAimRotation)` — the strafe angle for a 2D blendspace |
  | `bIsFalling` | `bool` | CMC `IsFalling()` |
  | `bIsCrouched` | `bool` | CMC `IsCrouching()` |
  | `bIsSprinting` | `bool` | **the ASC tag**, not a bool on the pawn — see below |
  | `AimPitch` | `float` | local: control rotation pitch. **Remote: `RemoteViewPitch`** — see below |
  | `bIsFirstPerson` | `bool` | is this instance driving `Mesh1P` |
  | `bHasWeapon` / `WeaponStanceIndex` | `bool` / `uint8` | from `UBREquipmentComponent`'s current slot |

- **`bIsSprinting` reads the GAS tag, never a pawn bool.** `gas-purity.md`: *state = GE-applied
  tags*. Query the ASC for the sprint tag (`Ability.Sprint`-family — read the real name from
  `Core/BRGameplayTags.h`, do not guess it). A duplicate `bSprinting` bool on the pawn would be
  a second source of truth for a state GAS already owns, and it would desync on a correction.
- **`AimPitch` on a remote pawn must come from `ACharacter::RemoteViewPitch`**, not from
  `GetControlRotation()`. A simulated proxy has no local controller, so control rotation is
  garbage there — every other client would see that pawn aiming flat forward. `BRCharacter.h`
  already names this as owed: *"remote aim pitch from replicated RemoteViewPitch."*
  `RemoteViewPitch` is a compressed `uint8`; decompress it (`* 360.f / 255.f`, unwound to
  ±180) rather than using it raw.
- **No Tick, no gameplay decisions, no numbers.** Blend times and thresholds that a designer
  would tune are `CT_Combat` rows, not literals. If you need one and it has no row, **file a
  `contract_gap` and use a named constant with a comment saying it is owed** — do not invent a
  table column from inside this packet.

Owner: **anim-builder** (fall back to **builder** if that agent is unavailable). Contracts:
`animation.md`, `gas-purity.md`, `data-and-assets.md`.

## A2. Class defaults belong in C++, not in a Blueprint container

`ABRGameMode`'s constructor (`Match/BRGameMode.cpp:29`) sets `GameStateClass` and **nothing
else**. `DefaultPawnClass`, `PlayerControllerClass` and `PlayerStateClass` are unset, so the
engine's defaults are in force and the R26 containers are carrying the wiring instead.

**All four are C++ class references and all four can be set in the constructor.** Do it:

```cpp
GameStateClass        = ABRGameState::StaticClass();
PlayerControllerClass = ABRPlayerController::StaticClass();
PlayerStateClass      = ABRPlayerState::StaticClass();
DefaultPawnClass      = ABRCharacter::StaticClass();   // see the DECIDE below
```

> **Why this is the founder's instruction applied, not scope creep.** These four lines are the
> single largest chunk of "stuff currently living in a Blueprint that C++ can express." Moving
> them means `BP_BRGameMode` no longer needs to exist *for wiring* — it survives only if
> something genuinely un-C++-able remains on it, and if nothing does, R26's exception has one
> fewer instance to defend.

**DECIDE (goes in the Log): what is `DefaultPawnClass`?**
- **(a) `ABRCharacter::StaticClass()`** — pure C++, no asset. But then **nothing assigns the
  skeletal meshes**, because a `USkeletalMesh` is an asset reference and C++ may not hard-ref it
  (law 3: no `ConstructorHelpers`). The pawn spawns invisible.
- **(b) `BP_BRCharacter`** — the R26 container, whose only job is to hold the two mesh
  references and the anim class. Visible pawn; costs one Blueprint asset that already exists.
- **(c) `ABRCharacter` in C++, meshes set from `Config/DefaultGame.ini`** under
  `[/Script/Breachpoint.BRCharacter]` with soft-object paths. Diffable, greppable, survives a
  clone with nobody opening the editor — the R26 corollary explicitly prefers this *"for
  anything a script can set."*

**Recommendation: (c), falling back to (b) if the components resist ini assignment.**
`Mesh1P` and the inherited `GetMesh()` are `VisibleAnywhere` private members, so (c) may require
adding `EditDefaultsOnly` `TSoftObjectPtr<USkeletalMesh>` properties to `ABRCharacter` plus a
`PostInitializeComponents` that resolves and applies them — **which is more C++ and therefore
more in line with the directive, not less.** Try (c) first; if it costs more than one build
cycle, take (b), record why, and move on. **Do not spend the packet on this.**

Owner: **builder**. Contracts: `data-and-assets.md`.

## A3. Build proof — R19, and it gates Phase B

Editor still CLOSED. `Tools/run-ubt.ps1 -Targets BreachpointEditor Breachpoint BreachpointServer`.
All three, from a clean state, exit 0, with the timestamp proof R19 requires (a binary's mtime is
not a proof). **If this fails, stop and fix it — Phase B against a stale editor binary means the
editor cannot see `UBRAnimInstance` and B3 becomes impossible in a way that looks like an MCP
problem.**

Owner: **verifier**.

---

# PHASE B — the editor, via MCP. Every step: plan → call → receipt.

**R37 shape for every step below.** Before any mutating MCP call: write the plan to
`Tools/bp19/<step>_plan.md` (or `.py` where it is executable) and commit it. After the calls:
write `Tools/bp19/<step>_receipt.md` — every tool name, its arguments, its result verbatim,
including refusals — and commit it with the asset. **A receipt written from memory after the
fact is not a receipt.** Append as you go.

## B0. The R26 rename, first, because everything below references these names

`Content/Characters/BP_BRcharacter.uasset` violates R26 condition 5 (`BP_<CppClassWithoutPrefix>`
→ `BP_BRCharacter`, capital C). Four more in `Content/Core/`: `GM_BR`, `GS_BR`, `PC_BR`, `PS_BR`.
`Tools/rename_r26/rename-r26.ps1` does all five and repoints
`Config/DefaultEngine.ini:39` **only after all five succeed**.

**It has never been run.** It also guards on "any editor process is live" — so **run it in the
CLOSED window at the end of Phase A**, not here. This step exists in Phase B only to say: by the
time you touch assets, the names are already correct, and every path below assumes it. Run
`-PlanOnly` first. `git lfs unlock` the five paths afterwards.

> If A2 lands option (c), `GM_BR` may end up holding nothing at all. **An R26 container with
> nothing in it should be deleted, not kept** — record that in the Log as a finding, do not
> delete it inside this packet without saying so.

## B1. The character asset: meshes, anim class, input config

Whichever of A2's options landed, this is where the pawn stops being invisible.

**REVISED 1 Aug 2026 — most of this step is ALREADY DONE, and one line of it was a trap.**
T2 landed the character's body in `bc5cf8f` and recorded the reasoning in
`docs/bus/…T2-to-T1…` after the fact (the commit swept the assets in without it). Read that bus
message before touching `BP_BRCharacter`. Current state:

| Property | State | What to do |
|---|---|---|
| `Mesh3P` (inherited `GetMesh()`) | **DONE** — `SKM_Manny_Simple` + `ABP_Unarmed`, transform already correct in C++ (Z −88 = capsule half-height, yaw −90) | swap `ABP_Unarmed` → `ABP_BR_ThirdPerson` after B3 |
| **`Mesh1P`** | **EMPTY, DELIBERATELY. DO NOT ASSIGN A MESH TO IT.** | leave empty; see below |
| `AnimClass` (3P) | `ABP_Unarmed` (template) | repoint after B3 |
| `DA_InputConfig` | **DONE** — PIE proves 11 rows resolved (4 native verbs + 7 ability rows, 14 bind handles) | nothing |

> ### Mesh1P is empty on purpose. This is the single most likely thing to be "fixed" here.
>
> T2 tried exactly what this ticket originally told you to do — assign `SKM_Manny_Simple` +
> `ABP_FP_Copy` "the way the template does" — ran PIE, and **got a foot filling the screen.**
>
> The template gets away with identity because **its topology is inverted from ours.**
> `breachpointCharacter.cpp:21` attaches the FP mesh to `GetMesh()` and `:28` hangs the camera
> off the mesh's `head` socket. `ARCHITECTURE §3.4` does the opposite — camera on the capsule at
> `BaseEyeHeight`, `Mesh1P` **under the camera** — so identity puts the body's *origin* at eye
> level. Offsetting by −152 (88 capsule + 64 eye) only moved the problem: the head then sat
> ~18 cm above the camera, from inside it.
>
> **The offset was never the bug.** §3.4 specifies `Mesh1P` as *"arms + weapon"* — Tier-4 sourced
> art this project does not own. **A full body on a camera that pitches is the wrong asset at any
> offset.** So `Mesh1P` stays cleared: first person is a clean view, `Mesh3P` still gives
> everyone else and the death cam a correct silhouette.
>
> **AN FPS ARMS MESH IS OWED (Tier 4, sourced art).** Until it exists, leave `Mesh1P` empty and
> do not spend a window on offsets. *This paragraph exists because the first draft of this
> ticket told you to assign it — one hour after T2 proved you shouldn't.*

**Do not** add a component, a variable, or a graph node. If the details panel tempts you to add
one, the answer is C++ and a second Phase-A window.

The template assets are legitimate here: BP01's Log records the founder decision that *"the
template C++ and ALL Content stay on disk… Content **assets** (meshes, anims, weapon meshes,
materials, level geometry) are the one inheritance."*

Owner: **builder** (MCP executes). 

## B2. The three ability sets — `DT_Weapons.csv` points at assets that do not exist

`Content/Data/DT_Weapons.csv` row 1 names:

```
AbilitySet   = /Game/AbilitySets/DA_AbilitySet_AR.DA_AbilitySet_AR
MeshSoftPath = /Game/Weapons/AR/SM_AR.SM_AR
```

**Neither path exists on disk.** `find Content -iname "*AbilitySet*"` returns nothing, and
`Content/Weapons/` holds `Rifle/`, `Pistol/`, `GrenadeLauncher/` — not `AR/`. So the seam
`c4a50f8` just closed ("six abilities stop being unreachable code") resolves to **null at
runtime**: the weapon equips and still cannot fire, one layer further down.

Two halves:

**REVISED 1 Aug 2026: `/Game/Abilities/DA_AbilitySet_Core` ALREADY EXISTS** (T2, `bc5cf8f`).
It grants `BRGA_Sprint` on `InputTag.Sprint` and applies `GE_InitStats`. It names **only** Sprint
because **only `BRGA_Sprint` is compiled into the running editor binary** — Melee, Grenade,
Grapple, WeaponFire and WeaponUtility are committed as source with no build behind them. **That
is an ordering fact, not an oversight: add those rows only after Phase A's rung-1 build.**
Note the folder is `/Game/Abilities/`, **not** `/Game/AbilitySets/` — the CSV's paths are what is
wrong, not the folder.

**(i) Create three per-weapon `UBRAbilitySet` instances** — `DA_AbilitySet_AR`,
`DA_AbilitySet_Magnum`, `DA_AbilitySet_Rocket`, **in `/Game/Abilities/` beside `_Core`**, and
repoint `DT_Weapons.csv` to that folder. `UBRAbilitySet` is a `UPrimaryDataAsset`, so a
weapon names an **instance**, not a class (`c4a50f8`'s commit message is emphatic about this —
a soft *class* pointer resolves to the CDO and grants nothing). Each set grants the abilities
that weapon needs: `BRGA_WeaponFire`, `BRGA_WeaponUtility`, and whichever of
`BRGA_Melee` / `BRGA_Grenade` / `BRGA_Grapple` belong to the base kit rather than the weapon —
**read `TICKET_BP03_WEAPONS_FIRE.md` and `TICKET_BP05_TRIANGLE.md` for which is which; do not
assign by intuition.** Input tags come from the ability's own declared tag, not from a new one.

**(ii) Fix the three `MeshSoftPath` values** to point at meshes that exist. **DECIDE:** repoint
the CSV to the template meshes (`/Game/Weapons/Rifle/Meshes/SKM_Rifle`,
`/Game/Weapons/Pistol/Meshes/SKM_Pistol`, `/Game/Weapons/GrenadeLauncher/Meshes/SKM_GrenadeLauncher`)
**— recommended**, consistent with the founder's template-assets-are-the-inheritance decision —
or create three new meshes, which is art nobody has. Editing the CSV is a **text** change; it
belongs in the CLOSED window with a reimport, not in an MCP call.

Owner: **builder**, **sim-builder** consults on which abilities each set grants.

## B3. The two AnimBlueprints — Tier 4, and the ONLY new assets this ticket may create

`ABP_BR_FirstPerson` and `ABP_BR_ThirdPerson` in `/Game/Animation/`.

**Parent class: `UBRAnimInstance`** (A1). Not `UAnimInstance`, not a template ABP. If the
reparent option is missing, the editor is running a stale binary — go back to A3.

**What may be in the graph:** state machines, blendspaces, transitions, and layer blends, wired
to read **only** the `BlueprintReadOnly` properties A1 exposes. Locomotion from `GroundSpeed` +
`MovementDirection`; jump/fall from `bIsFalling`; crouch from `bIsCrouched`; sprint from
`bIsSprinting`; aim offset from `AimPitch`.

**What may NOT be in the graph — this is the line, and it is a `high` finding if crossed:**
- any arithmetic that decides gameplay (a damage number, a cooldown, a hit window),
- any branch on something other than an A1 property,
- an Event Graph doing per-frame work — **the graph does not compute, it selects**. If you find
  yourself adding a Blueprint variable to hold a computed value, that variable belongs in A1.

Source animations exist and are the inheritance: `/Game/Characters/Mannequins/Anims/Rifle/*`
(Jog, Walk, Jump, AIM, HitReact), `.../Unarmed/BS_Idle_Walk_Run`, `.../Pistol/*`. Existing
template ABPs — `ABP_FP_Copy`, `ABP_FP_Weapon`, `ABP_TP_Rifle`, `ABP_Unarmed` — are worth
**opening to see how the sourced anims are wired**, and must not be reparented or edited: they
belong to the template and other things reference them.

Owner: **anim-builder**. Contract: `animation.md` (law 4: a notify announces a *moment*; the sim
decides the consequence — notify windows raise `Event.*` per R17 and carry no logic).

## B4. Montage notify windows — only if the anims lack them

`BRGA_Melee` and `BRGA_WeaponUtility` wait on `Event.Melee.WindowBegin` / `WindowEnd` and
`Event.Weapon.ReloadCommit` / `SwapCommit` (R17). If the sourced montages carry no notify at
those moments, add the notify **window** — the notify raises the tag and **nothing else**. No
logic in a notify, ever.

**Check before adding.** If the abilities already drive off timers and no montage is wired yet,
this step is **not applicable** — say so in the Log rather than adding notifies nothing consumes.

Owner: **anim-builder**.

---

# PHASE C — the level, and the proof

## C1. `BR_Arena01` is configured to run our game

- **World Settings → GameMode Override** = the game mode class (whichever A2 landed).
- At least one `PlayerStart` inside the blockout, on the floor, not clipping — BP07's manifest
  has eight scored spawn points; **one is enough for this ticket**, and using the manifest's
  SP1 coordinates keeps it consistent with BP07 rather than inventing a location.
- Confirm `Config/DefaultEngine.ini` already points `EditorStartupMap` and `GameDefaultMap` at
  `/Game/Maps/BR_Arena01` — it does (lines 23 and 30). **Do not change them.**

`BR_Arena01.umap` is a binary this ticket **owns for the duration** — `git lfs lock` it, and
coordinate with BP07 rather than co-writing.

## C2. PIE, and say exactly what it proves

**ALREADY PROVEN, 1 Aug (T2, PIE single-player) — do not re-derive these, verify they still
hold after Phase A changes the spawn path:**

```
BRPlayerController 'PC_BR_C_0': added mapping context 'IMC_Default' at priority 0.
BRCharacter 'BP_BRcharacter_C_0': PossessedBy (server) -- InitAbilityActorInfo(owner='PS_BR_C_0', avatar='BP_BRcharacter_C_0').
BRCharacter: input bound via UBRInputComponent -- 4 native verbs (Move/Look/Jump/Crouch), 7 ability rows from config 'DA_InputConfig' -> controller 'PC_BR_C_0' (14 bind handles).
```

Our GameMode spawned our pawn, our controller possessed it, the ASC on our PlayerState
initialised, all 11 input rows resolved. **T2's own caveat, and it is the honest one:
*"I DID NOT PRESS A KEY — 'bound' is proven, 'moves' is not."*** Pressing the keys is this step.

**Four defects PIE surfaced that no static check would have.** None is BP19's to fix; all four
will appear in your log and must not be mistaken for this packet's failures:
1. `BRPowerWeaponSpawner`: *"no PickupClass set; node not armed"* — the arena's rocket node is
   inert. Needs a pickup class that does not exist (R4, and `ControlRocket`).
2. CommonUI: *"Using CommonUI without a CommonGameViewportClient derived game viewport client.
   Input routing will not function correctly."* — **BP10's UI will not route input** until
   `DefaultEngine.ini` names one. Expect the HUD checkbox below to fail for this reason.
3. *"No GameplayCueNotifyPaths were specified in DefaultGame.ini"* — every cue lookup scans all
   of `/Game/`. Cheap to fix, and it is a one-line ini entry, but it is the cue work's call.
4. *"Unable to find RecastNavMesh instance"* — the arena has NavMeshBounds but **no built
   navmesh**, so bots cannot path (BP08).

Play in Editor, one player. Walk the list and record each **PASS/FAIL** in the Log, with the
failing log line where it fails:

- [x] The pawn that spawns is `ABRCharacter` — **proven 1 Aug, see above**
- [ ] `MaxHealth`/`MaxShields` are non-zero after spawn (A0). **If this fails nothing below is
      meaningful** — an uninitialised ASC makes every ability and every anim tag read false
- [ ] Mesh visible in third person, no T-pose. **First person shows NO mesh, and that is the
      expected PASS** until an arms mesh is sourced — see B1
- [ ] `Move` — WASD moves in control-rotation space
- [ ] `Look` — mouse aims; no inversion surprise (inversion is an IMC concern, not code)
- [ ] `Jump` — hold-to-hold, release-to-cut
- [ ] `Crouch` — hold semantics; capsule and mesh offset both correct
- [ ] `Sprint` — the **GAS ability** activates, the ASC tag is applied, and the anim reacts to
      the tag (this is the one that proves A1's `bIsSprinting` is wired to GAS, not to a bool)
- [ ] `Fire` — `BRGA_WeaponFire` activates. If it does not, `LogBRAbility` says why; **an
      ability that fails to activate because its ability set is empty is a B2 defect, not a
      C2 one** — record which
- [ ] Reload / Swap / Melee / Grenade / Grapple — each activates or reports why not
- [ ] Locomotion blends: idle → walk → jog, strafe, jump/fall/land
- [ ] The HUD root (`WBP_RootLayout`) appears, if BP10's controller path is wired

**Then state the rung honestly (law 6).** PIE is **rung 3 at best, and single-process**. This
ticket proves *"the character works in PIE."* It proves **nothing** about multiplayer: not the
listen host, not a remote client, not prediction. Rung 4a/4b (R30) is BP00's, and remains
BLOCKED while Gauntlet does not compile. **Do not write "works" without the rung.**

## C3. Critic pass (REFUTER)

Each answer needs input → wrong outcome, not vibes:
- Does any ABP node compute a value that belongs in `UBRAnimInstance`? Name the node.
- Does any asset touched here hold a gameplay **number** that also exists in a CSV? (Silent
  drift — the R26 condition-4 class of defect.)
- Was a sixth Blueprint class created anywhere, under any justification?
- Does `AimPitch` use `RemoteViewPitch` on simulated proxies, or would every remote pawn aim
  flat forward? **This one cannot be seen in single-player PIE** — say so rather than passing it.
- Does any MCP-landed asset lack a committed plan, or a receipt naming the calls (R37)?
- Does `bIsSprinting` read the ASC tag, or did a convenience bool creep onto the pawn?

Owner: **critic**.

## Done when

- [ ] A pawn that has just spawned has non-zero `MaxHealth`/`MaxShields` and holds the
      `DA_AbilitySet_Core` grants — i.e. `ApplyInitStats()` and the startup grant run on the
      server spawn path, guarded against the double-fire of `PossessedBy` + `OnRep_PlayerState`
- [ ] `Mesh1P` is still **empty**, and the Log says an FPS arms mesh is owed (Tier 4)
- [ ] `Source/Breachpoint/Animation/BRAnimInstance.{h,cpp}` exists, computes every listed
      property in `NativeThreadSafeUpdateAnimation`, reads sprint from the ASC tag, and uses
      `RemoteViewPitch` for non-locally-controlled pawns
- [ ] `ABRGameMode`'s constructor sets all four class defaults in C++; the A2 DECIDE is recorded
- [ ] Rung 1 green, all three targets, R19 timestamp proof, at a HEAD that includes A1 and A2
- [ ] All five R26 renames done; `Config/DefaultEngine.ini` repointed; the audit's condition 5
      is clean
- [ ] `/Game/Abilities/DA_AbilitySet_{AR,Magnum,Rocket}` exist beside `_Core`, `_Core` has had
      its five post-build ability rows added, and `DT_Weapons.csv`'s
      `AbilitySet` and `MeshSoftPath` columns resolve to assets that exist — **verified by
      loading them, not by reading the path**
- [ ] `ABP_BR_FirstPerson` and `ABP_BR_ThirdPerson` exist, are parented to `UBRAnimInstance`,
      and contain zero gameplay decisions
- [ ] `BR_Arena01` has the GameMode override and a `PlayerStart`; PIE spawns `ABRCharacter`
- [ ] Every C2 checklist line has PASS or FAIL **with evidence**; failures are filed, not hidden
- [ ] Every MCP-landed asset has a committed plan and a receipt in `Tools/bp19/` (R37)
- [ ] Critic findings addressed or explicitly waived in the Log
- [ ] Findings + decisions written to this ticket's Log

## Notes

- **Crew:** **anim-builder** (A1, B3, B4) · **builder** (A2, B1, B2, C1) · **sim-builder**
  (consults on ability-set contents) · **verifier** (A3, C2) · **critic** (C3).
- **Binary files this ticket OWNS** (lock before editing): `Content/Characters/BP_BRCharacter`,
  `Content/Core/BP_BR*` (the four), `Content/Abilities/*` (`_Core` exists; three are new),
  `Content/Animation/ABP_BR_*` (new), `Content/Maps/BR_Arena01.umap`.
- **Out of scope — a well-meaning session must NOT do these here:** weapon *behaviour* changes
  (BP03 owns the fire path); new gameplay tags (R23 — `Core/` is closed, file a `contract_gap`);
  HUD work beyond confirming the root layout appears (BP10); the blockout's geometry (BP07);
  any rung-4 claim (BP00, and Gauntlet does not compile); tuning any number in any CSV other
  than the two dangling-path columns B2 names.
- **The mode-window cost is real.** Three windows minimum. If the editor is open and you need a
  build, **close it** — R36, and the LNK1104 that proved it.

## Log

(append findings here, dated, newest last — this is what the next session reads)

**1 Aug 2026 — cut, then revised within the hour, and the revision is the more useful entry.**

Cut from disk state: no `UBRAnimInstance` anywhere in `Source/Breachpoint/`, `DT_Weapons.csv`
pointing at `/Game/AbilitySets/DA_AbilitySet_AR` and `/Game/Weapons/AR/SM_AR` (neither exists),
and `ABRGameMode` setting only `GameStateClass`.

**Then T2's bus message landed and three of the steps were wrong.** Not wrong in the abstract —
wrong because T2 had already done the work an hour earlier and `bc5cf8f` swept the assets in
under a commit message about grenade tuning, so the reasoning was in the bus and not in the
tree. Corrected:

1. **B1 told you to assign a mesh to `Mesh1P`** — the exact thing T2 had already tried, proven
   wrong in PIE (a foot filling the screen), and explicitly asked not to be repeated. Our camera
   topology is inverted from the template's, so identity puts the body's origin at eye level and
   no offset fixes it: §3.4 wants *arms*, which is Tier-4 art nobody has sourced.
2. **B2 named `/Game/AbilitySets/`** — the folder is `/Game/Abilities/`, and
   `DA_AbilitySet_Core` already exists there granting Sprint only, because only `BRGA_Sprint` is
   in the running editor binary.
3. **The actual blocker was missing entirely** — nothing grants a startup set or calls
   `ApplyInitStats()` on spawn, so the pawn has no health, no shields and no Sprint. That is now
   **A0**, and it is the step that decides whether "playable" means anything.

*The reusable part:* this is the same failure the project has hit five times now — **the
document a reader hits first wins, and it is usually the stale one.** What is new is the speed:
this ticket was stale on arrival, by sixty minutes, against work that was committed but whose
*reasoning* lived only in a bus message. A ticket written from `git log` inherits every commit
message's omissions. **Read `docs/bus/` before cutting a packet, not only the tree.**
