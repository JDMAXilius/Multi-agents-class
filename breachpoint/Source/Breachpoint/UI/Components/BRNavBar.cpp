#include "UI/Components/BRNavBar.h"

#include "Breachpoint.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Input/CommonUIInputTypes.h"
#include "InputAction.h"
#include "UI/Components/BRButtonPrompt.h"
#include "UI/Components/BRHairlineBorder.h"

// ===========================================================================================
// UBRNavTab
// ===========================================================================================

void UBRNavTab::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// A tab is a selection, not a command: CommonUI owns which one is selected, and clicking the
	// selected tab must not deselect it (a nav bar with nothing selected shows nothing).
	SetIsSelectable(true);
	SetIsToggleable(false);

	if (RootSizeBox)
	{
		RootSizeBox->SetWidthOverride(TabWidth);
		RootSizeBox->SetHeightOverride(TabHeight);
	}

	if (Border)
	{
		// COMPONENT-SPECS Sec 3: a closed RECTANGLE -- all four edges, none dimmed, full height.
		// This is the one place in the front end where the border is NOT the menu row's four
		// partial lines, so it is set from C++ rather than trusting a WBP default.
		FBRHairlineStyle Style = Border->GetHairlineStyle();
		Style.Edges = static_cast<int32>(EBRBorderEdge::Top)
			| static_cast<int32>(EBRBorderEdge::Bottom)
			| static_cast<int32>(EBRBorderEdge::Left)
			| static_cast<int32>(EBRBorderEdge::Right);
		Style.DimmedEdges = static_cast<int32>(EBRBorderEdge::None);
		Style.StrokeToken = StrokeToken;
		Style.FillToken = EBRUIColorToken::None;
		Style.SideTickLength = 0.0f;

		// See UBRNavTab::ActiveStrokeWeightPx -- 3px OUTSIDE is not expressible in
		// EBRStrokeWeight, so both states draw at Emphasis (2px) and the state read is opacity.
		Style.Weight = EBRStrokeWeight::Emphasis;
		Border->SetHairlineStyle(Style);
	}

	if (Icon)
	{
		Icon->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Force the inactive treatment through the one code path rather than trusting WBP defaults.
	bIsActive = true;
	ApplyActiveState(false);
}

void UBRNavTab::ApplyActiveState(bool bActive)
{
	if (bIsActive == bActive)
	{
		return;
	}

	bIsActive = bActive;

	// COMPONENT-SPECS Sec 3: `Active=False` dims the WHOLE component to 0.6. It is not a
	// per-child tint and not a colour swap.
	SetRenderOpacity(bActive ? 1.0f : InactiveOpacity);
}

void UBRNavTab::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);

	ApplyActiveState(true);
}

void UBRNavTab::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);

	ApplyActiveState(false);
}

void UBRNavTab::SetTabLabel(const FText& InText)
{
	if (Label)
	{
		Label->SetText(InText);
	}
}

void UBRNavTab::SetTabIcon(const TSoftObjectPtr<UTexture2D>& InIcon)
{
	if (!Icon)
	{
		return;
	}

	// ponytail: synchronous resolve of a 24x24 icon at tab-build time (once per screen push, not
	// per frame). If the front end ever streams nav icons from a pak, move this to
	// UAssetManager::GetStreamableManager().RequestAsyncLoad with a weak-this callback.
	UTexture2D* IconTexture = InIcon.IsNull() ? nullptr : InIcon.LoadSynchronous();
	if (!IconTexture)
	{
		// Optional in the reference: no icon means no slot, not an empty 24x24 hole.
		Icon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Icon->SetBrushFromTexture(IconTexture, false);
	Icon->SetDesiredSizeOverride(FVector2D(TabIconSize, TabIconSize));
	Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
}

// ===========================================================================================
// UBRNavBar
// ===========================================================================================

void UBRNavBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RootSizeBox)
	{
		RootSizeBox->SetWidthOverride(bSubLevel ? SubLevelBarWidth : BarWidth);
		RootSizeBox->SetHeightOverride(BarHeight);
	}

	GetOrCreateTabGroup();
	RegisterBumperActions();

	// Honest empty state on the first frame: no tab data has arrived yet, so show nothing.
	if (Tabs.Num() == 0)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBRNavBar::NativeDestruct()
{
	// Bound in GetOrCreateTabGroup; unbound here so nothing outlives the widget
	// (`ue5-ui-architecture` Sec 2: an unbound delegate surviving teardown is the crash).
	if (TabGroup)
	{
		TabGroup->NativeOnSelectedButtonBaseChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

UCommonButtonGroupBase* UBRNavBar::GetOrCreateTabGroup()
{
	if (!TabGroup)
	{
		TabGroup = NewObject<UCommonButtonGroupBase>(this);

		// A nav bar always has a selected tab; CommonUI enforces that rather than this class.
		TabGroup->SetSelectionRequired(true);
		TabGroup->NativeOnSelectedButtonBaseChanged.AddUObject(this, &UBRNavBar::HandleTabSelectionChanged);
	}

	return TabGroup;
}

void UBRNavBar::RegisterBumperActions()
{
	// SCREEN-MANIFEST Sec 6.1 / COMPONENT-SPECS Sec 3: the bar carries a bumper prompt at each
	// end. They are CommonUI ACTIONS, not raw LB/RB keys, so the glyph follows the input device
	// and the binding is scoped to this widget's activatable parents being active.
	UInputAction* PrevAction = PreviousTabAction.IsNull() ? nullptr : PreviousTabAction.LoadSynchronous();
	UInputAction* NextAction = NextTabAction.IsNull() ? nullptr : NextTabAction.LoadSynchronous();

	if (PrevAction)
	{
		FBindUIActionArgs PrevArgs(
			PrevAction,
			/*bShouldDisplayInActionBar*/ false,
			FSimpleDelegate::CreateUObject(this, &UBRNavBar::HandleSelectPreviousTab));
		PrevArgs.InputMode = ECommonInputMode::Menu;
		RegisterUIActionBinding(PrevArgs);
	}

	if (NextAction)
	{
		FBindUIActionArgs NextArgs(
			NextAction,
			/*bShouldDisplayInActionBar*/ false,
			FSimpleDelegate::CreateUObject(this, &UBRNavBar::HandleSelectNextTab));
		NextArgs.InputMode = ECommonInputMode::Menu;
		RegisterUIActionBinding(NextArgs);
	}

	// The on-screen glyphs. A null action collapses its prompt (UBRButtonPrompt::SetPrompt), so
	// an unconfigured bumper shows nothing rather than promising input that does not exist.
	if (BumperPrev)
	{
		BumperPrev->SetPrompt(PrevAction, PreviousTabVerb);
	}

	if (BumperNext)
	{
		BumperNext->SetPrompt(NextAction, NextTabVerb);
	}
}

void UBRNavBar::SetTabs(const TArray<FBRNavTabDefinition>& InTabs)
{
	UCommonButtonGroupBase* Group = GetOrCreateTabGroup();

	Group->RemoveAll();
	Tabs.Reset();

	if (TabContainer)
	{
		TabContainer->ClearChildren();
	}

	if (InTabs.Num() == 0)
	{
		// Honest empty state (`ue5-ui-architecture` Sec 7): front-end data is legitimately absent
		// for frames after travel. An empty outline reads as a broken screen.
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UClass* ResolvedTabClass = TabWidgetClass.IsNull() ? nullptr : TabWidgetClass.LoadSynchronous();
	if (!ResolvedTabClass || !TabContainer)
	{
		// Misconfiguration, not a data state: say so once and show nothing.
		UE_LOG(Logbreachpoint, Warning, TEXT("UBRNavBar: no TabWidgetClass or no TabContainer; %d tabs dropped."), InTabs.Num());
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Tabs.Reserve(InTabs.Num());

	for (const FBRNavTabDefinition& Definition : InTabs)
	{
		UBRNavTab* Tab = CreateWidget<UBRNavTab>(this, ResolvedTabClass);
		if (!Tab)
		{
			continue;
		}

		Tab->SetTabLabel(Definition.Label);
		Tab->SetTabIcon(Definition.Icon);

		TabContainer->AddChild(Tab);
		Tabs.Add(Tab);

		// Added to the group AFTER the container so CommonUI's selection-required rule picks the
		// first tab once it is actually in the tree.
		Group->AddWidget(Tab);
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UBRNavBar::SetSelectedTabIndex(int32 InIndex)
{
	if (!Tabs.IsValidIndex(InIndex))
	{
		return;
	}

	// Routed through the group, never by poking the tab, so user selection and programmatic
	// selection are the same code path.
	GetOrCreateTabGroup()->SelectButtonAtIndex(InIndex, /*bAllowSound*/ false);
}

int32 UBRNavBar::GetSelectedTabIndex() const
{
	return TabGroup ? TabGroup->GetSelectedButtonIndex() : INDEX_NONE;
}

void UBRNavBar::HandleSelectPreviousTab()
{
	if (Tabs.Num() > 0)
	{
		GetOrCreateTabGroup()->SelectPreviousButton(bBumpersWrap);
	}
}

void UBRNavBar::HandleSelectNextTab()
{
	if (Tabs.Num() > 0)
	{
		GetOrCreateTabGroup()->SelectNextButton(bBumpersWrap);
	}
}

void UBRNavBar::HandleTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 SelectedIndex)
{
	// The bar reports an INDEX and nothing else. It does not push a screen, does not touch
	// replicated state and does not call an RPC -- routing is the owning screen's job through
	// the activatable stack (CLAUDE.md law 1, `ue5-ui-architecture` Sec 1).
	OnTabSelected.Broadcast(SelectedIndex);
}
