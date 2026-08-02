// Breachpoint. Shared UI vocabulary: layer tags, data states, hit markers, killfeed rows.
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "UITag.h"

#include "BRUITypes.generated.h"

struct FBRUITags : public FGameplayTagNativeAdder
{
	FUITag Layer_Game;
	FUITag Layer_GameMenu;
	FUITag Layer_Menu;
	FUITag Layer_Modal;

	virtual void AddTags() override
	{
		Layer_Game = FUITag::AddNativeTag(TEXT("Layer.Game"));
		Layer_GameMenu = FUITag::AddNativeTag(TEXT("Layer.GameMenu"));
		Layer_Menu = FUITag::AddNativeTag(TEXT("Layer.Menu"));
		Layer_Modal = FUITag::AddNativeTag(TEXT("Layer.Modal"));
	}

	static const FBRUITags& Get() { return Singleton; }

private:
	static BREACHPOINT_API FBRUITags Singleton;
};

UENUM(BlueprintType)
enum class EBRUIDataState : uint8
{
	Unknown,

	Live,

	Stale
};

UENUM(BlueprintType)
enum class EBRHitMarkerKind : uint8
{
	None,

	Shield,

	Flesh,

	Headshot,

	Kill
};

USTRUCT(BlueprintType)
struct FBRKillfeedViewEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	int32 SequenceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText KillerName;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText VictimName;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FGameplayTag DamageTag;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	uint8 KillerTeamId = 255;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	uint8 VictimTeamId = 255;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	bool bLocalPlayerInvolved = false;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText SpotterLine;
};

USTRUCT(BlueprintType)
struct FBRCombatAttributeBindings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FGameplayAttribute Health;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FGameplayAttribute MaxHealth;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FGameplayAttribute Shields;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FGameplayAttribute MaxShields;

	bool HasAny() const
	{
		return Health.IsValid() || MaxHealth.IsValid() || Shields.IsValid() || MaxShields.IsValid();
	}
};
