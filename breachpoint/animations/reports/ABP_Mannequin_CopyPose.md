# ABP_Mannequin_CopyPose.uasset - offline declaration inventory

Read from the checked-out `.uasset`. No editor involved.

- file size: 35,017 bytes
- name table: 186 entries at offset 622

## Accuracy cross-check vs `bp_inventory.json`

| | |
|---|---:|
| properties the editor reported | 11 |
| recovered offline | **1** |
| not in the name table | 10 |

Not found offline - inherited, not declared by this asset:

- `bPropagateNotifiesToLinkedInstances`
- `bReceiveNotifiesFromLinkedInstances`
- `bUseMainInstanceMontageEvaluationData`
- `onAllMontageInstancesEnded`
- `onMontageBlendedIn`
- `onMontageBlendingOut`
- `onMontageEnded`
- `onMontageSectionChanged`
- `onMontageStarted`
- `rootMotionMode`

## Referenced content packages (2)

- `/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_CopyPose`
- `/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Meshes/SK_Mannequin`

## Engine modules this graph pulls from (6)

- `/Script/AnimGraph`
- `/Script/AnimGraphRuntime`
- `/Script/BlueprintGraph`
- `/Script/CoreUObject`
- `/Script/Engine`
- `/Script/UnrealEd`

## AnimGraph node classes present (3)

- `AnimGraphNodeBinding_Base`
- `AnimGraphNode_CopyPoseFromMesh`
- `AnimGraphNode_Root`

## Anim runtime nodes (3)

- `AnimNode_Base`
- `AnimNode_CopyPoseFromMesh`
- `AnimNode_Root`

## Declared properties (inventory-confirmed, asset spelling) (1)

- `ComponentPlayingAnim`

## EdGraph internals (4)

- `EdGraph`
- `EdGraphNode_Comment`
- `EdGraphPinType`
- `EdGraphSchema_K2`

## Pin type vocabulary (4)

- `Class`
- `Name`
- `object`
- `struct`

## Other names (163)

Listed in full rather than dropped. Bone names, slot names, curve names, state and transition names, engine node properties and designer-authored graph names all land here, and which is which needs a human or the editor. No rule separates them by spelling; see the note in the source.

- `ABP_Mannequin_CopyPose`
- `ABP_Mannequin_CopyPose_C`
- `AnimBlueprint`
- `AnimBlueprintConstantData`
- `AnimBlueprintExtension_Attributes`
- `AnimBlueprintExtension_Base`
- `AnimBlueprintExtension_PropertyAccess`
- `AnimBlueprintFunction`
- `AnimBlueprintGeneratedClass`
- `AnimBlueprintGeneratedConstantData`
- `AnimClassInterface`
- `AnimGraph`
- `AnimInstance`
- `AnimNodeData`
- `AnimNodeExposedValueHandler_PropertyAccess`
- `AnimNodeFunctionRef`
- `AnimNodeStructData`
- `AnimSubsystemInstance`
- `AnimSubsystem_Base`
- `AnimSubsystem_PropertyAccess`
- `AnimationGraph`
- `AnimationGraphSchema`
- `ArrayProperty`
- `BPVariableDescription`
- `BPVariableMetaDataEntry`
- `BecomeRelevantFunction`
- `Binding`
- `BlueprintGuid`
- `BlueprintInternalUseOnly`
- `BlueprintSystemVersion`
- `BlueprintType`
- `BoolProperty`
- `BoundFunction`
- `ByteProperty`
- `COND_None`
- `Category`
- `CategoryName`
- `CategorySorting`
- `ClassName`
- `CommentColor`
- `Copy`
- `CopyBatchArray`
- `CopyRecords`
- `DataKey`
- `DataValue`
- `Default`
- `DefaultValue`
- `Default__ABP_Mannequin_CopyPose_C`
- `Default__AnimInstance`
- `DestPaths`
- `DisplayName`
- `ECommentBoxMode`
- `ECommentBoxMode::NoGroupMovement`
- `ELifetimeCondition`
- `Engine`
- `Entries`
- `ErrorType`
- `Event Graph`
- `EventGraph`
- `ExposedValueCopyRecord`
- `Extensions`
- `Flags`
- `FriendlyName`
- `Function`
- `FunctionGraphs`
- `FunctionName`
- `GeneratedClass`
- `GraphGuid`
- `Group`
- `Guid`
- `HideCategories`
- `InitialUpdateFunction`
- `IntProperty`
- `InterfaceProperty`
- `LayerGroup`
- `Library`
- `LinearColor`
- `LinkID`
- `Links`
- `MapProperty`
- `MetaDataArray`
- `MoveMode`
- `MultiLine`
- `NameProperty`
- `NameToIndexMap`
- `NewVariables`
- `Node`
- `NodeComment`
- `NodeGuid`
- `NodeHeight`
- `NodeIndex`
- `NodePosX`
- `NodePosY`
- `NodeTypeMap`
- `NodeWidth`
- `Nodes`
- `None`
- `NumProperties`
- `ObjectProperty`
- `OptionalPinFromProperty`
- `Package`
- `PackageLocalizationNamespace`
- `ParentClass`
- `PathSegments`
- `Pose`
- `PoseLink`
- `PropertyAccessCopyBatch`
- `PropertyAccessLibrary`
- `PropertyAccessPath`
- `PropertyAccessSegment`
- `PropertyFlags`
- `PropertyFriendlyName`
- `PropertyGuids`
- `PropertyName`
- `PropertyTooltip`
- `RepNotifyFunc`
- `ReplicationCondition`
- `Result`
- `RootBoneToCopy`
- `SKEL_ABP_Mannequin_CopyPose_C`
- `SK_Mannequin`
- `SceneThumbnailInfo`
- `Schema`
- `ScriptStruct`
- `ShowPinForProperties`
- `SkeletalMeshComponent`
- `Skeleton`
- `SourceLinkID`
- `SourceMeshComponent`
- `SrcPaths`
- `StrProperty`
- `StructProperty`
- `TargetSkeleton`
- `TextProperty`
- `ThumbnailInfo`
- `UInt32Property`
- `UInt64Property`
- `UbergraphPages`
- `UpdateFunction`
- `VarGuid`
- `VarName`
- `VarType`
- `__NameProperty`
- `__StructProperty`
- `bAllowDeletion`
- `bCanToggleVisibility`
- `bCommentBubblePinned`
- `bCommentBubbleVisible`
- `bCommentBubbleVisible_InDetailsPanel`
- `bCopyCurves`
- `bCopyCustomAttributes`
- `bHasOverridePin`
- `bIsMarkedForAdvancedDisplay`
- `bIsOverrideEnabled`
- `bIsOverridePinVisible`
- `bIsSetValuePinVisible`
- `bIsSparseClassDataSerializable`
- `bLegacyNeedToPurgeSkelRefs`
- `bPropertyIsCustomized`
- `bShowPin`
- `bUseAttachedParent`
- `bUseMeshPose`
- `bUsingCopyPoseFromMesh`

## Limits

Node topology (pin links, execution order, state transitions) is NOT read. This is a declaration inventory, not a graph dump.
