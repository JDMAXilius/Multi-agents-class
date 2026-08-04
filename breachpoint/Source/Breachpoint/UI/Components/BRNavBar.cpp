#include "UI/Components/BRNavBar.h"

#include "Breachpoint.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/HorizontalBoxSlot.h"
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
		// Named BorderStyle, not Style: `UCommonButtonBase` already has a member called `Style`
		// and a local of that name shadows it (C4458, which is an error under this project's
		// warning level). `UBRMenuRow::NativeOnInitialized` uses the same BorderStyle spelling.
		FBRHairlineStyle BorderStyle = Border->GetHairlineStyle();
		BorderStyle.Edges = static_cast<int32>(EBRBorderEdge::Top)
			| static_cast<int32>(EBRBorderEdge::Bottom)
			| static_cast<int32>(EBRBorderEdge::Left)
			| static_cast<int32>(EBRBorderEdge::Right);
		BorderStyle.DimmedEdges = static_cast<int32>(EBRBorderEdge::None);
		BorderStyle.StrokeToken = StrokeToken;
		BorderStyle.FillToken = EBRUIColorToken::None;
		BorderStyle.SideTickLength = 0.0f;

		// EBRStrokeWeight::Focus (3px) landed with the token pass; the "not expressible" era is
		// over. Inactive still reads as opacity 0.6 (COMPONENT-SPECS Sec 3), so the weight does
		// not swap per state -- the active tab's 3px IS the authored difference.
		BorderStyle.Weight = EBRStrokeWeight::Focus;
		Border->SetHairlineStyle(BorderStyle);
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

	// Honest empty state on the first frame: no tab data has arrived yet, so show nothing.
	if (Tabs.Num() == 0)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBRNavBar::NativeConstruct()
{
	Super::NativeConstruct();

	// CPP-AUDIT D2/D3: construct/destruct is per TREE ENTRY, initialize is per OBJECT — and a
	// popped-then-repushed screen re-enters the tree with the same object. Everything destruct
	// tears down must therefore be rebuilt HERE, not in NativeOnInitialized:
	//  - the tab-group delegate (RemoveAll'd below; the Transient TabGroup survives, so the
	//    !TabGroup guard in GetOrCreateTabGroup will never re-bind it),
	//  - the bumper action bindings (CommonUI unregisters the handles on destruct).
	// Symptom of getting this wrong: after one pop/re-push the bar renders perfectly, every tab
	// click is silently dropped, and LB/RB do nothing — invisible in a single-push PIE test.
	if (TabGroup)
	{
		TabGroup->NativeOnSelectedButtonBaseChanged.RemoveAll(this);
		TabGroup->NativeOnSelectedButtonBaseChanged.AddUObject(this, &UBRNavBar::HandleTabSelectionChanged);
	}

	RegisterBumperActions();
}

void UBRNavBar::NativeDestruct()
{
	// Bound in GetOrCreateTabGroup and re-bound in NativeConstruct; unbound here so nothing
	// outlives this tree entry (`ue5-ui-architecture` Sec 2).
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
	// CPP-AUDIT D5: the mid-loop selection broadcast (AddWidget under selection-required) can
	// route through the owning screen and re-enter this function while the loop below is live.
	// A re-entrant rebuild is always a feedback loop upstream — drop it and say so.
	if (bRebuildingTabs)
	{
		UE_LOG(Logbreachpoint, Warning, TEXT("UBRNavBar::SetTabs re-entered during a rebuild; dropped. The caller is echoing a selection broadcast back as a rebuild."));
		return;
	}
	TGuardValue<bool> RebuildGuard(bRebuildingTabs, true);

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

		UPanelSlot* PanelSlot = TabContainer->AddChild(Tab);

		// Pitch 150 on a 138 tab = 12 px gap, as slot padding on every tab after the first.
		// The gap lives HERE because the tabs are created here — the WBP ships an empty
		// container, so there is no authored slot for it to live in.
		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot); BoxSlot && Tabs.Num() > 0)
		{
			BoxSlot->SetPadding(FMargin(TabGap, 0.0f, 0.0f, 0.0f));
		}

		Tabs.Add(Tab);

		// Added to the group AFTER the container so CommonUI's selection-required rule picks the
		// first tab once it is actually in the tree.
		Group->AddWidget(Tab);
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// The rebuild held every selection broadcast (HandleTabSelectionChanged checks the guard).
	// Emit the ONE real resulting selection now, after the tree and Tabs agree — outside the
	// loop, so a listener that reacts by re-entering hits the guard's early-return instead of
	// corrupting a live rebuild.
	const int32 SelectedIndex = Group->GetSelectedButtonIndex();
	if (SelectedIndex != INDEX_NONE)
	{
		OnTabSelected.Broadcast(SelectedIndex);
	}
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
	// Held during a rebuild — SetTabs emits the single resulting selection itself (D5).
	if (bRebuildingTabs)
	{
		return;
	}

	// The bar reports an INDEX and nothing else. It does not push a screen, does not touch
	// replicated state and does not call an RPC -- routing is the owning screen's job through
	// the activatable stack (CLAUDE.md law 1, `ue5-ui-architecture` Sec 1).
	OnTabSelected.Broadcast(SelectedIndex);
}
