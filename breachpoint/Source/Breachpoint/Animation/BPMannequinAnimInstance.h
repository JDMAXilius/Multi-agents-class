#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"

#include "BPMannequinAnimInstance.generated.h"

/**
 * ABP_Mannequin_Base's ANIM STATE MODEL, in C++. Generated from a CDO read of the live
 * asset, not written from the variable names.
 *
 * WHERE THIS CAME FROM. `mcp-bp/bp_extract.py --set anim_spine` ->
 * `mcp-bp/bp_inventory_anim.json`. As that tool's own header says, an AnimBlueprint's CDO
 * carries the AnimInstance's variables, and that IS the anim state model - so this field
 * list is a measurement of the asset, not a reconstruction of it. 52 of the ABP's authored
 * properties are declared below with their REAL default values.
 *
 * WHAT THIS CLASS IS NOT, YET. It holds state and nothing drives it. The ABP's ubergraph -
 * the locomotion maths, the cardinal-direction selection, the root-yaw offset, the spring
 * interpolations - is not ported here, and neither is the pose graph, which is Tier 4 and
 * stays an asset either way. Nothing is reparented onto this class: exactly like
 * ABPFPSWeapon, it compiles and can be reviewed without touching a single asset, because
 * landing a half-finished port onto a live asset is what broke this project twice in one
 * day.
 *
 * THE INTERFACE MESSAGES ARE THE SEAM THAT MATTERS. BPI_FPST_AnimInterface's SetADS,
 * SetADS_Upper, SetSprinting, SetUnarmed, SetFPSMode and SetFPSWalkMode are what
 * AMyCharacter already sends and what the linked layers read. They are declared here as
 * BlueprintCallable so a future ABP child can implement the interface against this class
 * rather than against loose Blueprint variables.
 *
 * NOT AUTO-PORTABLE, and deliberately left out rather than guessed: 34 properties whose
 * types are objects, structs, arrays or maps (transforms, curve maps, montage refs, the
 * S_Procedural_* structs). Each needs a decision about whether it is state or an asset
 * reference, and a wrong guess there is the silent-corruption class of bug this project
 * keeps paying for. They are listed at the bottom of this file.
 */
UCLASS(Blueprintable)
class BREACHPOINT_API UBPMannequinAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	// ---------------------------------------------------------------------------------
	// BPI_FPST_AnimInterface. The messages AMyCharacter already sends every frame it
	// changes state; see MyCharacter::SendAnimInterfaceBool.
	// ---------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetADS(bool InADS);

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetADS_Upper(bool InADS_Upper);

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetSprinting(bool InSprinting);

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetUnarmed(bool InUnarmed);

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetFPSMode(uint8 InFPSMode);

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetFPSWalkMode(uint8 InFPSWalkMode);

	UFUNCTION(BlueprintPure, Category = "FPST|Interface")
	uint8 GetFPSMode() const { return BFPSMode ? 1 : 0; }

	UFUNCTION(BlueprintPure, Category = "FPST|Interface")
	bool GetADS() const { return bADS; }

	/**
	 * THE ONE FIELD NOT TAKEN FROM THE CDO, and it is flagged rather than guessed.
	 *
	 * The extraction exposes IsADS_Upper, GameplayTag_IsADS, ADSStateChanged and
	 * WasADSLastUpdate - but NO plain ADS bool under any obvious name. SetADS provably
	 * writes something (the graph read shows SetVariableOnPersistentFrame), so the
	 * variable exists; the extractor's camelCasing or a display-name/variable-name split
	 * is hiding which one. Declared here so the setter has a home and the gap is visible.
	 * Resolve it by opening ABP_Mannequin_Base's SetADS node and reading the target pin.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool bADS = false;

	// ---------------------------------------------------------------------------------
	// The state model, read from the CDO. Defaults are the asset's own.
	// ---------------------------------------------------------------------------------

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
	bool BFPSMode = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool BFPSWalkMode = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool BSprinting = false;

	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool BUnarmed = false;

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
	bool IsCrouching = false;

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
};

/*
 * NOT PORTED - types that need a human decision, with the schema the extractor reported:
 *   AimSpineWeights_UE4                    {'properties': {'head': {'type': 'number'}, 
 *   AimSpineWeights_UE5                    {'properties': {'head': {'type': 'number'}, 
 *   BasePoseLocation                       {'properties': {'x': {'type': 'number'}, 'y'
 *   BasePoseRotation                       {'properties': {'pitch': {'description': 'Pi
 *   CamRotCurrent                          {'properties': {'pitch': {'description': 'Pi
 *   CamRotOffset                           {'properties': {'x': {'type': 'number'}, 'y'
 *   CamRotPrev                             {'properties': {'pitch': {'description': 'Pi
 *   CamRotRate                             {'properties': {'pitch': {'description': 'Pi
 *   CurrPoseLocation                       {'properties': {'x': {'type': 'number'}, 'y'
 *   CurrPoseRotation                       {'properties': {'pitch': {'description': 'Pi
 *   LastLinkedLayer                        {'description': 'Represents a reference to a
 *   LeanOppRotation                        {'properties': {'pitch': {'description': 'Pi
 *   LeanRotation                           {'properties': {'pitch': {'description': 'Pi
 *   LeanSpineWeights_UE4                   {'properties': {'head(Opposite Angle Weight)
 *   LeanSpineWeights_UE5                   {'properties': {'head(Opposite Angle Weight)
 *   LeftJointTargetLocation                {'properties': {'x': {'type': 'number'}, 'y'
 *   LocalAcceleration2D                    {'properties': {'x': {'type': 'number'}, 'y'
 *   LocalVelocity2D                        {'properties': {'x': {'type': 'number'}, 'y'
 *   MontagePoseOffsetLocation              {'properties': {'x': {'type': 'number'}, 'y'
 *   MontagePoseOffsetRotation              {'properties': {'pitch': {'description': 'Pi
 *   OnAllMontageInstancesEnded             {}
 *   OnMontageBlendedIn                     {}
 *   OnMontageBlendingOut                   {}
 *   OnMontageEnded                         {}
 *   OnMontageSectionChanged                {}
 *   OnMontageStarted                       {}
 *   PitchRotator                           {'properties': {'pitch': {'description': 'Pi
 *   PivotDirection2D                       {'properties': {'x': {'type': 'number'}, 'y'
 *   ProcApplyLocation                      {'properties': {'x': {'type': 'number'}, 'y'
 *   ProcApplyRotation                      {'properties': {'pitch': {'description': 'Pi
 *   RightJointTargetLocation               {'properties': {'x': {'type': 'number'}, 'y'
 *   WorldLocation                          {'properties': {'x': {'type': 'number'}, 'y'
 *   WorldRotation                          {'properties': {'pitch': {'description': 'Pi
 *   WorldVelocity                          {'properties': {'x': {'type': 'number'}, 'y'
 */
