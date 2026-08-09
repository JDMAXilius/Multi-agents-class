
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/TimelineComponent.h"
#include "Data/Enumeration/PlayerEnums.h"
#include "Data/Structure/PlayerStructs.h"
#include "Data/Enumeration/WeaponEnums.h"
#include "Data/Structure/WeaponStructs.h"
#include "PlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USkeletalMeshComponent;
class UCapsuleComponent;
class USceneComponent;
class UStaticMeshComponent;
class USphereComponent;
class UCharacterMovementComponent;
class UInventory;
class UGrenadeSystem;
class APickupBase;
class AWeaponBaseActor;
class AMainPlayerController;
class USoundBase;
class UCameraShakeBase;
class UAnimMontage;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class SHOOTERCORE_API APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

	//---------------------------------------------- FUNCTION DECLARATION ---------------------------------------

public:
	APlayerCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	UFUNCTION(BlueprintCallable,BlueprintPure)
	AWeaponBaseActor* GetPrimaryWeapon();
	UFUNCTION(BlueprintCallable,BlueprintPure)
	AWeaponBaseActor* GetSecondaryWeapon();
	void UpdateEquippedWeapon(AWeaponBaseActor* Weapon);
	void PickupWeapon(AWeaponBaseActor* Weapon);
	UCameraComponent* GetPlayerCamera();
	void PlayCameraShake();
	void TraceForPickups();
	void SetInHandMesh(bool bShow, EPickupType Type);

	UFUNCTION(BlueprintImplementableEvent)
	void PushPickupNotification(EPickupType Type, UTexture2D* NotificationIcon);

	UFUNCTION(BlueprintImplementableEvent)
	void PushEmptyNotification(EEmptyNotificationType Type);

	UFUNCTION(BlueprintImplementableEvent)
	void CrosshairAnimation(bool bDead);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Notification")
	void LoadingNotification(ELoadingNotificationType Type);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Crosshair")
	void UpdateCrossHair(bool bShow, EWeaponType Type);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Crosshair")
	void PlayEnemyDetectedCrosshairAnim(bool bDetected);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Crosshair")
	void PlayCrosshairSpreadAnim(bool bPlay);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Crosshair")
	void PlayCrosshairAimAnim(bool bDown);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Notification")
	void PushPickupInteractNotification(EPickupInteractNotificationType Type, FPickupData Data);

	UFUNCTION(BlueprintCallable)
	void UpdateLocomotionState(ELocomotionState State);

	UFUNCTION(BlueprintCallable)
	void UpdateGaitSettings(EGait Gait);

	FORCEINLINE USceneComponent* GetGrenadeThrowComponent() const
	{
		return GrenadeThrowTransform;
	}

protected:
	virtual void BeginPlay() override;
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartWalk();
	void StopWalk();
	void CustomCrouch();
	void StartInteract();
	void StopInteract();
	void Interact();
	void AdsButtonPressed();
	void AdsButtonReleased();
	void StartShoot();
	void StopShoot();
	void Shoot();
	void Reloading();
	void EquipUnequip();
	void UpdateWeaponAttachment();
	void StorePrimaryWeapon();
	void PlayReloadMontage();
	void PlayShootMontage();
	void CheckIfEnemyInFiringRange();
	void CrosshairSpreadSetup();
	void PlayUnequipWeaponMontage(EWeaponType Type, EWeaponSlot Slot);
	void PlayEquipWeaponMontage(EWeaponType Type, EWeaponSlot Slot);
	void UpdateWeaponAttachment(bool bEquip);


	UFUNCTION(BlueprintCallable)
	void StartGrenadeLogic();

	UFUNCTION(BlueprintCallable)
	void ThrowGrenade();

	UFUNCTION(BlueprintCallable)
	void CancelGrenade();

	UFUNCTION(BlueprintCallable)
	void UnequipWeapon();

	UFUNCTION(BlueprintCallable)
	void EquipWeapon();

	UFUNCTION(BlueprintCallable)
	void ApplyBandage();

	UFUNCTION(BlueprintCallable)
	void UseEnergyDrink();

	UFUNCTION(BlueprintCallable)
	void Switch();

	UFUNCTION(BlueprintCallable, Category = "UI|Inputs")
	bool EquipPrimaryWeapon(EWeaponType WeaponType);

	UFUNCTION()
	void OnMontageNotifyBegin(FName MontageNotify, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload);

private:
	UFUNCTION()
	void UpdateSpringArmOnCrouch(float Value);

	UFUNCTION()
	void UpdateFieldOfView(float Value);

	UFUNCTION()
	void OnInteractionAreaBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void OnInteractionAreaEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);



	//---------------------------------------------- VARIABLE DECLARATION ---------------------------------------



public:
	AMainPlayerController* PlayerController;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Setup|Inventory")
	UInventory* PlayerInventory;

	UPROPERTY(BlueprintReadOnly)
	AWeaponBaseActor* PrimaryWeapon;

	UPROPERTY(BlueprintReadOnly)
	AWeaponBaseActor* SecondaryWeapon;

protected:
	UPROPERTY(BlueprintReadOnly)
	UAnimInstance* PlayerAnimInstance;

	UPROPERTY(BlueprintReadOnly)
	float GroundDistance;

	UPROPERTY(BlueprintReadOnly)
	EGait CurrentGait;

	UPROPERTY(BlueprintReadOnly)
	ELocomotionState CurrentLocomotionState;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Crosshair")
	bool bEnemyDetected;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Crosshair")
	bool bPlayCrosshairSpreadAnim;;

private:
	USkeletalMeshComponent* PlayerMesh;
	UCapsuleComponent* PlayerCapsule;
	UCharacterMovementComponent* PlayerMovement;
	FTimeline UpdateFieldOfViewTimeline;
	FTimeline UpdateSpringArmOnCrouchTimeline;
	FTimerHandle ShootTimerHandle;
	FTimerHandle InteractTimerHandle;
	bool bCanShoot;
	bool bPrimaryWeaponStored;
	bool bCrouched;
	bool bAdsButtonPressed;
	bool bSwitchingWeapon;
	AWeaponBaseActor* CurrentEquippedWeapon;
	int32 SwitchingWeaponTimes_Rifle;
	int32 SwitchingWeaponTimes_Pistol;
	APickupBase* FocusedPickupActor;
	EWeaponSlot CurrentWeaponSlot;
	FName PrimaryWeaponSocket;
	FName SecondaryWeaponSocket;
	FName PrimaryWeaponStoredSocket;
	FWeaponSocketController WeaponSocketController;


	//----- Components

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GrenadeSystem", meta = (AllowPrivateAccess = "true"))
	USceneComponent* GrenadeThrowTransform;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GrenadeSystem", meta = (AllowPrivateAccess = "true"))
	UGrenadeSystem* GrenadeSystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Mesh", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* InHandMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Sphere", meta = (AllowPrivateAccess = "true"))
	USphereComponent* InteractionArea;

	//Mesh

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Mesh", meta = (AllowPrivateAccess = "true"))
	UStaticMesh* GrenadeMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Mesh", meta = (AllowPrivateAccess = "true"))
	UStaticMesh* EnergyDrinkMesh;



	//----- Animations

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Curve", meta = (AllowPrivateAccess = "true"))
	UCurveFloat* UpdateFieldOfViewCurve;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Data", meta = (AllowPrivateAccess = "true"))
	TMap<EGait, FGaitSetting> GaitSettingMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Layers", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UAnimInstance> UnarmedAnimLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Layers", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UAnimInstance> RifleAnimLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Layers", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UAnimInstance> PistolAnimLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Layers", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UAnimInstance> ShotgunAnimLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Rifle", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* ReloadRifleMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Pistol", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* ReloadPistolMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Shotgun", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* ReloadShotgunMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Rifle", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* ShootRifleMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Pistol", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* ShootPistolMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Shotgun", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* ShootShotgunMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Health", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* ApplyBandageMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Health", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* UseEnergyDrinkMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Grenade", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* GrenadeStartMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Grenade", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* GrenadeCancelMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Grenade", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* GrenadeThrowMontage;
	
	//New Setup
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Rifle", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* UnequipToPrimaryWeaponSocketRifleMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Rifle", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* UnequipToSecondaryWeaponSocketRifleMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Pistol", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* UnequipToPrimaryWeaponSocketPistolMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Pistol", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* UnequipToSecondaryWeaponSocketPistolMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Rifle", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* EquipFromPrimaryWeaponSocketRifleMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Rifle", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* EquipFromSecondaryWeaponSocketRifleMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Pistol", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* EquipFromPrimaryWeaponSocketPistolMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Animation|Montages|Pistol", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* EquipFromSecondaryWeaponSocketPistolMontage;

	//----- Audio

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Audio", meta = (AllowPrivateAccess = "true"))
	USoundBase* WeaponRaiseSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Audio", meta = (AllowPrivateAccess = "true"))
	USoundBase* WeaponDownSound;

	//----- CameraShake

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|CameraShake", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UCameraShakeBase> ShootCameraShake;

	//----- Socket

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Sockets", meta = (AllowPrivateAccess = "true"))
	FName InHandGrenadeSocketName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Sockets", meta = (AllowPrivateAccess = "true"))
	FName InHandEnergyDrinkSocketName;

	//----- Inputs

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* WalkAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* CrouchAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* InteractAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* AdsAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* ShootAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* ReloadingAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* SwitchAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* EquipUnequipAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* ApplyBandageAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* UseEnergyDrinkAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* RadialMenuAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inputs", meta = (AllowPrivateAccess = "true"))
	UInputAction* ThrowGrenadeAction;

};
