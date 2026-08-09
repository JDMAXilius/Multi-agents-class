#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "BPPlayerController.generated.h"

class UBRAbilitySystemComponent;
class UInputAction;
class UInputMappingContext;

/**
 * BP PlayerController — patterned on ABRPlayerController.
 * Pawn owns Move/Look; this controller owns ability verbs (incl. Jump) via ASC input tags.
 * Soft IMC + soft IA paths from DefaultGame.ini — no editor graph required.
 */
UCLASS(config = Game, meta = (DisplayName = "BP Player Controller"))
class BREACHPOINT_API ABPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	ABPPlayerController();

	/** PlayerState first, possessed pawn second. Null-check callers. */
	UBRAbilitySystemComponent* GetBRAbilitySystemComponent() const;

protected:

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputMappingContext> MouseLookMappingContext;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> SwapAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> GrenadeAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> MeleeAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> GrappleAction;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Properties | Input")
	TSoftObjectPtr<UInputAction> SprintAction;

	virtual void SetupInputComponent() override;
	virtual void SetPawn(APawn* InPawn) override;

	void LogKeyCensus(const TArray<const UInputAction*>& BoundActions) const;

	static const UInputAction* ResolveAction(const TSoftObjectPtr<UInputAction>& SoftAction, const TCHAR* VerbName);

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

private:

	/** Resolved IMC pointers for key census (same frame as SetupInputComponent). */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> ResolvedDefaultMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> ResolvedMouseLookMappingContext;
};
