#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UITag.h"
#include "BNUITypes.generated.h"

/** The CommonUI layer tags — a SECOND registrar beside BNTags, because it has to be: the stacks
 *  key on CommonUI's FUITag, and BNTags' UE_DEFINE_GAMEPLAY_TAG macros produce plain
 *  FGameplayTags.
 *
 *  THE BODIES ARE ROOT-RELATIVE (critic, R7 W0): FUITag::AddNativeTag PREPENDS its "UI." root,
 *  so TEXT("Layer.Game") registers as UI.Layer.Game — passing a full "UI.Layer.Game" here would
 *  register the garbage UI.UI.Layer.Game. These therefore land on the SAME strings the old
 *  module registers, which is fine and deliberate: same-string native registration dedupes
 *  without complaint (the original collision fear was wrong twice — wrong strings, wrong
 *  hazard). Shape transcribed from the compiled reference (ROADMAP-7 §API). */
struct FBNUITags : public FGameplayTagNativeAdder
{
	FUITag Layer_Game;
	FUITag Layer_GameMenu;
	FUITag Layer_Menu;
	FUITag Layer_Modal;

	virtual void AddTags() override
	{
		Layer_Game     = FUITag::AddNativeTag(TEXT("Layer.Game"));
		Layer_GameMenu = FUITag::AddNativeTag(TEXT("Layer.GameMenu"));
		Layer_Menu     = FUITag::AddNativeTag(TEXT("Layer.Menu"));
		Layer_Modal    = FUITag::AddNativeTag(TEXT("Layer.Modal"));
	}

	static const FBNUITags& Get() { return Singleton; }

private:
	static BREACHPOINTNEXT_API FBNUITags Singleton;
};

/** Presentation timing shared by the ViewModel (expiry) and the director (the join-in-progress
 *  age filter): a joiner must not receive last-few-minutes kills as a burst of fresh lines, so
 *  the director skips ring entries older than this — the same number that would have expired
 *  them had the client been present. */
namespace BNUITiming
{
	inline constexpr double KillfeedLingerSeconds = 6.0;
}

/** Honest-unknown, the doctrine's load-bearing enum: a ViewModel field is Unknown until its
 *  DENOMINATORS are known (MaxHealth landed, the match state read once), and a widget renders
 *  dashes at Unknown — never a confident 0/100. The frames a mid-match joiner sees are the ones
 *  nobody tests; this enum is how they stay honest. */
UENUM()
enum class EBNUIDataState : uint8
{
	Unknown,
	Live,
	Stale
};

/** One killfeed line as the VIEW knows it. The wording is composed ONCE, where the relationship
 *  facts are in hand — the widget renders text and a tint and decides nothing. ExpiryTime is
 *  LOCAL world seconds, stamped on the entry itself (a parallel array desyncs — the old module
 *  proved it), and deliberately not server-synced: how long a line lingers is presentation. */
USTRUCT()
struct FBNKillfeedViewEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FText Line;

	/** The server's monotonic id — the dedupe key when a ring re-replicates. */
	UPROPERTY()
	int32 Sequence = INDEX_NONE;

	UPROPERTY()
	double ExpiryTime = 0.0;

	/** White row: the local player is in this line (the one colour that means "you" in a list). */
	UPROPERTY()
	bool bInvolvesSelf = false;
};

/** The palette, as SEMANTICS — one hue, one meaning, everywhere. C++ constants because a WBP
 *  never types a hex (twelve hand-typed copies is twelve places a rebrand breaks silently and
 *  the critic can diff none of them). Inherited from the old module's VISR-derived system:
 *  red is THREAT and is never spent on a warning that is not lethal; health is yellow, never
 *  green — green reads "fine" at exactly the moment the player is one shot from dead. */
namespace BNUIColors
{
	/** You: your shields, your bars, your reticle at rest. */
	inline const FLinearColor Shield  { 0.043f, 0.663f, 0.898f };  // #35D0F2
	/** Health beneath shields. Yellow, never green. */
	inline const FLinearColor Health  { 0.961f, 0.773f, 0.259f };  // #F5C542
	/** A clock is running: respawn countdown, match clock in its last stretch. */
	inline const FLinearColor Amber   { 1.000f, 0.639f, 0.200f };  // #FFA333
	/** Threat, only: incoming damage, the enemy. Never low-ammo, never UI errors. */
	inline const FLinearColor Threat  { 1.000f, 0.290f, 0.239f };  // #FF4A3D
	/** You, in a list of people: your killfeed lines, your scoreboard row. */
	inline const FLinearColor Self    { 1.000f, 1.000f, 1.000f };  // #FFFFFF
	/** Everyone else's history: killfeed lines you are not in, other rows. */
	inline const FLinearColor InkDim  { 0.514f, 0.592f, 0.663f };  // #8397A9
	/** Dead/dimmed: the Unknown state's dashes, a departed player's row. */
	inline const FLinearColor Dead    { 0.290f, 0.353f, 0.420f };  // #4A5A6B
}
