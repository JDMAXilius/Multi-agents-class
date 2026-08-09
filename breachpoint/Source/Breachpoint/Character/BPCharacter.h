#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UObject/SoftObjectPtr.h"

#include "BPCharacter.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UInputAction;
class USkeletalMeshComponent;
struct FInputActionValue;

/**
 * BP character — C++ only. Move/Look on the pawn; Jump and ability verbs live on
 * ABPPlayerController (BR split). Soft input paths from DefaultGame.ini.
 */
UCLASS(config = Game, meta = (DisplayName = "BP Character"))
class BREACHPOINT_API ABPCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties | Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties | Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCameraComponent;

protected:

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> MouseLookAction;

public:

	ABPCharacter(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	void InitializeAbilitySystem();

	UFUNCTION(BlueprintCallable, Category = "Properties | Input")
	virtual void DoAim(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category = "Properties | Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category = "Properties | Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category = "Properties | Input")
	virtual void DoJumpEnd();

protected:

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void MoveInput(const FInputActionValue& Value);
	void LookInput(const FInputActionValue& Value);

	static const UInputAction* ResolveAction(const TSoftObjectPtr<UInputAction>& SoftAction, const TCHAR* Name);
};
