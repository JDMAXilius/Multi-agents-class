# ABP_Mannequin_Retarget.uasset - offline declaration inventory

Read from the checked-out `.uasset`. No editor involved.

- file size: 65,790 bytes
- name table: 321 entries at offset 662

## Accuracy cross-check vs `bp_inventory.json`

| | |
|---|---|
| properties the editor reported | 11 |
| recovered offline | **1** |
| not in the name table | 10 |

Not found offline (inherited, not declared by this asset):

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

## Referenced content packages (3)

- `/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Retarget`
- `/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Meshes/SK_Mannequin`
- `/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Rig/RTG_Mannequin`

## Engine modules this graph pulls from (7)

- `/Script/AnimGraph`
- `/Script/BlueprintGraph`
- `/Script/CoreUObject`
- `/Script/Engine`
- `/Script/IKRig`
- `/Script/IKRigDeveloper`
- `/Script/UnrealEd`

## AnimGraph node classes present (3)

- `AnimGraphNodeBinding_Base`
- `AnimGraphNode_RetargetPoseFromMesh`
- `AnimGraphNode_Root`

## Anim runtime nodes (4)

- `AnimNode_Base`
- `AnimNode_CustomProperty`
- `AnimNode_RetargetPoseFromMesh`
- `AnimNode_Root`

## K2 (event graph) node classes present (8)

- `K2Node_CallFunction`
- `K2Node_DynamicCast`
- `K2Node_DynamicCast_AsCharacter`
- `K2Node_DynamicCast_bSuccess`
- `K2Node_Event`
- `K2Node_Event_DeltaTimeX`
- `K2Node_VariableGet`
- `K2Node_VariableSet`

## Declared properties (inventory-confirmed, asset spelling) (1)

- `ComponentPlayingAnim`

## EdGraph internals (4)

- `EdGraph`
- `EdGraphNode_Comment`
- `EdGraphPinType`
- `EdGraphSchema_K2`

## Pin type vocabulary (14)

- `Class`
- `Name`
- `Object`
- `ToolTip`
- `bool`
- `delegate`
- `exec`
- `execute`
- `float`
- `object`
- `real`
- `self`
- `struct`
- `then`

## Other names (277)

Listed in full rather than dropped - bone names, slot names, curve names, state and transition names, engine node properties and designer-authored graph names all land here, and which is which needs a human or the editor. No rule separates them by spelling; see the note in the source.

- `ABP_Mannequin_Retarget`
- `ABP_Mannequin_Retarget_C`
- `Actor`
- `AffectIKHorizontal`
- `AffectIKVertical`
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
- `AsCharacter`
- `BPVariableDescription`
- `BPVariableMetaDataEntry`
- `BecomeRelevantFunction`
- `Binding`
- `BlendToSource`
- `BlendToSourceWeights`
- `BlueprintGuid`
- `BlueprintInternalUseOnly`
- `BlueprintSystemVersion`
- `BlueprintType`
- `BlueprintUpdateAnimation`
- `BoolProperty`
- `BoundCurveValues`
- `BoundFunction`
- `ByteProperty`
- `COND_None`
- `CallFunc_GetOwner_ReturnValue`
- `CallFunc_GetOwningActor_ReturnValue`
- `CastFailed`
- `Category`
- `CategoryName`
- `CategorySorting`
- `ChainSettings`
- `Character`
- `ClassName`
- `Comment`
- `CommentColor`
- `CopyBasePoseRoot`
- `CopyBatchArray`
- `CopyRecords`
- `CustomRetargetProfile`
- `DataKey`
- `DataValue`
- `Debug`
- `Default`
- `DefaultGraphNode`
- `DefaultValue`
- `Default__ABP_Mannequin_Retarget_C`
- `Default__AnimInstance`
- `DeltaTimeX`
- `DestPaths`
- `DestPropertyNames`
- `DirectionChain`
- `DirectionSource`
- `DisplayName`
- `EBasicAxis`
- `EBasicAxis::Y`
- `ECommentBoxMode`
- `ECommentBoxMode::NoGroupMovement`
- `ELifetimeCondition`
- `ENodeEnabledState`
- `ENodeEnabledState::Disabled`
- `EPureState`
- `EPureState::Impure`
- `ERetargetSourceMode`
- `ERetargetSourceMode::ParentSkeletalMeshComponent`
- `ESelfContextInfo`
- `ESelfContextInfo::NotSelfContext`
- `EWarpingDirectionSource`
- `EWarpingDirectionSource::Goals`
- `EnabledState`
- `Engine`
- `Entries`
- `EntryPoint`
- `EnumProperty`
- `ErrorMsg`
- `ErrorType`
- `Event Graph`
- `EventGraph`
- `EventReference`
- `ExecuteUbergraph_ABP_Mannequin_Retarget`
- `ExposedValueCopyRecord`
- `Extensions`
- `Flags`
- `FloatProperty`
- `ForwardDirection`
- `FriendlyName`
- `Function`
- `FunctionGraphs`
- `FunctionName`
- `FunctionReference`
- `GeneratedClass`
- `GetOwner`
- `GetOwningActor`
- `GlobalSettings`
- `GraphGuid`
- `Group`
- `Guid`
- `HideCategories`
- `IKRetargeter`
- `IKRetargeterAsset`
- `InitialUpdateFunction`
- `InstancedPropertyBag`
- `IntProperty`
- `InterfaceProperty`
- `LODThreshold`
- `LODThresholdForIK`
- `LayerGroup`
- `Library`
- `LinearColor`
- `LinkID`
- `Links`
- `MapProperty`
- `MemberGuid`
- `MemberName`
- `MemberParent`
- `MemberReference`
- `Mesh`
- `MetaDataArray`
- `ModuleRelativePath`
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
- `OutputDelegate`
- `Output_Get`
- `OverrideSetsToApply`
- `Package`
- `PackageLocalizationNamespace`
- `ParentClass`
- `PathSegments`
- `Pawn`
- `Performance`
- `PointerToUberGraphFrame`
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
- `PureState`
- `RTG_Mannequin`
- `RepNotifyFunc`
- `ReplicationCondition`
- `Result`
- `RetargetFrom`
- `RetargetGlobalSettings`
- `RetargetOpProfile`
- `RetargetOpProfiles`
- `RetargetProfile`
- `ReturnValue`
- `RootSettings`
- `RotationAlpha`
- `RotationOffset`
- `Rotator`
- `RuntimeVariables`
- `SKEL_ABP_Mannequin_Retarget_C`
- `SK_Mannequin`
- `ScaleHorizontal`
- `ScaleVertical`
- `SceneThumbnailInfo`
- `Schema`
- `ScriptStruct`
- `SelfContextInfo`
- `Settings`
- `ShowPinForProperties`
- `SidewaysOffset`
- `SkeletalMeshComponent`
- `Skeleton`
- `Source`
- `SourceInstance`
- `SourceLinkID`
- `SourceMeshComponent`
- `SourcePropertyNames`
- `SourceRetargetPoseName`
- `SourceScaleFactor`
- `SrcPaths`
- `StrProperty`
- `StructProperty`
- `TargetChainSettings`
- `TargetInstance`
- `TargetRetargetPoseName`
- `TargetRootSettings`
- `TargetSkeleton`
- `TargetType`
- `TextProperty`
- `ThumbnailInfo`
- `TranslationAlpha`
- `TranslationOffset`
- `TryGetPawnOwner`
- `UInt32Property`
- `UInt64Property`
- `UberGraphFrame`
- `UberGraphFunction`
- `UbergraphPages`
- `UpdateFunction`
- `VarGuid`
- `VarName`
- `VarType`
- `VariableReference`
- `Vector`
- `WarpForwards`
- `WarpSplay`
- `__NameProperty`
- `__StructProperty`
- `bAllowDeletion`
- `bApplyChainSettings`
- `bApplyGlobalSettings`
- `bApplyRootSettings`
- `bApplySourceRetargetPose`
- `bApplyTargetRetargetPose`
- `bCanToggleVisibility`
- `bCommentBubblePinned`
- `bCommentBubbleVisible`
- `bCommentBubbleVisible_InDetailsPanel`
- `bCopyBasePose`
- `bDefaultsToPureFunc`
- `bEnableFK`
- `bEnableIK`
- `bEnablePost`
- `bEnableRoot`
- `bForceAllIKOff`
- `bHasCompilerMessage`
- `bHasOverridePin`
- `bIsMarkedForAdvancedDisplay`
- `bIsOverrideEnabled`
- `bIsOverridePinVisible`
- `bIsSetValuePinVisible`
- `bIsSparseClassDataSerializable`
- `bLegacyNeedToPurgeSkelRefs`
- `bOverrideFunction`
- `bPropertyIsCustomized`
- `bSelfContext`
- `bShowPin`
- `bSuccess`
- `bSuppressWarnings`
- `bUseAttachedParent`
- `bUseCustomOverrideSets`
- `bUsingCopyPoseFromMesh`
- `bWarping`

## Limits

Node topology (pin links, execution order, state transitions) is NOT read. This is a declaration inventory, not a graph dump.
