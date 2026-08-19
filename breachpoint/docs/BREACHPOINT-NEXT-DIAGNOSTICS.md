# THE DIAGNOSTIC SHEET — what BN tells you, and how to read it

**Cut:** 14 August 2026 by the cloud lead, on the founder's standing order: *"I want to see logs
on the stuff that we are experiencing that is happening wrong."*

Everything here is `LogBN`. Filter the output window to that category and the whole game reports
itself. **Nothing here fires per frame** — announcements are on edges and failures only.

## 1. What announces itself automatically, with no command typed

| Prefix | When | What it settles |
|---|---|---|
| `BNInput:` | every input-driven ability press | `ADS/melee/grenade/... -> ACTIVATED` or `REFUSED`. Covers every ability at once |
| `BNGA_ADS:` | ADS press, activate, end | The refusal REASON (sprinting / no row / `bCanADS` false), whether the tag and speed GE applied, and whether it ended by release or was CANCELLED (descoped by damage or sprint) |
| `BNGA_Melee:` | every swing | No weapon row / montage unset / montage refused to play (no ABP slot) / swing OK with montage name and length |
| `BNDamage:` | every point of damage | instigator → victim, amount, shield and health before/after |
| `BNCues:` | startup | which cue class won each tag |
| `BNEquipmentComponent:` | startup, per weapon | a startup row that does not exist in the table is named and skipped |
| `BNPlayerController:` | input setup | an empty `MappingContexts`, a context that failed to load, or an input tag with no `InputAction` in the config |
| `BNHit:` | per pawn, at BeginPlay | whether that body **can be shot at all** — mesh blocking the weapon/melee channels. `CANNOT BE SHOT` means a Blueprint out-serialised the C++ collision and every weapon misses every player |
| `BNLoadout:` | per weapon, at spawn | what each weapon can do: fire damage × pellets and the headshot multiplier, or `NONE (row has no AbilitySet)`, plus melee damage, reach and magazine. Falloff and body-section lines appear only for rows that opted in |
| `BNGameState:` | every match state change, both roles | `match state -> WaitingToStart / InProgress / WaitingPostMatch` — the ENGINE's own MatchState names since 19 Aug (the machine is `AGameMode`'s; the engine prints its own `LogGameMode: Match State Changed` beside ours) — and `winner resolved -> …` when a late-arriving winner reference lands |
| `BNGameMode:` | every elimination, and the buzzer | the kill line with the running score (`eliminated X. (Y: 3 kills)`), and `match over. Winner: …` — `none (tie)` is a legal outcome |

## 2. There are no console commands

`BNAimDebug`, `BNAimLog`, `BNAimNative`, `BNAimAxis`, `BNLeanAxis`, `BNLayerCheck`, `BNMelee`,
`BNDamageSelf`, `BNKillSelf` and `BNRefill` were **removed from the source on 14 Aug 2026**, along
with the `BNLink:`, `BNLayers:` and `BNPose:` log prefixes, when `BNCharacter`, `BNAnimInstance` and
`BNPlayerController` were rebuilt to production shape. Typing them now does nothing.

They were scaffolding for one investigation — the frozen aim chain — and what survives of it is
now the permanent code path: the linked layers are written by resolved address rather than a
per-frame name lookup, and no gate exists for the component path to win. The
`Input.Debug.DamageSelf` gameplay tag still exists in `BNGameplayTags`, unbound. Restoring any of
these means restoring the code, not this row.

**C++ is NOT yet the sole writer of the aim properties, and this sheet used to claim it was.**
`ABP_Mannequin_Base` still carries its twenty Lyra update functions and still runs them from
`BlueprintThreadSafeUpdateAnimation`, which the engine invokes *after* the native thread-safe pass.
Where both write, the graph wins. `PitchRotator` and `bFPSMode` are the exceptions — the asset
declares neither, so those two are sole-writer today.

## 3. Reading an aim or pose failure now

The chain is short enough to bisect by observation:

1. **Nothing poses at all, on any machine** — no anim layer is linked. The character links the
   current weapon's `AnimLayerClass`; an empty `DT_BNWeapons` cell or a row that failed to load
   lands here, and `BNEquipmentComponent:` will have named the bad row at startup.
2. **The body poses but the aim does not follow the camera** — a **shadowed property**. Read the
   `.uasset` name table before theorising: on 14 Aug 2026 `ABP_Mannequin_Base` was found to still
   declare its own `AimPitch`, `AimYaw` and `isCrouching` as Blueprint variables
   (`__CustomProperty_AimPitch_<guid>` and friends in the name table). After the reparent onto
   `UBNAnimInstance` there are then *two* properties of each name, and since the Blueprint class is
   the most-derived, every binding that resolves by name finds the Blueprint one. C++ writes a
   property nothing reads. The fix is to delete the Blueprint variable, not to touch C++.

   The earlier entry here blamed a **BN-owned duplicate** of a template layer. That was wrong and
   is retracted: there is no `Content/BN/` folder in this repo and no duplicated layer exists. The
   weapon layers are the FPSTemplate and Lyra originals, and `ABP_RifleAnimLayers` already contains
   the `IdleAimOffset`/`RelaxedAimOffset` blendspace nodes over the shipped `AO_MM_Rifle_*` assets.
   The pipe was always built; it was reading the wrong end.

   To audit any ABP for this class of fault without opening the editor, extract the printable
   strings from the `.uasset` and list every `__CustomProperty_<Name>_<guid>` — that is the exact
   set of Blueprint-declared variables. Anything in that set which also exists as a C++ `UPROPERTY`
   on the parent is a shadow, and a shadow is silent.

   > **MEASURED 15 Aug 2026 (terminal, live editor)** — the string-table audit OVERCOUNTS. The
   > `.uasset` name table for `ABP_Mannequin_Base` (post-906a4d9) still carries
   > `__CustomProperty_{AimPitch, AimYaw, isCrouching, isMoving2D}`, but the LOADED class has
   > exactly ONE property per name (108 walked via TFieldIterator, zero duplicates, zero
   > `__CustomProperty_*`), and `list_variables` shows none of the four as Blueprint variables.
   > Name-table strings survive from stale compiled-out bytecode; only the live class layout —
   > or the editor's own variable list — proves a shadow. There is nothing to delete on the
   > current asset; if aim still misbehaves, the shadow theory is exhausted for the main ABP
   > and §3.3 (axis) / §2's graph-wins note are the remaining suspects. Layer ABPs
   > (`ABP_ItemAnimLayersBase` both copies, `ABP_RifleAnimLayers`) audit clean even on strings.
3. **The aim follows but bends the wrong way** — the bone-space axis. `AimPitchAxis` and `LeanAxis`
   are `EditDefaultsOnly` on the ABP's own defaults; they are a measured property of the Manny rig,
   not a derivable one, so they are set there and not guessed in code.
4. **ADS changes FOV but the gun does not rise** — the PoseOffsets component or its `ChangePose`
   entry point was not found. The two halves of ADS are independent by design: FOV is the camera,
   the pose is the component.
5. **Press V and nothing happens** — `BNGA_Melee:` names the dead link, or `BNInput: Input.Melee ->
   REFUSED` names the refusal, or neither appears, which means the key never reached the ability
   system component at all and the problem is in the input assets.

## 3b. Reading a match failure

1. **No `BNGameState:` line at all, ever** — the world is not running `ABNGameMode` at all.
   Since 19 Aug the GameState class is FORCED in `ABNGameMode::InitGame` — after any Blueprint's
   serialisation, before the GameState spawns — so a `BP_BNGameMode` dropdown can no longer take
   it away and the old `TASK-R4-GAMESTATE-CLASS` ticket is closed as superseded. Check the map's
   World Settings GameMode override.
2. **Stuck in `WaitingToStart`** — the start gate was never satisfied: `MinPlayers` in
   `DefaultGame.ini`. The gate is `ReadyToStartMatch`, polled by the engine's own machine, and it
   counts HUMANS only — four bots alone never start a match, by design.
3. **A kill that does not score** — read the kill line's wording. `eliminated themselves` and
   `died` deliberately award no kill; only `X eliminated Y` does. A kill landing after the buzzer
   also does not score, by design — the scoreboard is final the moment the winner is announced.
4. **Anything happening during the post-match freeze** — a press should print
   `BNInput: … REFUSED`. Death and hit-react are the two deliberate exceptions: a grenade in
   flight still kills and still ragdolls after the buzzer, it just does not score.
5. **A player blinking out shortly after a restart** — a respawn timer from the previous round
   fired into the new one. That is generation-guarded (`fe958a2`); if it reappears, the guard
   regressed.

## 4. The rule these came from

ASSET-RULES §5c: anything that hands a value across a boundary it cannot verify must announce
the outcome, name the counterpart, and fail loudly. Every entry in §1 exists because a real bug
once hid behind that boundary in silence.
