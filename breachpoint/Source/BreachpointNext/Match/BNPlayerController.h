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
	ABNPlayerController();

protected:
	virtual void SetupInputComponent() override;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UBNInputConfig> InputConfig;

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
	void HandleADSPressed();
	void HandleADSReleased();
	void HandleMeleePressed();
	void HandleGrenadePressed();
	void HandleGrapplePressed();

	/** R7 — THE UI VERB: hold-to-show scoreboard, routed to the HUD director and NEVER to the
	 *  ASC. A UI verb is not an ability; forwarding it would log "NO granted ability carries
	 *  it" on every press, and the ASC has no business knowing the scoreboard exists. */
	void HandleScoreboardPressed();
	void HandleScoreboardReleased();

	/** R7.2 — opens the pause menu. Opening only: the menu closes itself. */
	void HandleMenuPressed();

public:
	/** Leave the match. The pause menu asks; the CONTROLLER travels — a widget never does.
	 *  Unset LeaveMatchMapPath is a designed miss: a loud warning and no travel, because a
	 *  half-guessed map path strands the player somewhere worse than the match. */
	void LeaveMatch();

protected:
	UPROPERTY(Config)
	FString LeaveMatchMapPath;

	UBNAbilitySystemComponent* GetBNAbilitySystemComponent() const;

	/** State.Dead replicates, so the machine that reads the input is the machine that refuses it. */
	bool IsDead() const;
};
