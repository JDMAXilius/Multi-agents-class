#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BNGameplayAbility.h"
#include "BNGA_Equip.generated.h"

class UBNEquipmentComponent;

/**
 * Equip · swap next · swap previous — one verb family, the BNMovementAbilities shape.
 *
 * ServerOnly, NOT LocalPredicted, and deliberately: CurrentIndex is a plain replicated int32
 * the server owns outright. A client that guessed it has no rollback — GAS predicts ability
 * execution, not replicated properties — and a rejected guess leaves the wrong weapon visible
 * and the wrong anim layer linked with no OnRep to correct it, because the server's value
 * never changed. Clients send the intent (the ASC RPCs a ServerOnly activation) and follow
 * through OnRep_CurrentIndex.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGA_Equip : public UBNGameplayAbility
{
	GENERATED_BODY()

public:
	UBNGA_Equip();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** The verb. The base equips EquipSlot when it names a real slot. */
	virtual void ExecuteEquip(UBNEquipmentComponent* Equipment);

	UPROPERTY(EditDefaultsOnly, Category = "BN")
	int32 EquipSlot = INDEX_NONE;
};

UCLASS()
class BREACHPOINTNEXT_API UBNGA_SwapNext : public UBNGA_Equip
{
	GENERATED_BODY()

protected:
	virtual void ExecuteEquip(UBNEquipmentComponent* Equipment) override;
};

UCLASS()
class BREACHPOINTNEXT_API UBNGA_SwapPrevious : public UBNGA_Equip
{
	GENERATED_BODY()

protected:
	virtual void ExecuteEquip(UBNEquipmentComponent* Equipment) override;
};
