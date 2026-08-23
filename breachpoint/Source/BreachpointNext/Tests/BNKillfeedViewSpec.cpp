#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/BNUITypes.h"
#include "UI/BNViewModels.h"

/**
 * THE FEED'S VIEW RING — the half of the killfeed that needs no world, no ASC and no replication:
 * a ViewModel handed lines and asked what is on screen.
 *
 * Every rule below was written for a real hazard, and each one is invisible until it breaks in
 * front of a player:
 *   - DEDUPE, because the GameState ring replicates WHOLE. A joining client receives every entry
 *     again, and index positions shift as the ring trims from the front, so the server's monotonic
 *     Sequence is the only safe identity.
 *   - THE EMPTY LINE, because the director's join-age filter has to mark an old kill SEEN without
 *     showing it. A skipped entry must still advance the sequence or it comes back forever.
 *   - THE CAP, because the WBP builds a FIXED pool of rows and the C++ claims them; a view holding
 *     more than the pool would silently drop the newest lines, which are the ones that matter.
 *
 * The cap is asserted as a literal 5 on purpose. It is private to the ViewModel and it is the same
 * 5 the WBP places as children — if one moves, this spec should fail until the other moves too.
 */
BEGIN_DEFINE_SPEC(FBNKillfeedViewSpec, "BreachpointNext.Sim.KillfeedView",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	UBNVM_Match* Match = nullptr;
	int32 ChangeBroadcasts = 0;

	void Push(int32 Sequence, const TCHAR* Line, bool bInvolvesSelf = false)
	{
		Match->PushKillfeedEntry(FText::FromString(Line), Sequence, bInvolvesSelf);
	}

	int32 Shown() const { return Match->GetKillfeedEntries().Num(); }

	FString LineAt(int32 Index) const
	{
		const TArray<FBNKillfeedViewEntry>& Entries = Match->GetKillfeedEntries();
		return Entries.IsValidIndex(Index) ? Entries[Index].Line.ToString() : FString(TEXT("<none>"));
	}

END_DEFINE_SPEC(FBNKillfeedViewSpec)

void FBNKillfeedViewSpec::Define()
{
	BeforeEach([this]()
	{
		Match = NewObject<UBNVM_Match>(GetTransientPackage(), NAME_None, RF_Transient);
		Match->AddToRoot();

		ChangeBroadcasts = 0;
		Match->OnKillfeedViewChanged.AddLambda([this]() { ++ChangeBroadcasts; });
	});

	AfterEach([this]()
	{
		if (Match)
		{
			Match->OnKillfeedViewChanged.Clear();
			Match->RemoveFromRoot();
			Match = nullptr;
		}
	});

	It("shows a pushed line, oldest first", [this]()
	{
		Push(1, TEXT("Marcus eliminated Vale"));
		Push(2, TEXT("Vale eliminated Rook"));

		TestEqual(TEXT("count"), Shown(), 2);
		TestEqual(TEXT("oldest is first"), LineAt(0), FString(TEXT("Marcus eliminated Vale")));
		TestEqual(TEXT("newest is last"), LineAt(1), FString(TEXT("Vale eliminated Rook")));
		TestEqual(TEXT("one broadcast per shown push"), ChangeBroadcasts, 2);
	});

	It("ignores an entry it has already shown", [this]()
	{
		Push(1, TEXT("Marcus eliminated Vale"));
		Push(1, TEXT("Marcus eliminated Vale"));
		// A whole ring re-replicating is the real case: same entries, same sequences.
		Push(1, TEXT("Marcus eliminated Vale"));

		TestEqual(TEXT("count"), Shown(), 1);
		TestEqual(TEXT("last sequence"), Match->GetLastKillfeedSequence(), 1);
	});

	It("ignores an entry older than the last one shown", [this]()
	{
		Push(5, TEXT("Rook eliminated Juno"));
		Push(2, TEXT("an older bunch, arriving late"));

		TestEqual(TEXT("count"), Shown(), 1);
		TestEqual(TEXT("last sequence stays at the newest"), Match->GetLastKillfeedSequence(), 5);
	});

	It("marks a filtered entry SEEN without showing it", [this]()
	{
		// The director's join-age filter: the line is empty, so nothing is drawn, but the sequence
		// must advance or the entry arrives again on the next replication and never stops.
		Push(3, TEXT(""));

		TestEqual(TEXT("nothing drawn"), Shown(), 0);
		TestEqual(TEXT("but it counts as seen"), Match->GetLastKillfeedSequence(), 3);
		TestEqual(TEXT("no broadcast for a line nobody sees"), ChangeBroadcasts, 0);

		// And the next real line still lands.
		Push(4, TEXT("Juno eliminated Rook"));
		TestEqual(TEXT("count"), Shown(), 1);
	});

	It("keeps only the newest five, dropping from the front", [this]()
	{
		for (int32 i = 1; i <= 7; ++i)
		{
			Push(i, *FString::Printf(TEXT("kill %d"), i));
		}

		TestEqual(TEXT("the pool size the WBP builds"), Shown(), 5);
		TestEqual(TEXT("oldest survivor"), LineAt(0), FString(TEXT("kill 3")));
		TestEqual(TEXT("newest"), LineAt(4), FString(TEXT("kill 7")));
	});

	It("carries the self flag through to the view", [this]()
	{
		Push(1, TEXT("Marcus eliminated YOU"), /*bInvolvesSelf=*/true);
		const TArray<FBNKillfeedViewEntry>& Entries = Match->GetKillfeedEntries();
		TestTrue(TEXT("the row that mentions me is marked"), Entries.Num() == 1 && Entries[0].bInvolvesSelf);
	});

	It("clears back to nothing on travel", [this]()
	{
		Push(1, TEXT("kill 1"));
		Match->ClearToUnknown();
		TestEqual(TEXT("count"), Shown(), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
