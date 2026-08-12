#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"

#include "BRMannequinAnimInstance.generated.h"

/**
 * ABP_Mannequin_Base's anim state model, in C++. NEW FILE, generated - not moved, and not
 * written from the variable names.
 *
 * TWO SOURCES, JOINED, BECAUSE NEITHER IS SUFFICIENT ALONE.
 *   - `animations/reports/ABP_Mannequin_Base.json` (the offline reader) gives the REAL
 *     declared names in their true UpperCamel form: AimPitch, not aimPitch.
 *   - `mcp-bp/bp_inventory_anim.json` (the live-editor read) gives types and the CDO's own
 *     default values, but LOWERCASES the first letter of every key.
 * An earlier attempt used the inventory alone and produced members named after the
 * lowercased keys. That is the casing trap `animations/FINDINGS.md` documents, where a
 * "properties are lowerCamel" heuristic scored 8 correct out of 116 and was deleted.
 *
 * 53 properties are declared here with their real names and real defaults. 28 are skipped -
 * objects, structs, arrays and maps whose types need a state-vs-asset-reference decision;
 * they are listed at the bottom.
 *
 * WHAT IS NOT HERE. The ubergraph (locomotion maths, cardinal direction, root-yaw offset,
 * spring interpolations) and the pose graph, which is Tier 4 and stays an asset. Also the 14
 * content dependencies FINDINGS.md enumerates - the Control Rig, ALI_ItemAnimLayers,
 * BPI_FPST_AnimInterface, SK_Mannequin, the blend spaces and montages - none of which appear
 * in a property list and all of which a real port must carry.
 *
 * NOTHING IS REPARENTED ONTO THIS CLASS. It compiles and can be reviewed without touching an
 * asset, which is the rule this project earned the hard way today.
 */
UCLASS(Blueprintable)
class BREACHPOINT_API UBRMannequinAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	/** BPI_FPST_AnimInterface, the seam AMyCharacter already drives. */
	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetADS(bool InADS);

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetADS_Upper(bool InADS_Upper);

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetSprinting(bool InSprinting);

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetUnarmed(bool InUnarmed);

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool ADSStateChanged = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float AdditiveLeanAngle = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float AimPitch = -2.78873f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float AimYaw = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float ApplyCrouchAlpha = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float ApplyPelvisWeight = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float ApplySwayAlpha = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float CardinalDirectionDeadZone = 10.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	uint8 CardinalDirectionFromAcceleration = 0;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool CrouchStateChange = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float DisplacementSinceLastUpdate = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float DisplacementSpeed = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool EnableControlRig = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float FPSPelvisWeight = 0.2f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool GameplayTag_IsADS = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool GameplayTag_IsDashing = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool GameplayTag_IsFiring = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool GameplayTag_IsMelee = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool GameplayTag_IsReloading = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float GroundDistance = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool HasAcceleration = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool HasVelocity = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float Head_CameraShake_Alpha = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool IsADS_Upper = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool IsFalling = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool IsFirstUpdate = true;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool IsJumping = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool IsOnGround = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool IsRunningIntoWall = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float LastPivotTime = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float LeanAdditiveAlpha = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool LinkedLayerChanged = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	uint8 LocalVelocityDirection = 0;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float LocalVelocityDirectionAngle = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float LocalVelocityDirectionAngleWithOffset = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	uint8 LocalVelocityDirectionNoOffset = 0;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float Pitch = -0.417859f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	uint8 PivotInitialDirection = 0;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	uint8 StartDirection = 0;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float TimeSinceFiredWeapon = 9999.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float TimeToJumpApex = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float TurnYawCurveValue = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float UpperbodyDynamicAdditiveWeight = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool UseFootPlacement = true;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool WasADSLastUpdate = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float YawDeltaSinceLastUpdate = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	float YawDeltaSpeed = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool bFPSMode = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool bFPSWalkMode = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool bSprinting = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool bUnarmed = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool isCrouching = false;
};

/*
 * NOT PORTED - need a state-vs-asset-reference decision:
 *   AimSpineWeights_UE4                  {'properties': {'head': {'type': 'number
 *   AimSpineWeights_UE5                  {'properties': {'head': {'type': 'number
 *   BasePoseLocation                     {'properties': {'x': {'type': 'number'},
 *   BasePoseRotation                     {'properties': {'pitch': {'description':
 *   CamRotCurrent                        {'properties': {'pitch': {'description':
 *   CamRotOffset                         {'properties': {'x': {'type': 'number'},
 *   CamRotPrev                           {'properties': {'pitch': {'description':
 *   CamRotRate                           {'properties': {'pitch': {'description':
 *   CurrPoseLocation                     {'properties': {'x': {'type': 'number'},
 *   CurrPoseRotation                     {'properties': {'pitch': {'description':
 *   LastLinkedLayer                      {'description': 'Represents a reference 
 *   LeanOppRotation                      {'properties': {'pitch': {'description':
 *   LeanRotation                         {'properties': {'pitch': {'description':
 *   LeanSpineWeights_UE4                 {'properties': {'head(Opposite Angle Wei
 *   LeanSpineWeights_UE5                 {'properties': {'head(Opposite Angle Wei
 *   LeftJointTargetLocation              {'properties': {'x': {'type': 'number'},
 *   LocalAcceleration2D                  {'properties': {'x': {'type': 'number'},
 *   LocalVelocity2D                      {'properties': {'x': {'type': 'number'},
 *   MontagePoseOffsetLocation            {'properties': {'x': {'type': 'number'},
 *   MontagePoseOffsetRotation            {'properties': {'pitch': {'description':
 *   PitchRotator                         {'properties': {'pitch': {'description':
 *   PivotDirection2D                     {'properties': {'x': {'type': 'number'},
 *   ProcApplyLocation                    {'properties': {'x': {'type': 'number'},
 *   ProcApplyRotation                    {'properties': {'pitch': {'description':
 *   RightJointTargetLocation             {'properties': {'x': {'type': 'number'},
 *   WorldLocation                        {'properties': {'x': {'type': 'number'},
 *   WorldRotation                        {'properties': {'pitch': {'description':
 *   WorldVelocity                        {'properties': {'x': {'type': 'number'},
 */
