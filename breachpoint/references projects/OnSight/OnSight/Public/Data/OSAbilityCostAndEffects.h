#pragma once
// Ability cost and effect wiring: FOSResource (attribute + amount), FOSGameplayEffectWrapper, FOSSpellTriggerPayload. Used by OSGameplayAbility and AbilityHelper.

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "OSAbilityCostAndEffects.generated.h"

// --- FOSResource ---
USTRUCT(BlueprintType)
struct FOSResource
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttribute Attribute;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Amount = 0.f;
};

// --- FOSGameplayEffectWrapper ---
USTRUCT(BlueprintType)
struct FOSOptionalFloat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bEnabled", EditConditionHides))
	float Value = 0.f;
};

	UENUM(BlueprintType)
	enum class EOSEffectType : uint8
	{
		OS_INSTANT	 UMETA(DisplayName = "Instant"),
		OS_DURATION  UMETA(DisplayName = "Duration"),
		OS_INFINITE  UMETA(DisplayName = "Infinite")
	};

USTRUCT(BlueprintType)
struct FOSGameplayEffectWrapper
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UOSGameplayEffect> Effect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bApplyToSelf = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bApplyToOther = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EOSEffectType Type = EOSEffectType::OS_INSTANT;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FOSOptionalFloat Potency;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type != EOSEffectType::OS_INSTANT", EditConditionHides))
	FOSOptionalFloat Period;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == EOSEffectType::OS_DURATION", EditConditionHides))
	FOSOptionalFloat Duration;
	
	bool IsValid() const
	{
		return (bApplyToOther || bApplyToSelf) && *Effect;
	}
};

// --- EOSSpellTrigger / FOSSpellTriggerPayload (used by projectiles) ---
UENUM(BlueprintType)
enum class EOSSpellTrigger : uint8
{
	OnHit,
	OnExpire,
	OnDestroy
};

USTRUCT(BlueprintType)
struct FOSSpellTriggerPayload
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EOSSpellTrigger Trigger = EOSSpellTrigger::OnHit;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FOSGameplayEffectWrapper> Effects;
};

