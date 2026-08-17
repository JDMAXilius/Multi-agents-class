#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UObject/SoftObjectPtr.h"

#include "Core/BRCore.h"

#include "BRCharacter.generated.h"

class UBRCharacterMovementComponent;
class UBREquipmentComponent;
class UBRInputConfig;
class UCameraComponent;
class UDataTable;
class UEnhancedInputComponent;
class UInputAction;
class USkeletalMeshComponent;
struct FInputActionValue;

UCLASS(config = Game, meta = (DisplayName = "BR Character"))
class BREACHPOINT_API ABRCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	ABRCharacter(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBREquipmentComponent> EquipmentComponent;

	UBREquipmentComponent* GetEquipment() const { return EquipmentComponent; }

protected:

	// Config, soft, and named by row: which gun a player spawns holding is a data choice,
	// and a hard DataTable ref on the character class would pull every weapon's mesh and
	// ability set into memory with the class itself.
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Equipment")
	TSoftObjectPtr<UDataTable> StartupWeaponTable;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Equipment")
	FName StartupWeaponRow;

	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

protected:

	void MoveInput(const FInputActionValue& Value);

	void LookInput(const FInputActionValue& Value);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

public:

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	USkeletalMeshComponent* GetMesh3P() const { return GetMesh(); }

	void InitializeAbilitySystem();
	UBRCharacterMovementComponent* GetBRCharacterMovement() const;

	// Server only. Idempotent: possession runs again on every respawn and on seamless
	// travel, and a second grant would throw away the weapon in hand with its live ammo.
	void GiveStartupWeapon();
};
