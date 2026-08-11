#pragma once

#include "CoreMinimal.h"

#include "BRPoseOffsetTypes.generated.h"

class UAnimSequence;
class UStaticMesh;

/**
 * ADS and scope pose offsets — the largest of the procedural subsystems (`BPC_FPST_Procedural_
 * PoseOffsets`, 35 properties) and the one with the least glamorous job.
 *
 * WHAT IT IS FOR. Aiming down sights is not an animation in a modern FPS; it is a **transform**
 * that puts a specific point on the weapon — the rear of the optic — exactly on the camera axis.
 * That offset is different for every weapon, different again for every scope fitted to it, and
 * different once more when crouched. Authoring it as a clip per combination is a combinatorial
 * explosion; authoring it as a transform per combination is a table.
 *
 * Ported from `S_Procedural_PoseInfo`, `_PoseOffsetInfo`, `_PoseOffsetItemInfo`,
 * `_ADS_PoseOffsetItemInfo`, `_PoseOffset_SetupInfo`, `_PoseOffset_Scope_SetupInfo`.
 *
 * EVERY ASSET REFERENCE HERE IS SOFT, without exception. The source holds hard refs to a base
 * pose `AnimSequence`, a scope `StaticMesh`, a weapon class and a default scope class. Law 3:
 * asset refs in data are `TSoftObjectPtr`. This matters more here than anywhere else in the port
 * — a hard-ref table of every weapon × every scope would pull the entire armoury into memory the
 * moment one row is touched.
 */

/**
 * One pose offset: where the hands go, and where the IK elbows point.
 *
 * `S_Procedural_PoseOffsetItemInfo` — `{offsetTransform, leftJointTargetLocation,
 * rightJointTargetLocation}`. The joint targets are why this is not simply a transform: moving
 * the hands without moving the elbow targets folds the arms through the chest.
 */
USTRUCT(BlueprintType)
struct FBRPoseOffsetItem
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FTransform OffsetTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FVector LeftJointTargetLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FVector RightJointTargetLocation = FVector::ZeroVector;
};

/**
 * A pose offset that applies only to one kind of optic.
 *
 * `S_Procedural_ADS_PoseOffsetItemInfo` — the item above plus `scopeType`. A red dot and a 4×
 * scope sit at different heights, so aiming with each needs the weapon held differently.
 * `ScopeType` is an `FName` row handle rather than the source's free string: a typo in a string
 * silently selects no offset and the player aims past the sight.
 */
USTRUCT(BlueprintType)
struct FBRADSPoseOffsetItem
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FBRPoseOffsetItem Item;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FName ScopeType;
};

/**
 * Every offset one weapon needs. `S_Procedural_PoseOffsetInfo`.
 *
 * Idle and crouch are single items; the ADS sets are arrays because they vary by fitted optic.
 * Crouched ADS is its own array and not a modifier — a lowered stance changes the shoulder line,
 * so the correction is not the crouch offset and the ADS offset added together.
 */
USTRUCT(BlueprintType)
struct FBRPoseOffsetInfo
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FBRPoseOffsetItem Idle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FBRPoseOffsetItem Crouch;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	TArray<FBRADSPoseOffsetItem> ADSItems;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	TArray<FBRADSPoseOffsetItem> ADSCrouchItems;
};

/** One fitted optic. `S_Procedural_PoseOffset_Scope_SetupInfo`. Mesh SOFT, per law 3. */
USTRUCT(BlueprintType)
struct FBRScopeSetup
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FName ScopeType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FName AttachSocket;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	TSoftObjectPtr<UStaticMesh> ScopeMesh;
};

/**
 * A weapon's pose setup. `S_Procedural_PoseOffset_SetupInfo`.
 *
 * The source's `weapon` field is a hard class reference to the weapon Blueprint. Replaced with a
 * **row handle**: C++ knows tags and row handles, never an asset (rework governing idea 5), which
 * is the same substitution `UBRAnimLayerInstance::WeaponRow` already makes.
 */
USTRUCT(BlueprintType)
struct FBRPoseSetup
{
	GENERATED_BODY()

	/** Which weapon row this setup describes. A handle, never a class pointer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FName WeaponRow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FName WeaponAttachSocket = TEXT("hand_r");

	/** The pose the offsets are measured FROM. Soft: one sequence per weapon adds up fast. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	TSoftObjectPtr<UAnimSequence> BasePoseAnim;

	/**
	 * The optic used when none is fitted. The source documents its own field:
	 * *"If this weapon does not have a base scope…"* — iron sights are still a sight, and still
	 * need an offset, which is the case most implementations forget.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FName DefaultScopeType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FBRPoseOffsetInfo Offsets;
};

/**
 * A named base pose with its offsets. `S_Procedural_PoseInfo`.
 *
 * The outermost wrapper: a weapon can have several base poses (holstered, ready, sprinting) and
 * each carries its own offset set.
 */
USTRUCT(BlueprintType)
struct FBRPoseInfo
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FName PoseName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FTransform BasePoseTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|PoseOffset")
	FBRPoseOffsetInfo PoseOffsets;
};
