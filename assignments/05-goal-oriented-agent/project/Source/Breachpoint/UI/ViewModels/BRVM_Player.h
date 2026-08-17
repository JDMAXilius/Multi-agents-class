#pragma once

#include "MVVMViewModelBase.h"
#include "UI/BRUITypes.h"

#include "BRVM_Player.generated.h"

class UTexture2D;

/**
 * Local-player presence, as the Profile Bar and every roster-adjacent screen show it.
 *
 * `Unknown` is the boot value and it is load-bearing: Steam identity, the save profile and the
 * PlayerState all arrive on different frames (`ue5-ui-architecture` Sec 7), so "not yet known"
 * must be distinguishable from "known to be offline".
 */
UENUM(BlueprintType)
enum class EBRPlayerPresence : uint8
{
	Unknown,

	Offline,

	Online,

	InLobby,

	InMatch
};

/**
 * `UBRVM_Player` -- SCREEN-MANIFEST Sec 10 gap G1 (blocks 8 screens, gates Wave 1).
 *
 * The local player's identity, career progression and wallet. Everything a screen needs to draw
 * the Profile Bar, the Progression Button and the currency rows.
 *
 * WHAT THIS CLASS DELIBERATELY DOES NOT DO:
 * It never reaches for `APlayerState`, an ASC, a controller or the GameState. There is no
 * producer for these fields yet (three contract gaps -- identity, progression, wallet); when one
 * exists it calls the `Set*` feeds below. A ViewModel that pulls from the world on construction
 * reads garbage on the frames a late joiner actually sees, and there is no Tick here to correct
 * it later (law 4).
 *
 * EVERY FIELD BOOTS TO AN EXPLICIT UNKNOWN. Numbers boot to `INDEX_NONE`, text to empty, states
 * to `EBRUIDataState::Unknown`. A confident `0` credits or `Rank 0` on the first frame is a lie
 * about the player's account, so widgets bind the matching `*State` field and render dashes.
 *
 * Three independent `EBRUIDataState` fields, not one, because the three feeds are independent:
 * Steam can hand over a gamertag long before the save profile has resolved the wallet.
 */
UCLASS(BlueprintType, DisplayName = "BR Player Viewmodel")
class BREACHPOINT_API UBRVM_Player : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:

	// -------------------------------------------------------------------------------------
	// Feeds. Each one is the ONLY way its fields change; a future identity/profile subsystem
	// calls these. Separate feeds because the data arrives separately -- a Steam avatar
	// resolves asynchronously well after the gamertag does.
	// -------------------------------------------------------------------------------------

	void SetGamertag(const FText& InGamertag);

	void SetEmblem(const TSoftObjectPtr<UTexture2D>& InEmblem);

	void SetPresence(EBRPlayerPresence InPresence);

	/**
	 * One call, because rank name / index / XP are one atomic truth: publishing them separately
	 * lets a widget render rank N's name over rank N+1's XP bar for a frame.
	 *
	 * `InXpToNextRank` is the size of the current rank's band, not a running total.
	 */
	void SetCareerRank(const FText& InRankName, int32 InRankIndex, int32 InXpIntoRank, int32 InXpToNextRank);

	void SetBalances(int32 InCredits, int32 InTokens);

	/**
	 * Travel, sign-out-in-progress, or a lost backend connection: what is on screen was true and
	 * is no longer being refreshed. Values are kept so the UI does not flash empty; the state
	 * fields tell widgets to dim. Unknown stays Unknown -- stale is a claim about data that once
	 * existed.
	 */
	void MarkStale();

	void ClearToUnknown();

	EBRUIDataState GetIdentityState() const { return IdentityState; }
	FText GetGamertag() const { return Gamertag; }
	TSoftObjectPtr<UTexture2D> GetEmblem() const { return Emblem; }
	EBRPlayerPresence GetPresence() const { return Presence; }

	EBRUIDataState GetProgressionState() const { return ProgressionState; }
	FText GetCareerRankName() const { return CareerRankName; }
	int32 GetCareerRankIndex() const { return CareerRankIndex; }
	int32 GetXpIntoRank() const { return XpIntoRank; }
	int32 GetXpToNextRank() const { return XpToNextRank; }

	/**
	 * DERIVED, never stored -- the `UBRVM_Match` clock precedent: one source of truth (the two XP
	 * numbers), the presentation value computed on read. A stored percent is a second source that
	 * can disagree with the numerals printed next to it.
	 *
	 * Returns 0 while progression is Unknown; widgets gate the bar on `GetProgressionState()`
	 * rather than treating an empty bar as "no progress".
	 */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Breachpoint|Player")
	float GetRankProgressPercent() const;

	EBRUIDataState GetWalletState() const { return WalletState; }
	int32 GetCredits() const { return Credits; }
	int32 GetTokens() const { return Tokens; }

private:

	// -------------------------------------------------------------------------------------
	// Identity -- fed by Steam identity + PlayerState.
	// -------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetIdentityState", Category = "Breachpoint|Player", meta = (AllowPrivateAccess))
	EBRUIDataState IdentityState = EBRUIDataState::Unknown;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetGamertag", Category = "Breachpoint|Player", meta = (AllowPrivateAccess))
	FText Gamertag;

	/** SOFT (law 3). The widget resolves it; a null soft ptr is the honest "no emblem yet" state. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetEmblem", Category = "Breachpoint|Player", meta = (AllowPrivateAccess))
	TSoftObjectPtr<UTexture2D> Emblem;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetPresence", Category = "Breachpoint|Player", meta = (AllowPrivateAccess))
	EBRPlayerPresence Presence = EBRPlayerPresence::Unknown;

	// -------------------------------------------------------------------------------------
	// Career progression -- fed by the save profile / progression service.
	// -------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetProgressionState", Category = "Breachpoint|Player", meta = (AllowPrivateAccess))
	EBRUIDataState ProgressionState = EBRUIDataState::Unknown;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetCareerRankName", Category = "Breachpoint|Player", meta = (AllowPrivateAccess))
	FText CareerRankName;

	/** Rank ladder position. `INDEX_NONE` = not yet known, which is NOT rank 0. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetCareerRankIndex", Category = "Breachpoint|Player", meta = (AllowPrivateAccess))
	int32 CareerRankIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetXpIntoRank", Category = "Breachpoint|Player", meta = (AllowPrivateAccess))
	int32 XpIntoRank = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetXpToNextRank", Category = "Breachpoint|Player", meta = (AllowPrivateAccess))
	int32 XpToNextRank = INDEX_NONE;

	// -------------------------------------------------------------------------------------
	// Wallet -- fed by the store / entitlement service.
	// -------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetWalletState", Category = "Breachpoint|Player", meta = (AllowPrivateAccess))
	EBRUIDataState WalletState = EBRUIDataState::Unknown;

	/** `INDEX_NONE` = unknown. Showing 0 to a player who owns 12,000 is the worst UI lie here. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetCredits", Category = "Breachpoint|Player", meta = (AllowPrivateAccess))
	int32 Credits = INDEX_NONE;

	/** The premium/earned token balance (the reference file's "Marks"). `INDEX_NONE` = unknown. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetTokens", Category = "Breachpoint|Player", meta = (AllowPrivateAccess))
	int32 Tokens = INDEX_NONE;
};
