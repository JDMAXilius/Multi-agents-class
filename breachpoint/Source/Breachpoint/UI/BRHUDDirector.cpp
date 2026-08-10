#include "UI/BRHUDDirector.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/BRAttributeSet.h"
#include "Breachpoint.h"
#include "Data/BRDataRows.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Match/BRGameState.h"
#include "Match/BRPlayerState.h"
#include "UI/BRUIManagerSubsystem.h"
#include "UI/BRViewModels.h"
#include "Weapons/BREquipmentComponent.h"
#include "Weapons/BRWeaponInstance.h"

void UBRHUDDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// H4: travel is the one lifecycle event neither the widgets nor the VMs can see. Both VMs
	// survive travel (GameInstance-outered) while their timers and TimeSource die with the old
	// world — without this hook, frozen scores stayed Live and the old map's kills carried
	// expiry stamps from a clock that no longer exists.
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UBRHUDDirector::HandlePostLoadMap);

	TryBindWorld();
}

void UBRHUDDirector::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	PostLoadMapHandle.Reset();

	UnbindPawn();
	UnbindWorld();

	Super::Deinitialize();
}

void UBRHUDDirector::HandlePostLoadMap(UWorld* LoadedWorld)
{
	UnbindPawn();
	UnbindWorld();

	// Honest reset FIRST: the new world's facts arrive through the rebind; until they do the
	// HUD says "unknown", never last map's numbers.
	if (UBRVM_Combat* Combat = GetCombatViewModel())
	{
		Combat->ClearToUnknown();
	}
	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		Match->ClearToUnknown();
	}
	bHUDShown = false;

	TryBindWorld();
}

void UBRHUDDirector::TryBindWorld()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	// The controller may repossess without a map change (respawn is the same pawn class but a
	// new pawn). One dynamic bind per world; AddUniqueDynamic makes re-entry safe.
	if (APlayerController* PC = LocalPlayer->GetPlayerController(World))
	{
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &UBRHUDDirector::HandlePossessedPawnChanged);
		if (APawn* Pawn = PC->GetPawn())
		{
			BindPawn(Pawn);
		}
	}

	ABRGameState* GameState = World->GetGameState<ABRGameState>();
	if (!GameState)
	{
		// Clients legitimately reach this before the GameState replicates. The world tells us
		// when it lands; nothing polls (law 4).
		GameStateSetHandle = World->GameStateSetEvent.AddUObject(
			this, &UBRHUDDirector::HandleGameStateSet);
		return;
	}

	BindGameState(GameState);
}

void UBRHUDDirector::HandleGameStateSet(AGameStateBase* NewGameState)
{
	if (ABRGameState* GameState = Cast<ABRGameState>(NewGameState))
	{
		if (UWorld* World = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr)
		{
			World->GameStateSetEvent.Remove(GameStateSetHandle);
			GameStateSetHandle.Reset();
		}
		BindGameState(GameState);
	}
}

void UBRHUDDirector::BindGameState(ABRGameState* GameState)
{
	BoundGameState = GameState;

	GameState->OnMatchClockChanged.AddUObject(this, &UBRHUDDirector::HandleMatchClockChanged);
	GameState->OnTeamScoreChanged.AddUObject(this, &UBRHUDDirector::HandleTeamScoreChanged);
	GameState->OnKillFeedEntryAdded.AddUObject(this, &UBRHUDDirector::HandleKillFeedEntryAdded);

	PushCurrentMatchState();
}

void UBRHUDDirector::UnbindWorld()
{
	if (ABRGameState* GameState = BoundGameState.Get())
	{
		GameState->OnMatchClockChanged.RemoveAll(this);
		GameState->OnTeamScoreChanged.RemoveAll(this);
		GameState->OnKillFeedEntryAdded.RemoveAll(this);
	}
	BoundGameState.Reset();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UWorld* World = LocalPlayer->GetWorld())
		{
			if (GameStateSetHandle.IsValid())
			{
				World->GameStateSetEvent.Remove(GameStateSetHandle);
			}
			if (APlayerController* PC = LocalPlayer->GetPlayerController(World))
			{
				PC->OnPossessedPawnChanged.RemoveDynamic(this, &UBRHUDDirector::HandlePossessedPawnChanged);
			}
		}
	}
	GameStateSetHandle.Reset();
}

void UBRHUDDirector::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	UnbindPawn();
	if (NewPawn)
	{
		BindPawn(NewPawn);
	}
}

void UBRHUDDirector::BindPawn(APawn* Pawn)
{
	UBRVM_Combat* Combat = GetCombatViewModel();
	if (!Combat || !Pawn)
	{
		return;
	}

	// The ASC lives on the PlayerState (project law) and survives respawn; BindToAbilitySystem
	// is rebind-safe on the same ASC, so calling it per possession is correct and cheap.
	if (ABRPlayerState* PlayerState = Pawn->GetPlayerState<ABRPlayerState>())
	{
		if (UAbilitySystemComponent* ASC = PlayerState->GetAbilitySystemComponent())
		{
			FBRCombatAttributeBindings Bindings;
			Bindings.Health = UBRAttributeSet::GetHealthAttribute();
			Bindings.MaxHealth = UBRAttributeSet::GetMaxHealthAttribute();
			Bindings.Shields = UBRAttributeSet::GetShieldsAttribute();
			Bindings.MaxShields = UBRAttributeSet::GetMaxShieldsAttribute();
			Combat->BindToAbilitySystem(ASC, Bindings);

			// Grenades sit outside the vitals bindings struct (they are equipment, not a bar).
			// Same one-way shape: attribute change -> push. Torn down against the SAME ASC.
			BoundASC = ASC;
			GrenadesChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
				UBRAttributeSet::GetGrenadesAttribute()).AddUObject(
					this, &UBRHUDDirector::HandleGrenadesChanged);

			bool bFound = false;
			const float Grenades = ASC->GetGameplayAttributeValue(UBRAttributeSet::GetGrenadesAttribute(), bFound);
			if (bFound)
			{
				Combat->SetGrenadeCount(FMath::RoundToInt(Grenades));
			}
		}
	}

	if (UBREquipmentComponent* Equipment = Pawn->FindComponentByClass<UBREquipmentComponent>())
	{
		BoundEquipment = Equipment;
		Equipment->OnEquippedWeaponChanged.AddUObject(this, &UBRHUDDirector::HandleEquippedWeaponChanged);
		PushWeaponState(Equipment);
	}

	EnsureHUDShown();
}

void UBRHUDDirector::UnbindPawn()
{
	if (UBRVM_Combat* Combat = GetCombatViewModel())
	{
		Combat->UnbindFromAbilitySystem();
	}

	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		if (GrenadesChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UBRAttributeSet::GetGrenadesAttribute()).Remove(GrenadesChangedHandle);
		}
	}
	BoundASC.Reset();
	GrenadesChangedHandle.Reset();

	if (UBREquipmentComponent* Equipment = BoundEquipment.Get())
	{
		Equipment->OnEquippedWeaponChanged.RemoveAll(this);
	}
	BoundEquipment.Reset();

	if (UBRWeaponInstance* Weapon = BoundWeapon.Get())
	{
		Weapon->OnAmmoChanged.RemoveAll(this);
	}
	BoundWeapon.Reset();
}

void UBRHUDDirector::HandleMatchClockChanged(float NewEndServerTime)
{
	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		Match->SetMatchEndServerTime(NewEndServerTime);
	}
}

void UBRHUDDirector::HandleTeamScoreChanged(uint8 TeamId, int32 NewScore)
{
	// The VM takes both scores atomically; re-reading both from the source beats caching one.
	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		if (const ABRGameState* GameState = BoundGameState.Get())
		{
			Match->SetTeamScores(GameState->GetTeamScore(0), GameState->GetTeamScore(1));
		}
	}
}

void UBRHUDDirector::HandleKillFeedEntryAdded(const FBRKillFeedEntry& Entry)
{
	UBRVM_Match* Match = GetMatchViewModel();
	if (!Match)
	{
		return;
	}

	const APlayerState* LocalPlayerState = nullptr;
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (const APlayerController* PC = LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()))
		{
			LocalPlayerState = PC->PlayerState;
		}
	}

	FBRKillfeedViewEntry View;
	// H5: ALWAYS the server-assigned sequence. It is the only id that is the same on every
	// machine, and the spotter line addresses rows by it.
	View.SequenceId = Entry.Sequence;
	View.KillerName = Entry.Killer
		? FText::AsCultureInvariant(Entry.Killer->GetPlayerName()) : FText::GetEmpty();
	View.VictimName = Entry.Victim
		? FText::AsCultureInvariant(Entry.Victim->GetPlayerName()) : FText::GetEmpty();
	View.DamageTag = Entry.CauseTag;
	View.KillerTeamId = Entry.KillerTeamId;
	View.VictimTeamId = Entry.VictimTeamId;
	View.bLocalPlayerInvolved = (LocalPlayerState != nullptr)
		&& (Entry.Killer == LocalPlayerState || Entry.Victim == LocalPlayerState);

	Match->PushKillfeedEntry(View);
}

void UBRHUDDirector::PushCurrentMatchState()
{
	UBRVM_Match* Match = GetMatchViewModel();
	const ABRGameState* GameState = BoundGameState.Get();
	if (!Match || !GameState)
	{
		return;
	}

	// A late joiner's catch-up: the same wires, pulled once. The killfeed ring is NOT
	// replayed — those kills predate the joiner and would render as fresh events with fresh
	// TTLs; an empty feed is the honest join state.
	Match->SetTimeSource(BoundGameState.Get());
	Match->SetMatchEndServerTime(GameState->GetMatchEndServerTime());
	Match->SetTeamScores(GameState->GetTeamScore(0), GameState->GetTeamScore(1));
}

void UBRHUDDirector::HandleEquippedWeaponChanged(UBREquipmentComponent* Equipment, UBRWeaponInstance* NewWeapon)
{
	PushWeaponState(Equipment);
}

void UBRHUDDirector::HandleAmmoChanged(UBRWeaponInstance* Weapon)
{
	if (UBRVM_Combat* Combat = GetCombatViewModel())
	{
		if (Weapon)
		{
			Combat->SetAmmo(Weapon->GetAmmoInMag(), Weapon->GetAmmoReserve());
		}
	}
}

void UBRHUDDirector::HandleGrenadesChanged(const FOnAttributeChangeData& Data)
{
	if (UBRVM_Combat* Combat = GetCombatViewModel())
	{
		Combat->SetGrenadeCount(FMath::RoundToInt(Data.NewValue));
	}
}

void UBRHUDDirector::PushWeaponState(UBREquipmentComponent* Equipment)
{
	UBRVM_Combat* Combat = GetCombatViewModel();
	if (!Combat || !Equipment)
	{
		return;
	}

	UBRWeaponInstance* Active = Equipment->GetActiveWeapon();

	// Re-point the ammo wire at the active weapon: each instance broadcasts its own changes.
	if (UBRWeaponInstance* Previous = BoundWeapon.Get())
	{
		Previous->OnAmmoChanged.RemoveAll(this);
	}
	BoundWeapon = Active;
	if (Active)
	{
		Active->OnAmmoChanged.AddUObject(this, &UBRHUDDirector::HandleAmmoChanged);
	}

	// The stowed weapon is whichever occupied slot is not active.
	UBRWeaponInstance* Stowed = nullptr;
	const int32 ActiveIndex = Equipment->GetActiveSlotIndex();
	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBRWeaponSlot::Count); ++SlotIndex)
	{
		if (SlotIndex == ActiveIndex)
		{
			continue;
		}
		if (UBRWeaponInstance* InSlot = Equipment->GetWeaponInSlot(static_cast<EBRWeaponSlot>(SlotIndex)))
		{
			Stowed = InSlot;
			break;
		}
	}

	const FBRWeaponRow* ActiveRow = Active ? Active->GetRow() : nullptr;
	const FBRWeaponRow* StowedRow = Stowed ? Stowed->GetRow() : nullptr;
	Combat->SetWeaponNames(
		ActiveRow ? ActiveRow->DisplayName : FText::GetEmpty(),
		StowedRow ? StowedRow->DisplayName : FText::GetEmpty());

	if (Active)
	{
		Combat->SetAmmo(Active->GetAmmoInMag(), Active->GetAmmoReserve());
	}
}

UBRVM_Combat* UBRHUDDirector::GetCombatViewModel() const
{
	const UBRUIManagerSubsystem* Manager = UBRUIManagerSubsystem::Get(GetLocalPlayer());
	return Manager ? Manager->GetCombatViewModel(GetLocalPlayer()) : nullptr;
}

UBRVM_Match* UBRHUDDirector::GetMatchViewModel() const
{
	const UBRUIManagerSubsystem* Manager = UBRUIManagerSubsystem::Get(GetLocalPlayer());
	return Manager ? Manager->GetMatchViewModel(GetLocalPlayer()) : nullptr;
}

void UBRHUDDirector::EnsureHUDShown()
{
	if (bHUDShown)
	{
		return;
	}

	// Only a world with our GameState is a match world; the front end has no HUD to show.
	if (!BoundGameState.IsValid())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UBRUIManagerSubsystem* Manager = UBRUIManagerSubsystem::Get(LocalPlayer);
	if (!Manager || !LocalPlayer)
	{
		return;
	}

	if (!Manager->GetRootLayout(LocalPlayer))
	{
		Manager->CreateLayoutForLocalPlayer(LocalPlayer);
	}

	bHUDShown = (Manager->ShowHUD(LocalPlayer) != nullptr);
	if (!bHUDShown)
	{
		UE_LOG(Logbreachpoint, Warning,
			TEXT("UBRHUDDirector: ShowHUD produced nothing — check BRUISettings' "
			     "RootLayoutClass/HUDLayoutClass paths resolve to built assets."));
	}
}
