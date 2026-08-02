#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "BRPlayerController.generated.h"

class UBRAbilitySystemComponent;
class UBRInputConfig;
class UInputAction;
class UInputMappingContext;
class UUserWidget;

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

	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	// Ability verbs. The pawn owns Move/Look/Jump; everything that routes to the ASC is
	// bound here because the controller outlives the pawn across respawns.
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

	virtual void BeginPlay() override;

	virtual void SetupInputComponent() override;

	virtual void SetPawn(APawn* InPawn) override;

	bool ShouldUseTouchControls() const;

	// Binding happens once per possession, so the blocking load is survivable here.
	static const UInputAction* ResolveAction(const TSoftObjectPtr<UInputAction>& SoftAction, const TCHAR* VerbName);

	// The relay. Both hand the tag to the ASC, which owns the held-input buffer and decides
	// what activates; bots reach the same two functions through ABRBotController.
	void ActivateByInputTag(FGameplayTag InputTag);

	void ReleaseInputTag(FGameplayTag InputTag);

	void OnFirePressed();
	void OnFireReleased();
	void OnReloadPressed();
	void OnSwapPressed();
	void OnGrenadePressed();
	void OnGrenadeReleased();
	void OnMeleePressed();
	void OnGrapplePressed();
	void OnSprintPressed();
	void OnSprintReleased();
};
