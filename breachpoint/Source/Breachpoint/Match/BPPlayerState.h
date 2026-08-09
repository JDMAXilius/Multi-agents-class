#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

#include "BPPlayerState.generated.h"

class UAbilitySystemComponent;
class UBRAbilitySystemComponent;

/**
 * BP PlayerState — C++ only. UBRAbilitySystemComponent so the controller can use the
 * held-input buffer (AbilityInputTagPressed/Released) like ABRPlayerController.
 */
UCLASS(config = Game, meta = (DisplayName = "BP Player State"))
class BREACHPOINT_API ABPPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	ABPPlayerState(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UBRAbilitySystemComponent* GetBRAbilitySystemComponent() const { return AbilitySystemComponent; }

	int32 GetKills() const { return Kills; }
	int32 GetDeaths() const { return Deaths; }

	void AddKill();
	void AddDeath();

protected:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties | Abilities")
	TObjectPtr<UBRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Properties | Score")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Properties | Score")
	int32 Deaths = 0;
};
