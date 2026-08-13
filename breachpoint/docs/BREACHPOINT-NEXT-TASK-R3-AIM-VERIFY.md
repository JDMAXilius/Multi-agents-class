# TICKET — R3 aim: verify the ABP consumes `Pitch`, then the FX assets
> STATUS: in-progress — mac terminal 13 Aug 2026 (ad6d383). Claimed on the founder's 'yes pick it up'; building first (BNAnimInstance changed at ad6d383), then editor+MCP.

**Cut:** 13 August 2026 by the cloud lead · **For:** the terminal session (editor + MCP)
**Binds to:** the NEXT doc family. **Read [`BREACHPOINT-NEXT-ASSET-RULES.md`](BREACHPOINT-NEXT-ASSET-RULES.md) FIRST** —
its §1 (reuse, never author) and §5 (do only what this ticket lists) govern every step here.

**Owner path:** `Content/BN/` + `Config/DefaultGame.ini`. **Never `Source/`.**

---

## Part 1 — the ONE question that decides whether R3 G1 worked

C++ now publishes `Pitch` on `UBNAnimInstance` (landed this session). The root cause it fixes:
**`SetControllerPitch` has never fired in either module**, so the ABP's `Pitch` was never written
and the weapon never followed the look.

**But publishing a property only helps if something reads it.** If the ABP's aim consumers hang
off the un-fired `SetControllerPitch`/`SetAimAndLeanInfo` interface events rather than off the
variables, the pose still will not move.

**Do this and report — it is one look, not a project:**

1. Open `ABP_Mannequin_Base`. Find every consumer of the variable **`Pitch`** (right-click →
   Find References). Report: does the **AnimGraph** read it (an aim-offset / rotation node), or
   is its only reader inside the event graph?
2. Same for **`LeanRotation`** and **`LeanOppRotation`**.
3. Confirm `AimSpineWeights_UE5` and `LeanSpineWeights_UE5` still hold their measured values on
   the CDO (C++ deliberately does NOT write these — they are UserDefinedStruct-typed and cannot
   be declared in C++; the asset owns the distribution, C++ owns the totals).

**Report the answer. Do NOT fix the graph.** If the consumers are event-graph-only, that is a
finding the lead turns into a packet — a graph edit is not in this ticket's scope.

## Part 2 — the FX assets, reuse only

**Author nothing.** Every item below exists in the FPSTemplate; find it and point at it. If
something genuinely does not exist, say so with what you searched — do not create a substitute.

| # | Item | Detail |
|---|---|---|
| 2.1 | **Tracer cue FX** | The R2 ticket wrongly specified a `BeamEnd` vector. The template's tracer takes **`User.ImpactPositions[]` (array) + `User.Trigger`** — confirmed from the template's own FireEffect graph. **Report the system's exact path and its full user-parameter list**; the C++ cue needs a code change to write those params, which is the LEAD's packet, not yours. Leave the cue's Effect ref unset until then |
| 2.2 | **Impact decal — the bullet hole** | Find the template's impact decal material/asset and the surface-typed impact FX `MyCharacter::ImpactEffect` uses. **Report paths only.** The decal cue class does not exist yet — that is C++ the lead owes |
| 2.3 | **Weapon sounds** | Find the template's fire and reload sounds for the rifle and pistol. **Report paths only** — the cue classes have no sound field yet; that is C++ the lead owes |

Part 2 is **discovery, not wiring.** Three C++ changes depend on what you find, and guessing the
parameter names is what broke the tracer the first time.

## Part 3 — one deletion

`/Game/BN/Animation/ABP_BNMannequin` is **dead** — the shipped reparent route orphaned it (open
item E2/D2, founder-agreed). Delete it and confirm nothing references it first. If anything does,
stop and report instead.

---

## Done means

Part 1's three answers, Part 2's paths and parameter lists, Part 3 deleted or reported, and the
read-back pasted into the Log. **That is the whole ticket.**

## Scope, restated because it has been a problem

- Do only what is listed. Extra work found is a **Log entry**, not a licence.
- **No `Source/` edits.** Three items here need C++; they are the lead's, and this ticket exists
  to give the lead the facts it needs.
- No asset creation. No graph edits.
- Do not touch assets this ticket does not name — the accidental `BPC_FPST_Lyra_FireEffectComp`
  dirty-save is the precedent for why.
- The read-back is the deliverable.

## Log

_(terminal: append the three answers, the asset paths and parameter lists, and the deletion result)_

- 13 Aug 2026 (mac terminal) — **Parts 1–2 DONE, Part 3 attempted and handed back.** All read
  through the live editor's MCP after a green rebuild of the ad6d383 C++.

  **Part 1 — the ABP DOES consume the variables; G1's fix lands.**
  1. **`Pitch`: read well beyond the event graph.** `BlueprintThreadSafeUpdateAnimation` calls
     `UpdateRotationData` every frame, whose DSL reads
     `SetPitchRotator(MakeRotator(GetPitch))`; `UpdateAimingData` similarly sets `AimPitch`
     (NormalizeAxis of a property access). The AnimGraph applies pitch through linked layer
     `FSP_FullBody_Aiming_Pitch_FPS_Upper` (+`_Neck`) — a spine_01→spine_05→neck_01/02→head
     Transform(Modify)Bone chain; spine_01's alpha literal (0.15) matches
     `AimSpineWeights_UE5.spine_01`. The EventGraph's `EventSetControllerPitch → SetPitch` is
     just another writer of the same variable, now superseded by C++. **Caveat recorded:** the
     ModifyBone rotation pins carry `AnimGraphNodeBinding_Base` property-access bindings whose
     source path the MCP cannot reflect — the chain is verified by mechanism and weight match,
     not by reading the binding string.
  2. **`LeanRotation` / `LeanOppRotation`: writers only.** EventGraph setters (off the
     never-fired `SetAimAndLeanInfo`) are the ONLY hits across all 126 graphs of
     `ABP_Mannequin_Base`; no reader anywhere, and no lean layer graph exists in
     `ABP_ItemAnimLayersBase`. The lean pose surface genuinely does not exist — consistent
     with Wave 3's "no visible tilt" note. Finding reported, graph NOT touched.
  3. **Spine weights hold on the CDO:** `AimSpineWeights_UE5` = spine_01 .15 / spine_02–05 .10
     / neck_01 .15 / neck_02 .20 / head .10; `LeanSpineWeights_UE5` = spine_01–04 .20 /
     spine_05 .10 / neck_01 .10 / neck_02 0 / head(OppositeAngleWeight) .25.

  **Part 2 — FX discovery (paths only, nothing wired):**
  - **2.1 Tracer:** `/Game/FPSTemplate/Demo/Effects/Particles/Weapons/NS_WeaponFire_Tracer`
    (also the CDO default of `BPC_FPST_Lyra_FireEffectComp.tracerNS`). Parameters the template
    itself writes (from `FireTracerEffect`): `User.ImpactPositions` (vector ARRAY, via
    NiagaraSetVectorArray) + `User.Trigger` (bool). The system's full exposedParameters list is
    NOT reflectable through this MCP (documented miss) — the graph-observed set is the contract
    the template exercises.
  - **2.2 Impact:** surface-driven via `ImpactEffectInfoMap` on the component CDO, fallback
    SurfaceType2. Decal is `NS_ImpactDecals` for ALL surfaces; effects: SurfaceType1 (character)
    `NS_ImpactSparksCharacter2` + `sfx_ImpactCharacter_nl_meta_Preset`, SurfaceType2 (concrete)
    `NS_ImpactConcrete` + `sfx_ImpactPlaster_nl_meta`, SurfaceType3 (glass) `NS_ImpactGlass` +
    `sfx_ImpactGlass_nl_meta_Preset`. Decal params written: `User.Trigger` (bool),
    `User.ImpactSurfaces` (int32[]), `User.ImpactPositions` (position[]), `User.ImpactNormals`
    (vector[]), `User.NumberOfHits` (int32); the impact system takes the same minus
    Trigger/Surfaces. Sound is `PlaySoundAtLocation` from the map row.
  - **2.3 Weapon sounds:** fire = `MSS_Weapons_Rifle2_Fire` (the `Rifle/` folder has only
    DryFire; `Rifle2/` carries the fire MetaSound) and `MSS_Weapons_Pistol_Fire`. **No
    standalone reload sound exists** — reload audio rides the montage as notifies
    (`AM_MM_Rifle_Reload` depends on `Weapons_Rifle_ClipOut_01`/`ClipIn_01`/`Bolt_01`), so
    `BNGA_Reload` playing the same montages gets its audio free; searched `Demo/Audio` for
    "Fire"/"Reload" and the three weapon folders.

  **Part 3 — deletion handed back.** `get_referencers` on `/Game/BN/Animation/ABP_BNMannequin`
  = [] (confirmed dead). `AssetTools.delete` returned true on BOTH the package and object path
  forms, but `exists` and the on-disk file refute it every time — the editor is silently
  refusing (likely a loaded-asset/confirmation path the tool cannot cross). Per the rules this
  is a hand-back, not a workaround: **one founder click** — Content Browser →
  `BN/Animation/ABP_BNMannequin` → Delete. `git rm` while the editor may hold the package was
  deliberately NOT done.
