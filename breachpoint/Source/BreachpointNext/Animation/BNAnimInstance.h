#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/UnrealType.h"
#include "BNAnimInstance.generated.h"

class ABNCharacter;
class UAbilitySystemComponent;
class UCameraComponent;
class UCharacterMovementComponent;

/**
 * Which component of a BONE-SPACE rotator carries an angle.
 *
 * The aim and lean chains are Transform(Modify)Bone nodes running in bone space on spine_01..05
 * plus the neck and head. A spine bone's local axes are NOT the world's, so "pitch the torso up"
 * is not FRotator::Pitch — on this skeleton it is Roll, which is why the asset's own graph builds
 * the aim rotator as MakeRotator(Pitch) into the FIRST pin. Measured on the Manny rig, not
 * derivable, so it stays data on the ABP's defaults.
 */
UENUM()
enum class EBNSpineAxis : uint8
{
	Roll  UMETA(DisplayName = "Roll (X)"),
	Pitch UMETA(DisplayName = "Pitch (Y)"),
	Yaw   UMETA(DisplayName = "Yaw (Z)")
};

/** One angle on one bone-space axis; every other component stays zero. */
FORCEINLINE FRotator BNMakeAxisRotator(EBNSpineAxis Axis, double Degrees)
{
	switch (Axis)
	{
	case EBNSpineAxis::Pitch: return FRotator(Degrees, 0.0, 0.0);
	case EBNSpineAxis::Yaw:   return FRotator(0.0, Degrees, 0.0);
	default:                  return FRotator(0.0, 0.0, Degrees);
	}
}

/** Ordinals match the asset's AnimEnum_CardinalDirection. Internal: the graph's enum-typed pins
 *  cannot bind a C++ byte property, so these are computed and consumed natively, never published. */
enum class EBNCardinal : uint8
{
	Forward = 0,
	Backward = 1,
	Left = 2,
	Right = 3
};

/** Ordinals match the asset's AnimEnum_RootYawOffsetMode. Internal, for the same reason. */
enum class EBNRootYawOffsetMode : uint8
{
	BlendOut = 0,
	Accumulate = 2
};

/**
 * The ONE channel from the game-thread pass to the worker pass. Written only in
 * NativeUpdateAnimation, read only in NativeThreadSafeUpdateAnimation. Nothing in the worker
 * pass touches a UObject, which is what makes the parallel update legal.
 */
struct FBNAnimSnapshot
{
	FVector WorldVelocity = FVector::ZeroVector;
	FVector WorldAcceleration = FVector::ZeroVector;
	FVector WorldLocation = FVector::ZeroVector;
	FRotator ActorRotation = FRotator::ZeroRotator;
	double GravityZ = -980.0;
	double BaseAimPitch = 0.0;
	double BaseAimYaw = 0.0;
	double GroundDistance = 0.0;
	bool bAnyMontagePlaying = false;
	bool bLayerChanged = false;
	bool bFalling = false;
	bool bInAirTag = false;
	bool bCrouching = false;
	bool bJumping = false;
	bool bUnarmed = false;
	bool bSprinting = false;
	bool bFPSMode = false;
	bool bLeanLeft = false;
	bool bLeanRight = false;
	bool bADS = false;
	bool bDead = false;
	bool bLocallyControlled = false;
};

/**
 * One linked layer, with its aim properties resolved to addresses.
 *
 * The aim layers are SEPARATE anim instances of Blueprint classes — no C++ type to include, so a
 * same-named property is the only address C++ has for them, and a layer that does not declare one
 * is skipped rather than crashed. Resolving eight names per layer per frame is a hash lookup the
 * pose does not need, so the addresses are resolved once, when the linked set changes.
 */
struct FBNLinkedLayer
{
	TWeakObjectPtr<UAnimInstance> Instance;

	/** The class the addresses below were resolved against. A Blueprint recompile can hand back an
	 *  instance whose class was rebuilt underneath us; the addresses are only valid for this one. */
	TWeakObjectPtr<UClass> ResolvedClass;

	const FBoolProperty* FPSMode = nullptr;
	const FBoolProperty* IsADS = nullptr;
	const FBoolProperty* IsADSUpper = nullptr;
	const FNumericProperty* Pitch = nullptr;
	const FNumericProperty* AimPitch = nullptr;
	const FNumericProperty* AimYaw = nullptr;
	const FStructProperty* PitchRotator = nullptr;
	const FStructProperty* YawRotator = nullptr;
	const FStructProperty* LeanRotation = nullptr;
	const FStructProperty* LeanOppRotation = nullptr;

	void Resolve(UAnimInstance& Layer);
};

/**
 * The mannequin ABP's per-frame brain. Two passes, never mixed: NativeUpdateAnimation (game
 * thread) snapshots UObject state and delivers the aim surface to the linked layers;
 * NativeThreadSafeUpdateAnimation (worker) computes and publishes every graph-facing property.
 *
 * Property names are the asset's (including `isCrouching`), because a name is how a binding
 * survives a reparent; they are deliberately not tidied.
 *
 * WRITER OWNERSHIP IS SPLIT, AND THAT IS TEMPORARY. `ABP_Mannequin_Base` still carries all twenty
 * of its own update functions and still runs them from BlueprintThreadSafeUpdateAnimation, which
 * the engine invokes AFTER this class's thread-safe pass. Where both write, the graph therefore
 * wins. The outputs stay BlueprintReadWrite for exactly that reason: marking them read-only makes
 * every one of the graph's Set nodes a compile error in the asset. When the graph is cleared, this
 * whole block becomes BlueprintReadOnly in the same change and C++ becomes the sole writer.
 *
 * `PitchRotator` and `bFPSMode` are already sole-writer: the asset declares neither.
 *
 * Everything in this class is per-machine presentation. Gameplay state arrives as replicated
 * gameplay tags off the ability system component and is never mirrored into a bool that travels.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUninitializeAnimation() override;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	// ---------------------------------------------------------------- tuning (ABP defaults)
	// BlueprintReadOnly: ABP_Mannequin_Base still Gets these from its leftover update functions.
	// The graph must not Set them — C++ is the writer.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BN|Tuning")
	double CardinalDirectionDeadZone = 10.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BN|Tuning")
	bool bEnableRootYawOffset = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BN|Tuning")
	FVector2D RootYawOffsetAngleClamp = FVector2D(-120.0, 100.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BN|Tuning")
	FVector2D RootYawOffsetAngleClampCrouched = FVector2D(-90.0, 80.0);

	/** Bone-space axis that bends the spine up/down. Roll is the asset's own answer — its
	 *  MakeRotator(Pitch) fills pin 1. */
	UPROPERTY(EditDefaultsOnly, Category = "BN|Tuning")
	EBNSpineAxis AimPitchAxis = EBNSpineAxis::Roll;

	UPROPERTY(EditDefaultsOnly, Category = "BN|Tuning")
	FVector2D AimPitchClamp = FVector2D(-90.0, 90.0);

	/** Bone-space axis that twists the torso left/right. Perpendicular to the pitch bend. */
	UPROPERTY(EditDefaultsOnly, Category = "BN|Tuning")
	EBNSpineAxis AimYawAxis = EBNSpineAxis::Yaw;

	UPROPERTY(EditDefaultsOnly, Category = "BN|Tuning")
	FVector2D AimYawClamp = FVector2D(-90.0, 90.0);

	/** Proxy-only smoothing for the aim pitch, in FInterpTo speed; 0 disables it.
	 *
	 *  A locally controlled pawn reads its own controller and is exact every frame. Every OTHER
	 *  machine's copy of that pawn reads `RemoteViewPitch`, which is a uint8 — 256 steps across
	 *  360 degrees, delivered at the pawn's net update rate, not per frame. Fed straight into the
	 *  spine that is a visible staircase whenever anyone else looks up or down. The server still
	 *  judges hits against the exact rotation off the real controller, so this smooths what a
	 *  client SEES without touching what the server SCORES. */
	UPROPERTY(EditDefaultsOnly, Category = "BN|Tuning")
	float ProxyAimInterpSpeed = 15.f;

	/** A DIFFERENT axis from the aim bend: the two motions are perpendicular on the same bones. */
	UPROPERTY(EditDefaultsOnly, Category = "BN|Tuning")
	EBNSpineAxis LeanAxis = EBNSpineAxis::Yaw;

	/** Source measurements: 12 degrees, chased at 8 — an interp, not a spring, because lean
	 *  settles without overshoot. */
	UPROPERTY(EditDefaultsOnly, Category = "BN|Tuning")
	double LeanAngle = 12.0;

	UPROPERTY(EditDefaultsOnly, Category = "BN|Tuning")
	double LeanInterpSpeed = 8.0;

	/** The reference's measured AimFOV and blend speed. The base FOV is captured from the camera
	 *  on first sight, never hardcoded. */
	UPROPERTY(EditDefaultsOnly, Category = "BN|Tuning")
	float ADSFOV = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "BN|Tuning")
	float ADSFOVInterpSpeed = 18.f;

	// ---------------------------------------------------------------- locomotion (output)
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Locomotion")
	FVector LocalVelocity2D = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Locomotion")
	FVector LocalAcceleration2D = FVector::ZeroVector;

	/** Ground actually covered per second — the distance-matching input, NOT |velocity|: a slide
	 *  to a stop keeps velocity long after it stops covering ground. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Locomotion")
	double DisplacementSpeed = 0.0;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Locomotion")
	double DisplacementSinceLastUpdate = 0.0;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Locomotion")
	double LocalVelocityDirectionAngle = 0.0;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Locomotion")
	double LocalVelocityDirectionAngleWithOffset = 0.0;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Locomotion")
	FVector PivotDirection2D = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Locomotion")
	bool HasVelocity = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Locomotion")
	bool HasAcceleration = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Locomotion")
	bool IsRunningIntoWall = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Locomotion")
	bool IsFirstUpdate = true;

	// ---------------------------------------------------------------- air (output)
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Air")
	bool IsFalling = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Air")
	bool IsOnGround = true;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Air")
	bool IsJumping = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Air")
	double TimeToJumpApex = 0.0;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Air")
	double GroundDistance = 0.0;

	// ---------------------------------------------------------------- crouch (output)
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Crouch")
	bool isCrouching = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Crouch")
	bool CrouchStateChange = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Crouch")
	double ApplyCrouchAlpha = 0.0;

	// ---------------------------------------------------------------- weapon / pose (output)
	/** The upper-body layer's pose selector — the asset's own `bUnarmed`. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Weapon")
	bool bUnarmed = false;

	/** The `fPS_Sprint` slot selector, driven by State.Movement.Sprinting. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Weapon")
	bool bSprinting = false;

	/** The asset's own name for its ADS flag — the pack modelled ADS as a gameplay-tag mirror
	 *  before BN existed, and State.Weapon.ADS is that tag. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Weapon")
	bool GameplayTag_IsADS = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Weapon")
	bool ADSStateChanged = false;

	// ---------------------------------------------------------------- aim / lean (output)
	/** The aim-offset PAIR. `AimPitch`/`AimYaw` are degrees of view relative to the body, clamped
	 *  to what the spine can actually reach — feed them to an AimOffset blendspace's Pitch/Yaw
	 *  pins. `Pitch` is the same value under the asset's own name, and `PitchRotator` is the same
	 *  value again pre-built for a bone-space ModifyBone chain. Three names, one number: the
	 *  template's layers consume it one way and a blendspace consumes it the other, and neither
	 *  path is allowed to be the one that decides what the number IS. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Aim")
	double AimPitch = 0.0;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Aim")
	double Pitch = 0.0;

	/** The asset's `bFPSMode` — the BlendListByBool gate in front of every aim-pitch layer. One
	 *  answer per machine: my own pawn poses first person, every other view of me poses third. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Aim")
	bool bFPSMode = false;

	/** Pre-built bone-space rotators for a ModifyBone chain. The native pose proxy applies the
	 *  same values after the graph evaluates, so a layer whose BlendListByBool never takes the
	 *  FPS branch still aims. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Aim")
	FRotator PitchRotator = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Aim")
	FRotator YawRotator = FRotator::ZeroRotator;

	/** View yaw relative to the body — the other half of the aim-offset pair. This is NOT
	 *  turn-in-place (`RootYawOffset`). On a pawn whose capsule yaws with the controller it
	 *  sits near zero; it is live on a proxy whose actor rotation is interpolating, and on
	 *  any future path that lets the hips lag the look. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Aim")
	double AimYaw = 0.0;

	/** The spine's lean and the head's counter-lean. The asset's LeanSpineWeights share them out
	 *  per bone; its head entry is labelled "Opposite Angle Weight" — the body tips, the gaze
	 *  stays level. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Aim")
	FRotator LeanRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Aim")
	FRotator LeanOppRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Aim")
	double YawDeltaSinceLastUpdate = 0.0;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Aim")
	double YawDeltaSpeed = 0.0;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Aim")
	double AdditiveLeanAngle = 0.0;

	// ---------------------------------------------------------------- turn in place (output)
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|TurnInPlace")
	double RootYawOffset = 0.0;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|TurnInPlace")
	double TurnYawCurveValue = 0.0;

	// ---------------------------------------------------------------- layers (output)
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Layers")
	double UpperbodyDynamicAdditiveWeight = 0.0;

	/** One-frame edge. The CHARACTER owns linking; this instance only observes the swap. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "BN|Layers")
	bool LinkedLayerChanged = false;

private:
	// ---- game thread
	void ResolveOwner();
	void ResolveAbilitySystem();
	void OnTagChanged(const FGameplayTag Tag, int32 NewCount);
	void RefreshLinkedLayers();

	/** Delivers last frame's aim surface to the LAYER instances. The layers are separate anim
	 *  instances: writing our own properties reaches any layer that binds through
	 *  GetMainAnimBPThreadSafe, and nothing else. One frame stale by construction — exactly as
	 *  stale as the template's own game-thread interface events. */
	void PushAimSurfaceToLinkedLayers();

	/** Owner-only, and the camera is the local player's alone: proxies read ADS off the pose. */
	void UpdateADSFieldOfView(float DeltaSeconds);

	/** Walking answers 0 without a trace; airborne traces down. */
	double ComputeGroundDistance() const;

	// ---- worker thread. Both operate on the private accumulators.
	void SetRootYawOffset(double InRootYawOffset);
	void ProcessTurnYawCurve();

	UPROPERTY(Transient)
	TObjectPtr<ABNCharacter> Character;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	/** Compare-only, for the LinkedLayerChanged edge. */
	UPROPERTY(Transient)
	TObjectPtr<UClass> ObservedLayerClass;

	/** The asset's SECOND ADS flag, driving the upper-body/weapon side of the aim pose. Reached by
	 *  reflection rather than declared: a C++ property of this name would rename the ABP's own
	 *  variable to IsADS_Upper_0 on the reparent compile, and the graph would silently follow. */
	const FBoolProperty* SelfIsADSUpper = nullptr;

	TArray<FBNLinkedLayer> LinkedLayers;

	FDelegateHandle CrouchTagHandle;
	FDelegateHandle JumpTagHandle;
	FDelegateHandle InAirTagHandle;
	FDelegateHandle SprintTagHandle;
	FDelegateHandle LeanLeftTagHandle;
	FDelegateHandle LeanRightTagHandle;
	FDelegateHandle ADSTagHandle;
	FDelegateHandle ReloadTagHandle;
	FDelegateHandle FiringTagHandle;
	FDelegateHandle MeleeTagHandle;
	FDelegateHandle DeadTagHandle;

	bool bTagInAir = false;
	bool bTagCrouching = false;
	bool bTagJumping = false;
	bool bTagSprinting = false;
	bool bTagLeanLeft = false;
	bool bTagLeanRight = false;
	bool bTagADS = false;
	bool bTagReloading = false;
	bool bTagFiring = false;
	bool bTagMelee = false;
	bool bTagDead = false;

	FBNAnimSnapshot Snapshot;

	// Worker-thread history and accumulators. Native math never reads a published property back.
	FFloatSpringState RootYawOffsetSpringState;
	double PreviousTurnYawCurveValue = 0.0;
	double PreviousYaw = 0.0;
	FVector PreviousWorldLocation = FVector::ZeroVector;
	bool bWasCrouchingLastUpdate = false;
	bool bWasADSLastUpdate = false;
	bool bWasMovingLastUpdate = false;
	bool bFirstUpdate = true;
	double CurrentLeaning = 0.0;
	double SmoothedAimPitch = 0.0;
	double SmoothedAimYaw = 0.0;
	EBNCardinal LocalVelocityDirection = EBNCardinal::Forward;
	EBNCardinal LocalVelocityDirectionNoOffset = EBNCardinal::Forward;
	EBNCardinal CardinalFromAcceleration = EBNCardinal::Forward;

	/** The camera's own FOV, captured on first sight; negative = not yet captured. */
	float DefaultFOV = -1.f;

	friend struct FBNAnimInstanceProxy;
};
