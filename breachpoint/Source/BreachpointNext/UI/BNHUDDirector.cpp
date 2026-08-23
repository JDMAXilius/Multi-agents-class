#include "UI/BNHUDDirector.h"
#include "BreachpointNext.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "Data/BNDataRows.h"
#include "Data/BNGameData.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/PlayerController.h"
#include "Match/BNGameState.h"
#include "Match/BNPlayerState.h"
#include "UI/BNActivatableWidget.h"
#include "UI/BNUIManager.h"
#include "UI/BNViewModels.h"
#include "Weapons/BNEquipmentComponent.h"
#include "Weapons/BNWeapon.h"

#define LOCTEXT_NAMESPACE "BreachpointNextUI"

void UBNHUDDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UBNHUDDirector::HandlePostLoadMap);

	// The world this player is already in — PIE and the first map beat the delegate.
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UWorld* World = LocalPlayer->GetWorld())
		{
			BindToWorld(World);
		}
	}
}

void UBNHUDDirector::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	UnbindAll();
	Super::Deinitialize();
}

void UBNHUDDirector::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// Travel reset: every binding belongs to the OLD world's objects, every ViewModel value to
	// the old match. Unknown is the only honest opening state for the new one.
	UnbindAll();
	if (UBNVM_Combat* Combat = GetCombatVM()) { Combat->ClearToUnknown(); }
	if (UBNVM_Match* Match = GetMatchVM()) { Match->ClearToUnknown(); }

	if (LoadedWorld)
	{
		BindToWorld(LoadedWorld);
	}
}

void UBNHUDDirector::BindToWorld(UWorld* World)
{
	// Initialize AND PostLoadMap both land here for the same world in PIE — one subscription.
	if (BoundWorld.Get() == World)
	{
		return;
	}
	if (UWorld* Old = BoundWorld.Get())
	{
		Old->GameStateSetEvent.Remove(GameStateSetHandle);
	}
	BoundWorld = World;

	// Clients BEAT the GameState: at world arrival the GameState channel may not have opened
	// yet, so the set-event is the reliable edge and the direct read is the fast path.
	GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &UBNHUDDirector::HandleGameStateSet);
	if (AGameStateBase* Existing = World->GetGameState())
	{
		HandleGameStateSet(Existing);
	}

	EnsurePlayerBindings();
}

void UBNHUDDirector::HandleGameStateSet(AGameStateBase* InGameState)
{
	if (ABNGameState* BNGameState = Cast<ABNGameState>(InGameState))
	{
		BindGameState(BNGameState);
	}
}

void UBNHUDDirector::BindGameState(ABNGameState* InGameState)
{
	if (!InGameState || BoundGameState.Get() == InGameState)
	{
		return;
	}

	// A replaced GameState (travel edge) releases the old one's bindings first.
	if (ABNGameState* Old = BoundGameState.Get())
	{
		Old->OnMatchStateChanged.Remove(MatchStateHandle);
		Old->OnKillfeedChanged.Remove(KillfeedHandle);
	}

	BoundGameState = InGameState;
	MatchStateHandle = InGameState->OnMatchStateChanged.AddUObject(this, &UBNHUDDirector::HandleMatchStateChanged);
	KillfeedHandle = InGameState->OnKillfeedChanged.AddUObject(this, &UBNHUDDirector::HandleKillfeedChanged);

	// Subscribe, then read ONCE — the contract every feed documents. A joiner mid-match reads
	// the running match here, before any delegate has fired for them.
	EnsurePlayerBindings();
	PushMatchSnapshot();
	HandleKillfeedChanged();
	EnsureHUDShown();
}

void UBNHUDDirector::HandleMatchStateChanged(FName NewState)
{
	bPostMatch = (NewState == MatchState::WaitingPostMatch);

	// The controller can trail the GameState on a joining client; this broadcast fires from the
	// initial bunch, which makes it the third acquisition edge.
	EnsurePlayerBindings();
	EnsureHUDShown();
	PushMatchSnapshot();

	// The buzzer outranks the grave (critic): a player dead at match end stays dead — respawns
	// stop post-match — and an un-popped death screen would occlude the winner and the standings
	// for the whole post-match. The scoreboard is the post-match's screen; the death screen
	// yields to it.
	// PushMatchSnapshot above already rebuilt the roster for this transition — the pinned
	// post-match board reads a list that includes the winner mark, leavers dropped.
	if (bPostMatch)
	{
		SetDeathScreenWanted(false);
	}
	UpdateScoreboardVisibility();
}

void UBNHUDDirector::PushMatchSnapshot()
{
	const ABNGameState* GS = BoundGameState.Get();
	UBNVM_Match* Match = GetMatchVM();
	if (!GS || !Match)
	{
		return;
	}

	// The winner line is composed HERE, with the reference in hand. A null winner during
	// post-match is a draw — a legal outcome, worded, never blank.
	FText WinnerLine;
	EBNMatchOutcome Outcome = EBNMatchOutcome::Undecided;
	if (GS->HasMatchEnded())
	{
		const ABNPlayerState* Winner = GS->GetWinner();
		WinnerLine = Winner
			? FText::Format(LOCTEXT("WinnerBanner", "{0} WINS"), FText::FromString(Winner->GetPlayerName()))
			: LOCTEXT("DrawBanner", "DRAW");

		// The SAME null-winner case the line above words as "DRAW" — read once, branched once.
		// Victory vs Defeat can only be decided here: the widget has no idea which PlayerState
		// is mine, and giving it one would put a gameplay branch in a WBP.
		const ABNPlayerState* MyPS = BoundPlayerState.Get();
		Outcome = !Winner ? EBNMatchOutcome::Draw
			: (MyPS && Winner == MyPS ? EBNMatchOutcome::Victory : EBNMatchOutcome::Defeat);
	}

	Match->SetMatchPhase(GS->GetMatchState(), WinnerLine, Outcome);
	Match->SetMatchClock(GS->GetMatchEndServerTime(), BoundGameState.Get());
	RecomputeScores();
}

void UBNHUDDirector::RecomputeScores()
{
	const ABNGameState* GS = BoundGameState.Get();
	UBNVM_Match* Match = GetMatchVM();
	if (!GS || !Match)
	{
		return;
	}

	// GetLeaders walks the replicated PlayerArray, so the leader is client-computable — no new
	// replication bought this readout.
	TArray<ABNPlayerState*> Leaders;
	GS->GetLeaders(Leaders);
	const int32 TopKills = Leaders.Num() > 0 ? Leaders[0]->GetKills() : 0;

	const ABNPlayerState* MyPS = BoundPlayerState.Get();
	Match->SetScores(MyPS ? MyPS->GetKills() : 0, TopKills, GS->GetScoreLimit());

	// The roster, rebuilt on the edges this director actually owns — a kill, my score, a match
	// state change, and the moment the board is OPENED — then handed to the VM sorted, so the
	// scoreboard renders rows in order and never touches PlayerArray itself. There is NO
	// PlayerArray add/remove hook (critic): between those edges a join or a leave is not seen,
	// which is why opening the board recomputes. Cheap at FFA scale; the VM stays silent when
	// nothing rendered actually changed.
	const ABNPlayerState* Winner = GS->GetWinner();
	TArray<FBNScoreRowView> Rows;
	Rows.Reserve(GS->PlayerArray.Num());
	for (APlayerState* PS : GS->PlayerArray)
	{
		const ABNPlayerState* BNPS = Cast<ABNPlayerState>(PS);
		if (!BNPS)
		{
			continue;
		}
		FBNScoreRowView& Row = Rows.AddDefaulted_GetRef();
		Row.PlayerName = BNPS->GetPlayerName();
		Row.Kills = BNPS->GetKills();
		Row.Deaths = BNPS->GetDeaths();
		Row.bIsSelf = BNPS == MyPS;
		Row.bIsWinner = Winner && BNPS == Winner;
	}
	Rows.Sort([](const FBNScoreRowView& A, const FBNScoreRowView& B)
	{
		if (A.Kills != B.Kills) { return A.Kills > B.Kills; }
		if (A.Deaths != B.Deaths) { return A.Deaths < B.Deaths; }
		return A.PlayerName < B.PlayerName;
	});
	Match->SetRoster(MoveTemp(Rows));
}

const FBNWeaponRow* UBNHUDDirector::FindWeaponRow(FName RowName) const
{
	if (RowName.IsNone())
	{
		return nullptr;
	}
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	const UBNGameData* GameData = GameInstance ? GameInstance->GetSubsystem<UBNGameData>() : nullptr;
	// The SAME lookup ABNWeapon::GetRow makes — the table is the weapon's identity on every
	// machine, so a client resolves the killer's weapon without ever seeing the weapon actor.
	return GameData ? GameData->FindWeaponRow(RowName) : nullptr;
}

TSoftObjectPtr<UTexture2D> UBNHUDDirector::ResolveWeaponIcon(FName RowName) const
{
	const FBNWeaponRow* Row = FindWeaponRow(RowName);
	// Spelled out rather than a ternary with nullptr (R7.1's fix): TSoftObjectPtr and nullptr_t
	// deduce no common type.
	const TSoftObjectPtr<UTexture2D> NoIcon;
	return Row ? Row->Icon : NoIcon;
}

FText UBNHUDDirector::ComposeWeaponLabel(FName RowName) const
{
	if (RowName.IsNone())
	{
		return FText::GetEmpty();
	}
	const FBNWeaponRow* Row = FindWeaponRow(RowName);
	// The row's authored name, or the source name itself — the fallback IS the design for the
	// rowless causes (Melee, Grenade), which is why they were named as words in BNDamageSource.
	return (Row && !Row->DisplayName.IsEmpty()) ? Row->DisplayName : FText::FromName(RowName);
}

FText UBNHUDDirector::ComposeKillfeedLine(const FBNKillfeedRingEntry& Entry) const
{
	// The kill log's three wordings, verbatim — one decision, told the same way everywhere.
	// Names come from the entry's STRINGS: the refs can null under a leaver, the line survives.
	if (Entry.KillerName.IsEmpty())
	{
		return FText::Format(LOCTEXT("KillfeedDied", "{0} died"), FText::FromString(Entry.VictimName));
	}
	// Refs first (identity), CASE-SENSITIVE name second (critic: FString's == is
	// case-insensitive, and "Bob" must not eliminate themselves when "bob" kills them).
	const bool bSuicide = (Entry.Killer && Entry.Killer == Entry.Victim)
		|| Entry.KillerName.Equals(Entry.VictimName, ESearchCase::CaseSensitive);
	if (bSuicide)
	{
		return FText::Format(LOCTEXT("KillfeedSuicide", "{0} eliminated themselves"), FText::FromString(Entry.VictimName));
	}
	return FText::Format(LOCTEXT("KillfeedKill", "{0} eliminated {1}"),
		FText::FromString(Entry.KillerName), FText::FromString(Entry.VictimName));
}

void UBNHUDDirector::HandleKillfeedChanged()
{
	// A fourth acquisition edge, per kill (critic): if every early edge fired before the local
	// controller was linked, this turns the healing window from "the next match state change"
	// into "the next kill". Idempotent, like every bind inside it.
	EnsurePlayerBindings();

	const ABNGameState* GS = BoundGameState.Get();
	UBNVM_Match* Match = GetMatchVM();
	if (!GS || !Match)
	{
		return;
	}

	const ABNPlayerState* MyPS = BoundPlayerState.Get();
	const double ServerNow = GS->GetServerWorldTimeSeconds();
	for (const FBNKillfeedRingEntry& Entry : GS->GetKillfeed())
	{
		if (Entry.Sequence <= Match->GetLastKillfeedSequence())
		{
			continue;
		}

		// The join-age filter (critic, R7 W1): a joiner receives the whole ring in the initial
		// bunch, and last-few-minutes kills must not land as a burst of fresh lines. Pushed
		// through the VM anyway so the SEQUENCE advances — a skipped entry is seen, not pending.
		if (ServerNow - Entry.ServerTime > BNUITiming::KillfeedLingerSeconds)
		{
			Match->PushKillfeedEntry(FText::GetEmpty(), Entry.Sequence, false);
			continue;
		}

		// Refs when mapped, NAMES as the fallback (critic): the ring can land before this
		// client's PlayerState GUIDs resolve, and a dedupe-by-sequence reader never looks again
		// — so the self test must not depend on pointers alone. Case-sensitive on the names.
		const FString MyName = MyPS ? MyPS->GetPlayerName() : FString();
		const bool bVictimIsMe = MyPS &&
			(Entry.Victim == MyPS || Entry.VictimName.Equals(MyName, ESearchCase::CaseSensitive));
		const bool bInvolvesSelf = bVictimIsMe || (MyPS &&
			(Entry.Killer == MyPS || Entry.KillerName.Equals(MyName, ESearchCase::CaseSensitive)));
		// R7.6, gap 6 — the line AND its parts. The composed line stays the record (it is the
		// only correct render for the wordings that have no killer); the parts are what let a WBP
		// place [Killer][glyph][Victim] at the design's measured x. Empty for a suicide or a
		// world death, which is exactly when the row must fall back to the line.
		FText KillerPart;
		FText VictimPart;
		if (!Entry.KillerName.IsEmpty()
			&& !(Entry.Killer && Entry.Killer == Entry.Victim)
			&& !Entry.KillerName.Equals(Entry.VictimName, ESearchCase::CaseSensitive))
		{
			KillerPart = FText::FromString(Entry.KillerName);
			VictimPart = FText::FromString(Entry.VictimName);
		}

		Match->PushKillfeedEntry(ComposeKillfeedLine(Entry), Entry.Sequence, bInvolvesSelf,
			ResolveWeaponIcon(Entry.SourceName), KillerPart, VictimPart);

		// My own newest death names my killer — the death screen's line. Written from the feed
		// rather than a second channel: if the ring bunch lands after the dead tag, this catches
		// up the moment it arrives. The SAME fallback test as above (critic): gating this on the
		// ref alone loses the line forever when the ref was unmapped at the one read.
		if (bVictimIsMe)
		{
			if (UBNVM_Combat* Combat = GetCombatVM())
			{
				FText KilledBy;
				if (Entry.KillerName.IsEmpty()) { KilledBy = LOCTEXT("KilledByWorld", "Eliminated"); }
				else if (Entry.Killer == MyPS || Entry.KillerName.Equals(MyName, ESearchCase::CaseSensitive)) { KilledBy = LOCTEXT("KilledBySelf", "You eliminated yourself"); }
				else { KilledBy = FText::Format(LOCTEXT("KilledBy", "Eliminated by {0}"), FText::FromString(Entry.KillerName)); }
				// R7.3 — and the second line, WITH WHAT. Empty stays empty: a death the door
				// could not name shows the killer alone rather than a guessed weapon.
				Combat->SetKilledByLine(KilledBy, ComposeWeaponLabel(Entry.SourceName),
					ResolveWeaponIcon(Entry.SourceName));
			}
		}
	}

	// Every kill moves a score; the feed change is the one event that already covers all of
	// them, mine and the leader's alike.
	RecomputeScores();
}

void UBNHUDDirector::EnsurePlayerBindings()
{
	APlayerController* PC = GetOwnPlayerController();
	if (!PC)
	{
		return;
	}

	if (BoundController.Get() != PC)
	{
		BoundController = PC;
		// Dynamic delegate, so AddUniqueDynamic — re-running this on every edge must not stack.
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &UBNHUDDirector::HandlePossessedPawnChanged);
	}

	// PlayerState-scoped bindings, once per PlayerState (it survives every pawn).
	ABNPlayerState* PS = PC->GetPlayerState<ABNPlayerState>();
	if (PS && BoundPlayerState.Get() != PS)
	{
		if (ABNPlayerState* OldPS = BoundPlayerState.Get())
		{
			OldPS->OnScoreChanged.Remove(ScoreHandle);
			OldPS->OnRespawnStampChanged.Remove(RespawnStampHandle);
		}
		BoundPlayerState = PS;
		ScoreHandle = PS->OnScoreChanged.AddUObject(this, &UBNHUDDirector::HandleScoreChanged);
		RespawnStampHandle = PS->OnRespawnStampChanged.AddUObject(this, &UBNHUDDirector::HandleRespawnStampChanged);

		UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
		if (ASC && BoundASC.Get() != ASC)
		{
			if (UAbilitySystemComponent* OldASC = BoundASC.Get())
			{
				OldASC->RegisterGameplayTagEvent(BNTags::State_Dead, EGameplayTagEventType::NewOrRemoved).Remove(DeadTagHandle);
			}
			BoundASC = ASC;
			DeadTagHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &UBNHUDDirector::HandleDeadTagChanged);

			if (UBNVM_Combat* Combat = GetCombatVM())
			{
				// The injected-attribute decoupling: the VM learns WHICH attributes, never whose.
				FBNCombatAttributeBindings Bindings;
				Bindings.Health = UBNAttributeSet::GetHealthAttribute();
				Bindings.MaxHealth = UBNAttributeSet::GetMaxHealthAttribute();
				Bindings.Shield = UBNAttributeSet::GetShieldAttribute();
				Bindings.MaxShield = UBNAttributeSet::GetMaxShieldAttribute();
				Bindings.Grenades = UBNAttributeSet::GetGrenadesAttribute();
				Bindings.MaxGrenades = UBNAttributeSet::GetMaxGrenadesAttribute();
				Combat->BindToAbilitySystem(ASC, Bindings);

				// Read the standing state once: a joiner can arrive already dead or mid-count —
				// and the SCREEN follows the read (critic): a tag event never fires for a tag
				// that was present before the bind, so without this a late-bound death shows
				// the VM's dead state with no death screen until the NEXT death.
				const bool bStandingDead = ASC->HasMatchingGameplayTag(BNTags::State_Dead);
				Combat->SetDead(bStandingDead);
				SetDeathScreenWanted(bStandingDead);
				HandleRespawnStampChanged(PS);
			}
		}
	}

	if (APawn* Pawn = PC->GetPawn())
	{
		BindPawn(Pawn);
	}

	EnsureHUDShown();
}

void UBNHUDDirector::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	// The PlayerState/ASC layer is possession-stable; only the pawn-scoped wiring re-runs.
	BindPawn(NewPawn);
	// A fresh body can also be the FIRST body — the seam where everything becomes bindable.
	EnsurePlayerBindings();
}

void UBNHUDDirector::BindPawn(APawn* Pawn)
{
	UBNEquipmentComponent* Equipment = nullptr;
	if (const ABNCharacter* Character = Cast<ABNCharacter>(Pawn))
	{
		Equipment = Character->GetEquipmentComponent();
	}

	if (BoundEquipment.Get() != Equipment)
	{
		if (UBNEquipmentComponent* Old = BoundEquipment.Get())
		{
			Old->OnEquippedWeaponChanged.Remove(EquippedHandle);
		}
		UnbindWeapon();

		BoundEquipment = Equipment;
		if (Equipment)
		{
			EquippedHandle = Equipment->OnEquippedWeaponChanged.AddUObject(this, &UBNHUDDirector::HandleEquippedWeaponChanged);
			// Read once: the swap that equipped the current weapon fired before this bind.
			HandleEquippedWeaponChanged(Equipment, Equipment->GetCurrentWeapon());
		}
		else if (UBNVM_Combat* Combat = GetCombatVM())
		{
			// No body or not ours: the hand is UNKNOWN, not "unarmed" — dashes, no lie. The stowed
			// slot goes with it: an empty slot, not last life's next weapon.
			Combat->SetEquippedWeapon(FText::GetEmpty(), INDEX_NONE, INDEX_NONE, /*bKnown=*/false);
			Combat->SetStowedWeapon(FText::GetEmpty());
		}
	}
}

void UBNHUDDirector::HandleEquippedWeaponChanged(UBNEquipmentComponent* Equipment, ABNWeapon* Current)
{
	if (BoundWeapon.Get() != Current)
	{
		UnbindWeapon();
		BoundWeapon = Current;
		if (Current)
		{
			AmmoHandle = Current->OnAmmoChanged.AddUObject(this, &UBNHUDDirector::HandleAmmoChanged);
		}
	}

	UBNVM_Combat* Combat = GetCombatVM();
	if (!Combat)
	{
		return;
	}

	// R7.3 — the stowed slot, pushed on EVERY hand change including the unarmed one: what one
	// swap press gives you changes with the current index, so the two readings move together or
	// the tray lies.
	//
	// THE TWO NULLS ARE DIFFERENT (and the carry really does hold a null unarmed slot): no next
	// slot at all clears the line, while a next slot holding nothing says "Unarmed" — the same
	// word the hand uses for the same state, because the swap will genuinely empty your hands.
	const bool bHasStowedSlot = Equipment && Equipment->HasNextSlot();
	const ABNWeapon* Next = bHasStowedSlot ? Equipment->GetNextWeapon() : nullptr;
	const FBNWeaponRow* NextRow = Next ? Next->GetRow() : nullptr;
	const TSoftObjectPtr<UTexture2D> NoStowedIcon;
	FText StowedName = FText::GetEmpty();
	if (bHasStowedSlot)
	{
		StowedName = Next
			? (NextRow ? NextRow->DisplayName : FText::FromName(Next->GetRowName()))
			: LOCTEXT("UnarmedName", "Unarmed");
	}
	Combat->SetStowedWeapon(StowedName, NextRow ? NextRow->Icon : NoStowedIcon);

	if (!Current)
	{
		// The Unarmed SLOT — a real, known state of the hand, unlike the no-equipment Unknown.
		Combat->SetEquippedWeapon(LOCTEXT("UnarmedName", "Unarmed"), INDEX_NONE, INDEX_NONE, /*bKnown=*/true);
		return;
	}

	const TSoftObjectPtr<UTexture2D> NoIcon;
	const FBNWeaponRow* Row = Current->GetRow();
	const FText Name = Row ? Row->DisplayName : FText::FromName(Current->GetRowName());
	// A magazine-less row (the knife) shows dashes, never a confident 0/0.
	const bool bHasMagazine = Current->GetMagazineSize() > 0;
	Combat->SetEquippedWeapon(Name,
		bHasMagazine ? Current->GetCurrentAmmo() : static_cast<int32>(INDEX_NONE),
		bHasMagazine ? Current->GetAmmoReserve() : static_cast<int32>(INDEX_NONE),
		/*bKnown=*/true,
		// R7.1 — the silhouette, straight off the row. Soft all the way to the widget, which
		// loads it through Slate's own async path; an unset column simply draws nothing.
		// The empty soft ptr is SPELLED OUT rather than nullptr: the compiled reference records
		// that a ternary between TSoftObjectPtr<T> and nullptr_t does not deduce a common type.
		Row ? Row->Icon : NoIcon,
		// The per-weapon reticle rides the same edge: one row read, both slots fed, and a swap
		// moves them together because they are one notify.
		Row ? Row->Reticle : NoIcon);
}

void UBNHUDDirector::HandleAmmoChanged(ABNWeapon* Weapon)
{
	UBNVM_Combat* Combat = GetCombatVM();
	if (Combat && Weapon && Weapon == BoundWeapon.Get() && Weapon->GetMagazineSize() > 0)
	{
		Combat->SetAmmo(Weapon->GetCurrentAmmo(), Weapon->GetAmmoReserve());
	}
}

void UBNHUDDirector::UnbindWeapon()
{
	if (ABNWeapon* Old = BoundWeapon.Get())
	{
		Old->OnAmmoChanged.Remove(AmmoHandle);
	}
	BoundWeapon.Reset();
	AmmoHandle.Reset();
}

void UBNHUDDirector::HandleScoreChanged(ABNPlayerState* ChangedPlayerState)
{
	RecomputeScores();
}

void UBNHUDDirector::HandleRespawnStampChanged(ABNPlayerState* OwnPlayerState)
{
	if (UBNVM_Combat* Combat = GetCombatVM())
	{
		Combat->SetRespawnStamp(OwnPlayerState ? OwnPlayerState->GetRespawnAtServerTime() : 0.0, BoundGameState.Get());
	}
}

void UBNHUDDirector::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const bool bDead = NewCount > 0;
	if (UBNVM_Combat* Combat = GetCombatVM())
	{
		Combat->SetDead(bDead);
	}
	SetDeathScreenWanted(bDead);
}

void UBNHUDDirector::EnsureHUDShown()
{
	// Both halves or nothing: a BN GameState to project and a controller to own the widgets.
	if (HUDWidget.IsValid() || !BoundGameState.IsValid() || !GetOwnPlayerController())
	{
		return;
	}

	UBNUIManager* Manager = UBNUIManager::Get(GetLocalPlayer());
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!Manager || !LocalPlayer)
	{
		return;
	}

	HUDWidget = Manager->PushWidgetToLayer(LocalPlayer, FBNUITags::Get().Layer_Game, Manager->GetHUDLayoutClass());
	if (HUDWidget.IsValid())
	{
		UE_LOG(LogBN, Log, TEXT("BNUI: HUD up for %s."), *GetNameSafe(LocalPlayer));
	}
}

void UBNHUDDirector::SetDeathScreenWanted(bool bWanted)
{
	bDeathScreenWanted = bWanted;
	UpdateGameMenuLayer();
}

void UBNHUDDirector::SetScoreboardHeld(bool bHeld)
{
	bScoreboardHeld = bHeld;

	// Recompute BEFORE showing (critic's blocking find): with no PlayerArray hook, a lobby that
	// filled with bots after the GameState bind — or a leaver — is invisible to the roster until
	// the next kill. Opening the board is the one moment its staleness is guaranteed to be seen,
	// so it is the one moment worth paying for a rebuild.
	if (bHeld)
	{
		RecomputeScores();
	}

	UpdateScoreboardVisibility();
}

void UBNHUDDirector::OpenPauseMenu()
{
	// Dead players do not get a menu, and the request is REFUSED rather than queued: latching it
	// would pop a pause menu at the moment of respawn — the exact surprise this rework removes.
	if (bDeathScreenWanted)
	{
		UE_LOG(LogBN, Verbose, TEXT("BNUI: pause refused — the death screen owns Layer.GameMenu."));
		return;
	}

	bPauseRequested = true;
	UpdateGameMenuLayer();
}

void UBNHUDDirector::NotifyPauseClosed()
{
	// Intent only. The layer is not re-evaluated here on purpose: this is called from INSIDE the
	// removal path (a stack deactivates as it pops), and re-entering the decision mid-removal is
	// how single-owner logic grows a second owner again.
	bPauseRequested = false;
	PauseWidget.Reset();
}

void UBNHUDDirector::UpdateGameMenuLayer()
{
	UBNUIManager* Manager = UBNUIManager::Get(GetLocalPlayer());
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!Manager || !LocalPlayer)
	{
		return;
	}

	const FUITag Layer = FBNUITags::Get().Layer_GameMenu;

	// PAUSE FIRST, and always: whether it should go up or come down, it is settled before the
	// death screen touches the stack, so the two can never be resident together.
	const bool bWantPause = bPauseRequested && !bDeathScreenWanted;
	if (!bWantPause && PauseWidget.IsValid())
	{
		// Clear the pointer BEFORE removing: RemoveWidget deactivates, the screen calls
		// NotifyPauseClosed synchronously, and it must not stomp a pointer we still need.
		UBNActivatableWidget* Closing = PauseWidget.Get();
		PauseWidget.Reset();
		bPauseRequested = false;
		Manager->RemoveWidgetFromLayer(LocalPlayer, Layer, Closing);
	}

	if (bDeathScreenWanted && !DeathScreenWidget.IsValid())
	{
		DeathScreenWidget = Manager->PushWidgetToLayer(LocalPlayer, Layer, Manager->GetDeathScreenClass());
	}
	else if (!bDeathScreenWanted && DeathScreenWidget.IsValid())
	{
		Manager->RemoveWidgetFromLayer(LocalPlayer, Layer, DeathScreenWidget.Get());
		DeathScreenWidget.Reset();
	}

	if (bWantPause && !PauseWidget.IsValid())
	{
		PauseWidget = Manager->PushWidgetToLayer(LocalPlayer, Layer, Manager->GetPauseScreenClass());
	}
}

void UBNHUDDirector::UpdateScoreboardVisibility()
{
	// ONE decision: held OR pinned by the post-match. The widget never decides its own life.
	const bool bWantVisible = bScoreboardHeld || bPostMatch;

	UBNUIManager* Manager = UBNUIManager::Get(GetLocalPlayer());
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!Manager || !LocalPlayer)
	{
		return;
	}

	if (bWantVisible && !ScoreboardWidget.IsValid())
	{
		ScoreboardWidget = Manager->PushWidgetToLayer(LocalPlayer, FBNUITags::Get().Layer_Game, Manager->GetScoreboardClass());
	}
	else if (!bWantVisible && ScoreboardWidget.IsValid())
	{
		Manager->RemoveWidgetFromLayer(LocalPlayer, FBNUITags::Get().Layer_Game, ScoreboardWidget.Get());
		ScoreboardWidget.Reset();
	}
}

UBNVM_Combat* UBNHUDDirector::GetCombatVM() const
{
	const UBNUIManager* Manager = UBNUIManager::Get(GetLocalPlayer());
	return Manager ? const_cast<UBNUIManager*>(Manager)->GetCombatViewModel(GetLocalPlayer()) : nullptr;
}

UBNVM_Match* UBNHUDDirector::GetMatchVM() const
{
	const UBNUIManager* Manager = UBNUIManager::Get(GetLocalPlayer());
	return Manager ? const_cast<UBNUIManager*>(Manager)->GetMatchViewModel(GetLocalPlayer()) : nullptr;
}

APlayerController* UBNHUDDirector::GetOwnPlayerController() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()) : nullptr;
}

void UBNHUDDirector::UnbindAll()
{
	if (UWorld* World = BoundWorld.Get())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
	}
	BoundWorld.Reset();
	GameStateSetHandle.Reset();

	if (ABNGameState* GS = BoundGameState.Get())
	{
		GS->OnMatchStateChanged.Remove(MatchStateHandle);
		GS->OnKillfeedChanged.Remove(KillfeedHandle);
	}
	BoundGameState.Reset();
	MatchStateHandle.Reset();
	KillfeedHandle.Reset();

	if (ABNPlayerState* PS = BoundPlayerState.Get())
	{
		PS->OnScoreChanged.Remove(ScoreHandle);
		PS->OnRespawnStampChanged.Remove(RespawnStampHandle);
	}
	BoundPlayerState.Reset();
	ScoreHandle.Reset();
	RespawnStampHandle.Reset();

	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->RegisterGameplayTagEvent(BNTags::State_Dead, EGameplayTagEventType::NewOrRemoved).Remove(DeadTagHandle);
	}
	BoundASC.Reset();
	DeadTagHandle.Reset();

	if (UBNEquipmentComponent* Equipment = BoundEquipment.Get())
	{
		Equipment->OnEquippedWeaponChanged.Remove(EquippedHandle);
	}
	BoundEquipment.Reset();
	EquippedHandle.Reset();

	UnbindWeapon();

	if (APlayerController* PC = BoundController.Get())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &UBNHUDDirector::HandlePossessedPawnChanged);
	}
	BoundController.Reset();

	// The widgets died with their world; the weak ptrs just need to agree.
	HUDWidget.Reset();
	DeathScreenWidget.Reset();
	ScoreboardWidget.Reset();
	PauseWidget.Reset();
	bScoreboardHeld = false;
	bPostMatch = false;
	bDeathScreenWanted = false;
	bPauseRequested = false;
}

#undef LOCTEXT_NAMESPACE
