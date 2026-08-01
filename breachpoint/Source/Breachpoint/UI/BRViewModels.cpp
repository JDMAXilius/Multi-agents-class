// Breachpoint. The two ViewModels.

#include "UI/BRViewModels.h"

#include "AbilitySystemComponent.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"
#include "UI/BRUISettings.h"

#define LOCTEXT_NAMESPACE "BreachpointUI"

// ===========================================================================
// UBRVM_Combat
// ===========================================================================

void UBRVM_Combat::BindToAbilitySystem(UAbilitySystemComponent* InASC, const FBRCombatAttributeBindings& InBindings)
{
	// Idempotent by construction: re-binding is the normal path (respawn, spectate target change),
	// so tear the old subscription down first rather than leaking a second one onto the same ASC.
	UnbindFromAbilitySystem();

	if (!InASC || !InBindings.HasAny())
	{
		// NOT an error. This is what a join-in-progress client looks like for the frames between
		// possession and InitAbilityActorInfo. Unknown is the honest answer; the caller calls again.
		UE_LOG(LogBRUI, Verbose, TEXT("Combat VM: no ASC/attributes yet - staying Unknown."));
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

	// State is a GE-applied tag (gas-purity.md), so the shield-broken signal is a tag event, not a
	// threshold test on Shields <= 0. Reading the tag is what keeps the HUD agreeing with the sim.
	ShieldsBrokenTagHandle = InASC->RegisterGameplayTagEvent(
			BRGameplayTags::State_Shields_Broken, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UBRVM_Combat::HandleShieldsBrokenTagChanged);

	// Publish now. Attribute delegates only fire on CHANGE, so a HUD that waits for one shows
	// dashes until the player is first shot - correct-looking and useless.
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

	// Values are KEPT. The death cam wants the last known bar; zeroing here would invent a fact.
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

	// Only claim Live once the ASC actually answered. An ASC that exists but has no attribute set
	// yet returns bFound == false for everything, and that is still Unknown, not zero.
	SetVitalsState(bAnyFound ? EBRUIDataState::Live : EBRUIDataState::Unknown);
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
	SetVitalsState(EBRUIDataState::Live);
}

void UBRVM_Combat::HandleShieldsBrokenTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const bool bBroken = NewCount > 0;
	if (bShieldsBroken == bBroken)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(bShieldsBroken, bBroken);

	// Two distinct signatures, because "shields down" and "shields back" are different decisions
	// for the player. Firing one event with a bool would push that branch into the widget graph.
	if (bBroken)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShieldsBrokenEvent);
	}
	else
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShieldsRestoredEvent);
	}
}

void UBRVM_Combat::RecomputeVitalRatios()
{
	// Derived in C++ so the WBP does no arithmetic. A "divide" node in a widget graph is one
	// refactor away from being a gameplay decision in a widget graph.
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
	switch (InKind)
	{
	case EBRHitMarkerKind::Shield:
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShieldHitConfirmed);
		break;
	case EBRHitMarkerKind::Flesh:
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FleshHitConfirmed);
		break;
	case EBRHitMarkerKind::Headshot:
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HeadshotHitConfirmed);
		break;
	case EBRHitMarkerKind::Kill:
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(KillConfirmed);
		break;
	case EBRHitMarkerKind::None:
	default:
		return;
	}

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

	// State last: the WBP switches on state, so it must flip to Unknown only once the numbers
	// underneath it are already meaningless. Flipping it first would show one frame of real
	// numbers labelled "unknown", which is the same class of lie in the other direction.
	SetVitalsState(EBRUIDataState::Unknown);
	SetEquipmentState(EBRUIDataState::Unknown);
}

void UBRVM_Combat::BeginDestroy()
{
	UnbindFromAbilitySystem();
	OnHitMarkerEvent.Clear();
	Super::BeginDestroy();
}

// ===========================================================================
// UBRVM_Match
// ===========================================================================

void UBRVM_Match::SetTimeSource(AGameStateBase* InGameState)
{
	TimeSource = InGameState;

	if (InGameState)
	{
		UE_MVVM_SET_PROPERTY_VALUE(MatchState, EBRUIDataState::Live);
	}
	else
	{
		// GameState gone (travel). Keep the last numbers, mark them stale, stop the clock rather
		// than let it free-run off a dead time base.
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
	// Falls back to the outer chain (this VM is outered to the GameInstance subsystem).
	return GetWorld();
}

FText UBRVM_Match::FormatClock(int32 InSeconds)
{
	if (InSeconds < 0)
	{
		// The honest placeholder. Never "0:00" - that reads as "time is up", which is a
		// different and much more actionable claim than "I do not know the time yet".
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
		// No time base => no claim. Not zero, not the last value pretending to still be counting.
		UE_MVVM_SET_PROPERTY_VALUE(SecondsRemaining, static_cast<int32>(INDEX_NONE));
		UE_MVVM_SET_PROPERTY_VALUE(MatchClockText, FormatClock(INDEX_NONE));
		UE_MVVM_SET_PROPERTY_VALUE(bClockRunning, false);
		UE_MVVM_SET_PROPERTY_VALUE(RocketSecondsRemaining, static_cast<int32>(INDEX_NONE));
		UE_MVVM_SET_PROPERTY_VALUE(RocketCountdownText, FormatClock(INDEX_NONE));
		StopClockUpdates();
		return;
	}

	// GetServerWorldTimeSeconds() is the engine's client-corrected server clock. Using
	// GetWorld()->GetTimeSeconds() here instead would give each client its own answer, which is
	// the exact desync the "one replicated end time" rule exists to prevent.
	const double Now = GameState->GetServerWorldTimeSeconds();

	// --- match clock ---
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

	// --- rocket countdown ---
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
		// Nothing is counting: no timer at all. An idle HUD costs zero callbacks, which is the
		// difference between "no Tick" as a rule and "no Tick" as a result.
		StopClockUpdates();
		return;
	}

	// Land on the next whole second of the SERVER clock, so the displayed digit changes when the
	// value actually changes. One callback per visible change.
	const double Now = GameState->GetServerWorldTimeSeconds();
	double Delay = 1.0 - (Now - FMath::FloorToDouble(Now));
	if (Delay < 0.01)
	{
		Delay = 1.0;
	}

	World->GetTimerManager().SetTimer(
		ClockTimerHandle, this, &UBRVM_Match::UpdateClocks, static_cast<float>(Delay), /*bLoop=*/false);
}

void UBRVM_Match::StopClockUpdates()
{
	if (UWorld* World = GetTimerWorld())
	{
		World->GetTimerManager().ClearTimer(ClockTimerHandle);
	}
	ClockTimerHandle.Invalidate();
}

void UBRVM_Match::PushKillfeedEntry(const FBRKillfeedEntry& InEntry)
{
	const UBRUISettings& Settings = UBRUISettings::Get();
	const int32 MaxVisible = FMath::Max(1, Settings.KillfeedMaxVisibleEntries);

	FBRKillfeedEntry Entry = InEntry;
	if (Entry.SequenceId == INDEX_NONE)
	{
		Entry.SequenceId = NextKillfeedSequenceFallback++;
	}

	while (KillfeedEntries.Num() >= MaxVisible)
	{
		// LOUD ON PURPOSE. A silent cap is indistinguishable from "the feed showed everything",
		// and the one place that matters is the moment a team trade wipes four players at once.
		UE_LOG(LogBRUI, Warning,
			TEXT("Killfeed window full (%d rows); dropping oldest row seq=%d ('%s' -> '%s'). ")
			TEXT("Raise BRUISettings.KillfeedMaxVisibleEntries or shorten KillfeedEntryLifetimeSeconds."),
			MaxVisible,
			KillfeedEntries[0].SequenceId,
			*KillfeedEntries[0].KillerName.ToString(),
			*KillfeedEntries[0].VictimName.ToString());

		KillfeedEntries.RemoveAt(0);
		if (KillfeedExpiryTimes.Num() > 0)
		{
			KillfeedExpiryTimes.RemoveAt(0);
		}
	}

	const UWorld* World = GetTimerWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	KillfeedEntries.Add(Entry);
	KillfeedExpiryTimes.Add(Now + FMath::Max(0.5f, Settings.KillfeedEntryLifetimeSeconds));

	// NOT UE_MVVM_SET_PROPERTY_VALUE: that macro compares old to new with operator==, and
	// TArray<FBRKillfeedEntry> has no element comparison. Mutate then broadcast explicitly.
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(KillfeedEntries);
	PublishKillfeedChanged();

	OnKillfeedEntryAddedEvent.Broadcast(Entry);
	ScheduleKillfeedExpiry();
}

void UBRVM_Match::AppendSpotterLine(int32 InSequenceId, const FText& InLine)
{
	for (FBRKillfeedEntry& Entry : KillfeedEntries)
	{
		if (Entry.SequenceId == InSequenceId)
		{
			Entry.SpotterLine = InLine;
			UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(KillfeedEntries);
			PublishKillfeedChanged();
			return;
		}
	}

	// Late line for a row that already expired. Dropping it is correct - the HUD never waits on
	// the LLM, and offline is an identical HUD minus flavour.
	UE_LOG(LogBRUI, Verbose, TEXT("Spotter line for expired killfeed row seq=%d discarded."), InSequenceId);
}

void UBRVM_Match::ExpireKillfeedEntries()
{
	const UWorld* World = GetTimerWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	int32 NumRemoved = 0;
	while (KillfeedExpiryTimes.Num() > 0 && KillfeedExpiryTimes[0] <= Now)
	{
		KillfeedExpiryTimes.RemoveAt(0);
		if (KillfeedEntries.Num() > 0)
		{
			KillfeedEntries.RemoveAt(0);
		}
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

	if (KillfeedExpiryTimes.Num() == 0)
	{
		// Empty feed => no timer. Same discipline as the clock.
		World->GetTimerManager().ClearTimer(KillfeedExpiryTimerHandle);
		KillfeedExpiryTimerHandle.Invalidate();
		return;
	}

	const double Delay = FMath::Max(0.05, KillfeedExpiryTimes[0] - World->GetTimeSeconds());
	World->GetTimerManager().SetTimer(
		KillfeedExpiryTimerHandle, this, &UBRVM_Match::ExpireKillfeedEntries,
		static_cast<float>(Delay), /*bLoop=*/false);
}

void UBRVM_Match::PublishKillfeedChanged()
{
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(KillfeedChanged);
	OnKillfeedChangedEvent.Broadcast();
}

void UBRVM_Match::ClearKillfeed()
{
	if (KillfeedEntries.Num() == 0 && KillfeedExpiryTimes.Num() == 0)
	{
		return;
	}

	KillfeedEntries.Reset();
	KillfeedExpiryTimes.Reset();
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
	OnKillfeedEntryAddedEvent.Clear();
	OnKillfeedChangedEvent.Clear();
	Super::BeginDestroy();
}

#undef LOCTEXT_NAMESPACE
