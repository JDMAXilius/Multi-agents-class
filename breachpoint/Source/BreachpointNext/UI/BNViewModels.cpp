#include "UI/BNViewModels.h"
#include "BreachpointNext.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayEffectTypes.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "BreachpointNextUI"

// ---------------------------------------------------------------------------------------------
// UBNVM_Combat
// ---------------------------------------------------------------------------------------------

void UBNVM_Combat::BeginDestroy()
{
	UnbindFromAbilitySystem();
	StopRespawnClock();
	Super::BeginDestroy();
}

void UBNVM_Combat::BindToAbilitySystem(UAbilitySystemComponent* InASC, const FBNCombatAttributeBindings& InBindings)
{
	// Rebinds happen on every possession; the old subscription must not survive the new one.
	UnbindFromAbilitySystem();

	if (!InASC)
	{
		return;
	}

	BoundASC = InASC;
	Bindings = InBindings;

	const FGameplayAttribute Attributes[] = { Bindings.Health, Bindings.MaxHealth, Bindings.Shield, Bindings.MaxShield,
		Bindings.Grenades, Bindings.MaxGrenades };
	for (const FGameplayAttribute& Attribute : Attributes)
	{
		if (Attribute.IsValid())
		{
			AttributeHandles.Emplace(Attribute,
				InASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &UBNVM_Combat::HandleAttributeChanged));
		}
	}

	// Subscribe, THEN read once — the values that replicated before this bind are not missed
	// events, they are the starting truth.
	RawHealth = Bindings.Health.IsValid() ? InASC->GetNumericAttribute(Bindings.Health) : 0.f;
	RawMaxHealth = Bindings.MaxHealth.IsValid() ? InASC->GetNumericAttribute(Bindings.MaxHealth) : 0.f;
	RawShield = Bindings.Shield.IsValid() ? InASC->GetNumericAttribute(Bindings.Shield) : 0.f;
	RawMaxShield = Bindings.MaxShield.IsValid() ? InASC->GetNumericAttribute(Bindings.MaxShield) : 0.f;
	RawGrenades = Bindings.Grenades.IsValid() ? InASC->GetNumericAttribute(Bindings.Grenades) : 0.f;
	RawMaxGrenades = Bindings.MaxGrenades.IsValid() ? InASC->GetNumericAttribute(Bindings.MaxGrenades) : 0.f;
	bAnyVitalsSeen = true;
	RefreshVitals();
	RefreshGrenades();
}

void UBNVM_Combat::UnbindFromAbilitySystem()
{
	// Against the STORED object (H9): a fresh lookup unbinds from whatever exists NOW, and if
	// the ASC was swapped the old binding outlives this ViewModel.
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Bound : AttributeHandles)
		{
			if (Bound.Value.IsValid())
			{
				ASC->GetGameplayAttributeValueChangeDelegate(Bound.Key).Remove(Bound.Value);
			}
		}
	}
	AttributeHandles.Reset();
	BoundASC.Reset();
}

void UBNVM_Combat::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (Data.Attribute == Bindings.Health) { RawHealth = Data.NewValue; }
	else if (Data.Attribute == Bindings.MaxHealth) { RawMaxHealth = Data.NewValue; }
	else if (Data.Attribute == Bindings.Shield) { RawShield = Data.NewValue; }
	else if (Data.Attribute == Bindings.MaxShield) { RawMaxShield = Data.NewValue; }
	else if (Data.Attribute == Bindings.Grenades) { RawGrenades = Data.NewValue; RefreshGrenades(); return; }
	else if (Data.Attribute == Bindings.MaxGrenades) { RawMaxGrenades = Data.NewValue; RefreshGrenades(); return; }

	bAnyVitalsSeen = true;
	RefreshVitals();
}

void UBNVM_Combat::RefreshVitals()
{
	// THE GATE: Live requires the DENOMINATOR, not merely an arrival. Health landing one bunch
	// before MaxHealth used to render an empty bar on a living player. MaxShield is deliberately
	// not required — shields are OFF by ruling (MaxShield = 0 is a valid, known configuration),
	// and a zero max renders as a zero-height bar, honestly.
	const bool bDenominatorsKnown = RawMaxHealth > 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(VitalsState, (bAnyVitalsSeen && bDenominatorsKnown) ? EBNUIDataState::Live : EBNUIDataState::Unknown);

	UE_MVVM_SET_PROPERTY_VALUE(HealthValue, FMath::CeilToInt32(RawHealth));
	UE_MVVM_SET_PROPERTY_VALUE(ShieldValue, FMath::CeilToInt32(RawShield));
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, RawMaxHealth > 0.f ? FMath::Clamp(RawHealth / RawMaxHealth, 0.f, 1.f) : 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(ShieldPercent, RawMaxShield > 0.f ? FMath::Clamp(RawShield / RawMaxShield, 0.f, 1.f) : 0.f);
}

void UBNVM_Combat::RefreshGrenades()
{
	// The pouch's honest-unknown: a CAPACITY is the denominator here, exactly as MaxHealth is for
	// the bar. No capacity yet (or a deliberate zero, which is grenades-off) means the slot has
	// nothing true to say, and INDEX_NONE tells the widget to hide rather than draw a zero.
	const bool bKnown = RawMaxGrenades > 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(GrenadeCapacity, bKnown ? FMath::FloorToInt32(RawMaxGrenades) : static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(GrenadeCount, bKnown ? FMath::Max(0, FMath::FloorToInt32(RawGrenades)) : static_cast<int32>(INDEX_NONE));
}

void UBNVM_Combat::SetEquippedWeapon(const FText& InName, int32 InMagAmmo, int32 InReserveAmmo, bool bKnown,
	const TSoftObjectPtr<UTexture2D>& InIcon, const TSoftObjectPtr<UTexture2D>& InReticle)
{
	UE_MVVM_SET_PROPERTY_VALUE(WeaponName, InName);
	UE_MVVM_SET_PROPERTY_VALUE(MagAmmo, InMagAmmo);
	UE_MVVM_SET_PROPERTY_VALUE(ReserveAmmo, InReserveAmmo);
	UE_MVVM_SET_PROPERTY_VALUE(EquipmentState, bKnown ? EBNUIDataState::Live : EBNUIDataState::Unknown);
	UE_MVVM_SET_PROPERTY_VALUE(WeaponIcon, InIcon);
	UE_MVVM_SET_PROPERTY_VALUE(WeaponReticle, InReticle);
}

void UBNVM_Combat::SetStowedWeapon(const FText& InName, const TSoftObjectPtr<UTexture2D>& InIcon)
{
	UE_MVVM_SET_PROPERTY_VALUE(StowedWeaponName, InName);
	UE_MVVM_SET_PROPERTY_VALUE(StowedWeaponIcon, InIcon);
}

void UBNVM_Combat::SetAmmo(int32 InMagAmmo, int32 InReserveAmmo)
{
	UE_MVVM_SET_PROPERTY_VALUE(MagAmmo, InMagAmmo);
	UE_MVVM_SET_PROPERTY_VALUE(ReserveAmmo, InReserveAmmo);
}

void UBNVM_Combat::SetDead(bool bInDead)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsDead, bInDead);

	if (!bInDead)
	{
		// Alive again: the death panel's data goes with the state, so the NEXT death never
		// flashes the last one's line for a frame.
		UE_MVVM_SET_PROPERTY_VALUE(KilledByLine, FText::GetEmpty());
		UE_MVVM_SET_PROPERTY_VALUE(KilledByWeapon, FText::GetEmpty());
		SetRespawnStamp(0.0, nullptr);
	}
}

void UBNVM_Combat::SetKilledByLine(const FText& InLine, const FText& InWeapon)
{
	UE_MVVM_SET_PROPERTY_VALUE(KilledByLine, InLine);
	UE_MVVM_SET_PROPERTY_VALUE(KilledByWeapon, InWeapon);
}

void UBNVM_Combat::SetRespawnStamp(double InRespawnAtServerTime, AGameStateBase* InTimeSource)
{
	RespawnAtServerTime = InRespawnAtServerTime;
	RespawnTimeSource = InTimeSource;

	if (RespawnAtServerTime <= 0.0 || !InTimeSource)
	{
		StopRespawnClock();
		UE_MVVM_SET_PROPERTY_VALUE(RespawnSecondsRemaining, static_cast<int32>(INDEX_NONE));
		return;
	}

	UpdateRespawnClock();
}

void UBNVM_Combat::UpdateRespawnClock()
{
	const AGameStateBase* GameState = RespawnTimeSource.Get();
	if (!GameState || RespawnAtServerTime <= 0.0)
	{
		StopRespawnClock();
		UE_MVVM_SET_PROPERTY_VALUE(RespawnSecondsRemaining, static_cast<int32>(INDEX_NONE));
		return;
	}

	const double Remaining = RespawnAtServerTime - GameState->GetServerWorldTimeSeconds();
	// Ceil, so "3" shows the moment of death and "0" never lingers: at 2.4 remaining the honest
	// integer a player reads is 3-2-1, not 2-1-0-0.
	UE_MVVM_SET_PROPERTY_VALUE(RespawnSecondsRemaining, FMath::Max(0, FMath::CeilToInt32(Remaining)));

	if (Remaining > 0.0)
	{
		ScheduleNextRespawnUpdate();
	}
}

void UBNVM_Combat::ScheduleNextRespawnUpdate()
{
	const AGameStateBase* GameState = RespawnTimeSource.Get();
	UWorld* World = GetTimerWorld();
	if (!GameState || !World)
	{
		return;
	}

	// Phase-locked to the STAMP's own fraction, so the displayed integer flips exactly when the
	// true remaining crosses a whole second — the match clock's discipline, at death's scale.
	const double Remaining = RespawnAtServerTime - GameState->GetServerWorldTimeSeconds();
	double Delay = Remaining - (FMath::CeilToDouble(Remaining) - 1.0);
	if (Delay < 0.01)
	{
		Delay += 1.0;
	}
	World->GetTimerManager().SetTimer(RespawnTimerHandle, this, &UBNVM_Combat::UpdateRespawnClock,
		static_cast<float>(Delay), /*bLoop=*/false);
}

void UBNVM_Combat::StopRespawnClock()
{
	if (UWorld* World = GetTimerWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}
	RespawnTimerHandle.Invalidate();
}

UWorld* UBNVM_Combat::GetTimerWorld() const
{
	if (const AGameStateBase* GameState = RespawnTimeSource.Get())
	{
		return GameState->GetWorld();
	}
	return GetWorld();
}

void UBNVM_Combat::ClearToUnknown()
{
	UnbindFromAbilitySystem();
	StopRespawnClock();

	RawHealth = RawMaxHealth = RawShield = RawMaxShield = 0.f;
	RawGrenades = RawMaxGrenades = 0.f;
	bAnyVitalsSeen = false;
	RespawnAtServerTime = 0.0;
	RespawnTimeSource.Reset();

	UE_MVVM_SET_PROPERTY_VALUE(VitalsState, EBNUIDataState::Unknown);
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(ShieldPercent, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(HealthValue, 0);
	UE_MVVM_SET_PROPERTY_VALUE(ShieldValue, 0);
	UE_MVVM_SET_PROPERTY_VALUE(WeaponName, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(MagAmmo, static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(ReserveAmmo, static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(EquipmentState, EBNUIDataState::Unknown);
	UE_MVVM_SET_PROPERTY_VALUE(GrenadeCount, static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(GrenadeCapacity, static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(WeaponIcon, TSoftObjectPtr<UTexture2D>());
	UE_MVVM_SET_PROPERTY_VALUE(StowedWeaponName, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(StowedWeaponIcon, TSoftObjectPtr<UTexture2D>());
	UE_MVVM_SET_PROPERTY_VALUE(bIsDead, false);
	UE_MVVM_SET_PROPERTY_VALUE(KilledByLine, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(KilledByWeapon, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(RespawnSecondsRemaining, static_cast<int32>(INDEX_NONE));
}

// ---------------------------------------------------------------------------------------------
// UBNVM_Match
// ---------------------------------------------------------------------------------------------

void UBNVM_Match::BeginDestroy()
{
	StopClockUpdates();
	if (UWorld* World = GetTimerWorld())
	{
		World->GetTimerManager().ClearTimer(KillfeedExpiryTimerHandle);
	}
	Super::BeginDestroy();
}

void UBNVM_Match::SetMatchPhase(FName InMatchState, const FText& InWinnerLine,
	EBNMatchOutcome InOutcome)
{
	UE_MVVM_SET_PROPERTY_VALUE(MatchStateName, InMatchState);
	UE_MVVM_SET_PROPERTY_VALUE(MatchDataState, EBNUIDataState::Live);

	// Composed HERE so the widget renders text and decides nothing (a branch on gameplay state
	// in a widget is the exact thing the grep gate hunts). Empty while the match is live —
	// the band shows a banner only when there is something to say.
	FText Banner;
	if (InMatchState == FName(TEXT("WaitingToStart")))
	{
		Banner = LOCTEXT("WarmupBanner", "WARMUP — waiting for players");
	}
	else if (InMatchState == FName(TEXT("WaitingPostMatch")))
	{
		Banner = InWinnerLine;
	}
	UE_MVVM_SET_PROPERTY_VALUE(PhaseBannerText, Banner);
	UE_MVVM_SET_PROPERTY_VALUE(Outcome, InOutcome);
}

void UBNVM_Match::SetScores(int32 InMyKills, int32 InTopKills, int32 InScoreLimit)
{
	UE_MVVM_SET_PROPERTY_VALUE(MyKills, InMyKills);
	UE_MVVM_SET_PROPERTY_VALUE(TopKills, InTopKills);
	UE_MVVM_SET_PROPERTY_VALUE(ScoreLimit, InScoreLimit);
}

void UBNVM_Match::SetMatchClock(double InMatchEndServerTime, AGameStateBase* InTimeSource)
{
	MatchEndServerTime = InMatchEndServerTime;
	TimeSource = InTimeSource;
	UpdateClocks();
}

void UBNVM_Match::UpdateClocks()
{
	const AGameStateBase* GameState = TimeSource.Get();
	if (!GameState || MatchEndServerTime <= 0.0)
	{
		StopClockUpdates();
		// An em dash, not 0:00 — "no clock stamped" and "time expired" are different truths.
		UE_MVVM_SET_PROPERTY_VALUE(MatchClockText, LOCTEXT("NoClock", "—:——"));
		return;
	}

	const double Remaining = FMath::Max(0.0, MatchEndServerTime - GameState->GetServerWorldTimeSeconds());
	const int32 TotalSeconds = FMath::CeilToInt32(Remaining);
	UE_MVVM_SET_PROPERTY_VALUE(MatchClockText,
		FText::FromString(FString::Printf(TEXT("%d:%02d"), TotalSeconds / 60, TotalSeconds % 60)));

	if (Remaining > 0.0)
	{
		ScheduleNextClockUpdate();
	}
	else
	{
		StopClockUpdates();
	}
}

void UBNVM_Match::ScheduleNextClockUpdate()
{
	const AGameStateBase* GameState = TimeSource.Get();
	UWorld* World = GetTimerWorld();
	if (!GameState || !World)
	{
		return;
	}

	// Phase-locked to the whole server second, so every readout in the game flips the same
	// digit in the same frame. ADD when under threshold — a fire at frac 0.995 that re-armed
	// with "= 1.0" perpetually re-landed out of phase (the old module's H3, kept fixed).
	const double Now = GameState->GetServerWorldTimeSeconds();
	double Delay = 1.0 - (Now - FMath::FloorToDouble(Now));
	if (Delay < 0.01)
	{
		Delay += 1.0;
	}
	World->GetTimerManager().SetTimer(ClockTimerHandle, this, &UBNVM_Match::UpdateClocks,
		static_cast<float>(Delay), /*bLoop=*/false);
}

void UBNVM_Match::StopClockUpdates()
{
	if (UWorld* World = GetTimerWorld())
	{
		World->GetTimerManager().ClearTimer(ClockTimerHandle);
	}
	ClockTimerHandle.Invalidate();
}

void UBNVM_Match::PushKillfeedEntry(const FText& InLine, int32 InSequence, bool bInvolvesSelf,
	const TSoftObjectPtr<UTexture2D>& InWeaponIcon, const FText& InKillerText, const FText& InVictimText)
{
	if (InSequence <= LastKillfeedSequence)
	{
		// The ring re-replicates whole; entries already shown are not news.
		return;
	}
	LastKillfeedSequence = InSequence;

	// An empty line is the director's age filter saying "seen, not shown" — the sequence
	// advanced above, and no blank row reaches the screen.
	if (InLine.IsEmpty())
	{
		return;
	}

	const UWorld* World = GetTimerWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	// Mutate-then-notify: UE_MVVM_SET_PROPERTY_VALUE would compare the array to itself. The
	// broadcast is the macro-free half of the same contract, fired once per push.
	FBNKillfeedViewEntry& Entry = KillfeedEntries.AddDefaulted_GetRef();
	Entry.Line = InLine;
	Entry.Sequence = InSequence;
	Entry.ExpiryTime = Now + BNUITiming::KillfeedLingerSeconds;
	Entry.bInvolvesSelf = bInvolvesSelf;
	Entry.WeaponIcon = InWeaponIcon;
	Entry.KillerText = InKillerText;
	Entry.VictimText = InVictimText;

	while (KillfeedEntries.Num() > KillfeedMaxVisibleEntries)
	{
		KillfeedEntries.RemoveAt(0);
	}

	OnKillfeedViewChanged.Broadcast();
	ScheduleKillfeedExpiry();
}

void UBNVM_Match::ExpireKillfeedEntries()
{
	const UWorld* World = GetTimerWorld();
	if (!World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	const int32 Removed = KillfeedEntries.RemoveAll([Now](const FBNKillfeedViewEntry& Entry)
	{
		return Entry.ExpiryTime <= Now;
	});

	if (Removed > 0)
	{
		OnKillfeedViewChanged.Broadcast();
	}
	if (KillfeedEntries.Num() > 0)
	{
		ScheduleKillfeedExpiry();
	}
}

void UBNVM_Match::ScheduleKillfeedExpiry()
{
	UWorld* World = GetTimerWorld();
	if (!World || KillfeedEntries.Num() == 0)
	{
		return;
	}

	// One timer aimed at the OLDEST entry's expiry — the front of the array is always next.
	const double Delay = FMath::Max(0.05, KillfeedEntries[0].ExpiryTime - World->GetTimeSeconds());
	World->GetTimerManager().SetTimer(KillfeedExpiryTimerHandle, this, &UBNVM_Match::ExpireKillfeedEntries,
		static_cast<float>(Delay), /*bLoop=*/false);
}

void UBNVM_Match::SetRoster(TArray<FBNScoreRowView>&& InRoster)
{
	// Silent when nothing the scoreboard renders changed — the director rebuilds on broad
	// edges, and a no-op rebuild must not repaint eight rows.
	const bool bSame = Roster.Num() == InRoster.Num() &&
		[&]{
			for (int32 i = 0; i < Roster.Num(); ++i)
			{
				const FBNScoreRowView& A = Roster[i];
				const FBNScoreRowView& B = InRoster[i];
				if (A.Kills != B.Kills || A.Deaths != B.Deaths || A.bIsSelf != B.bIsSelf ||
					A.bIsWinner != B.bIsWinner || !A.PlayerName.Equals(B.PlayerName, ESearchCase::CaseSensitive))
				{
					return false;
				}
			}
			return true;
		}();
	if (bSame)
	{
		return;
	}

	Roster = MoveTemp(InRoster);
	OnRosterViewChanged.Broadcast();
}

UWorld* UBNVM_Match::GetTimerWorld() const
{
	if (const AGameStateBase* GameState = TimeSource.Get())
	{
		return GameState->GetWorld();
	}
	return GetWorld();
}

void UBNVM_Match::ClearToUnknown()
{
	StopClockUpdates();
	if (UWorld* World = GetTimerWorld())
	{
		World->GetTimerManager().ClearTimer(KillfeedExpiryTimerHandle);
	}
	KillfeedExpiryTimerHandle.Invalidate();

	MatchEndServerTime = 0.0;
	TimeSource.Reset();
	LastKillfeedSequence = INDEX_NONE;
	KillfeedEntries.Reset();
	// Only when there was something to clear — SetRoster's silence contract, kept symmetric.
	if (Roster.Num() > 0)
	{
		Roster.Reset();
		OnRosterViewChanged.Broadcast();
	}

	UE_MVVM_SET_PROPERTY_VALUE(MatchStateName, FName());
	UE_MVVM_SET_PROPERTY_VALUE(PhaseBannerText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(MatchClockText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(MyKills, 0);
	UE_MVVM_SET_PROPERTY_VALUE(TopKills, 0);
	UE_MVVM_SET_PROPERTY_VALUE(ScoreLimit, 0);
	UE_MVVM_SET_PROPERTY_VALUE(MatchDataState, EBNUIDataState::Unknown);
	OnKillfeedViewChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
