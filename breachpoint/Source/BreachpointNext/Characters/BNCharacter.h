#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "UObject/SoftObjectPath.h"
#include "BNCharacter.generated.h"

class UBNEquipmentComponent;
class UBNHealthComponent;
class UCameraComponent;
class USpringArmComponent;
struct FOnAttributeChangeData;

UCLASS(Config=Game)
class BREACHPOINTNEXT_API ABNCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	/** ObjectInitializer form since BN23: the ctor swaps the movement component class for
	 *  UBNCharacterMovementComponent (the grapple's predicted pull) — the one sanctioned
	 *  way to replace an ACharacter default subobject. */
	ABNCharacter(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	UClass* GetCurrentWeaponAnimLayer() const;
	UBNEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }
	UCameraComponent* GetFirstPersonCamera() const { return CameraComponent; }
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	void InitializeAnimLayer();

	/** The FP template's attach: 3P weapon stays on GetMesh (AttachmentReplication), 1P weapon
	 *  snaps to the owner-only mesh. Runs on every machine — a second component's parent is not
	 *  the actor attachment and does not travel with it. */
	void AttachWeaponMeshes(class ABNWeapon* Weapon);

	/** The view, on every machine. Owner reads the controller; a simulated proxy decompresses
	 *  RemoteViewPitch. Pitch is normalized — GetBaseAimRotation returns 0..360 on a proxy, and
	 *  an aim offset fed that raw number bends the spine the wrong way. This is the one getter
	 *  the anim instance pulls; nothing else re-derives it. */
	FRotator GetAimRotation() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	FName CameraAttachSocket = TEXT("head");

	/** THE layer set — Lyra only, by the founder's 22 Aug ruling. Children of
	 *  `ABP_ItemAnimLayersBase` — that base is the interface parent, not a pose to link. */
	UPROPERTY(Config)
	FSoftClassPath LyraItemAnimLayersBase;

	UPROPERTY(Config)
	FSoftClassPath LyraUnarmedAnimLayer;

	UPROPERTY(Config)
	FSoftClassPath LyraPistolAnimLayer;

	UPROPERTY(Config)
	FSoftClassPath LyraRifleAnimLayer;

	UPROPERTY(Config)
	FSoftClassPath LyraShotgunAnimLayer;

	UPROPERTY(Transient)
	TObjectPtr<UClass> LinkedAnimLayerClass;

	void InitializeAbilitySystem();
	UClass* ResolveAnimLayerClass();
	UClass* ResolveLyraLayerForRow(FName RowName) const;
	void OnMoveSpeedChanged(const FOnAttributeChangeData& Data);
	void HandleDeath(UBNHealthComponent* Component);

	/** The weapon-pose half of ADS, driven by the replicated State.Weapon.ADS tag rather than the
	 *  input edge, so every machine poses its own view of this character. */
	void HandleADSTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** Hip/aim × stand/crouch. Reads the ADS tag (or the count from its edge) and bIsCrouched,
	 *  so a crouch mid-aim does not leave the stand pose stuck. */
	void RefreshWeaponPose(int32 ADSCount = -1);

	/** The PoseOffsets component is a Blueprint class with no C++ base, so its ChangePose entry
	 *  point is reachable only by reflection. The component is found once — scanning every
	 *  component on each aim press is the part worth caching; the function lookup that follows is
	 *  a hash hit, and leaving it live is what keeps a Blueprint recompile from stranding us on a
	 *  stale address. */
	void ResolvePoseOffsets();

	/** Announces at BeginPlay whether this body can actually be hit. A Blueprint pawn can
	 *  out-serialise the constructor's channel responses, and when it does, every weapon in the
	 *  game misses every player with no error — so the answer is printed rather than assumed. */
	void VerifyDamageCollision() const;

	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> PoseOffsetsComponent;

	FDelegateHandle ADSPoseTagHandle;
	FActiveGameplayEffectHandle CrouchStateHandle;

	/** Owner-only body. Same skeleton as GetMesh, posed by it (leader pose) so there is one anim
	 *  brain. Tagged FirstPerson so the camera's FirstPersonScale / FirstPersonFOV apply — that is
	 *  how the official FP template keeps the arms in the view without parenting them to the
	 *  camera. GetMesh is the world representation everyone else sees. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	/** Zero-length boom between the 1P head socket and the camera: position from the socket with
	 *  lag, rotation from the controller. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBNEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBNHealthComponent> HealthComponent;

	FDelegateHandle MoveSpeedChangedHandle;

	// UnPossessed nulls PlayerState before the corpse is destroyed; EndPlay uses this cache.
	TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystem;
};
