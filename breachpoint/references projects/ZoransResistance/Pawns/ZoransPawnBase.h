// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "ZoransResistance/Teams/TeamStateInterface.h"
#include "ZoransPawnBase.generated.h"

class UZoransGameplayEffect;
class UZoransGameplayAbility;
class UZoransAttributeSet;
class UZoransAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPawnReadyForPlayDelegate);

UCLASS()
class ZORANSRESISTANCE_API AZoransPawnBase : public APawn, public IAbilitySystemInterface, public ITeamStateInterface
{
	GENERATED_BODY()

public:
	
	AZoransPawnBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TObjectPtr<UZoransAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability System")
	TArray<TSubclassOf<UZoransAttributeSet>> DefaultAttributeSets;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability System")
	TArray<TSubclassOf<UZoransGameplayAbility>> DefaultGameplayAbilities;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability System")
	TArray<TSubclassOf<UZoransGameplayEffect>> DefaultEffects;

	virtual void SetTeamAssignment(int32 InIndex) override;

	virtual int32 GetTeamAssignment() const override;
	
	UPROPERTY(BlueprintAssignable)
	FPawnReadyForPlayDelegate OnPawnReadyForPlay;

	UFUNCTION(BlueprintImplementableEvent)
	void On_PawnReadyForPlay();

protected:

	void BeginPlay() override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_TeamAssignment", Meta = (ExposeOnSpawn = true))
	int32 TeamAssignment = -1;

	UFUNCTION(BlueprintImplementableEvent)
	void OnRep_TeamAssignment();
	
	bool bReadyForPlay = false;
	
	void InitAbilitySystem();
};