# `ABP_Mannequin_Base` — offline extraction, findings

Target: `/Script/Engine.AnimBlueprint'/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base.ABP_Mannequin_Base'`
Read from `Content/FPSTemplate/Demo/.../ABP_Mannequin_Base.uasset` — 2,996,361 bytes, read-only,
no editor, no MCP. Reproduce with the commands in `README.md`.

## Accuracy, measured before anything was concluded

Cross-checked against the live-editor extraction already committed in `mcp-bp/bp_inventory.json`:

| Asset | Editor reported | Recovered offline | Reading |
|---|---:|---:|---|
| `ABP_Mannequin_Base` | 96 | **88** | the 8 misses are all stock `UAnimInstance` — `onMontage*` ×6, `rootMotionMode`, `bUseMainInstanceMontageEvaluationData` |
| `ABP_Mannequin_Retarget` | 11 | 1 | 10 of 11 inherited |
| `ABP_Mannequin_CopyPose` | 11 | 1 | 10 of 11 inherited |

A name table lists what a package **declares**, never what it inherits. So 88/88 of what this
asset actually introduces is recovered — and the `_Retarget` / `_CopyPose` numbers independently
corroborate the ledger's *"all 11 are stock ⇒ it is a graph and nothing else"* verdict, this time
from the files rather than from the editor.

**One heuristic was built, measured, and deleted.** *"A declared property is lowerCamel"* scored
8 correct out of 116: Blueprint properties are stored UpperCamel (`AimPitch`) and it is the MCP
inventory that lowercases the first letter (`aimPitch`), so the rule collected engine node
internals and skeleton bone names instead. Properties are now reported only where the inventory
confirms them; a run without `--inventory` prints `props=?`. The rejected rule and its score live
in the source so the next person does not reinvent it.

## The 14 content dependencies

The bindings a port has to carry, none of which appear in the property list:

| Reference | Kind |
|---|---|
| `Rig/CR_Mannequin_FootPlant` | **Control Rig** — `AnimGraphNode_ControlRig` is in the graph |
| `LinkedLayers/ALI_ItemAnimLayers` | anim layer interface |
| `Interfaces/BPI_FPST_AnimInterface` | Blueprint interface |
| `Meshes/SK_Mannequin` | skeletal mesh |
| `Locomotion/Rifle/BS_MM_Rifle_Jog_Leans` | blend space |
| `Actions/MM_Rifle_Reload_Additive_WithBasePose` | montage |
| `AnimNotifies/TransitionToLocomotion` | notify |
| `AnimEnum_CardinalDirection`, `AnimEnum_RootYawOffsetMode` | enums — both re-expressed in C++ |
| `S_Procedural_{Aim,Lean}SpineInfoItem_{UE4,UE5}` ×4 | the procedural structs the ledger's correction was about |

Engine modules the graph pulls from: `AnimGraph`, `AnimGraphRuntime`, `BlueprintGraph`,
`ControlRig`, `ControlRigDeveloper`, `PropertyAccessNode`, `SlateCore`, `UnrealEd`, `Engine`,
`CoreUObject`.

## Graph vocabulary — 24 AnimGraph node classes, 50 K2 node classes

`StateMachine`, `StateResult`, `TransitionResult`, `LayeredBoneBlend`, `Inertialization`,
`LegIK`, `ControlRig`, `LinkedAnimLayer`, `LinkedInputPose`, `RotateRootBone`,
`SaveCachedPose`/`UseCachedPose`, `BlendSpacePlayer`, `TwoWayBlend`, `ApplyAdditive`,
`BlendListByBool`, `CopyBone`, `Slot`, `ComponentToLocalSpace`/`LocalToComponentSpace`,
`IdentityPose`, `Root`, plus `AnimGraphNodePropertyBinding`.

`K2Node_PropertyAccess` and `AnimGraphNodePropertyBinding` both present ⇒ the template already
used thread-safe property bindings — the mechanism `UBRAnimInstance`'s two-pass game/worker split
exists to serve.

## Coverage

Walking `Content/FPSTemplate` rather than naming targets:

| | |
|---|---:|
| graph-bearing assets on disk | **99** |
| present in `bp_inventory.json` | 27 |
| **never extracted** | **72** |

The ledger's headline was corrected once already, 14 → 43. Measured from the files it is **99**.
Among the unexamined: `ABP_Mannequin_Base_UE4` (48 K2 / 22 AnimGraph node classes),
`ABP_ItemAnimLayersBase_UE4` (24/28), `BP_FPST_Character_UE4` (50 K2, 54 content refs), and the
notify graphs `AN_FPST_ProceduralRecoil` (8), `AN_MontagePoseOffset` (8), `AN_FootPlant_Left` (11)
— notifies being exactly the seam `UBRAnimInstance::ForwardNotifyAsGameplayEvent` handles.

A node-class count means an asset **carries** a graph, not that the graph is worth porting. This
settles which assets have never been looked at; `docs/ANIM-PORT-LEDGER.md` still decides each
verdict. Full table in `reports/sweep.md`.

## The limit, stated plainly

**No topology.** Pin links, execution order, state transitions and blend weights are not read and
not guessed — they live in the export table's tagged property streams, and export walks against
these packages were rejected by their own validator rather than published at low confidence.

This says *what is in* a graph, never *how it is wired*. Any verdict needing topology still needs
the editor, and should say so.

`ABP_Mannequin_TopDown` sits in the same folder and is absent from `bp_inventory.json` — it was
extracted here for the first time.
