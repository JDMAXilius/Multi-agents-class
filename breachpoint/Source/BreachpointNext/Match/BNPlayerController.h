#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BNPlayerController.generated.h"

class UBNAbilitySystemComponent;
class UBNInputConfig;
class UInputMappingContext;
struct FInputActionValue;

UCLASS(Config=Game)
class BREACHPOINTNEXT_API ABNPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// THE TESTING LEVERS, and they live on the CONTROLLER rather than on a UCheatManager for one
	// reason: APlayerController::AddCheats needs World->GetAuthGameMode(), which is null on a
	// client, so a client window never instantiates a cheat manager and every cheat typed there is
	// swallowed with no output at all. ProcessConsoleExec reaches a PlayerController's own execs
	// with no cheat manager and no EnableCheats, so these work from ANY window.
	//
	// Each one forwards itself to the server before it touches anything — damage is the
	// authority's alone. The BODIES are compiled out of shipping; the declarations are not
	// guarded, because UHT's handling of the preprocessor around UFUNCTION is not worth betting a
	// build on. Numbers are logged where they change, in UBNAttributeSet::PostGameplayEffectExecute.
	UFUNCTION(Exec)
	void BNDamageSelf(float Amount = 25.f);

	UFUNCTION(Exec)
	void BNKillSelf();

	UFUNCTION(Exec)
	void BNRefill();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// The controller's only asset references, and the only place input assets are named.
	// Soft + Config, so DefaultGame.ini sets them and no Blueprint child is needed.
	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UBNInputConfig> InputConfig;

	// Added at priority = index: ini order is context order.
	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TArray<TSoftObjectPtr<UInputMappingContext>> MappingContexts;

	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpPressed();
	void HandleJumpReleased();
	void HandleCrouchPressed();
	void HandleCrouchReleased();
	void HandleWeaponNextPressed();
	void HandleWeaponPreviousPressed();
	void HandleFirePressed();
	void HandleFireReleased();
	void HandleReloadPressed();
	void HandleSprintPressed();
	void HandleSprintReleased();
	void HandleLeanLeftPressed();
	void HandleLeanLeftReleased();
	void HandleLeanRightPressed();
	void HandleLeanRightReleased();
	void HandleDebugDamagePressed();

	UBNAbilitySystemComponent* GetBNAbilitySystemComponent() const;
};
