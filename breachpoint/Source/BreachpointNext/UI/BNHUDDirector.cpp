#include "UI/BNHUDDirector.h"
#include "BreachpointNext.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "Data/BNDataRows.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/PlayerController.h"
#include "Match/BNGameState.h"
#include "Match/BNPlayerState.h"
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
	// Clients BEAT the GameState: at world arrival the GameState channel may not have opened
	// yet, so the set-event is the reliable edge and the direct read is the fast path.
	World->GameStateSetEvent.AddUObject(this, &UBNHUDDirector::HandleGameStateSet);
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
	if (GS->HasMatchEnded())
	{
		const ABNPlayerState* Winner = GS->GetWinner();
		WinnerLine = Winner
			? FText::Format(LOCTEXT("WinnerBanner", "{0} WINS"), FText::FromString(Winner->GetPlayerName()))
			: LOCTEXT("DrawBanner", "DRAW");
	}

	Match->SetMatchPhase(GS->GetMatchState(), WinnerLine);
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
}

FText UBNHUDDirector::ComposeKillfeedLine(const FBNKillfeedEntry& Entry) const
{
	// The kill log's three wordings, verbatim — one decision, told the same way everywhere.
	// Names come from the entry's STRINGS: the refs can null under a leaver, the line survives.
	if (Entry.KillerName.IsEmpty())
	{
		return FText::Format(LOCTEXT("KillfeedDied", "{0} died"), FText::FromString(Entry.VictimName));
	}
	if (Entry.KillerName == Entry.VictimName)
	{
		return FText::Format(LOCTEXT("KillfeedSuicide", "{0} eliminated themselves"), FText::FromString(Entry.VictimName));
	}
	return FText::Format(LOCTEXT("KillfeedKill", "{0} eliminated {1}"),
		FText::FromString(Entry.KillerName), FText::FromString(Entry.VictimName));
}

void UBNHUDDirector::HandleKillfeedChanged()
{
	const ABNGameState* GS = BoundGameState.Get();
	UBNVM_Match* Match = GetMatchVM();
	if (!GS || !Match)
	{
		return;
	}

	const ABNPlayerState* MyPS = BoundPlayerState.Get();
	const double ServerNow = GS->GetServerWorldTimeSeconds();
	for (const FBNKillfeedEntry& Entry : GS->GetKillfeed())
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
		// — so the self test must not depend on pointers alone.
		const bool bInvolvesSelf = MyPS &&
			(Entry.Victim == MyPS || Entry.Killer == MyPS ||
			 Entry.VictimName == MyPS->GetPlayerName() || Entry.KillerName == MyPS->GetPlayerName());
		Match->PushKillfeedEntry(ComposeKillfeedLine(Entry), Entry.Sequence, bInvolvesSelf);

		// My own newest death names my killer — the death screen's line. Written from the feed
		// rather than a second channel: if the ring bunch lands after the dead tag, this catches
		// up the moment it arrives.
		if (MyPS && Entry.Victim == MyPS)
		{
			if (UBNVM_Combat* Combat = GetCombatVM())
			{
				FText KilledBy;
				if (Entry.KillerName.IsEmpty()) { KilledBy = LOCTEXT("KilledByWorld", "Eliminated"); }
				else if (Entry.Killer == MyPS) { KilledBy = LOCTEXT("KilledBySelf", "You eliminated yourself"); }
				else { KilledBy = FText::Format(LOCTEXT("KilledBy", "Eliminated by {0}"), FText::FromString(Entry.KillerName)); }
				Combat->SetKilledByLine(KilledBy);
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
				Combat->BindToAbilitySystem(ASC, Bindings);

				// Read the standing state once: a joiner can arrive already dead or mid-count.
				Combat->SetDead(ASC->HasMatchingGameplayTag(BNTags::State_Dead));
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
			// No body or not ours: the hand is UNKNOWN, not "unarmed" — dashes, no lie.
			Combat->SetEquippedWeapon(FText::GetEmpty(), INDEX_NONE, INDEX_NONE, /*bKnown=*/false);
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

	if (!Current)
	{
		// The Unarmed SLOT — a real, known state of the hand, unlike the no-equipment Unknown.
		Combat->SetEquippedWeapon(LOCTEXT("UnarmedName", "Unarmed"), INDEX_NONE, INDEX_NONE, /*bKnown=*/true);
		return;
	}

	const FBNWeaponRow* Row = Current->GetRow();
	const FText Name = Row ? Row->DisplayName : FText::FromName(Current->GetRowName());
	// A magazine-less row (the knife) shows dashes, never a confident 0/0.
	const bool bHasMagazine = Current->GetMagazineSize() > 0;
	Combat->SetEquippedWeapon(Name,
		bHasMagazine ? Current->GetCurrentAmmo() : static_cast<int32>(INDEX_NONE),
		bHasMagazine ? Current->GetAmmoReserve() : static_cast<int32>(INDEX_NONE),
		/*bKnown=*/true);
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
	ShowDeathScreen(bDead);
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

void UBNHUDDirector::ShowDeathScreen(bool bShow)
{
	UBNUIManager* Manager = UBNUIManager::Get(GetLocalPlayer());
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!Manager || !LocalPlayer)
	{
		return;
	}

	if (bShow && !DeathScreenWidget.IsValid())
	{
		DeathScreenWidget = Manager->PushWidgetToLayer(LocalPlayer, FBNUITags::Get().Layer_GameMenu, Manager->GetDeathScreenClass());
	}
	else if (!bShow && DeathScreenWidget.IsValid())
	{
		Manager->RemoveWidgetFromLayer(LocalPlayer, FBNUITags::Get().Layer_GameMenu, DeathScreenWidget.Get());
		DeathScreenWidget.Reset();
	}
}

void UBNHUDDirector::SetScoreboardHeld(bool bHeld)
{
	bScoreboardHeld = bHeld;
	UpdateScoreboardVisibility();
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
	bScoreboardHeld = false;
	bPostMatch = false;
}

#undef LOCTEXT_NAMESPACE
