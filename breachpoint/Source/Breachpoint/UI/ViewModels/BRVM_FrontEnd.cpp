#include "UI/ViewModels/BRVM_FrontEnd.h"

void UBRVM_FrontEnd::SetFeatureCards(const TArray<FBRFeatureCardEntry>& InCards)
{
	// Arrays are assigned then broadcast rather than pushed through UE_MVVM_SET_PROPERTY_VALUE:
	// that macro compares old against new, and these element structs define no operator==. This
	// is the shipped `UBRVM_Match::PushKillfeedEntry` pattern, for the same reason.
	FeatureCards = InCards;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FeatureCards);

	SetFocusedFeatureCardIndex(FeatureCards.Num() > 0 ? 0 : INDEX_NONE);

	UE_MVVM_SET_PROPERTY_VALUE(MenuState, EBRUIDataState::Live);
}

void UBRVM_FrontEnd::SetFocusedFeatureCardIndex(int32 InIndex)
{
	const int32 NewIndex = FeatureCards.IsValidIndex(InIndex) ? InIndex : static_cast<int32>(INDEX_NONE);
	UE_MVVM_SET_PROPERTY_VALUE(FocusedFeatureCardIndex, NewIndex);
}

void UBRVM_FrontEnd::SetNavTabLabels(const TArray<FText>& InLabels)
{
	NavTabLabels = InLabels;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(NavTabLabels);
}

void UBRVM_FrontEnd::SetNavTab(int32 InTabIndex, const TArray<FBRMenuRowEntry>& InRows)
{
	MenuRows = InRows;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MenuRows);

	UE_MVVM_SET_PROPERTY_VALUE(ActiveNavTabIndex, InTabIndex);

	SetFocusedMenuRowIndex(MenuRows.Num() > 0 ? 0 : INDEX_NONE);

	// Unconditional, on top of whatever the focus setter broadcast: a tab swap usually lands on
	// index 0 again, so the index does NOT change while the description behind it does.
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetFocusedRowDescription);

	UE_MVVM_SET_PROPERTY_VALUE(MenuState, EBRUIDataState::Live);
}

void UBRVM_FrontEnd::SetFocusedMenuRowIndex(int32 InIndex)
{
	const int32 NewIndex = MenuRows.IsValidIndex(InIndex) ? InIndex : static_cast<int32>(INDEX_NONE);
	if (UE_MVVM_SET_PROPERTY_VALUE(FocusedMenuRowIndex, NewIndex))
	{
		// The description has no backing field; nothing else broadcasts it.
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetFocusedRowDescription);
	}
}

FText UBRVM_FrontEnd::GetFocusedRowDescription() const
{
	return MenuRows.IsValidIndex(FocusedMenuRowIndex) ? MenuRows[FocusedMenuRowIndex].Description : FText::GetEmpty();
}

void UBRVM_FrontEnd::MarkStale()
{
	if (MenuState == EBRUIDataState::Live)
	{
		UE_MVVM_SET_PROPERTY_VALUE(MenuState, EBRUIDataState::Stale);
	}
}

void UBRVM_FrontEnd::ClearToUnknown()
{
	FeatureCards.Reset();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FeatureCards);

	MenuRows.Reset();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MenuRows);

	NavTabLabels.Reset();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(NavTabLabels);

	UE_MVVM_SET_PROPERTY_VALUE(FocusedFeatureCardIndex, static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(ActiveNavTabIndex, static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(FocusedMenuRowIndex, static_cast<int32>(INDEX_NONE));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetFocusedRowDescription);

	UE_MVVM_SET_PROPERTY_VALUE(MenuState, EBRUIDataState::Unknown);
}
