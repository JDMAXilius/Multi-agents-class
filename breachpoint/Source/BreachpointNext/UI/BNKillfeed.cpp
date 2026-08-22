#include "UI/BNKillfeed.h"
#include "BreachpointNext.h"
#include "Components/PanelWidget.h"
#include "Engine/LocalPlayer.h"
#include "UI/BNKillfeedEntry.h"
#include "UI/BNUIManager.h"
#include "UI/BNViewModels.h"

void UBNKillfeed::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// The pool, collected ONCE from what the asset placed. No CreateWidget, ever: the row count
	// is the WBP's decision and the ticket's read-back, not a runtime negotiation.
	Rows.Reset();
	if (EntryContainer)
	{
		for (int32 Index = 0; Index < EntryContainer->GetChildrenCount(); ++Index)
		{
			if (UBNKillfeedEntry* Row = Cast<UBNKillfeedEntry>(EntryContainer->GetChildAt(Index)))
			{
				Rows.Add(Row);
			}
		}
	}
	if (Rows.Num() == 0)
	{
		UE_LOG(LogBN, Warning, TEXT("BNKillfeed: the WBP placed no UBNKillfeedEntry rows in EntryContainer — the feed will render nothing. TASK-R7-WBP-HUD names the tree."));
	}
}

void UBNKillfeed::NativeConstruct()
{
	Super::NativeConstruct();

	UBNUIManager* Manager = UBNUIManager::Get(this);
	UBNVM_Match* Match = Manager ? Manager->GetMatchViewModel(GetOwningLocalPlayer()) : nullptr;
	if (!Match)
	{
		return;
	}
	BoundViewModel = Match;
	KillfeedHandle = Match->OnKillfeedViewChanged.AddUObject(this, &UBNKillfeed::HandleKillfeedChanged);

	Refresh();
}

void UBNKillfeed::NativeDestruct()
{
	if (UBNVM_Match* Match = BoundViewModel.Get())
	{
		Match->OnKillfeedViewChanged.Remove(KillfeedHandle);
	}
	KillfeedHandle.Reset();
	BoundViewModel.Reset();

	Super::NativeDestruct();
}

void UBNKillfeed::HandleKillfeedChanged()
{
	Refresh();
}

void UBNKillfeed::Refresh()
{
	const UBNVM_Match* Match = BoundViewModel.Get();
	const TArray<FBNKillfeedViewEntry>* Entries = Match ? &Match->GetKillfeedEntries() : nullptr;
	const int32 EntryCount = Entries ? Entries->Num() : 0;

	// Newest at the BOTTOM, claiming rows top-down over the window that fits. When the view
	// ring outruns the placed rows, the OLDEST visible drop off the top — and the shortage is
	// logged once, because a silently capped feed reads as complete.
	const int32 Shown = FMath::Min(EntryCount, Rows.Num());
	if (EntryCount > Rows.Num() && !bWarnedRowShortage)
	{
		bWarnedRowShortage = true;
		UE_LOG(LogBN, Log, TEXT("BNKillfeed: %d view entries for %d placed rows — oldest dropped. More rows is a WBP edit."),
			EntryCount, Rows.Num());
	}

	const int32 FirstShownEntry = EntryCount - Shown;
	for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
	{
		UBNKillfeedEntry* Row = Rows[RowIndex];
		if (!Row)
		{
			continue;
		}
		if (RowIndex < Shown)
		{
			const FBNKillfeedViewEntry& Entry = (*Entries)[FirstShownEntry + RowIndex];
			Row->SetEntry(Entry.Line, Entry.bInvolvesSelf, Entry.WeaponIcon);
		}
		else
		{
			Row->ClearEntry();
		}
	}
}
