#include "UI/ViewModels/BRVM_Player.h"

void UBRVM_Player::SetGamertag(const FText& InGamertag)
{
	UE_MVVM_SET_PROPERTY_VALUE(Gamertag, InGamertag);
	UE_MVVM_SET_PROPERTY_VALUE(IdentityState, EBRUIDataState::Live);
}

void UBRVM_Player::SetEmblem(const TSoftObjectPtr<UTexture2D>& InEmblem)
{
	UE_MVVM_SET_PROPERTY_VALUE(Emblem, InEmblem);
	UE_MVVM_SET_PROPERTY_VALUE(IdentityState, EBRUIDataState::Live);
}

void UBRVM_Player::SetPresence(EBRPlayerPresence InPresence)
{
	UE_MVVM_SET_PROPERTY_VALUE(Presence, InPresence);
}

void UBRVM_Player::SetCareerRank(const FText& InRankName, int32 InRankIndex, int32 InXpIntoRank, int32 InXpToNextRank)
{
	UE_MVVM_SET_PROPERTY_VALUE(CareerRankName, InRankName);
	UE_MVVM_SET_PROPERTY_VALUE(CareerRankIndex, InRankIndex);
	UE_MVVM_SET_PROPERTY_VALUE(XpIntoRank, InXpIntoRank);
	UE_MVVM_SET_PROPERTY_VALUE(XpToNextRank, InXpToNextRank);

	// The derived percent has no backing field, so nothing above broadcasts it. Without this the
	// XP bar is the classic "UI doesn't refresh" bug while the numerals beside it update.
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetRankProgressPercent);

	UE_MVVM_SET_PROPERTY_VALUE(ProgressionState, EBRUIDataState::Live);
}

void UBRVM_Player::SetBalances(int32 InCredits, int32 InTokens)
{
	UE_MVVM_SET_PROPERTY_VALUE(Credits, InCredits);
	UE_MVVM_SET_PROPERTY_VALUE(Tokens, InTokens);
	UE_MVVM_SET_PROPERTY_VALUE(WalletState, EBRUIDataState::Live);
}

float UBRVM_Player::GetRankProgressPercent() const
{
	if (ProgressionState == EBRUIDataState::Unknown || XpIntoRank < 0 || XpToNextRank <= 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(static_cast<float>(XpIntoRank) / static_cast<float>(XpToNextRank), 0.0f, 1.0f);
}

void UBRVM_Player::MarkStale()
{
	if (IdentityState == EBRUIDataState::Live)
	{
		UE_MVVM_SET_PROPERTY_VALUE(IdentityState, EBRUIDataState::Stale);
	}
	if (ProgressionState == EBRUIDataState::Live)
	{
		UE_MVVM_SET_PROPERTY_VALUE(ProgressionState, EBRUIDataState::Stale);
	}
	if (WalletState == EBRUIDataState::Live)
	{
		UE_MVVM_SET_PROPERTY_VALUE(WalletState, EBRUIDataState::Stale);
	}
}

void UBRVM_Player::ClearToUnknown()
{
	UE_MVVM_SET_PROPERTY_VALUE(Gamertag, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(Emblem, TSoftObjectPtr<UTexture2D>());
	UE_MVVM_SET_PROPERTY_VALUE(Presence, EBRPlayerPresence::Unknown);
	UE_MVVM_SET_PROPERTY_VALUE(IdentityState, EBRUIDataState::Unknown);

	UE_MVVM_SET_PROPERTY_VALUE(CareerRankName, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(CareerRankIndex, static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(XpIntoRank, static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(XpToNextRank, static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(ProgressionState, EBRUIDataState::Unknown);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetRankProgressPercent);

	UE_MVVM_SET_PROPERTY_VALUE(Credits, static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(Tokens, static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(WalletState, EBRUIDataState::Unknown);
}
