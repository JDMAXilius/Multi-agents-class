#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Templates/SubclassOf.h"
#include "BNPlayerState.generated.h"

class UBNAbilitySet;
class UBNAbilitySystemComponent;
class UBNAttributeSet;
class UGameplayEffect;

/** Broadcast on the AUTHORITY when this player's pawn dies. The seam law 7 asks for: the death
 *  ability announces, and whoever cares subscribes — instead of the ability reaching into the
 *  GameMode by name. The PlayerState is the broadcaster because it is the persistent object and
 *  the one the GameMode already tracks per player.
 *
 *  Victim first, then killer. Killer may be NULL (world damage, disconnected mid-flight) and may
 *  EQUAL the victim (their own grenade) — both are real cases the subscriber decides how to word.
 *
 *  This block sits ABOVE the UCLASS() macro on purpose: UnrealHeaderTool requires the class
 *  definition to immediately follow UCLASS(), and anything between them stops the build. */
class ABNPlayerState;
DECLARE_MULTICAST_DELEGATE_TwoParams(FBNPlayerDeathSignature, ABNPlayerState* /*Victim*/, ABNPlayerState* /*Killer*/);

UCLASS()
class BREACHPOINTNEXT_API ABNPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ABNPlayerState();

	/** Authority only, and deliberately NOT a UPROPERTY delegate: subscribers are C++ systems
	 *  (GameMode now; killfeed and scoring later), and a death is announced once per life. */
	FBNPlayerDeathSignature OnPlayerDeath;

	/** Called by UBNGA_Death, which is what reads the killer off the attribute set's capture.
	 *  Nothing else should broadcast this. */
	void BroadcastDeath(ABNPlayerState* Killer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UBNAbilitySystemComponent* GetBNAbilitySystemComponent() const { return AbilitySystemComponent; }
	UBNAttributeSet* GetAttributeSet() const { return AttributeSet; }

	void GrantDefaults();

	/** The ONE way attributes reach their starting numbers — first life and every respawn.
	 *  Authority only. Nothing anywhere hand-sets Health, Shield or MoveSpeed. */
	void ApplyInitAttributes();

protected:
	UPROPERTY()
	TObjectPtr<UBNAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UBNAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UBNAbilitySet> DefaultAbilitySet;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> InitEffect;

	bool bDefaultsGranted = false;
};
