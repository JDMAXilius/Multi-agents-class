#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "BRPlayerController.generated.h"

class UBRAbilitySystemComponent;
class UInputAction;
class UInputMappingContext;

UCLASS(config="Game")
class BREACHPOINT_API ABRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABRPlayerController();

	// PlayerState first, possessed pawn second. Null between spawn and PlayerState
	// replication, which is a real window on a joining client, so callers null-check.
	UBRAbilitySystemComponent* GetBRAbilitySystemComponent() const;

protected:

	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	// Was MobileExcludedMappingContexts; renamed with a PropertyRedirect in DefaultEngine.ini so
	// PC_BR keeps the value it already holds - IMC_MouseLook. Both lists are now added
	// unconditionally; the touch split they came from is gone.
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> AdditionalMappingContexts;

	// Ability verbs. The pawn owns Move and Look only; everything that routes to the ASC is
	// bound here because the controller outlives the pawn across respawns.
	UPROPERTY(EditDefaultsOnly, Config, Category="Input|Abilities")
	TSoftObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Config, Category="Input|Abilities")
	TSoftObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditDefaultsOnly, Config, Category="Input|Abilities")
	TSoftObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditDefaultsOnly, Config, Category="Input|Abilities")
	TSoftObjectPtr<UInputAction> SwapAction;

	UPROPERTY(EditDefaultsOnly, Config, Category="Input|Abilities")
	TSoftObjectPtr<UInputAction> GrenadeAction;

	UPROPERTY(EditDefaultsOnly, Config, Category="Input|Abilities")
	TSoftObjectPtr<UInputAction> MeleeAction;

	UPROPERTY(EditDefaultsOnly, Config, Category="Input|Abilities")
	TSoftObjectPtr<UInputAction> GrappleAction;

	UPROPERTY(EditDefaultsOnly, Config, Category="Input|Abilities")
	TSoftObjectPtr<UInputAction> SprintAction;

	virtual void SetupInputComponent() override;

	virtual void SetPawn(APawn* InPawn) override;

	// Answers the one question a successful bind cannot: does a KEY exist for it?
	void LogKeyCensus(const TArray<const UInputAction*>& BoundActions) const;

	// Binding happens once per possession, so the blocking load is survivable here.
	static const UInputAction* ResolveAction(const TSoftObjectPtr<UInputAction>& SoftAction, const TCHAR* VerbName);

	// The relay. Both hand the tag to the ASC, which owns the held-input buffer and decides
	// what activates; bots reach the same two functions through ABRBotController.
	void ActivateByInputTag(FGameplayTag InputTag);

	void ReleaseInputTag(FGameplayTag InputTag);

	void OnJumpPressed();
	void OnJumpReleased();
	void OnFirePressed();
	void OnFireReleased();
	void OnReloadPressed();
	void OnReloadReleased();
	void OnSwapPressed();
	void OnSwapReleased();
	void OnGrenadePressed();
	void OnGrenadeReleased();
	void OnMeleePressed();
	void OnMeleeReleased();
	void OnGrapplePressed();
	void OnGrappleReleased();
	void OnSprintPressed();
	void OnSprintReleased();
};
