#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "BRGameplayEffects.generated.h"

class UAbilitySystemComponent;
struct FGameplayEffectContextHandle;
struct FGameplayEffectSpecHandle;

UCLASS(meta = (DisplayName = "GE_RecentDamage"))
class BREACHPOINT_API UBRGE_RecentDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_RecentDamage();

	static const FName DurationSetByCallerName;
};

UCLASS(meta = (DisplayName = "GE_Damage"))
class BREACHPOINT_API UBRGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_Damage();

	static FGameplayEffectSpecHandle MakeSpec(UAbilitySystemComponent* SourceASC, float BaseDamage, const FGameplayTagContainer& DamageTags, const FGameplayEffectContextHandle& Context);

	static bool ApplyToTarget(const FGameplayEffectSpecHandle& SpecHandle, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC);
};

UCLASS(meta = (DisplayName = "GE_Regen"))
class BREACHPOINT_API UBRGE_Regen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_Regen();
};

UCLASS(meta = (DisplayName = "GE_Cooldown"))
class BREACHPOINT_API UBRGE_Cooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_Cooldown();
};

UCLASS(meta = (DisplayName = "GE_InitStats"))
class BREACHPOINT_API UBRGE_InitStats : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_InitStats();

	static const FName MaxHealthName;
	static const FName MaxShieldsName;
	static const FName HealthName;
	static const FName ShieldsName;

	static const FName MaxGrenadesName;
	static const FName GrenadesName;

	static const FName MoveSpeedBaseName;
	static const FName SprintSpeedMultiplierName;
};

UCLASS(meta = (DisplayName = "GE_Death"))
class BREACHPOINT_API UBRGE_Death : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_Death();
};

UCLASS(meta = (DisplayName = "GE_ShieldsBroken"))
class BREACHPOINT_API UBRGE_ShieldsBroken : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_ShieldsBroken();
};

UCLASS(meta = (DisplayName = "GE_GrenadeCost"))
class BREACHPOINT_API UBRGE_GrenadeCost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBRGE_GrenadeCost();

	static const FName CostSetByCallerName;

	static const FName GrenadeCountAttributeName;

	static FGameplayAttribute ResolveGrenadeCountAttribute();

	static bool IsOperational();

	static FGameplayEffectSpecHandle MakeSpec(UAbilitySystemComponent* SpenderASC, float GrenadesPerThrow, const FGameplayEffectContextHandle& Context);
};
