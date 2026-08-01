// Breachpoint. Shared UI vocabulary: layer tags, data states, hit markers, killfeed rows.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "UITag.h"

#include "BRUITypes.generated.h"

/**
 * BRUITypes — the vocabulary every other file in UI/ speaks.
 *
 * Specification: BREACHPOINT-ARCHITECTURE.md 3.9. This file is NOT one of the four named
 * units; it exists because those four all need the same enums/structs and a header cycle is
 * the alternative. Nothing here has behaviour.
 *
 * TAG NOTE (contract_gap, filed in BP10's Log): 3.1 enumerates no `UI.*` family, so the four
 * layer tags below are declared here as CommonUI's typed `FUITag` rather than in
 * Core/BRGameplayTags.h. They CANNOT live there: `UE_DECLARE_GAMEPLAY_TAG_EXTERN` produces a
 * plain `FGameplayTag`, and CommonUI's typed-tag machinery requires the `FUITag` subtype
 * (see UITag.h, END_UI_TAG_DECL). If 3.1 is ever amended to enumerate `UI.Layer.*`, the
 * amendment must say "FUITag" or it will not compile.
 */

// ---------------------------------------------------------------------------
// Layer tags
// ---------------------------------------------------------------------------

/**
 * The four CommonUI layers, added natively at startup (no DefaultGameplayTags.ini entry).
 *
 * Order below IS the z-order and IS the input-routing precedence:
 *   UI.Layer.Game      HUD. Always resident while a match is live. Input mode Game.
 *   UI.Layer.GameMenu  In-match overlays that do not stop the world (scoreboard, death cam,
 *                      carnage report). Input mode All - the player is still being shot at.
 *   UI.Layer.Menu      Front end and full-screen flows. Input mode Menu.
 *   UI.Layer.Modal     Confirmations and errors. Input mode Menu, modal focus trap.
 *
 * ARCHITECTURE 3.9 and the ue5-ui-architecture skill both say THREE layers
 * (GameHUD/Menu/Modal). The BP10 packet says four (game / game-menu / menu / modal). Four is
 * implemented because the death overlay and carnage report are in-match screens that must sit
 * above the HUD without taking Menu-only input; folding them into Menu would deafen the game
 * layer during a live respawn. Recorded as a doc delta, not a silent choice.
 */
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

// ---------------------------------------------------------------------------
// Honest empty states (skill 7 / netcode.md law 7)
// ---------------------------------------------------------------------------

/**
 * Every ViewModel feed carries one of these ALONGSIDE its numbers, and the WBP switches on it
 * before it renders a single digit.
 *
 * The rule this enum exists to make un-forgettable: a late joiner's PlayerState is null for
 * one or more frames, the ASC has not run InitAbilityActorInfo, and GameState arrives before
 * team assignment. During those frames Health is 0.0f because C++ zero-initialises floats, and
 * `0 / 100` on a HUD is a LIE about a fight the player is currently in. Unknown means "render
 * dashes"; it is never a value.
 */
UENUM(BlueprintType)
enum class EBRUIDataState : uint8
{
	/** No data has ever arrived on this feed. Render placeholders (dashes, dimmed), never zeros. */
	Unknown,

	/** Data has arrived and the source is still alive. The numbers mean what they say. */
	Live,

	/** The source went away (death, seamless travel, host loss). Numbers are the LAST KNOWN ones. */
	Stale
};

// ---------------------------------------------------------------------------
// Hit markers -- gameplay information, not decoration
// ---------------------------------------------------------------------------

/**
 * Founder directive (BP10): shield-vs-flesh hit markers are gameplay information. The player
 * decides whether to keep pressure or break contact on this signal, so it must be as distinct
 * and as trustworthy as an ammo counter.
 *
 * These are modelled as DISTINCT EVENTS on UBRVM_Combat (one FMVVMEventField each) rather than
 * one event with a payload, so the WBP layer is a pure visual switch: each event drives one
 * animation. A WBP that branches on a payload enum would be gameplay logic in a widget graph
 * (law 7 / data-and-assets.md), which is exactly what this shape forbids.
 */
UENUM(BlueprintType)
enum class EBRHitMarkerKind : uint8
{
	None,

	/** Damage landed on shields. */
	Shield,

	/** Damage landed on health -- shields are down. This is the "press" signal. */
	Flesh,

	/** Headshot multiplier applied (Damage.Headshot). Louder than Flesh. */
	Headshot,

	/** The target died from this hit. Supersedes the others; never fires alongside them. */
	Kill
};

// ---------------------------------------------------------------------------
// Killfeed
// ---------------------------------------------------------------------------

/**
 * One killfeed row. Built by gameplay (authority-side ring buffer on GameState, replicated),
 * handed to UBRVM_Match::PushKillfeedEntry on the client that renders it.
 *
 * All text is already resolved to FText by the pusher: the UI does no name lookup, because a
 * name lookup from a widget is a poll of PlayerState and PlayerState is the thing that is null
 * on the frames that matter.
 */
USTRUCT(BlueprintType)
struct FBRKillfeedEntry
{
	GENERATED_BODY()

	/** Monotonic id assigned by the pusher. Used to append a Spotter line to an existing row. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	int32 SequenceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText KillerName;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText VictimName;

	/** One of BRGameplayTags Damage.* (Kinetic / Explosive / Melee / Headshot / Rear). */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FGameplayTag DamageTag;

	/** 255 == unknown. Do not colour a row whose team is unknown; render it neutral. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	uint8 KillerTeamId = 255;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	uint8 VictimTeamId = 255;

	/** True if the local player killed or died. Drives emphasis only. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	bool bLocalPlayerInvolved = false;

	/**
	 * Spotter flavour line. MAY BE EMPTY FOREVER and that is a supported, tested state:
	 * offline or LLM-down means an identical HUD minus flavour. The WBP reserves the slot and
	 * renders it empty; it never collapses layout and never waits.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText SpotterLine;
};

// ---------------------------------------------------------------------------
// The ASC seam
// ---------------------------------------------------------------------------

/**
 * The attributes UBRVM_Combat subscribes to, supplied BY THE CALLER.
 *
 * Why by the caller and not by including BRAttributeSet.h: BP02 owns AbilitySystem/ and is
 * being written in parallel with this packet, and the UI layer has no business naming a
 * concrete attribute set anyway - a bot ASC, a spectated ASC, and a future second attribute
 * set all feed the same HUD. The caller does:
 *
 *     FBRCombatAttributeBindings B;
 *     B.Health = UBRAttributeSet::GetHealthAttribute();   // etc
 *     VM->BindToAbilitySystem(ASC, B);
 *
 * Any member left default (invalid) is simply not subscribed; it is not an error. That is what
 * lets HUD v1 ship against BP03's partial attribute set.
 */
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

	/** True if at least one attribute is bindable. An all-invalid struct leaves the feed Unknown. */
	bool HasAny() const
	{
		return Health.IsValid() || MaxHealth.IsValid() || Shields.IsValid() || MaxShields.IsValid();
	}
};
