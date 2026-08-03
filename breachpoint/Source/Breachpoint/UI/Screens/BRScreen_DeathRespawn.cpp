#include "UI/Screens/BRScreen_DeathRespawn.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/BRUITypes.h"
#include "UI/BRViewModels.h"
#include "UI/Components/BRComponentTokens.h"
#include "UI/Styles/BRUITokens.h"

const FName UBRScreen_DeathRespawn::RespawnRingSweepParameterName(TEXT("Sweep"));

UBRScreen_DeathRespawn::UBRScreen_DeathRespawn()
{
	// Pushed by the UI manager, activated by the stack. NOT auto-activate: a screen that
	// activates itself on construction is a second navigation spine.
	bAutoActivate = false;

	// Explicit, because the default being right is not the same as the decision being made.
	// Back/cancel while dead must reach whatever is beneath (the pause stack), not be eaten by
	// an overlay the player cannot dismiss anyway -- respawn is on the server's timer and there
	// is nothing here to back out of. Back-handling belongs to the stack.
	bIsBackHandler = false;
}

TOptional<FUIInputConfig> UBRScreen_DeathRespawn::GetDesiredInputConfig() const
{
	// GAME MODE, CAPTURED, CURSOR HIDDEN -- and this is the one screen where the choice can
	// strand a player, so here is the whole reasoning.
	//
	// WHY NOT `Menu`: `ECommonInputMode::Menu` routes input to the widget stack and takes it off
	// the player controller. Every gameplay-side action a dead player still needs would stop
	// arriving: the scoreboard hold (assumed to stay available while dead -- being dead is
	// exactly when you look at the score), the look input that keeps the kill cam alive, and any
	// future respawn confirm. The failure mode is not cosmetic: the overlay pops on death, the
	// player's buttons stop doing anything, and with no interactive widget on screen there is
	// nothing to press to get out. That is the stranding this screen exists to avoid.
	//
	// WHY NOT `All`: `All` exists to let a menu coexist with gameplay input. There is no menu
	// here -- this screen has ZERO focusable widgets (see below) -- so `All` would buy nothing
	// and would turn the mouse cursor back on over a full-screen kill cam.
	//
	// WHY THIS IS NOT `InputMode` on the base class: that property is `EditDefaultsOnly`, so the
	// WBP could set it to `Menu` and silently reintroduce exactly the stranding above. Overriding
	// here takes the decision out of the asset. This is deliberate divergence from the base's
	// enum, not an oversight.
	//
	// GAMEPAD PATH, MOUSE UNPLUGGED: nothing on this screen takes focus. The killer line, the
	// ring, the countdown and the nested match band are all non-interactive text and images, and
	// the `prompt` row (0,596,1280,15) is a `UBRButtonPrompt` GLYPH -- it renders the binding, it
	// is not a button. So the screen cannot trap gamepad navigation and cannot present a
	// mouse-only path: every action reachable while dead is a gameplay binding on the player
	// controller, which `Game` mode leaves untouched. There is no click-only affordance here
	// because there is no affordance here at all.
	return FUIInputConfig(
		ECommonInputMode::Game,
		EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown,
		/*bHideCursorDuringViewportCapture=*/true);
}

void UBRScreen_DeathRespawn::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Gap 2 (see the header): no ViewModel turns a `DamageTag` into a weapon display name, and
	// `FBRWeaponRow::IconSoftPath` is BP25 and still open. Collapsed, not hidden: an empty frame
	// holding 200x34 of layout under the killer's name is a hole a reviewer reads as a bug.
	if (KillingWeaponRoot)
	{
		KillingWeaponRoot->SetVisibility(ESlateVisibility::Collapsed);
	}

	// First frame on screen is an honest unknown, before any ViewModel has been consulted.
	ClearToUnknown();
}

void UBRScreen_DeathRespawn::BindViewModels()
{
	Super::BindViewModels();

	UBRVM_Match* Match = GetMatchViewModel();
	if (!Match)
	{
		// Legal, and not rare: the death overlay can be raised on the frames after travel or a
		// join, before the subsystem has built this local player's ViewModel. Show the unknown
		// state and wait to be re-activated rather than naming a killer nobody reported.
		ClearToUnknown();
		return;
	}

	// One subscription. The killer's name is read out of the killfeed ring, so the ring's own
	// change broadcast is the only signal this screen needs -- `UBRKillfeed` uses the same one.
	// `LocalTeamId` is NOT subscribed: it is pushed once at possession, which is necessarily
	// before any death, so it is already known by the time this screen can exist.
	//
	// TODO(BP23): add a FieldNotify subscription to `RespawnCountdownText` here when
	// `UBRVM_Match` grows it. That is the whole wiring -- the ViewModel's existing 1 Hz
	// server-second timer already re-derives it, and this widget must not arm a second one.
	Match->OnKillfeedChanged().AddUObject(this, &UBRScreen_DeathRespawn::HandleKillfeedChanged);

	Refresh();
}

void UBRScreen_DeathRespawn::UnbindViewModels()
{
	// Unbind on deactivate, unconditionally: a ViewModel binding that outlives its widget is the
	// crash the death cam finds (`ue5-ui-architecture` Sec 2) -- and this IS the death cam.
	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		Match->OnKillfeedChanged().RemoveAll(this);
	}

	Super::UnbindViewModels();
}

void UBRScreen_DeathRespawn::HandleKillfeedChanged()
{
	Refresh();
}

void UBRScreen_DeathRespawn::Refresh()
{
	const UBRVM_Match* Match = GetMatchViewModel();
	if (!Match)
	{
		ClearToUnknown();
		return;
	}

	const FBRKillfeedViewEntry* DeathEntry = FindLocalDeathEntry();

	if (KillerNameText)
	{
		if (!DeathEntry || DeathEntry->KillerName.IsEmpty())
		{
			// The kill can reach the screen before it reaches the feed, and a fall or a
			// killzone has no killer at all. The dash is the true statement in both cases.
			KillerNameText->SetText(BRUI::UnknownValueText());
			KillerNameText->SetColorAndOpacity(FSlateColor(BR::Tokens::InkDim()));
		}
		else
		{
			KillerNameText->SetText(DeathEntry->KillerName);

			// Deliberately NOT `UBRKillfeed::ResolveEntryTint`: that function colours a ROW the
			// local player appears in and correctly returns `SelfWhite` for it. Here the local
			// player is the victim and the text is somebody else, so the useful statement is
			// which side they were on. 255 is the ViewModel's unknown-team sentinel -- dim
			// rather than guess a side. Threat red is absent on purpose: a name is not incoming
			// damage, and spending the threat channel on it means the channel says nothing when
			// damage actually is incoming.
			const uint8 LocalTeamId = Match->GetLocalTeamId();
			const FLinearColor KillerTint =
				(LocalTeamId > 1 || DeathEntry->KillerTeamId > 1)
					? BR::Tokens::InkDim()
					: (DeathEntry->KillerTeamId == LocalTeamId ? BR::Tokens::Shield()
															   : BR::Tokens::TeamThem());

			KillerNameText->SetColorAndOpacity(FSlateColor(KillerTint));
		}
	}

	ApplyRespawnCountdown();
}

void UBRScreen_DeathRespawn::ApplyRespawnCountdown()
{
	// GAP 1, BP23, AND THE ONLY HONEST RENDERING OF IT.
	//
	// `UBRVM_Match` exposes no respawn countdown of any kind today -- grep it: match clock,
	// rocket clock, nothing else. `ABRGameMode::StartRespawnTimer` computes the number and it
	// never leaves the server. So there is no value to show, and the three ways to fake one are
	// all worse than a dash: a local `SetTimer` guesses a duration nobody has specified, a zero
	// tells the player they can respawn now, and hiding the ring hides the fact that the screen
	// is incomplete.
	//
	// When BP23 lands, this function becomes two lines against
	// `Match->GetRespawnSecondsRemaining()` / `GetRespawnCountdownText()` and the sweep becomes
	// `Remaining / TotalWait`. Nothing else in this file changes.
	if (RespawnCountdownText)
	{
		RespawnCountdownText->SetText(BRUI::UnknownValueText());
	}

	if (RespawnRingSweep)
	{
		// Push the parameter anyway so the contract is exercised the moment the material exists:
		// `GetDynamicMaterial` returns null while the brush is still a plain image or colour, and
		// this is the only place in the project that names the parameter.
		if (UMaterialInstanceDynamic* SweepMaterial = RespawnRingSweep->GetDynamicMaterial())
		{
			SweepMaterial->SetScalarParameterValue(RespawnRingSweepParameterName, 0.0f);
		}

		// Hidden, not collapsed: the sweep is stacked on the static track circle in a canvas
		// slot, so collapsing it would not move anything and would only cost a layout pass. A
		// zero-length sweep left visible would read as "0% elapsed", which is a claim about the
		// timer, and the truth is that there is no timer.
		RespawnRingSweep->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UBRScreen_DeathRespawn::ClearToUnknown()
{
	if (KillerNameText)
	{
		KillerNameText->SetText(BRUI::UnknownValueText());
		KillerNameText->SetColorAndOpacity(FSlateColor(BR::Tokens::InkDim()));
	}

	if (RespawnCountdownText)
	{
		RespawnCountdownText->SetText(BRUI::UnknownValueText());
	}

	if (RespawnRingSweep)
	{
		RespawnRingSweep->SetVisibility(ESlateVisibility::Hidden);
	}
}

const FBRKillfeedViewEntry* UBRScreen_DeathRespawn::FindLocalDeathEntry() const
{
	const UBRVM_Match* Match = GetMatchViewModel();
	if (!Match)
	{
		return nullptr;
	}

	const uint8 LocalTeamId = Match->GetLocalTeamId();
	const TArray<FBRKillfeedViewEntry>& Entries = Match->GetKillfeedEntries();

	// Newest first: this screen is raised by THIS death, so the most recent line the local
	// player is in is it. The team test rejects the one common wrong answer -- a trade where my
	// own kill of an enemy was pushed after my death -- because on my death the victim is on my
	// team. It cannot reject a teamkill (killer and victim share my team either way); see the
	// reported gap on `bLocalPlayerInvolved` in the header.
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		const FBRKillfeedViewEntry& Entry = Entries[Index];
		if (!Entry.bLocalPlayerInvolved)
		{
			continue;
		}

		if (LocalTeamId > 1 || Entry.VictimTeamId == LocalTeamId)
		{
			return &Entry;
		}
	}

	return nullptr;
}
