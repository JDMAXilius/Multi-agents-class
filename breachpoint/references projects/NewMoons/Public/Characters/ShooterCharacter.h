// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/NMCharacterBase.h"
#include "InputActionValue.h"
#include "Player/Data/LocalPlayerData.h"
#include "ShooterCharacter.generated.h"

class AWeapon;
class UInputAction;
class UCameraComponent;
class UNMWeaponComponent;
class USpringArmComponent;
class UInputMappingContext;

UCLASS()
class NEWMOONS_API AShooterCharacter : public ANMCharacterBase
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AShooterCharacter(const FObjectInitializer& ObjectInitializer);

	/** Called every frame */
	virtual void Tick(float DeltaTime) override;
	/** Called to bind functionality to input */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	FORCEINLINE UAnimMontage* GetFireMontage() const { return FireMontage; }

	/** The maximum ground speed when sprinting */
	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxWalkSpeedSprinting;

	UFUNCTION(BlueprintImplementableEvent, Category = "HoverEvents")
	void OnStopHover();

	UFUNCTION(BlueprintImplementableEvent, Category = "HoverEvents")
	void OnStartHover();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsHovering = false;
	FVector CurrentVelocity;

	FRotator OriginalTurnRate;
	bool OriginalOrientRotation;
	float OriginalGravityScale;
	float OriginalWalkingSpeed;
	float OriginalDeceleration;
	float OriginalAcceleration;
	float OriginalAirControl;
	bool OriginalDesiredRotation;

	// Hover variables
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hover")
	FRotator HoverTurnRate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hover")
	float HoverGravityScale;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hover")
	float HoverWalkingSpeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hover")
	float HoverDeceleration;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hover")
	float HoverAcceleration;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hover")
	float HoverAirControl;
	
protected:

#pragma region Inputs

	/** Player Actions/Bindings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* SprintAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* LookAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* JumpAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* FireAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InteractAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* HoverAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* AimAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* CrouchAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventorySlot1Action;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventorySlot2Action;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventorySlot3Action;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventorySlot4Action;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);
	/** Called for camera movement, using the mouse input */
	void Look(const FInputActionValue& Value);
	/** Called for activating/disactivating input */
	void Sprint();
	/** Called for activating/disactivating input */
	void StopSprinting();
	/** Called when player pressing/holding fire button */
	void Fire();
	/** Called when player pressing interact button */
	void Interact();
	/** Called when player starts hovering */
	void StartHover();
	/** Called when player stops hovering */
	void StopHover();
	/** Called when player starts aiming */
	void StartAim();
	/** Called when player stops aiming */
	void StopAim();
	/** Interp the camera properties when pressed/releasing the aim */
	void CameraInterpZoom(float DeltaTime);
	/** Called for crouching/un-crouching input */
	void Crouch();
	/** Called when player press the inventory slot buttons 1 -> 4 */
	void InventorySlot1();
	void InventorySlot2();
	void InventorySlot3();
	void InventorySlot4();

#pragma endregion

	bool CanStartHover();
	void RecordOriginalSettings();
	void ApplyOriginalSettings();
	void DescentPlayer(float DeltaTime);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover")
	float DescendingRate = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover")
	float MaxHoverDuration = 3.f; // In seconds

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover")
	float HoverCooldownAfterMaxDuration = 0.25f; // Cooldown for hover ability

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadOnly, Category = "Loadout System")
	FPlayerLoadout PlayerLoadout;

private:
	/** The actual mesh of the character that will be real-time retargeted from the parent skeletal mesh component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* RetargetedMesh;

	/** True when aiming */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	bool bAiming;
	/** Montage for firing the weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	UAnimMontage* FireMontage;
	/** Default camera field of view value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	float CameraDefaultFOV;
	/** Field of view value for when zoomed in */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	float CameraZoomedFOV;
	/** Current field of view this frame */
	float CameraCurrentFOV;
	/** Interp speed for zooming when aiming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	float ZoomInterpSpeed;

	/** Hover ability duration and cooldown */
	FTimerHandle HoverDurationTimer;
	FTimerHandle HoverCooldownTimer;
	bool bHoverOnCooldown;

	void EndHoverCooldown();
};
