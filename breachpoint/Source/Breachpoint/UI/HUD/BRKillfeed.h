#pragma once

#include "UI/BRActivatableWidget.h"

#include "BRKillfeed.generated.h"

class UBRKillfeedEntryWidget;
class UPanelWidget;
struct FBRKillfeedViewEntry;

/**
 * `UBRKillfeed` -- the recycled feed (`ue5-ui-architecture` Sec 5).
 *
 * THE POOL IS THE POINT. Rows are created ONCE, on first activation, parented to
 * `EntryContainer` once, and thereafter CLAIMED by index and RELEASED by collapsing. There is
 * no `CreateWidget` and no `AddChild`/`RemoveFromParent` anywhere on the per-kill path,
 * because the per-kill path runs in the middle of a firefight -- the worst possible moment to
 * allocate a widget, build its Slate tree and invalidate the container's layout.
 *
 * THE RING BUFFER IS AUTHORITATIVE, THIS IS A PROJECTION. `UBRVM_Match` holds the entries and
 * owns their lifetime (`PushKillfeedEntry` caps the ring, `ExpireKillfeedEntries` ages them
 * out on a timer). This widget renders a window over that array and stores no feed state of
 * its own: nothing here decides what a kill is, when it appeared, or when it goes away. Every
 * refresh re-reads the array, which is also why a row's Spotter line can arrive seconds late
 * and simply appear.
 *
 * POOL EXHAUSTION DROPS THE OLDEST AND SAYS SO. If the ring ever holds more entries than the
 * pool has rows, the oldest are dropped and the drop is LOGGED. A silent cap reads as "the
 * feed covered everything" when it did not, which is the failure the honesty law exists to
 * stop. Pool size and ring cap are read from the same setting
 * (`UBRUISettings::KillfeedMaxVisibleEntries`), so the two can only diverge if someone changes
 * one of them -- and this log is how they find out.
 *
 * THE SPOTTER NEVER GATES A KILL. An entry renders the instant it is pushed, with whatever
 * `SpotterLine` it has (usually empty). When the line lands later,
 * `UBRVM_Match::AppendSpotterLine` re-broadcasts and the SAME row re-renders in place. The
 * feed must not wait on the LLM and must not reflow when it answers: offline is the identical
 * HUD minus the flavour text. Reserving the append slot is the WBP's job -- see the class
 * comment gap below.
 *
 * NO AUTHORITY: reads the ViewModel, nothing else. No GameState, no PlayerState, no RPC. It is
 * non-interactive, takes no focus, and cannot trap gamepad navigation.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRKillfeed : public UBRActivatableWidget
{
	GENERATED_BODY()

public:
	UBRKillfeed();

	/** How many rows the pool was built with. Zero means the entry class never resolved. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Killfeed")
	int32 GetPoolSize() const { return Rows.Num(); }

protected:
	//~ Begin UBRActivatableWidget interface
	virtual void BindViewModels() override;
	virtual void UnbindViewModels() override;
	//~ End UBRActivatableWidget interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	/**
	 * Where the pooled rows live for the whole lifetime of the widget. A `UVerticalBox` in
	 * practice; typed as the panel base so the WBP can use whatever stacks them.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|Killfeed")
	TObjectPtr<UPanelWidget> EntryContainer;

private:
	/** Builds the fixed pool. Runs at most once; a no-op on every later activation. */
	void EnsurePool();

	/** Re-projects the ViewModel's ring onto the pool. The only place a row is written. */
	void Refresh();

	void HandleKillfeedChanged();

	/** All rows collapsed, nothing claimed. The honest empty feed. */
	void ReleaseAllRows();

	/**
	 * `visr` channel for one row. White is "you, in a list of people"; `team-them` is the other
	 * team; `shield` is a team-mate; `ink-dim` is a kill whose teams have not replicated.
	 * Threat red is NOT spent here -- a killfeed line is history, not an incoming threat.
	 */
	static FLinearColor ResolveEntryTint(const FBRKillfeedViewEntry& Entry, uint8 LocalTeamId);

	/** The pool. Index-stable, always parented to `EntryContainer`, never resized after build. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBRKillfeedEntryWidget>> Rows;
};
