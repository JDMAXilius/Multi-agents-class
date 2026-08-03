#include "UI/BRViewModels.h"

#include "AbilitySystemComponent.h"
#include "Breachpoint.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"
#include "UI/BRUISettings.h"

#define LOCTEXT_NAMESPACE "BreachpointUI"

void UBRVM_Combat::BindToAbilitySystem(UAbilitySystemComponent* InASC, const FBRCombatAttributeBindings& InBindings)
{
	UnbindFromAbilitySystem();

	if (!InASC || !InBindings.HasAny())
	{
		SetVitalsState(EBRUIDataState::Unknown);
		return;
	}

	BoundASC = InASC;
	Bindings = InBindings;

	if (Bindings.Health.IsValid())
	{
		HealthChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(Bindings.Health)
			.AddUObject(this, &UBRVM_Combat::HandleAttributeChanged);
	}
	if (Bindings.MaxHealth.IsValid())
	{
		MaxHealthChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(Bindings.MaxHealth)
			.AddUObject(this, &UBRVM_Combat::HandleAttributeChanged);
	}
	if (Bindings.Shields.IsValid())
	{
		ShieldsChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(Bindings.Shields)
			.AddUObject(this, &UBRVM_Combat::HandleAttributeChanged);
	}
	if (Bindings.MaxShields.IsValid())
	{
		MaxShieldsChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(Bindings.MaxShields)
			.AddUObject(this, &UBRVM_Combat::HandleAttributeChanged);
	}

	ShieldsBrokenTagHandle = InASC->RegisterGameplayTagEvent(
			BRGameplayTags::State_Shields_Broken, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UBRVM_Combat::HandleShieldsBrokenTagChanged);

	PublishCurrentAttributeValues();
}

void UBRVM_Combat::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		if (HealthChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Bindings.Health).Remove(HealthChangedHandle);
		}
		if (MaxHealthChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Bindings.MaxHealth).Remove(MaxHealthChangedHandle);
		}
		if (ShieldsChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Bindings.Shields).Remove(ShieldsChangedHandle);
		}
		if (MaxShieldsChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Bindings.MaxShields).Remove(MaxShieldsChangedHandle);
		}
		if (ShieldsBrokenTagHandle.IsValid())
		{
			ASC->UnregisterGameplayTagEvent(ShieldsBrokenTagHandle,
				BRGameplayTags::State_Shields_Broken, EGameplayTagEventType::NewOrRemoved);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	ShieldsChangedHandle.Reset();
	MaxShieldsChangedHandle.Reset();
	ShieldsBrokenTagHandle.Reset();

	BoundASC.Reset();
	Bindings = FBRCombatAttributeBindings();

	if (VitalsState == EBRUIDataState::Live)
	{
		SetVitalsState(EBRUIDataState::Stale);
	}
}

void UBRVM_Combat::PublishCurrentAttributeValues()
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!ASC)
	{
		return;
	}

	bool bAnyFound = false;

	if (Bindings.Health.IsValid())
	{
		bool bFound = false;
		const float Value = ASC->GetGameplayAttributeValue(Bindings.Health, bFound);
		if (bFound)
		{
			bAnyFound = true;
			UE_MVVM_SET_PROPERTY_VALUE(Health, Value);
		}
	}
	if (Bindings.MaxHealth.IsValid())
	{
		bool bFound = false;
		const float Value = ASC->GetGameplayAttributeValue(Bindings.MaxHealth, bFound);
		if (bFound)
		{
			bAnyFound = true;
			UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, Value);
		}
	}
	if (Bindings.Shields.IsValid())
	{
		bool bFound = false;
		const float Value = ASC->GetGameplayAttributeValue(Bindings.Shields, bFound);
		if (bFound)
		{
			bAnyFound = true;
			UE_MVVM_SET_PROPERTY_VALUE(Shields, Value);
		}
	}
	if (Bindings.MaxShields.IsValid())
	{
		bool bFound = false;
		const float Value = ASC->GetGameplayAttributeValue(Bindings.MaxShields, bFound);
		if (bFound)
		{
			bAnyFound = true;
			UE_MVVM_SET_PROPERTY_VALUE(MaxShields, Value);
		}
	}

	RecomputeVitalRatios();

	const bool bBrokenNow = ASC->HasMatchingGameplayTag(BRGameplayTags::State_Shields_Broken);
	UE_MVVM_SET_PROPERTY_VALUE(bShieldsBroken, bBrokenNow);

	// HUD-CPP-AUDIT H2: Live requires the DENOMINATORS, not "any attribute". On a client the
	// four attributes arrive in a replication bunch with no intra-frame ordering guarantee; with
	// `bAnyFound` alone, Health landing before MaxHealth published Live + HealthPercent 0.0 and
	// the widget rendered an empty health bar on a living player — host right, client wrong.
	const bool bDenominatorsKnown = (MaxHealth > 0.0f) && (MaxShields > 0.0f);
	SetVitalsState((bAnyFound && bDenominatorsKnown) ? EBRUIDataState::Live : EBRUIDataState::Unknown);
}

void UBRVM_Combat::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (Data.Attribute == Bindings.Health)
	{
		UE_MVVM_SET_PROPERTY_VALUE(Health, Data.NewValue);
	}
	else if (Data.Attribute == Bindings.MaxHealth)
	{
		UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, Data.NewValue);
	}
	else if (Data.Attribute == Bindings.Shields)
	{
		UE_MVVM_SET_PROPERTY_VALUE(Shields, Data.NewValue);
	}
	else if (Data.Attribute == Bindings.MaxShields)
	{
		UE_MVVM_SET_PROPERTY_VALUE(MaxShields, Data.NewValue);
	}
	else
	{
		return;
	}

	RecomputeVitalRatios();

	// H2, same gate as the initial publish: a single attribute change must not promote a
	// partial set to Live. Once both denominators are in, any change keeps it Live.
	if ((MaxHealth > 0.0f) && (MaxShields > 0.0f))
	{
		SetVitalsState(EBRUIDataState::Live);
	}
}

void UBRVM_Combat::HandleShieldsBrokenTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// The bShieldsBroken FieldNotify below is the one channel for this state — the paired
	// broken/restored FMVVMEventFields were cut (HUD-CPP-AUDIT: zero consumers; the widgets
	// observe the property).
	const bool bBroken = NewCount > 0;
	UE_MVVM_SET_PROPERTY_VALUE(bShieldsBroken, bBroken);
}

void UBRVM_Combat::RecomputeVitalRatios()
{
	const float NewHealthPercent = (MaxHealth > 0.0f) ? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) : 0.0f;
	const float NewShieldPercent = (MaxShields > 0.0f) ? FMath::Clamp(Shields / MaxShields, 0.0f, 1.0f) : 0.0f;

	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, NewHealthPercent);
	UE_MVVM_SET_PROPERTY_VALUE(ShieldPercent, NewShieldPercent);
}

void UBRVM_Combat::SetVitalsState(EBRUIDataState InState)
{
	UE_MVVM_SET_PROPERTY_VALUE(VitalsState, InState);
}

void UBRVM_Combat::SetEquipmentState(EBRUIDataState InState)
{
	UE_MVVM_SET_PROPERTY_VALUE(EquipmentState, InState);
}

void UBRVM_Combat::SetAmmo(int32 InMagazine, int32 InReserve)
{
	UE_MVVM_SET_PROPERTY_VALUE(MagazineAmmo, InMagazine);
	UE_MVVM_SET_PROPERTY_VALUE(ReserveAmmo, InReserve);
	SetEquipmentState(EBRUIDataState::Live);
}

void UBRVM_Combat::SetWeaponNames(const FText& InActiveWeapon, const FText& InStowedWeapon)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActiveWeaponName, InActiveWeapon);
	UE_MVVM_SET_PROPERTY_VALUE(StowedWeaponName, InStowedWeapon);
	SetEquipmentState(EBRUIDataState::Live);
}

void UBRVM_Combat::SetGrenadeCount(int32 InCount)
{
	UE_MVVM_SET_PROPERTY_VALUE(GrenadeCount, InCount);
	SetEquipmentState(EBRUIDataState::Live);
}

void UBRVM_Combat::SetGrappleCooldownStarted(float InDurationSeconds)
{
	UE_MVVM_SET_PROPERTY_VALUE(GrappleCooldownDuration, InDurationSeconds);
	UE_MVVM_SET_PROPERTY_VALUE(bGrappleReady, false);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GrappleCooldownStarted);
}

void UBRVM_Combat::SetGrappleReady()
{
	if (bGrappleReady)
	{
		return;
	}
	UE_MVVM_SET_PROPERTY_VALUE(bGrappleReady, true);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GrappleReady);
}

void UBRVM_Combat::ReportHitMarker(EBRHitMarkerKind InKind)
{
	if (InKind == EBRHitMarkerKind::None)
	{
		return;
	}

	// ONE channel. The four per-kind FMVVMEventFields were cut (HUD-CPP-AUDIT): the reticle and
	// the layout both consume this native delegate, and two channels for one signal is how a
	// consumer picks the dead one.
	OnHitMarkerEvent.Broadcast(InKind);
}

void UBRVM_Combat::ClearToUnknown()
{
	UnbindFromAbilitySystem();

	UE_MVVM_SET_PROPERTY_VALUE(Health, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(Shields, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxShields, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(ShieldPercent, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(bShieldsBroken, false);

	UE_MVVM_SET_PROPERTY_VALUE(MagazineAmmo, 0);
	UE_MVVM_SET_PROPERTY_VALUE(ReserveAmmo, 0);
	UE_MVVM_SET_PROPERTY_VALUE(ActiveWeaponName, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(StowedWeaponName, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(GrenadeCount, 0);
	UE_MVVM_SET_PROPERTY_VALUE(GrappleCooldownDuration, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(bGrappleReady, true);

	SetVitalsState(EBRUIDataState::Unknown);
	SetEquipmentState(EBRUIDataState::Unknown);
}

void UBRVM_Combat::BeginDestroy()
{
	UnbindFromAbilitySystem();
	OnHitMarkerEvent.Clear();
	Super::BeginDestroy();
}

void UBRVM_Match::SetTimeSource(AGameStateBase* InGameState)
{
	TimeSource = InGameState;

	if (InGameState)
	{
		UE_MVVM_SET_PROPERTY_VALUE(MatchState, EBRUIDataState::Live);
	}
	else
	{
		if (MatchState == EBRUIDataState::Live)
		{
			UE_MVVM_SET_PROPERTY_VALUE(MatchState, EBRUIDataState::Stale);
		}
		StopClockUpdates();
		return;
	}

	UpdateClocks();
}

void UBRVM_Match::SetMatchEndServerTime(float InServerTimeSeconds)
{
	MatchEndServerTime = InServerTimeSeconds;
	UpdateClocks();
}

void UBRVM_Match::SetRocketSpawnServerTime(float InServerTimeSeconds)
{
	RocketSpawnServerTime = InServerTimeSeconds;
	UpdateClocks();
}

void UBRVM_Match::SetRocketAvailable(bool bInAvailable)
{
	UE_MVVM_SET_PROPERTY_VALUE(bRocketAvailable, bInAvailable);
	if (bInAvailable)
	{
		RocketSpawnServerTime = 0.0f;
	}
	UpdateClocks();
}

void UBRVM_Match::SetTeamScores(int32 InTeam0Score, int32 InTeam1Score)
{
	UE_MVVM_SET_PROPERTY_VALUE(Team0Score, InTeam0Score);
	UE_MVVM_SET_PROPERTY_VALUE(Team1Score, InTeam1Score);
}

void UBRVM_Match::SetLocalTeamId(uint8 InTeamId)
{
	UE_MVVM_SET_PROPERTY_VALUE(LocalTeamId, InTeamId);
}

void UBRVM_Match::SetMatchPhaseTag(FGameplayTag InPhaseTag)
{
	UE_MVVM_SET_PROPERTY_VALUE(MatchPhaseTag, InPhaseTag);
}

UWorld* UBRVM_Match::GetTimerWorld() const
{
	if (const AGameStateBase* GameState = TimeSource.Get())
	{
		return GameState->GetWorld();
	}
	return GetWorld();
}

FText UBRVM_Match::FormatClock(int32 InSeconds)
{
	if (InSeconds < 0)
	{
		return LOCTEXT("ClockUnknown", "--:--");
	}

	FNumberFormattingOptions TwoDigits;
	TwoDigits.SetMinimumIntegralDigits(2);
	TwoDigits.SetUseGrouping(false);

	return FText::Format(LOCTEXT("ClockFormat", "{0}:{1}"),
		FText::AsNumber(InSeconds / 60),
		FText::AsNumber(InSeconds % 60, &TwoDigits));
}

void UBRVM_Match::UpdateClocks()
{
	const AGameStateBase* GameState = TimeSource.Get();
	if (!GameState)
	{
		UE_MVVM_SET_PROPERTY_VALUE(SecondsRemaining, static_cast<int32>(INDEX_NONE));
		UE_MVVM_SET_PROPERTY_VALUE(MatchClockText, FormatClock(INDEX_NONE));
		UE_MVVM_SET_PROPERTY_VALUE(bClockRunning, false);
		UE_MVVM_SET_PROPERTY_VALUE(RocketSecondsRemaining, static_cast<int32>(INDEX_NONE));
		UE_MVVM_SET_PROPERTY_VALUE(RocketCountdownText, FormatClock(INDEX_NONE));
		StopClockUpdates();
		return;
	}

	const double Now = GameState->GetServerWorldTimeSeconds();

	int32 NewMatchSeconds = INDEX_NONE;
	bool bNewClockRunning = false;
	if (MatchEndServerTime > 0.0f)
	{
		const double Remaining = static_cast<double>(MatchEndServerTime) - Now;
		NewMatchSeconds = FMath::Max(0, FMath::CeilToInt(Remaining));
		bNewClockRunning = Remaining > 0.0;
	}
	UE_MVVM_SET_PROPERTY_VALUE(SecondsRemaining, NewMatchSeconds);
	UE_MVVM_SET_PROPERTY_VALUE(MatchClockText, FormatClock(NewMatchSeconds));
	UE_MVVM_SET_PROPERTY_VALUE(bClockRunning, bNewClockRunning);

	int32 NewRocketSeconds = INDEX_NONE;
	if (!bRocketAvailable && RocketSpawnServerTime > 0.0f)
	{
		const double Remaining = static_cast<double>(RocketSpawnServerTime) - Now;
		NewRocketSeconds = FMath::Max(0, FMath::CeilToInt(Remaining));
	}
	UE_MVVM_SET_PROPERTY_VALUE(RocketSecondsRemaining, NewRocketSeconds);
	UE_MVVM_SET_PROPERTY_VALUE(RocketCountdownText, FormatClock(NewRocketSeconds));

	ScheduleNextClockUpdate();
}

void UBRVM_Match::ScheduleNextClockUpdate()
{
	UWorld* World = GetTimerWorld();
	const AGameStateBase* GameState = TimeSource.Get();
	if (!World || !GameState)
	{
		StopClockUpdates();
		return;
	}

	const bool bMatchClockNeedsTicking = bClockRunning;
	const bool bRocketClockNeedsTicking = (RocketSecondsRemaining > 0);
	if (!bMatchClockNeedsTicking && !bRocketClockNeedsTicking)
	{
		StopClockUpdates();
		return;
	}

	const double Now = GameState->GetServerWorldTimeSeconds();
	double Delay = 1.0 - (Now - FMath::FloorToDouble(Now));
	if (Delay < 0.01)
	{
		// HUD-CPP-AUDIT H3: ADD a second, never REPLACE with one. `Delay = 1.0` from a fire at
		// frac ~0.99 re-lands at frac ~0.99 — the degenerate condition reproduces itself and the
		// clock ticks permanently out of phase with the server second. Adding keeps the phase:
		// the next fire still lands on (just past) a whole-second boundary.
		Delay += 1.0;
	}

	World->GetTimerManager().SetTimer(
		ClockTimerHandle, this, &UBRVM_Match::UpdateClocks, static_cast<float>(Delay), false);
}

void UBRVM_Match::StopClockUpdates()
{
	if (UWorld* World = GetTimerWorld())
	{
		World->GetTimerManager().ClearTimer(ClockTimerHandle);
	}
	ClockTimerHandle.Invalidate();
}

void UBRVM_Match::PushKillfeedEntry(const FBRKillfeedViewEntry& InEntry)
{
	// HUD-CPP-AUDIT H12: no world means no clock to stamp an expiry against and no timer
	// manager to arm — a `0.0`-based stamp either never expires or expires everything in one
	// burst when a real world appears. A kill pushed during travel is a kill nobody can see
	// anyway; drop it loudly rather than poison the ring.
	const UWorld* World = GetTimerWorld();
	if (!World)
	{
		UE_LOG(Logbreachpoint, Warning, TEXT("UBRVM_Match::PushKillfeedEntry with no world; entry dropped."));
		return;
	}

	// H5: no fallback sequence id. The fallback was per-VM, so host and client assigned
	// DIFFERENT ids to the same kill and AppendSpotterLine could attach a line to a different
	// row per machine. The producer owns the id (server-assigned FBRKillFeedEntry::Sequence);
	// an id-less entry still renders — it just can never receive a spotter line, which is the
	// honest outcome for an entry nothing can address.
	const UBRUISettings& Settings = UBRUISettings::Get();
	const int32 MaxVisible = FMath::Max(1, Settings.KillfeedMaxVisibleEntries);

	while (KillfeedEntries.Num() >= MaxVisible)
	{
		KillfeedEntries.RemoveAt(0);
	}

	FBRKillfeedViewEntry Entry = InEntry;
	Entry.ExpiryTime = World->GetTimeSeconds() + FMath::Max(0.5f, Settings.KillfeedEntryLifetimeSeconds);

	KillfeedEntries.Add(Entry);

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(KillfeedEntries);
	PublishKillfeedChanged();
	ScheduleKillfeedExpiry();
}

void UBRVM_Match::AppendSpotterLine(int32 InSequenceId, const FText& InLine)
{
	for (FBRKillfeedViewEntry& Entry : KillfeedEntries)
	{
		if (Entry.SequenceId == InSequenceId)
		{
			Entry.SpotterLine = InLine;
			UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(KillfeedEntries);
			PublishKillfeedChanged();
			return;
		}
	}
}

void UBRVM_Match::ExpireKillfeedEntries()
{
	const UWorld* World = GetTimerWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();

	// The stamp lives ON the entry (HUD-CPP-AUDIT: the old parallel KillfeedExpiryTimes array
	// was hand-synced with guards that HID desync rather than catching it). Entries are pushed
	// in time order, so the ring's head is always the next to die.
	int32 NumRemoved = 0;
	while (KillfeedEntries.Num() > 0 && KillfeedEntries[0].ExpiryTime <= Now)
	{
		KillfeedEntries.RemoveAt(0);
		++NumRemoved;
	}

	if (NumRemoved > 0)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(KillfeedEntries);
		PublishKillfeedChanged();
	}

	ScheduleKillfeedExpiry();
}

void UBRVM_Match::ScheduleKillfeedExpiry()
{
	UWorld* World = GetTimerWorld();
	if (!World)
	{
		return;
	}

	if (KillfeedEntries.Num() == 0)
	{
		World->GetTimerManager().ClearTimer(KillfeedExpiryTimerHandle);
		KillfeedExpiryTimerHandle.Invalidate();
		return;
	}

	const double Delay = FMath::Max(0.05, KillfeedEntries[0].ExpiryTime - World->GetTimeSeconds());
	World->GetTimerManager().SetTimer(
		KillfeedExpiryTimerHandle, this, &UBRVM_Match::ExpireKillfeedEntries,
		static_cast<float>(Delay), false);
}

void UBRVM_Match::PublishKillfeedChanged()
{
	// The one channel. The KillfeedChanged FMVVMEventField was cut (HUD-CPP-AUDIT): both
	// consumers subscribe to this native delegate, and this function used to fire both.
	OnKillfeedChangedEvent.Broadcast();
}

void UBRVM_Match::ClearKillfeed()
{
	if (KillfeedEntries.Num() == 0)
	{
		return;
	}

	KillfeedEntries.Reset();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(KillfeedEntries);
	PublishKillfeedChanged();
	ScheduleKillfeedExpiry();
}

void UBRVM_Match::ClearToUnknown()
{
	StopClockUpdates();
	ClearKillfeed();

	TimeSource.Reset();
	MatchEndServerTime = 0.0f;
	RocketSpawnServerTime = 0.0f;

	UE_MVVM_SET_PROPERTY_VALUE(SecondsRemaining, static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(MatchClockText, FormatClock(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(bClockRunning, false);
	UE_MVVM_SET_PROPERTY_VALUE(Team0Score, 0);
	UE_MVVM_SET_PROPERTY_VALUE(Team1Score, 0);
	UE_MVVM_SET_PROPERTY_VALUE(LocalTeamId, static_cast<uint8>(255));
	UE_MVVM_SET_PROPERTY_VALUE(MatchPhaseTag, FGameplayTag());
	UE_MVVM_SET_PROPERTY_VALUE(RocketSecondsRemaining, static_cast<int32>(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(RocketCountdownText, FormatClock(INDEX_NONE));
	UE_MVVM_SET_PROPERTY_VALUE(bRocketAvailable, false);

	UE_MVVM_SET_PROPERTY_VALUE(MatchState, EBRUIDataState::Unknown);
}

void UBRVM_Match::BeginDestroy()
{
	StopClockUpdates();
	if (UWorld* World = GetTimerWorld())
	{
		World->GetTimerManager().ClearTimer(KillfeedExpiryTimerHandle);
	}
	OnKillfeedChangedEvent.Clear();
	Super::BeginDestroy();
}

#undef LOCTEXT_NAMESPACE
