#pragma once

#include "CoreMinimal.h"
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

	/**
	 * LOCAL wall-clock (`World->GetTimeSeconds()`) at which this entry leaves the feed, stamped
	 * by `UBRVM_Match::PushKillfeedEntry`. On the entry rather than in a parallel array — the
	 * old `KillfeedExpiryTimes` was hand-kept index-parallel behind guards that hid desync
	 * (HUD-CPP-AUDIT). Local time on purpose: a display decay must not be server-synced, so the
	 * same line leaves a laggy client slightly later than the host, which is intentional.
	 */
	double ExpiryTime = 0.0;
};

// FBRCombatAttributeBindings moved to BRViewModels.h (HUD-CPP-AUDIT): it had exactly two
// consumers, both in that file's classes, while its AttributeSet.h include rode this header
// into effectively every UI translation unit through the three hub headers that include it.
