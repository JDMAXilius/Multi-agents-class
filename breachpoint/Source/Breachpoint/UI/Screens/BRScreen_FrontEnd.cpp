#include "UI/Screens/BRScreen_FrontEnd.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/UserWidget.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "INotifyFieldValueChanged.h"
#include "UI/Components/BRComponentTokens.h"
#include "UI/Components/BRFeatureCard.h"
#include "UI/Components/BRLeftRail.h"
#include "UI/Components/BRMenuRow.h"
#include "UI/Components/BRNavBar.h"
#include "UI/ViewModels/BRVM_FrontEnd.h"

#define LOCTEXT_NAMESPACE "BRScreenFrontEnd"

DEFINE_LOG_CATEGORY_STATIC(LogBRScreenFrontEnd, Log, All);

namespace BRScreenFrontEndInternal
{
	UCanvasPanelSlot* GetCanvasSlot(const UWidget* InWidget)
	{
		return InWidget ? Cast<UCanvasPanelSlot>(InWidget->Slot) : nullptr;
	}

	/**
	 * Write y and PRESERVE x. The Party List is RIGHT-anchored (that is the whole ultrawide story
	 * on `UBRRosterPanel`), so its slot x is whatever the anchor produced -- overwriting it with a
	 * design-canvas number would silently break every aspect ratio but 16:9.
	 */
	void SetSlotY(UWidget* InWidget, float InY)
	{
		if (UCanvasPanelSlot* CanvasSlot = GetCanvasSlot(InWidget))
		{
			CanvasSlot->SetPosition(FVector2D(CanvasSlot->GetPosition().X, InY));
		}
	}
}

UBRScreen_FrontEnd::UBRScreen_FrontEnd()
{
	// NOT a back handler, deliberately. This screen is the ROOT of `Layer.Menu`: there is nothing
	// beneath it on the stack, so consuming back here could only mean faking navigation. Screens
	// pushed ON TOP of it (an article, a modal) handle their own back and the stack restores focus
	// here when they pop -- which is the whole reason the stack exists (`ue5-ui-architecture` Sec 1).
	bIsBackHandler = false;
}

TOptional<FUIInputConfig> UBRScreen_FrontEnd::GetDesiredInputConfig() const
{
	// `ue5-ui-architecture` Sec 2: the ONE place input mode is declared. No `SetInputMode*` call
	// exists anywhere in this family, and the base class's `InputMode` details-panel knob is
	// deliberately bypassed -- a front-end screen's input mode is not a per-WBP choice.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

void UBRScreen_FrontEnd::SetFrontEndViewModel(UBRVM_FrontEnd* InViewModel)
{
	if (BoundViewModel.Get() == InViewModel)
	{
		return;
	}

	// Drop the old subscription before adopting the new one, or the previous ViewModel keeps
	// pushing into a screen that no longer reads it.
	if (UBRVM_FrontEnd* Previous = BoundViewModel.Get())
	{
		Previous->RemoveAllFieldValueChangedDelegates(this);
	}

	BoundViewModel = InViewModel;

	// Only subscribe while activated. Outside activation this is a stored pointer and nothing else.
	if (IsActivated())
	{
		BindViewModels();
	}
}

void UBRScreen_FrontEnd::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	MenuRowPool.SetWorld(GetWorld());
	MenuRowPool.SetDefaultPlayerController(GetOwningPlayer());

	if (!MenuRowWidgetClass.IsNull())
	{
		// Once, at initialise. A sync load on a tab change would be a hitch on the front end.
		ResolvedMenuRowClass = MenuRowWidgetClass.LoadSynchronous();
	}
}

void UBRScreen_FrontEnd::ReleaseSlateResources(bool bReleaseChildren)
{
	MenuRowPool.ReleaseAllSlateResources();

	Super::ReleaseSlateResources(bReleaseChildren);
}

UWidget* UBRScreen_FrontEnd::NativeGetDesiredFocusTarget() const
{
	// The rail hands out its own entry point (feature card, else first row). Reaching into its
	// children from here would be exactly the hand-rolled focus math `ue5-ui-architecture` Sec 6
	// forbids, and it would go stale the day the rail drops its feature card.
	if (LeftRail)
	{
		if (UWidget* RailTarget = LeftRail->GetFirstFocusTarget())
		{
			return RailTarget;
		}
	}

	return Super::NativeGetDesiredFocusTarget();
}

void UBRScreen_FrontEnd::BindViewModels()
{
	if (NavBar)
	{
		// CommonUI owns tab navigation: the bar registers LB/RB as input ACTIONS and groups its
		// tabs in a `UCommonButtonGroupBase`. This screen binds the RESULT and binds no key.
		NavBar->OnTabSelected.AddDynamic(this, &UBRScreen_FrontEnd::HandleNavTabSelected);
	}

	if (LeftRail)
	{
		if (UBRFeatureCard* FeatureCard = LeftRail->GetFeatureCard())
		{
			FeatureCard->OnClicked().AddUObject(this, &UBRScreen_FrontEnd::HandleFeatureCardClicked);
		}
	}

	if (PartyList)
	{
		// There is no party/roster ViewModel (BP24). The honest state is dashes, and this screen is
		// forbidden from reading `PlayerState` to fill the gap -- which is also why it survives the
		// frames after travel when every one of those pointers is null.
		PartyList->ClearToUnknown();
	}

	if (UBRVM_FrontEnd* ViewModel = BoundViewModel.Get())
	{
		// SPLIT ON PURPOSE. Structure changes rebuild widgets; focus changes must not. Refreshing
		// everything on a focus move would rebuild the whole row list on every stick flick -- the
		// per-frame widget churn `ue5-ui-architecture` Sec 5 names.
		const UE::FieldNotification::FFieldId StructureFields[] = {
			UBRVM_FrontEnd::FFieldNotificationClassDescriptor::MenuState,
			UBRVM_FrontEnd::FFieldNotificationClassDescriptor::NavTabLabels,
			UBRVM_FrontEnd::FFieldNotificationClassDescriptor::ActiveNavTabIndex,
			UBRVM_FrontEnd::FFieldNotificationClassDescriptor::MenuRows,
			UBRVM_FrontEnd::FFieldNotificationClassDescriptor::FeatureCards
		};

		for (const UE::FieldNotification::FFieldId& FieldId : StructureFields)
		{
			ViewModel->AddFieldValueChangedDelegate(
				FieldId,
				INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
					this, &UBRScreen_FrontEnd::HandleStructureChanged));
		}

		const UE::FieldNotification::FFieldId FocusFields[] = {
			UBRVM_FrontEnd::FFieldNotificationClassDescriptor::FocusedMenuRowIndex,
			UBRVM_FrontEnd::FFieldNotificationClassDescriptor::FocusedFeatureCardIndex
		};

		for (const UE::FieldNotification::FFieldId& FieldId : FocusFields)
		{
			ViewModel->AddFieldValueChangedDelegate(
				FieldId,
				INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
					this, &UBRScreen_FrontEnd::HandleFocusChanged));
		}
	}

	// Legal for the ViewModel to be null here: nothing constructs a `UBRVM_FrontEnd` yet, and after
	// travel it can arrive frames late. `RefreshAll` renders the honest unknown state in that case
	// rather than an empty front end that looks like a shipped one.
	RefreshAll();
}

void UBRScreen_FrontEnd::UnbindViewModels()
{
	if (UBRVM_FrontEnd* ViewModel = BoundViewModel.Get())
	{
		// By delegate object: covers every field in one call and cannot leave a handle bound to a
		// field the lists above stopped naming.
		ViewModel->RemoveAllFieldValueChangedDelegates(this);
	}

	if (NavBar)
	{
		NavBar->OnTabSelected.RemoveDynamic(this, &UBRScreen_FrontEnd::HandleNavTabSelected);
	}

	if (LeftRail)
	{
		if (UBRFeatureCard* FeatureCard = LeftRail->GetFeatureCard())
		{
			FeatureCard->OnClicked().RemoveAll(this);
		}
	}

	// Every row delegate bound in RebuildMenuRows dies here. A row callback that outlives the
	// screen's activation is the leak this pair exists to prevent.
	ReleaseMenuRows();
}

void UBRScreen_FrontEnd::HandleStructureChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	RefreshAll();
}

void UBRScreen_FrontEnd::HandleFocusChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	ApplyFocus();
	ApplyFeatureCard();
}

void UBRScreen_FrontEnd::HandleNavTabSelected(int32 TabIndex)
{
	if (bApplyingViewModelState)
	{
		// We are the ones who just moved the selection. Reporting it back as a REQUEST would loop.
		return;
	}

	if (TabSwapAnim)
	{
		// C++ owns WHEN, the WBP owns the curve and the duration. MOTION-MEASURED has no measured
		// number for a nav-tab swap, so nothing here says how long this takes.
		PlayAnimationForward(TabSwapAnim);
	}

	// INTENT OUT, nothing changed here. This screen does not own the per-tab menu definition (that
	// is table data, law 3) and must not decide what tab 2 contains. The caller answers by calling
	// `UBRVM_FrontEnd::SetNavTab`, and the change comes back in through the field notification --
	// one path in, so the widget and the data can never disagree about which tab is live.
	OnTabChangeRequested.ExecuteIfBound(TabIndex);
}

void UBRScreen_FrontEnd::HandleMenuRowClicked(FName RowId)
{
	// A route NAME leaves the widget. What it means -- a push, a session flow, a modal -- belongs
	// to whoever raised this screen. No session call and no RPC is reachable from here.
	OnRouteRequested.ExecuteIfBound(RowId);
}

void UBRScreen_FrontEnd::HandleMenuRowFocused(int32 RowIndex)
{
	if (UBRVM_FrontEnd* ViewModel = BoundViewModel.Get())
	{
		// Local UI focus, which is what the ViewModel's own contract says this setter is for
		// ("driven by gamepad navigation ... this is what moves the Description Strip"). It is not
		// game state and it is not replicated.
		ViewModel->SetFocusedMenuRowIndex(RowIndex);
		return;
	}

	// No ViewModel: still move the caret, so the rail is not visibly dead before data arrives.
	if (LeftRail)
	{
		LeftRail->SetFocusedRowIndex(RowIndex);
	}
}

void UBRScreen_FrontEnd::HandleFeatureCardClicked()
{
	const UBRVM_FrontEnd* ViewModel = BoundViewModel.Get();
	if (!ViewModel)
	{
		return;
	}

	const TArray<FBRFeatureCardEntry>& Cards = ViewModel->GetFeatureCards();
	const int32 Index = ViewModel->GetFocusedFeatureCardIndex();
	if (!Cards.IsValidIndex(Index))
	{
		// Nothing focused is not an error and not a route. A click on an empty carousel does
		// nothing rather than opening whatever happened to be first.
		return;
	}

	OnRouteRequested.ExecuteIfBound(Cards[Index].TargetId);
}

void UBRScreen_FrontEnd::RebuildNavTabs()
{
	if (!NavBar)
	{
		return;
	}

	const UBRVM_FrontEnd* ViewModel = BoundViewModel.Get();

	TArray<FBRNavTabDefinition> Definitions;
	if (ViewModel)
	{
		const TArray<FText>& Labels = ViewModel->GetNavTabLabels();
		Definitions.Reserve(Labels.Num());

		for (const FText& Label : Labels)
		{
			// Icon left unset: COMPONENT-SPECS Sec 3 makes the 24x24 tab icon optional and no
			// ViewModel field carries one. An unset soft ref collapses the slot in `UBRNavTab`.
			FBRNavTabDefinition& Definition = Definitions.AddDefaulted_GetRef();
			Definition.Label = Label;
		}
	}

	// Empty collapses the whole bar, by that class's contract. Before data arrives an empty 666x30
	// outline would read as a broken screen; no bar reads as "not yet", which is the truth.
	NavBar->SetTabs(Definitions);

	if (ViewModel)
	{
		// Guarded: `SetSelectedTabIndex` broadcasts for programmatic selection too, by design.
		TGuardValue<bool> ApplyGuard(bApplyingViewModelState, true);
		NavBar->SetSelectedTabIndex(ViewModel->GetActiveNavTabIndex());
	}
}

void UBRScreen_FrontEnd::RebuildMenuRows()
{
	if (!LeftRail)
	{
		return;
	}

	UPanelWidget* RowSlot = LeftRail->GetMenuRowSlot();
	if (!RowSlot)
	{
		return;
	}

	ReleaseMenuRows();

	const UBRVM_FrontEnd* ViewModel = BoundViewModel.Get();
	const TArray<FBRMenuRowEntry> Rows = ViewModel ? ViewModel->GetMenuRows() : TArray<FBRMenuRowEntry>();

	if (Rows.IsEmpty())
	{
		// The rail still lays out; `ApplyEmptyState` says which kind of nothing this is.
		LeftRail->RefreshLayout();
		return;
	}

	if (!ResolvedMenuRowClass)
	{
		// A missing row class is a WBP authoring error, not a runtime state. Silence here would
		// render an empty rail that looks like a tab with no options.
		UE_LOG(LogBRScreenFrontEnd, Warning,
			TEXT("%s: MenuRowWidgetClass is unset or failed to load; %d menu row(s) dropped."),
			*GetName(), Rows.Num());
		LeftRail->RefreshLayout();
		return;
	}

	MenuRowWidgets.Reserve(Rows.Num());

	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FBRMenuRowEntry& Entry = Rows[Index];

		// Pooled: a tab change re-uses the widgets it released a line ago instead of allocating a
		// fresh list every time the player taps a bumper.
		UBRMenuRow* RowWidget = MenuRowPool.GetOrCreateInstance<UBRMenuRow>(ResolvedMenuRowClass);
		if (!RowWidget)
		{
			break;
		}

		RowWidget->SetLabelText(Entry.Label);
		RowWidget->SetIsEnabled(Entry.bEnabled);

		// Payload binding, so no row has to know its own index and no index has to be kept in sync.
		RowWidget->OnClicked().AddUObject(this, &UBRScreen_FrontEnd::HandleMenuRowClicked, Entry.RowId);

		// BOTH, and they route to one handler: hover is the mouse path, focus is the gamepad path,
		// and the caret plus the description strip must follow either. Binding only hover is how a
		// screen ends up mouse-only.
		RowWidget->OnHovered().AddUObject(this, &UBRScreen_FrontEnd::HandleMenuRowFocused, Index);
		RowWidget->OnFocusReceived().AddUObject(this, &UBRScreen_FrontEnd::HandleMenuRowFocused, Index);

		RowSlot->AddChild(RowWidget);
		MenuRowWidgets.Add(RowWidget);

		if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(RowWidget->Slot))
		{
			// ui-presentation Sec 5: h28 on a 40 pitch. The 12 is DERIVED from the two measured
			// numbers and carried as bottom padding on every row but the last, so the rail's
			// computed height and what is actually in it cannot disagree.
			const float BottomPadding = (Index == Rows.Num() - 1)
				? 0.0f
				: (UBRMenuRow::RowPitch - UBRMenuRow::RowHeight);
			BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	// One source of truth for the panel height: the rail adopts the real child count.
	LeftRail->RefreshLayout();
}

void UBRScreen_FrontEnd::ReleaseMenuRows()
{
	for (const TObjectPtr<UBRMenuRow>& RowWidget : MenuRowWidgets)
	{
		if (RowWidget)
		{
			RowWidget->OnClicked().RemoveAll(this);
			RowWidget->OnHovered().RemoveAll(this);
			RowWidget->OnFocusReceived().RemoveAll(this);
		}
	}

	MenuRowWidgets.Reset();

	if (LeftRail)
	{
		if (UPanelWidget* RowSlot = LeftRail->GetMenuRowSlot())
		{
			RowSlot->ClearChildren();
		}
	}

	MenuRowPool.ReleaseAll(/*bReleaseSlate*/ false);
}

void UBRScreen_FrontEnd::ApplyFocus()
{
	if (!LeftRail)
	{
		return;
	}

	const UBRVM_FrontEnd* ViewModel = BoundViewModel.Get();

	// INDEX_NONE hides the caret, by the rail's contract. Nothing focused renders as nothing --
	// never as a caret parked on row 0, which would say something false about where the player is.
	LeftRail->SetFocusedRowIndex(ViewModel ? ViewModel->GetFocusedMenuRowIndex() : INDEX_NONE);
}

void UBRScreen_FrontEnd::ApplyFeatureCard()
{
	if (!LeftRail)
	{
		return;
	}

	UBRFeatureCard* FeatureCard = LeftRail->GetFeatureCard();
	if (!FeatureCard)
	{
		// Legal: `ST_ControlPanel` and `OP_Customize` run the rail with no card at all.
		return;
	}

	const UBRVM_FrontEnd* ViewModel = BoundViewModel.Get();
	if (!ViewModel)
	{
		FeatureCard->ClearFeature();
		return;
	}

	const TArray<FBRFeatureCardEntry>& Cards = ViewModel->GetFeatureCards();
	const int32 Index = ViewModel->GetFocusedFeatureCardIndex();
	if (!Cards.IsValidIndex(Index))
	{
		// No cards, or none focused: the card's own honest empty state (plate kept, dashed caption)
		// rather than a stale article from the previous tab.
		FeatureCard->ClearFeature();
		return;
	}

	// Soft ref straight through; the card streams it in and never sync-loads on a swap.
	FeatureCard->SetFeatureImage(Cards[Index].Image);
	FeatureCard->SetCaptionText(Cards[Index].Title);
}

void UBRScreen_FrontEnd::ApplyTabLayout(int32 TabIndex)
{
	if (!TabLayouts.IsValidIndex(TabIndex))
	{
		// THE DEFAULT PATH, and today the only one. No entry means the FE_Play geometry stands and
		// nothing is moved. The two FE_Community deltas are recorded as constants in the header and
		// deliberately NOT applied -- see the drift note there.
		return;
	}

	const FBRFrontEndTabLayout& Layout = TabLayouts[TabIndex];

	BRScreenFrontEndInternal::SetSlotY(ProgressionButton, Layout.ProgressionButtonY);
	BRScreenFrontEndInternal::SetSlotY(PartyList, Layout.PartyListY);
}

void UBRScreen_FrontEnd::ApplyContentWidget(int32 TabIndex)
{
	if (!ContentSlot)
	{
		return;
	}

	ContentSlot->ClearChildren();

	const FBRFrontEndTabLayout* Layout = TabLayouts.IsValidIndex(TabIndex) ? &TabLayouts[TabIndex] : nullptr;
	if (!Layout || Layout->ContentWidgetClass.IsNull())
	{
		// The measured state of all three FE frames: the centre column is identical, so there is
		// nothing to swap. Collapsed, not hidden -- an empty reserved band is not in the reference.
		ContentSlot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	TObjectPtr<UUserWidget>* Cached = ContentWidgets.Find(TabIndex);
	UUserWidget* Content = Cached ? Cached->Get() : nullptr;

	if (!Content)
	{
		// ponytail: sync load, because no tab ships content today and a load that never runs needs
		// no streaming handle. The first tab that actually names a content widget should move this
		// to the async path `UBRFeatureCard` already uses for its art.
		UClass* ContentClass = Layout->ContentWidgetClass.LoadSynchronous();
		if (!ContentClass)
		{
			UE_LOG(LogBRScreenFrontEnd, Warning,
				TEXT("%s: tab %d names a ContentWidgetClass that failed to load."), *GetName(), TabIndex);
			ContentSlot->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}

		Content = CreateWidget<UUserWidget>(this, ContentClass);
		if (!Content)
		{
			ContentSlot->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}

		// Kept, so switching tabs back and forth re-parents an existing widget instead of building
		// a new one each time.
		ContentWidgets.Add(TabIndex, Content);
	}

	ContentSlot->AddChild(Content);
	ContentSlot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UBRScreen_FrontEnd::ApplyEmptyState()
{
	if (!EmptyStateLabel)
	{
		return;
	}

	const UBRVM_FrontEnd* ViewModel = BoundViewModel.Get();

	// THREE STATES, NOT TWO (`ue5-ui-architecture` Sec 7). "Nobody has told us yet" and "we were
	// told, and the answer is nothing" are different facts, and collapsing them is the
	// join-in-progress lie. Stale still renders, because stale is the last TRUE thing we were told.
	if (!ViewModel || ViewModel->GetMenuState() == EBRUIDataState::Unknown)
	{
		EmptyStateLabel->SetText(BRUI::UnknownValueText());
		EmptyStateLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	if (ViewModel->GetMenuRows().IsEmpty())
	{
		EmptyStateLabel->SetText(LOCTEXT("FrontEndTabEmpty", "NOTHING AVAILABLE"));
		EmptyStateLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	EmptyStateLabel->SetVisibility(ESlateVisibility::Collapsed);
}

void UBRScreen_FrontEnd::RefreshAll()
{
	const UBRVM_FrontEnd* ViewModel = BoundViewModel.Get();
	const int32 TabIndex = ViewModel ? ViewModel->GetActiveNavTabIndex() : INDEX_NONE;

	// Order matters exactly once: the rows must exist before the caret is placed on one.
	RebuildNavTabs();
	ApplyTabLayout(TabIndex);
	ApplyContentWidget(TabIndex);
	RebuildMenuRows();
	ApplyFeatureCard();
	ApplyFocus();
	ApplyEmptyState();
}

#undef LOCTEXT_NAMESPACE
