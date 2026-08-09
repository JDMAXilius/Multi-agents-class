// Copyright Optimum Athena. All Rights Reserved.

#include "Actors/OSControlPoint.h"

#include "OSLogCategories.h"
#include "Characters/Player/OSPlayer.h"
#include "Characters/OSCharacter.h"
#include "Core/OSPlayerController.h"
#include "Core/OSPlayerState.h"
#include "Core/GameModes/OSGameMode_Domination.h"
#include "Core/GameStates/OSGameState_Domination.h"
#include "AbilitySystemComponent.h"
#include "Data/OSGameplayTags.h"
#include "Net/UnrealNetwork.h"



AOSControlPoint::AOSControlPoint()
{
	PrimaryActorTick.bCanEverTick = false;  // All timing driven by FTimerHandle, not Tick
	bReplicates = true;
}

bool AOSControlPoint::IsPointValid() const
{
	return !Colliders.IsEmpty();
}

FString AOSControlPoint::NormalizeLabelString( const FText& Text )
{
	FString S = Text.ToString();
	S.TrimStartAndEndInline();
	S = S.ToUpper();

	// normalize to A-Z
	FString Out;
	Out.Reserve(S.Len());
	for (TCHAR C : S)
	{
		if (C >= 'A' && C <= 'Z')
			Out.AppendChar(C);
	}

	return Out;
}

bool AOSControlPoint::IsLabelValid( const FString& Label )
{
	return !Label.IsEmpty();
}

// Bijective base-26: 0->"A", 25->"Z", 26->"AA", 27->"AB", etc.
FString AOSControlPoint::IndexToAlphaLabel( int32 Index )
{
	FString Out;
	int32 N = Index;

	do
	{
		const int32 R = N % 26;
		Out.InsertAt(0, static_cast<TCHAR>('A' + R));
		N = (N / 26) - 1;
	}
	while (N >= 0);

	return Out;
}

int32 AOSControlPoint::GetCaptureProgress() const
{
	return CaptureProgress;
}

int32 AOSControlPoint::GetMaxCap() const
{
	return MaxCap;
}

void AOSControlPoint::BeginPlay()
{
	Super::BeginPlay();

	InitializePoint();

	if (!HasAuthority()) return;

	// Populate collider array from level-placed children, then register with the GameMode
	Server_UpdateColliders();

	if (auto* GM = GetWorld()->GetAuthGameMode<AOSGameMode_Domination>())
	{
		GM->RegisterControlPoint(this);
	}
}

void AOSControlPoint::Register(FName Name)
{
	PointName = Name;
	bRegistered = true;

	UE_LOG(LogOSDomination, Log, TEXT("[CP][REGISTER] %s Label=%s HasAuth=%d"),
		*GetPointName(), *PointLabel.ToString(), HasAuthority());

	for (int32 i = Colliders.Num() - 1; i >= 0; --i)
	{
		UShapeComponent* Col = Colliders[i];
		if (!IsValid(Col))
		{
			Colliders.RemoveAtSwap(i);
			continue;
		}
		Col->Activate();
	}

	ForceNetUpdate();
	OnPointProgressUpdated.Broadcast(this, Uninitialized, GetControlPointState());
}

void AOSControlPoint::RequestUnregister()
{
	if (bUnregisterInProgress || !bRegistered) return;
	if (auto* GM = GetGameModeCasted<AOSGameMode_Domination>())
		GM->UnregisterControlPoint(PointName);
}

void AOSControlPoint::Unregister_Internal()
{
	UE_LOG(LogOSDomination, Warning,
		TEXT("[CP][UNREGISTER_INTERNAL] %s InProgress=%d"),
		*GetPointName(), bUnregisterInProgress);
	if (bUnregisterInProgress) return;
	bUnregisterInProgress = true;

	bRegistered = false;

	GetWorldTimerManager().ClearTimer(CaptureTimer);
	GetWorldTimerManager().ClearTimer(ScoreTimer);

	
	for (int32 i = Colliders.Num() - 1; i >= 0; --i)
	{
		auto* Col = Colliders[i];
		if (!IsValid(Col))
		{
			Colliders.RemoveAtSwap(i);
			continue;
		}
		Col->Deactivate();
	}
	
	bUnregisterInProgress = false;
}

// Derives the absolute capture state from replicated fields.
// Priority: Contested > Neutralizing > Captured > Capturing > Neutral.
EControlPointState AOSControlPoint::GetControlPointState() const
{
	if (bContested) return Contested;
	if (OwningTeamIndex == INDEX_NONE && CaptureProgress == 0) return Neutral;
	// Check Neutralizing before Captured — an enemy on a fully-capped point
	// is neutralizing it, even though progress is still at MaxCap
	if (OwningTeamIndex != INDEX_NONE && CapturingTeamIndex != INDEX_NONE
		&& CapturingTeamIndex != OwningTeamIndex)
		return Neutralizing;
	if (IsCaptured()) return Captured;
	if (CapturingTeamIndex != INDEX_NONE) return Capturing;
	return Neutral;
}

void AOSControlPoint::InitializePoint()
{
	const bool bWasAlreadyInit = ControlPointMesh != nullptr;

	if (!ControlPointMesh)
	{
		ControlPointMesh = FindComponentByTag<UStaticMeshComponent>("VisualMesh");
		if (!ControlPointMesh)
			ControlPointMesh = Cast<UStaticMeshComponent>(GetRootComponent());
	}

	if (!ControlPointMesh) return;

	// Only broadcast + apply visuals on first init or if mesh was just found.
	// Prevents PostLogin from re-triggering audio/UI on already-initialized points.
	if (!bWasAlreadyInit)
		NotifyPointStateChanged();
	else
		ApplyPointVisuals();
}

FLinearColor AOSControlPoint::GetColorForStateAndRelationship(EControlPointState State, EAudioTeamRelationship Rel) const
{
	const bool bFriendly = (Rel == EAudioTeamRelationship::Friendly);
	switch (State)
	{
	case Capturing:    return bFriendly ? ColorFriendlyCapturing : ColorEnemyCapturing;
	case Captured:     return bFriendly ? ColorFriendlyCaptured  : ColorEnemyCaptured;
	// Neutralizing is action-on-owner: Friendly relationship = my point being drained (enemy action).
	case Neutralizing: return bFriendly ? ColorEnemyCapturing    : ColorFriendlyCapturing;
	case Contested:    return ColorContested;
	default:           return ColorNeutral;
	}
}

FLinearColor AOSControlPoint::GetDisplayColorForTeam(int32 ViewerTeamIndex) const
{
	// Color is driven by who's acting on the point vs the viewer, which is
	// finer-grained than a Friendly/Enemy relationship flag. During Neutralizing
	// both the owner and the drainer have an "Enemy" relationship (one has an
	// enemy actor, the other has an enemy-owned point), so a single bool can't
	// distinguish them. Branch on the actual team indices instead.
	const EControlPointState State = GetControlPointState();

	switch (State)
	{
	case Contested:
		return ColorContested;

	case Capturing:
	case Neutralizing:
	{
		// Both states are driven by CapturingTeamIndex — that's the acting team.
		// Capturing: acting team is building progress on a neutral/enemy point.
		// Neutralizing: acting team is draining an owner's progress.
		const bool bViewerIsActing = (CapturingTeamIndex != INDEX_NONE && CapturingTeamIndex == ViewerTeamIndex);
		return bViewerIsActing ? ColorFriendlyCapturing : ColorEnemyCapturing;
	}

	case Captured:
	{
		const bool bViewerOwns = (OwningTeamIndex != INDEX_NONE && OwningTeamIndex == ViewerTeamIndex);
		return bViewerOwns ? ColorFriendlyCaptured : ColorEnemyCaptured;
	}

	default:
		return ColorNeutral;
	}
}

FLinearColor AOSControlPoint::GetDisplayColor() const
{
	return GetDisplayColorForTeam(GetLocalClientTeamIndex());
}

void AOSControlPoint::EndPlay( const EEndPlayReason::Type EndPlayReason )
{
	UE_LOG(LogOSDomination, Warning,
		TEXT("[CP][ENDPLAY] %s Reason=%d Registered=%d"),
		*GetPointName(), (int32)EndPlayReason, bRegistered);
	RequestUnregister();
	Super::EndPlay(EndPlayReason);
}

void AOSControlPoint::OnRep_ControlPointState()
{
	// Detect state transitions on the client and broadcast granular delegates.
	// State broadcast is absolute — consumers use GetLocalClientTeamRelationship() for perspective.
	const EControlPointState NewState = GetControlPointState();
	if (NewState != LastBroadcastState)
	{
		const EControlPointState OldState = LastBroadcastState;

		// Spurious revert: state enum returned to Captured with the same owner
		// (e.g. enemy entered a capped point and left before changing anything).
		// No ownership change, so no transition/capture events fire — visuals
		// still refresh below via NotifyPointStateChanged.
		const bool bSpuriousRevert = (NewState == Captured && OwningTeamIndex == LastKnownOwningTeamIndex);

		if (!bSpuriousRevert)
		{
			OnPointStateTransition.Broadcast(this, OldState, NewState, OwningTeamIndex, CapturingTeamIndex);

			if (NewState == Captured)
				OnPointCaptured.Broadcast(this, OwningTeamIndex, LastKnownOwningTeamIndex);
			else if (NewState == Neutral && OldState != Uninitialized)
				OnPointNeutralized.Broadcast(this, LastKnownOwningTeamIndex, CapturingTeamIndex);
		}
	}

	if (PlayerNumber != LastReplicatedPlayerNumber)
	{
		LastReplicatedPlayerNumber = PlayerNumber;
		OnPlayerCountChanged.Broadcast(this, PlayerNumber);
	}

	NotifyPointStateChanged();
	LastBroadcastState = NewState;
	LastKnownOwningTeamIndex = OwningTeamIndex;
}


bool AOSControlPoint::Server_PlayerEntered_Validate(AOSPlayerController* player)
{
	// Basic sanity — server's own overlap (NotifyActorBeginOverlap) is the authoritative path.
	// Strict overlap check here would false-reject fast movers due to replication lag.
	return IsValid(player) && player->GetPawn() != nullptr;
}

bool AOSControlPoint::Server_PlayerExited_Validate(AOSPlayerController* player)
{
	return IsValid(player);
}

void AOSControlPoint::Server_PlayerEntered_Implementation( AOSPlayerController* player )
{
	if (!player || !HasAuthority()) return;

	if (!player->GetPawn()) return;

	auto* state = player->GetPlayerState<AOSPlayerState>();
	if (!state) return;

	if (UWorld* World = GetWorld())
	{
		if (auto* GS = World->GetGameState<AOSGameState>())
		{
			if (GS->IsMatchOver()) return;
		}
	}

	if (auto* ASC = state->GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(FOSGameplayTags::Get().IsDead))
			return;
	}

	if (!CurrentPlayers.Contains(state))
		CurrentPlayers.Add(state);

	PlayerNumber = CurrentPlayers.Num();
	if (PlayerNumber != LastReplicatedPlayerNumber)
	{
		LastReplicatedPlayerNumber = PlayerNumber;
		OnPlayerCountChanged.Broadcast(this, PlayerNumber);
	}
	UpdateCaptureProgress();
}

void AOSControlPoint::Server_PlayerExited_Implementation( AOSPlayerController* player )
{
	if (!player || !HasAuthority()) return;

	auto* state = player->GetPlayerState<AOSPlayerState>();
	if (!state) return;

	if (CurrentPlayers.Contains(state))
		CurrentPlayers.Remove(state);

	if (CurrentLeader == state)
		CurrentLeader = nullptr;

	PlayerNumber = CurrentPlayers.Num();
	if (PlayerNumber != LastReplicatedPlayerNumber)
	{
		LastReplicatedPlayerNumber = PlayerNumber;
		OnPlayerCountChanged.Broadcast(this, PlayerNumber);
	}
	UpdateCaptureProgress();
}

// Overlap events fire on both client and server.
// Local delegate broadcast happens on ALL machines (for client-side UI/audio).
// Server RPC handles the authoritative capture tracking.
void AOSControlPoint::NotifyActorBeginOverlap( AActor* OtherActor )
{
	Super::NotifyActorBeginOverlap(OtherActor);

	auto* player = Cast<AOSPlayer>(OtherActor);
	if (!player) return;

	auto* PC = player->GetOSPlayerController();
	if (PC)
		Server_PlayerEntered(PC);

	if (auto* PS = player->GetPlayerState<AOSPlayerState>())
		OnPlayerEnteredPoint.Broadcast(this, PS);
}

void AOSControlPoint::NotifyActorEndOverlap( AActor* OtherActor )
{
	Super::NotifyActorEndOverlap(OtherActor);

	auto* player = Cast<AOSPlayer>(OtherActor);
	if (!player) return;

	auto* PC = player->GetOSPlayerController();
	if (PC)
		Server_PlayerExited(PC);

	if (auto* PS = player->GetPlayerState<AOSPlayerState>())
		OnPlayerExitedPoint.Broadcast(this, PS);
}

// Core capture loop — called on a timer at CaptureInterval.
// Three branches: empty (decay), contested (frozen), active capture (progress toward cap/neutral).
void AOSControlPoint::CaptureTick()
{
	if (!HasAuthority()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Stop all timers if match is over
	if (auto* GS = World->GetGameState<AOSGameState>())
	{
		if (GS->IsMatchOver())
		{
			GetWorldTimerManager().ClearTimer(CaptureTimer);
			GetWorldTimerManager().ClearTimer(ScoreTimer);
			return;
		}
	}

	const int32 Old = CaptureProgress;

	// --- Branch 1: Empty point — decay toward stable resting value ---
	// Unowned points decay to 0; owned points heal back to MaxCap
	if (CurrentPlayers.Num() == 0)
	{
		if (OwningTeamIndex == INDEX_NONE)
			CaptureProgress = FMath::Max(0, CaptureProgress - DecayRate);
		else
			CaptureProgress = FMath::Min(MaxCap, CaptureProgress + DecayRate);

		if (CaptureProgress != Old)
		{
			ForceNetUpdate();
			ApplyPointVisuals();
			OnPointProgressUpdated.Broadcast(this, LastBroadcastState, GetControlPointState());
		}

		// Stop ticking once decay reaches its target
		if ((OwningTeamIndex == INDEX_NONE && CaptureProgress == 0) ||
			(OwningTeamIndex != INDEX_NONE && CaptureProgress >= MaxCap))
		{
			GetWorldTimerManager().ClearTimer(CaptureTimer);
		}

		OnCaptureTickUpdate();
		return;
	}

	// --- Branch 2: Contested — multiple teams, no progress change ---
	if (bContested)
	{
		OnCaptureTickUpdate();
		return;
	}
	if (!IsValid(CurrentLeader)) return;

	// --- Branch 3: Active capture — single team present ---
	// Positive rate = capturing, negative rate = neutralizing an enemy-owned point
	const float Rate = ResolveCapRate();
	CaptureProgress = FMath::Clamp(CaptureProgress + (int32)Rate, 0, MaxCap);

	if (CaptureProgress != Old)
	{
		ForceNetUpdate();
		ApplyPointVisuals();
		OnPointProgressUpdated.Broadcast(this, LastBroadcastState, GetControlPointState());

		// BANDAID — see OSControlPoint-refactor-plan.md Phase 2.
		// Progress changed, so the state enum may have flipped (e.g. Neutral -> Capturing
		// when progress first goes positive). UpdateCaptureProgress only observed the
		// pre-tick state (progress still 0 on first overlap) and couldn't detect it, so
		// transitions would silently skip. The spurious-revert guard here is duplicated
		// from OnRep / UpdateCaptureProgress — three copies of the same logic, which is
		// exactly the drift surface the PublishStateTransition helper is meant to retire.
		const EControlPointState NewStateAfterTick = GetControlPointState();
		if (NewStateAfterTick != LastBroadcastState)
		{
			const EControlPointState OldState = LastBroadcastState;
			const bool bSpuriousRevert = (NewStateAfterTick == Captured && OwningTeamIndex == LastKnownOwningTeamIndex);
			if (!bSpuriousRevert)
			{
				OnPointStateTransition.Broadcast(this, OldState, NewStateAfterTick, OwningTeamIndex, CapturingTeamIndex);
			}
			LastBroadcastState = NewStateAfterTick;
			LastKnownOwningTeamIndex = OwningTeamIndex;
		}
	}

	// Progress drained to 0 — either neutralize an owned point or clear stale progress on a neutral point
	if (Rate < 0.f && CaptureProgress == 0)
	{
		if (OwningTeamIndex != INDEX_NONE)
		{
			OnNeutralized();
		}
		else
		{
			// Neutral point drained — new team can now capture
			LastCapturingTeamIndex = CapturingTeamIndex;
		}
		UpdateCaptureProgress();
		OnCaptureTickUpdate();
		return;
	}

	// Captured: friendly team filled progress to max
	if (Rate > 0.f && CaptureProgress >= MaxCap)
	{
		OnCaptured();
		OnCaptureTickUpdate();
		return;
	}

	OnCaptureTickUpdate();
}

// Called whenever a player enters/exits or state needs re-evaluation.
// Prunes dead players, resolves contested state, picks leader/occupant, and manages the capture timer.
void AOSControlPoint::UpdateCaptureProgress()
{
	if (!HasAuthority()) return;

	TSet<int32> TeamsOnPoint;
	const auto& OSTags = FOSGameplayTags::Get();

	// Prune dead/invalid players and collect unique team indices
	for (auto It = CurrentPlayers.CreateIterator(); It; ++It)
	{
		if (!It->IsValid()) { It.RemoveCurrent(); continue; }
		auto* PS = It->Get();
		auto* ASC = PS->GetAbilitySystemComponent();
		if (!ASC || ASC->HasMatchingGameplayTag(OSTags.IsDead))
		{
			It.RemoveCurrent();
			continue;
		}
		TeamsOnPoint.Add(PS->GetTeamIndex());
	}

	PlayerNumber = CurrentPlayers.Num();
	bContested = TeamsOnPoint.Num() > 1;

	if (!bContested && TeamsOnPoint.Num() == 1)
		CapturingTeamIndex = *TeamsOnPoint.begin();
	else
		CapturingTeamIndex = INDEX_NONE;  // No one capturing when contested or empty

	// Track which team is putting progress on this neutral point
	if (CapturingTeamIndex != INDEX_NONE && OwningTeamIndex == INDEX_NONE && CaptureProgress == 0)
		LastCapturingTeamIndex = CapturingTeamIndex;

	CurrentLeader = ResolveLeader();
	ResolveOccupant();

	LogState("UPDATE");

	// Start or stop the capture timer based on whether progress should be changing
	const bool bShouldCaptureTick = CurrentPlayers.Num() > 0 || ShouldDecay();
	if (!bShouldCaptureTick)
	{
		GetWorldTimerManager().ClearTimer(CaptureTimer);
	}
	else if (!GetWorldTimerManager().IsTimerActive(CaptureTimer))
	{
		GetWorldTimerManager().SetTimer(
			CaptureTimer, this, &AOSControlPoint::CaptureTick, CaptureInterval, true);
	}

	// Start or stop the score timer based on whether the point is captured.
	// ScoreTick self-cancels when state leaves Captured (e.g. during Neutralizing),
	// so on the revert back to Captured the timer must be re-armed here.
	const bool bShouldScoreTick = (GetControlPointState() == Captured);
	if (!bShouldScoreTick)
	{
		GetWorldTimerManager().ClearTimer(ScoreTimer);
	}
	else if (!GetWorldTimerManager().IsTimerActive(ScoreTimer))
	{
		GetWorldTimerManager().SetTimer(
			ScoreTimer, this, &AOSControlPoint::ScoreTick, ScoreInterval, true);
	}

	ForceNetUpdate();

	// Detect and broadcast state transitions (server-side — clients get this via OnRep)
	const EControlPointState NewState = GetControlPointState();
	if (NewState != LastBroadcastState)
	{
		const EControlPointState OldState = LastBroadcastState;

		// Spurious revert: Contested/Neutralizing → Captured with the same owner.
		// Ownership never changed — skip the transition broadcast so BP handlers
		// don't treat this as a new capture. Visuals still refresh below.
		const bool bSpuriousRevert = (NewState == Captured && OwningTeamIndex == LastKnownOwningTeamIndex);

		if (!bSpuriousRevert)
		{
			OnPointStateTransition.Broadcast(this, OldState, NewState, OwningTeamIndex, CapturingTeamIndex);
		}

		NotifyPointStateChanged();
		LastBroadcastState = NewState;
		LastKnownOwningTeamIndex = OwningTeamIndex;
	}
	else
	{
		ApplyPointVisuals();
	}
}

FText AOSControlPoint::GetLabeledPointNameText() const
{
	const FText NameText = FText::FromString(GetPointName());
	if (PointLabel.IsEmpty())
		return NameText;

	return FText::Format(FText::FromString(TEXT("{0} | {1}")), PointLabel, NameText);
}

void AOSControlPoint::SetPointLabel( const FText& InLabel )
{
	if (!HasAuthority()) return;
	PointLabel = InLabel;
	ForceNetUpdate();
	OnRep_ControlPointState();
}


FString AOSControlPoint::GetPointName() const
{
	return PointName != NAME_None ? *PointName.ToString() : *GetName();
}

void AOSControlPoint::GetLifetimeReplicatedProps( TArray<class FLifetimeProperty>& OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AOSControlPoint, CurrentLeader);
	DOREPLIFETIME(AOSControlPoint, CurrentOccupant);
	DOREPLIFETIME(AOSControlPoint, CaptureProgress);
	DOREPLIFETIME(AOSControlPoint, PlayerNumber);
	DOREPLIFETIME(AOSControlPoint, bContested);
	DOREPLIFETIME(AOSControlPoint, PointName);
	DOREPLIFETIME(AOSControlPoint, PointLabel);
	DOREPLIFETIME(AOSControlPoint, OwningTeamIndex);
	DOREPLIFETIME(AOSControlPoint, CapturingTeamIndex);
	DOREPLIFETIME(AOSControlPoint, LastCapturingTeamIndex);
	DOREPLIFETIME(AOSControlPoint, bIsCaptured);
}

void AOSControlPoint::Server_UpdateColliders_Implementation()
{
	auto comp = FindComponentByTag<USceneComponent>("ColliderHolder");
	if (!comp) return;
	auto colliders = comp->GetAttachChildren();
	Colliders.Empty();
	for (auto collider : colliders)
	{
		if (auto c = Cast<UShapeComponent>(collider))
			Colliders.Add(c);
	}
}

void AOSControlPoint::ScoreTick()
{
	if (!HasAuthority()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	if (auto* GS = World->GetGameState<AOSGameState>())
	{
		if (GS->IsMatchOver())
		{
			GetWorldTimerManager().ClearTimer(ScoreTimer);
			return;
		}
	}

	// Stop scoring if not captured, contested, or being neutralized by enemy
	const EControlPointState State = GetControlPointState();
	if (State != Captured)
	{
		GetWorldTimerManager().ClearTimer(ScoreTimer);
		return;
	}

	UE_LOG(LogOSDomination, Log, TEXT("[CP][SCORE_TICK] %s Team=%d"), *GetPointName(), OwningTeamIndex);

	if (auto* GM = GetWorld()->GetAuthGameMode<AOSGameMode_Domination>())
		GM->OnScoreTick(this, OwningTeamIndex);

	// Multicast score tick to all clients for audio/VFX feedback
	if (auto* GS = World->GetGameState<AOSGameState_Domination>())
		Multicast_ScoreTick(OwningTeamIndex, GS->GetTeamScore(OwningTeamIndex));
}

void AOSControlPoint::Multicast_ScoreTick_Implementation(int32 TeamIndex, int32 NewScore)
{
	OnScoreTick.Broadcast(TeamIndex, NewScore);
}

void AOSControlPoint::OnCaptured()
{
	LogState("Captured");
	if (!HasAuthority()) return;

	const int32 PreviousOwner = OwningTeamIndex;
	OwningTeamIndex = CapturingTeamIndex;
	bIsCaptured = true;
	LastCapturingTeamIndex = INDEX_NONE;

	GetWorldTimerManager().SetTimer(ScoreTimer, this, &AOSControlPoint::ScoreTick, ScoreInterval, true);
	GetWorldTimerManager().ClearTimer(CaptureTimer);

	ForceNetUpdate();

	// Broadcast the transition explicitly so listen-server BP handlers see it —
	// UpdateCaptureProgress is not re-entered on this path.
	const EControlPointState OldState = LastBroadcastState;
	if (OldState != Captured)
		OnPointStateTransition.Broadcast(this, OldState, Captured, OwningTeamIndex, CapturingTeamIndex);

	NotifyPointStateChanged();
	LastBroadcastState = Captured;
	LastKnownOwningTeamIndex = OwningTeamIndex;
	OnPointCaptured.Broadcast(this, OwningTeamIndex, PreviousOwner);

	if (auto* GM = GetWorld()->GetAuthGameMode<AOSGameMode_Domination>())
		GM->OnPointStateChanged(this);
}

void AOSControlPoint::OnNeutralized()
{
	LogState("Neutralized");
	if (!HasAuthority()) return;

	const int32 PreviousOwner = OwningTeamIndex;
	OwningTeamIndex = INDEX_NONE;
	bIsCaptured = false;
	CurrentOccupant = nullptr;

	GetWorldTimerManager().ClearTimer(ScoreTimer);

	ForceNetUpdate();

	// Broadcast the transition explicitly so listen-server BP handlers see it.
	const EControlPointState OldState = LastBroadcastState;
	if (OldState != Neutral)
		OnPointStateTransition.Broadcast(this, OldState, Neutral, OwningTeamIndex, CapturingTeamIndex);

	NotifyPointStateChanged();
	LastBroadcastState = Neutral;
	LastKnownOwningTeamIndex = OwningTeamIndex;
	OnPointNeutralized.Broadcast(this, PreviousOwner, CapturingTeamIndex);

	if (auto* GM = GetWorld()->GetAuthGameMode<AOSGameMode_Domination>())
		GM->OnPointStateChanged(this);

}

AOSPlayerState* AOSControlPoint::ResolveLeader()
{
	if (bContested) return nullptr;
	if (CurrentPlayers.Num() == 0) return nullptr;
	auto it = CurrentPlayers.begin();
	if (!it || !it->IsValid()) return nullptr;
	return it->Get();
}

void AOSControlPoint::ResolveOccupant()
{
	if (bContested) return;
	if (!IsValid(CurrentLeader)) return;

	// Set occupant to leader if no one owns the point or progress is at 0,
	// but do NOT set OwningTeamIndex here — that only happens in OnCaptured()
	// when progress reaches MaxCap. Setting it prematurely causes partially
	// captured neutral points to self-complete via decay.
	if (!IsValid(CurrentOccupant) || CaptureProgress <= 0)
	{
		CurrentOccupant = CurrentLeader;
	}
}

// Compute effective capture rate: positive = gaining progress, negative = draining.
// Team presence scaling: rate = base * min(cap, 1 + ln(N) * scaling).
float AOSControlPoint::ResolveCapRate() const
{
	if (!IsValid(CurrentLeader)) return 0.0f;

	int32 TeamCount = 0;
	for (const auto& PS : CurrentPlayers)
	{
		if (PS.IsValid() && PS->GetTeamIndex() == CapturingTeamIndex)
			TeamCount++;
	}

	const float Raw = TeamCount > 0
		? 1.0f + FMath::Loge(static_cast<float>(TeamCount)) * TeamPresenceCaptureScaling
		: 1.0f;
	const float Multiplier = FMath::Min(Raw, MaxTeamPresenceMultiplier);

	// Owned point with enemy capturing — drain toward neutral
	if (OwningTeamIndex != INDEX_NONE && OwningTeamIndex != CapturingTeamIndex)
		return -CaptureRate * Multiplier;

	// Neutral point with stale progress from a different team — drain first
	if (OwningTeamIndex == INDEX_NONE
		&& LastCapturingTeamIndex != INDEX_NONE
		&& LastCapturingTeamIndex != CapturingTeamIndex)
		return -CaptureRate * Multiplier;

	// Same team as owner, or fresh neutral point — gain progress
	return CaptureRate * Multiplier;
}

void AOSControlPoint::LogState( const FString& event ) const
{
	UE_LOG(LogOSDomination, Log,
		TEXT("[CP][%s] %s Players=%d Leader=%s Occupant=%s Contested=%d OwningTeam=%d CapTeam=%d Progress=%d/%d State=%d"),
		*event,
		*GetPointName(),
		CurrentPlayers.Num(),
		IsValid(CurrentLeader) ? *CurrentLeader->GetName() : TEXT("NONE"),
		IsValid(CurrentOccupant) ? *CurrentOccupant->GetName() : TEXT("NONE"),
		bContested,
		OwningTeamIndex,
		CapturingTeamIndex,
		CaptureProgress,
		MaxCap,
		(int32)GetControlPointState());
}

void AOSControlPoint::NotifyPointStateChanged()
{
	const EControlPointState Current = GetControlPointState();
	OnPointProgressUpdated.Broadcast(this, LastBroadcastState, Current);
	ApplyPointVisuals();
}

void AOSControlPoint::ApplyPointVisuals()
{
	if (!ControlPointMesh) return;
	if (!MID) MID = ControlPointMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (!MID) return;

	MID->SetVectorParameterValue(ColorParamName, GetDisplayColor());
}

int32 AOSControlPoint::GetLocalClientTeamIndex() const
{
	UWorld* World = GetWorld();
	if (!World) return INDEX_NONE;
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return INDEX_NONE;
	auto* PS = PC->GetPlayerState<AOSPlayerState>();
	return PS ? PS->GetTeamIndex() : INDEX_NONE;
}

EAudioTeamRelationship AOSControlPoint::GetLocalClientTeamRelationship() const
{
	
	return GetTeamRelationshipForTeam(GetLocalClientTeamIndex());
}

EAudioTeamRelationship AOSControlPoint::GetTeamRelationshipForTeam(int32 ViewerTeamIndex) const
{
	if (ViewerTeamIndex == INDEX_NONE) return EAudioTeamRelationship::Friendly;
	if (OwningTeamIndex != INDEX_NONE && OwningTeamIndex != ViewerTeamIndex)
		return EAudioTeamRelationship::Enemy;
	if (CapturingTeamIndex != INDEX_NONE && CapturingTeamIndex != ViewerTeamIndex)
		return EAudioTeamRelationship::Enemy;
	return EAudioTeamRelationship::Friendly;
}


TArray<AOSPlayerState*> AOSControlPoint::GetPlayersOnPoint() const
{
	TArray<AOSPlayerState*> Result;
	for (const auto& Weak : CurrentPlayers)
	{
		if (Weak.IsValid())
			Result.Add(Weak.Get());
	}
	return Result;
}

int32 AOSControlPoint::GetTeamPlayerCount(int32 TeamIndex) const
{
	int32 Count = 0;
	for (const auto& Weak : CurrentPlayers)
	{
		if (Weak.IsValid() && Weak->GetTeamIndex() == TeamIndex)
			Count++;
	}
	return Count;
}

void AOSControlPoint::SetMatchConfig(int32 InCaptureRate, int32 InDecayRate, int32 InMaxCap,
	float InCaptureInterval, float InScoreInterval,
	float InTeamPresenceCaptureScaling, float InMaxTeamPresenceMultiplier)
{
	CaptureRate = FMath::Max(1, InCaptureRate);
	DecayRate = FMath::Max(1, InDecayRate);
	MaxCap = FMath::Max(1, InMaxCap);
	CaptureInterval = FMath::Max(0.1f, InCaptureInterval);
	ScoreInterval = FMath::Max(0.1f, InScoreInterval);
	TeamPresenceCaptureScaling = FMath::Max(0.0f, InTeamPresenceCaptureScaling);
	MaxTeamPresenceMultiplier = FMath::Max(1.0f, InMaxTeamPresenceMultiplier);
}

void AOSControlPoint::UpdateScoreInterval(float NewInterval)
{
	if (!HasAuthority()) return;
	ScoreInterval = FMath::Max(0.1f, NewInterval);
	if (GetWorldTimerManager().IsTimerActive(ScoreTimer))
	{
		GetWorldTimerManager().ClearTimer(ScoreTimer);
		GetWorldTimerManager().SetTimer(ScoreTimer, this, &AOSControlPoint::ScoreTick, ScoreInterval, true);
	}
}

// Decay keeps the capture timer alive when the point is empty but not at rest.
// Unowned points rest at 0; owned points rest at MaxCap.
bool AOSControlPoint::ShouldDecay() const
{
	if (CurrentPlayers.Num() > 0) return false;
	if (OwningTeamIndex == INDEX_NONE && CaptureProgress > 0) return true;
	if (OwningTeamIndex != INDEX_NONE && CaptureProgress < MaxCap) return true;
	return false;
}

void AOSControlPoint::ForceCapture(int32 TeamIndex)
{
	if (!HasAuthority()) return;

	OwningTeamIndex = TeamIndex;
	CapturingTeamIndex = TeamIndex;
	CaptureProgress = MaxCap;
	bIsCaptured = true;
	bContested = false;
	LastCapturingTeamIndex = INDEX_NONE;

	GetWorldTimerManager().ClearTimer(CaptureTimer);
	GetWorldTimerManager().SetTimer(ScoreTimer, this, &AOSControlPoint::ScoreTick, ScoreInterval, true);

	ForceNetUpdate();

	const EControlPointState OldState = LastBroadcastState;
	if (OldState != Captured)
		OnPointStateTransition.Broadcast(this, OldState, Captured, OwningTeamIndex, CapturingTeamIndex);

	NotifyPointStateChanged();
	LastBroadcastState = Captured;
	LastKnownOwningTeamIndex = OwningTeamIndex;
	OnPointCaptured.Broadcast(this, TeamIndex, INDEX_NONE);

	if (auto* GM = GetWorld()->GetAuthGameMode<AOSGameMode_Domination>())
		GM->OnPointStateChanged(this);

	UE_LOG(LogOSDomination, Log, TEXT("[CP][FORCE_CAPTURE] %s Team=%d"), *GetPointName(), TeamIndex);
}

void AOSControlPoint::ResetToNeutral()
{
	if (!HasAuthority()) return;

	const int32 PreviousOwner = OwningTeamIndex;
	OwningTeamIndex = INDEX_NONE;
	CapturingTeamIndex = INDEX_NONE;
	CaptureProgress = 0;
	bIsCaptured = false;
	LastCapturingTeamIndex = INDEX_NONE;
	CurrentLeader = nullptr;
	CurrentOccupant = nullptr;
	bContested = false;

	GetWorldTimerManager().ClearTimer(CaptureTimer);
	GetWorldTimerManager().ClearTimer(ScoreTimer);

	ForceNetUpdate();

	const EControlPointState OldState = LastBroadcastState;
	if (OldState != Neutral)
		OnPointStateTransition.Broadcast(this, OldState, Neutral, OwningTeamIndex, CapturingTeamIndex);

	NotifyPointStateChanged();
	LastBroadcastState = Neutral;
	LastKnownOwningTeamIndex = OwningTeamIndex;
	if (PreviousOwner != INDEX_NONE)
		OnPointNeutralized.Broadcast(this, PreviousOwner, INDEX_NONE);

	if (auto* GM = GetWorld()->GetAuthGameMode<AOSGameMode_Domination>())
		GM->OnPointStateChanged(this);

	UE_LOG(LogOSDomination, Log, TEXT("[CP][RESET_NEUTRAL] %s"), *GetPointName());
}

#if WITH_EDITOR
void AOSControlPoint::PostEditChangeProperty(FPropertyChangedEvent& E)
{
	Super::PostEditChangeProperty(E);

	if (E.Property && E.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AOSControlPoint, PointLabel))
	{
		const FString Normalized = NormalizeLabelString(PointLabel);
		PointLabel = FText::FromString(Normalized);
	}
}
#endif
