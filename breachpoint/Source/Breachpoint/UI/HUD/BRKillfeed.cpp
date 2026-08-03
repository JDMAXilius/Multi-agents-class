#include "UI/HUD/BRKillfeed.h"

#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "UI/BRHUDLayout.h"
#include "UI/BRUISettings.h"
#include "UI/BRUITypes.h"
#include "UI/BRViewModels.h"
#include "UI/Styles/BRUITokens.h"

DEFINE_LOG_CATEGORY_STATIC(LogBRKillfeed, Log, All);

UBRKillfeed::UBRKillfeed()
{
	// A HUD element, not a stack screen. `InputMode` stays `Inherit`: the feed must never
	// change input config or focus just by appearing.
	bAutoActivate = true;
}

void UBRKillfeed::BindViewModels()
{
	Super::BindViewModels();

	EnsurePool();

	UBRVM_Match* Match = GetMatchViewModel();
	if (!Match)
	{
		// First frames after travel: the ViewModel for this local player may not exist yet.
		// An empty feed is the honest answer -- there is no such thing as a stale kill.
		ReleaseAllRows();
		return;
	}

	Match->OnKillfeedChanged().AddUObject(this, &UBRKillfeed::HandleKillfeedChanged);

	// Re-activation mid-match (death cam, menu, rejoin) must show the kills that happened
	// while we were not listening -- they are still in the authoritative ring.
	Refresh();
}

void UBRKillfeed::UnbindViewModels()
{
	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		Match->OnKillfeedChanged().RemoveAll(this);
	}

	// Release, do not destroy. The pool survives deactivation so the next activation costs
	// nothing; collapsing the rows stops a stale feed showing for one frame on the way back.
	ReleaseAllRows();

	Super::UnbindViewModels();
}

void UBRKillfeed::ReleaseSlateResources(bool bReleaseChildren)
{
	// The rows are real children of `EntryContainer`, so the standard teardown reaches them.
	// This override exists only to make that fact explicit next to the word "pool", which
	// usually implies the off-tree kind that leaks unless released by hand.
	Super::ReleaseSlateResources(bReleaseChildren);
}

void UBRKillfeed::EnsurePool()
{
	if (Rows.Num() > 0 || !EntryContainer)
	{
		return;
	}

	const UBRUISettings& Settings = UBRUISettings::Get();

	// POOL SIZE PROVENANCE: `UBRUISettings::KillfeedMaxVisibleEntries` (config = Game, default
	// 5, clamped 1..16 in the settings object). That is the SAME value
	// `UBRVM_Match::PushKillfeedEntry` caps its ring buffer with, so the pool is exactly as
	// deep as the window it renders -- by construction, not by coincidence. It is a config
	// value rather than a constant here so the number lives in one place for both.
	const int32 PoolSize = FMath::Clamp(Settings.KillfeedMaxVisibleEntries, 1, 16);

	// SOFT class reference, resolved once at activation (CLAUDE.md law 3: no hard asset refs,
	// no ConstructorHelpers). This is the only `CreateWidget` in the file and it is nowhere
	// near the per-kill path.
	TSubclassOf<UBRKillfeedEntryWidget> EntryClass;
	if (!Settings.KillfeedEntryClass.IsNull())
	{
		EntryClass = Settings.KillfeedEntryClass.LoadSynchronous();
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (!EntryClass || !OwningPlayer)
	{
		UE_LOG(LogBRKillfeed, Warning,
			TEXT("Killfeed pool not built: entry class %s, owning player %s. The feed will ")
			TEXT("render empty rather than allocate rows per kill."),
			EntryClass ? TEXT("resolved") : TEXT("UNRESOLVED (UBRUISettings.KillfeedEntryClass)"),
			OwningPlayer ? TEXT("present") : TEXT("NULL"));
		return;
	}

	Rows.Reserve(PoolSize);
	for (int32 Index = 0; Index < PoolSize; ++Index)
	{
		UBRKillfeedEntryWidget* Row = CreateWidget<UBRKillfeedEntryWidget>(OwningPlayer, EntryClass);
		if (!Row)
		{
			break;
		}

		Row->SetVisibility(ESlateVisibility::Collapsed);
		EntryContainer->AddChild(Row);
		Rows.Add(Row);
	}

	UE_LOG(LogBRKillfeed, Log, TEXT("Killfeed pool built with %d rows (requested %d)."),
		Rows.Num(), PoolSize);
}

void UBRKillfeed::HandleKillfeedChanged()
{
	Refresh();
}

void UBRKillfeed::Refresh()
{
	const UBRVM_Match* Match = GetMatchViewModel();
	if (!Match || Rows.Num() == 0)
	{
		ReleaseAllRows();
		return;
	}

	const TArray<FBRKillfeedViewEntry>& Entries = Match->GetKillfeedEntries();
	const uint8 LocalTeamId = Match->GetLocalTeamId();

	// The ring is oldest-first. When it is deeper than the pool, the window keeps the NEWEST
	// rows -- the ones still relevant to the fight -- and the oldest fall off the top.
	const int32 NumVisible = FMath::Min(Entries.Num(), Rows.Num());
	const int32 FirstIndex = Entries.Num() - NumVisible;

	if (FirstIndex > 0)
	{
		// Doctrine, verbatim: the drop is logged. Warning, not Verbose -- if this fires the
		// pool and the ring have diverged and the player is being shown a feed that quietly
		// omits kills. Reachable only if `KillfeedMaxVisibleEntries` changed after the pool
		// was built, or if a future ring cap stops using that setting.
		UE_LOG(LogBRKillfeed, Warning,
			TEXT("Killfeed pool exhausted: ring holds %d entries, pool has %d rows. Dropped ")
			TEXT("the %d oldest visible entr%s."),
			Entries.Num(), Rows.Num(), FirstIndex, (FirstIndex == 1) ? TEXT("y") : TEXT("ies"));
	}

	for (int32 SlotIndex = 0; SlotIndex < Rows.Num(); ++SlotIndex)
	{
		UBRKillfeedEntryWidget* Row = Rows[SlotIndex];
		if (!Row)
		{
			continue;
		}

		if (SlotIndex >= NumVisible)
		{
			// Released. Collapsed rather than hidden: an unclaimed row must not reserve a
			// line of vertical space in the feed. (The SPOTTER slot INSIDE a claimed row is
			// the opposite case -- it must reserve its space; see the header.)
			Row->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const FBRKillfeedViewEntry& Entry = Entries[FirstIndex + SlotIndex];

		// ponytail: whole-row tint. `UBRKillfeedEntryWidget` exposes no BindWidget text or
		// colour surface (it carries the struct and fires a BP event), so the only C++ way to
		// put the VISR channel on a row is `UUserWidget::ColorAndOpacity`, which multiplies
		// the entire subtree including the Spotter line. Filed as a contract_gap against
		// BP66: the entry widget wants `KillerText` / `VictimText` / `SpotterText` BindWidget
		// members plus a pushed tint, at which point this becomes a per-element colour and
		// this line goes away. Doing it here anyway keeps the colour DECISION in C++ with
		// tokens instead of in a widget graph.
		Row->SetColorAndOpacity(ResolveEntryTint(Entry, LocalTeamId));

		// Re-set unconditionally: rows shift by one slot whenever an entry expires off the
		// front, so slot N holds a different kill than it did last refresh. This is also the
		// path a late Spotter line takes -- same slot, same kill, one more string.
		Row->SetEntry(Entry);
		Row->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UBRKillfeed::ReleaseAllRows()
{
	for (UBRKillfeedEntryWidget* Row : Rows)
	{
		if (Row)
		{
			Row->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

FLinearColor UBRKillfeed::ResolveEntryTint(const FBRKillfeedViewEntry& Entry, uint8 LocalTeamId)
{
	if (Entry.bLocalPlayerInvolved)
	{
		// "You, in a list of people." Wins over the team colours: a line you are in reads
		// first, whether you got the kill or took it.
		return BR::Tokens::SelfWhite();
	}

	// 255 is the ViewModel's unknown-team sentinel, and a kill can legitimately arrive before
	// team assignment has replicated. Dim rather than guessing a side.
	if (LocalTeamId > 1 || Entry.KillerTeamId > 1)
	{
		return BR::Tokens::InkDim();
	}

	return (Entry.KillerTeamId == LocalTeamId) ? BR::Tokens::Shield() : BR::Tokens::TeamThem();
}
