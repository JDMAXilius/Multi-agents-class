// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NMCharacterBase.h"
#include "Player/Data/LocalPlayerData.h"
#include "NMPlayerCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS(Blueprintable)
class NEWMOONS_API ANMPlayerCharacter : public ANMCharacterBase
{
	GENERATED_BODY()

public:
	ANMPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	/** Returns Retargeted Mesh sub-object */
	FORCEINLINE USkeletalMeshComponent* GetRetargetedMesh() const { return RetargetedMesh; }

	/** Called every frame */
	virtual void Tick(float DeltaTime) override;
	
	/** The maximum ground speed when sprinting */
	UPROPERTY(Category = "Properties | Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxWalkSpeedSprinting;
	FVector CurrentVelocity;

	// In Player or Weapon Class
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Weapon")
	bool bSniperZoom = false;
	
protected:
	// To add mapping context
	virtual void BeginPlay();

	virtual void PossessedBy(AController* NewController) override;
	
#pragma region Inputs
	
	/** Called to bind functionality to input */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	/** Player Actions/Bindings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputMappingContext* DefaultMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* MoveAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* SprintAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* LookAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* JumpAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* FireAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* ThrowGrenade;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* ReloadAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* InteractAction;;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* AimAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* CrouchAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* InventorySlot1Action;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* InventorySlot2Action;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* InventorySlot3Action;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* InventorySlot4Action;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* InventorySlot5Action;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties | Input")
	UInputAction* MainMenuAction;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);
	/** Called for looking input */
	void Look(const FInputActionValue& Value);
	void StartAim();
	/** Called when player stops aiming */
	void StopAim();
	/** Interp the camera properties when pressed/releasing the aim */
	void CameraInterpZoom(float DeltaTime);
	/** Called for activating/disactivating input */
	void Sprint();
	/** Called for activating/disactivating input */
	void StopSprinting();
	/** Called when player pressing/holding fire button */
	void Fire();
	void StopFire();
	UFUNCTION()
	void Throw();
	void Reload();
	/** Called for crouching/un-crouching input */
	void Crouch();
	/** Called when player press the inventory slot buttons 1 -> 5 */
	void InventorySlot1();
	void InventorySlot2();
	void InventorySlot3();
	void InventorySlot4();
	void InventorySlot5();
	/** Called when player press the main menu button */
	void MainMenu();

#pragma endregion		

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Properties | Loadout System")
	FPlayerLoadout PlayerLoadout;
	
private:
	/** The actual mesh of the character that will be real-time retargeted from the parent skeletal mesh component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties | Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* RetargetedMesh;
	/** Default camera field of view value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties | Camera", meta = (AllowPrivateAccess = "true"))
	float CameraDefaultFOV;
	/** Field of view value for when zoomed in */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties | Camera", meta = (AllowPrivateAccess = "true"))
	float CameraZoomedFOV;
	/** Current field of view this frame */
	float CameraCurrentFOV;
	/** Interp speed for zooming when aiming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties | Camera", meta = (AllowPrivateAccess = "true"))
	float ZoomInterpSpeed;

	UFUNCTION(Server, Reliable)
	void ServerSprint();

	UFUNCTION(Server, Reliable)
	void ServerStopSprinting();
};
