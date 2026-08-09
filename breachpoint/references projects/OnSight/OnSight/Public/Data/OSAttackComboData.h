#pragma once
// Combo/attack data: FOSAbilityID, FOSAttackExtraData, FOSAttackHitInfo. Data-only until combo system is driven from code.

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Data/OSCombatTypes.h"
#include "Data/OSHitDamageContext.h"
#include "Data/OSAbilityCostAndEffects.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "OSAttackComboData.generated.h"

// --- FOSAbilityID ---
USTRUCT(BlueprintType)
struct FOSAbilityID
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag AbilityTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Index = 1;

	bool IsValid() const { return AbilityTag.IsValid() && Index > 0; }
	bool operator==(const FOSAbilityID& Other) const { return AbilityTag == Other.AbilityTag && Index == Other.Index; }
};

FORCEINLINE uint32 GetTypeHash(const FOSAbilityID& Id)
{
	uint32 Hash = GetTypeHash(Id.AbilityTag);
	Hash = HashCombine(Hash, ::GetTypeHash(Id.Index));
	return Hash;
}

// --- FOSAttackExtraData ---
USTRUCT(BlueprintType)
struct FOSAttackExtraData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FOSAbilityID> AllowedPredecessors;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Priority = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float RandomWeight = 1.0f;
};

// --- FOSAttackHitInfo ---
USTRUCT(BlueprintType)
struct FOSAttackHitInfo
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EOSAttackType Type = EOSAttackType::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsClashing = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (UIMin = "0", UIMax = "360", ClampMin = "0", ClampMax = "360"))
	int32 ClashGraceAngle = 135;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MaxHitDistance = 300;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FOSGameplayEffectWrapper> Effects;
};
