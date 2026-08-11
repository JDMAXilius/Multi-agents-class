# ABP_Mannequin_Base — offline extraction, findings

Target: `/Script/Engine.AnimBlueprint'/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base.ABP_Mannequin_Base'`
Read from `Content/FPSTemplate/Demo/.../ABP_Mannequin_Base.uasset` (2,996,361 bytes). No editor, no MCP, read-only.

Nothing was written to `Source/`, `Content/`, or `mcp-bp/`. All output is in the scratchpad.

## What was built

`abp_offline_extract.py` — reads a Blueprint/AnimBlueprint `.uasset` straight off disk.

`mcp-bp/read_graphs.py` needs the editor live on `:8000` and a path per asset. This needs
neither, which is the point: the ledger's own correction records that a curated 14-asset target
list silently became a claim about what mattered. A reader that walks the tree cannot make that
mistake, because the files answer instead of the list.

## Accuracy — measured, not asserted

Cross-checked against the live-editor extraction already committed in `mcp-bp/bp_inventory.json`:

| Asset | Inventory props | Recovered offline | Reading |
|---|---:|---:|---|
| `ABP_Mannequin_Base` | 96 | **88** | the 8 misses are all stock `UAnimInstance` — `onMontage*` ×6, `rootMotionMode`, `bUseMainInstanceMontageEvaluationData` |
| `ABP_Mannequin_Retarget` | 11 | 1 | 10 of 11 inherited |
| `ABP_Mannequin_CopyPose` | 11 | 1 | 10 of 11 inherited |

The name table lists what a package **declares**, never what it inherits. So 88/88 of what this
asset actually introduces is recovered, and the `_Retarget` / `_CopyPose` numbers independently
corroborate the ledger's "all 11 are stock ⇒ it is a graph and nothing else" verdict — this time
from the files rather than from the editor.

**One heuristic was built, measured, and deleted.** "A declared property is lowerCamel" scored
8 correct out of 116: Blueprint properties are stored UpperCamel (`AimPitch`) and it is the MCP
inventory that lowercases the first letter (`aimPitch`), so the rule collected engine node
internals and bone names instead. Properties are now reported only where the inventory confirms
them; everything else is listed as `other_names` for a human. The rejected rule and its score are
recorded in the source so it doesn't get reinvented.

## What the asset actually depends on (14 content packages)

The bindings a port has to carry, none of which are in the property list:

| Reference | Kind |
|---|---|
| `Rig/CR_Mannequin_FootPlant` | **Control Rig** — `AnimGraphNode_ControlRig` is in the graph |
| `LinkedLayers/ALI_ItemAnimLayers` | anim layer interface |
| `Interfaces/BPI_FPST_AnimInterface` | Blueprint interface |
| `Meshes/SK_Mannequin` | skeletal mesh |
| `Locomotion/Rifle/BS_MM_Rifle_Jog_Leans` | blend space |
| `Actions/MM_Rifle_Reload_Additive_WithBasePose` | montage |
| `AnimNotifies/TransitionToLocomotion` | notify |
| `AnimEnum_CardinalDirection`, `AnimEnum_RootYawOffsetMode` | enums (both re-expressed in C++) |
| `S_Procedural_{Aim,Lean}SpineInfoItem_{UE4,UE5}` ×4 | the procedural structs the ledger's correction was about |

Engine modules pulled in: `AnimGraph`, `AnimGraphRuntime`, `BlueprintGraph`, `ControlRig`,
`ControlRigDeveloper`, `PropertyAccessNode`, `SlateCore`, `UnrealEd`, `Engine`, `CoreUObject`.

## Graph vocabulary (24 AnimGraph node classes, 50 K2 node classes)

`StateMachine`, `StateResult`, `TransitionResult`, `LayeredBoneBlend`, `Inertialization`,
`LegIK`, `ControlRig`, `LinkedAnimLayer`, `LinkedInputPose`, `RotateRootBone`,
`SaveCachedPose`/`UseCachedPose`, `BlendSpacePlayer`, `TwoWayBlend`, `ApplyAdditive`,
`BlendListByBool`, `CopyBone`, `Slot`, `ComponentToLocalSpace`/`LocalToComponentSpace`,
`IdentityPose`, `Root`, plus `AnimGraphNodePropertyBinding`.

`K2Node_PropertyAccess` and `AnimGraphNodePropertyBinding` both present ⇒ the template already
used thread-safe property bindings, which is the mechanism `UBRAnimInstance`'s two-pass split
exists to serve.

## Coverage finding

Walking `Content/FPSTemplate` instead of naming targets:

| | |
|---|---:|
| graph-bearing assets on disk | **99** |
| present in `bp_inventory.json` | 27 |
| **never extracted** | **72** |

The ledger's headline was corrected once already, 14 → 43. Measured from the files it is **99**.
Among the unexamined: `ABP_Mannequin_Base_UE4` (48 K2 / 22 anim node classes),
`ABP_ItemAnimLayersBase_UE4` (24/28), `BP_FPST_Character_UE4` (50 K2, 54 content refs), and the
notify graphs `AN_FPST_ProceduralRecoil` (8), `AN_MontagePoseOffset` (8), `AN_FootPlant_Left` (11)
— notifies being exactly the seam `UBRAnimInstance::ForwardNotifyAsGameplayEvent` handles.

Node-class count means an asset **carries** a graph, not that it is worth porting. This settles
which assets have never been looked at; the ledger still decides each verdict.

## The limit, stated plainly

**No topology.** Pin links, execution order, state transitions and blend weights are not read and
are not guessed. They live in the export table's tagged property streams, and this package comes
from a source engine build whose `FPackageFileSummary` does not match the documented layout
(`SavedHash` replacing the package GUID, legacy file version −9). Every attempt to walk exports
was rejected by the validator rather than reported at low confidence.

So this tool answers *what is in a graph*, never *how it is wired*. Any verdict needing topology
still needs the editor and should say so.

## Files

| Path (scratchpad) | What |
|---|---|
| `abp_offline_extract.py` | the reader; `--sweep DIR`, `--inventory PATH`, `--out DIR` |
| `out/ABP_Mannequin_Base.{json,md}` | full inventory for the target asset |
| `out/ABP_Mannequin_{Retarget,CopyPose,TopDown}.{json,md}` | the sibling ABPs |
| `out/sweep.{json,md}` | the 99/27/72 coverage table |

`ABP_Mannequin_TopDown` is in the folder and absent from the inventory — it was extracted here
for the first time.
