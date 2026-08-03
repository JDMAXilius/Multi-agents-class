#include "UI/BRHUDLayout.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Core/BRCore.h"
#include "MVVMSubsystem.h"
#include "UI/BRUISettings.h"
#include "UI/BRViewModels.h"
#include "View/MVVMView.h"

void UBRKillfeedEntryWidget::SetEntry(const FBRKillfeedViewEntry& InEntry)
{
	Entry = InEntry;

	if (KillerNameText)
	{
		KillerNameText->SetText(Entry.KillerName);
	}
	if (VictimNameText)
	{
		VictimNameText->SetText(Entry.VictimName);
	}

	// Empty renders as empty text in a reserved slot -- never collapsed, so a late-arriving
	// line appears in place without reflowing the feed (see the header).
	if (SpotterLineText)
	{
		SpotterLineText->SetText(Entry.SpotterLine);
	}

	// Collapsed until weapon glyph art exists; a brushless image is BP70 D2's blank rectangle.
	if (WeaponIcon)
	{
		WeaponIcon->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UBRHUDLayout::UBRHUDLayout()
{
	InputMode = EBRWidgetInputMode::Game;
}

void UBRHUDLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// LAYOUT-DOCTRINE Sec 7 / HUD-CPP-AUDIT H7: HitTestInvisible propagates to every child, so
	// asserting it ONCE on the frame's root makes the entire HUD passive to hit-testing. Set in
	// C++, not the WBP, because the generator cannot author visibility and a details-panel
	// default is decided by nobody. The pause menu never fights the HUD for clicks again.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBRHUDLayout::BindViewModels()
{
	Super::BindViewModels();

	PushViewModelsIntoMVVMView();
}

void UBRHUDLayout::PushViewModelsIntoMVVMView()
{
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!View)
	{
		return;
	}

	const UBRUISettings& Settings = UBRUISettings::Get();

	if (UBRVM_Combat* Combat = GetCombatViewModel())
	{
		View->SetViewModel(Settings.CombatViewModelContextName, Combat);
	}
	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		View->SetViewModel(Settings.MatchViewModelContextName, Match);
	}
}
