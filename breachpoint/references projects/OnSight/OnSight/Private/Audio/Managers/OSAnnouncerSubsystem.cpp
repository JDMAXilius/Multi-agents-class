// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/Managers/OSAnnouncerSubsystem.h"
#include "Core/OSGameState.h"
#include "Core/GameStates/OSGameState_Domination.h"
#include "Actors/OSControlPoint.h"
#include "Core/OSPlayerState.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

static EAudioTeamRelationship ResolveRelationship(const UObject* WorldCtx, int32 ActingTeamIndex)
{
	// ActingTeamIndex comes from AOSControlPoint (OwningTeamIndex/CapturingTeamIndex) — uses INDEX_NONE
	if (ActingTeamIndex == INDEX_NONE) return EAudioTeamRelationship::Friendly;

	const APlayerController* LocalPC = UGameplayStatics::GetPlayerController(WorldCtx, 0);
	if (!LocalPC) return EAudioTeamRelationship::Friendly;
	const AOSPlayerState* LocalPS = LocalPC->GetPlayerState<AOSPlayerState>();
	if (!LocalPS) return EAudioTeamRelationship::Friendly;

	const int32 LocalTeam = LocalPS->GetTeamIndex();
	// GetTeamIndex() uses FGenericTeamId::NoTeam (255) for "no team"
	if (LocalTeam == FGenericTeamId::NoTeam) return EAudioTeamRelationship::Friendly;

	return (LocalTeam == ActingTeamIndex) ? EAudioTeamRelationship::Friendly : EAudioTeamRelationship::Enemy;
}

void UOSAnnouncerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BindToGameState();
}

void UOSAnnouncerSubsystem::Deinitialize()
{
	// Unbind from GameState
	if (UWorld* World = GetWorld())
	{
		if (auto* GS = World->GetGameState<AOSGameState>())
			GS->OnOneMinuteWarning.RemoveAll(this);

		if (auto* CPGS = World->GetGameState<AOSGameState_Domination>())
			CPGS->OnControlPointsChanged.RemoveDynamic(this, &UOSAnnouncerSubsystem::OnControlPointListChanged);
	}

	// Unbind from all control points
	for (auto& Elem : BoundControlPoints)
	{
		if (Elem.IsValid())
		{
			Elem->OnPointStateTransition.RemoveAll(this);
			Elem->OnPointCaptured.RemoveAll(this);
			Elem->OnPointNeutralized.RemoveAll(this);
		}
	}
	
	BoundControlPoints.Empty();
	Super::Deinitialize();
}

void UOSAnnouncerSubsystem::BindToGameState()
{
	UWorld* World = GetWorld();
	if (!World) return;
	
	AOSGameState* GS = World->GetGameState<AOSGameState>();
	if (!GS) return;
	
	GS->OnOneMinuteWarning.AddDynamic(this, &UOSAnnouncerSubsystem::OnOneMinuteWarningReceived);
	
	if (AOSGameState_Domination* CPGS = Cast<AOSGameState_Domination>(GS))
	{
		CPGS->OnControlPointsChanged.AddDynamic(this, &UOSAnnouncerSubsystem::OnControlPointListChanged);
		
		//Bind to any points already registered at world begin (listen server/host)
		for (AOSControlPoint* Point : CPGS->ControlPoints)
		{
			BindToControlPoint(Point);
		}
	}
}

void UOSAnnouncerSubsystem::BindToControlPoint(AOSControlPoint* Point)
{
	if (!IsValid(Point)) return;
	
	Point->OnPointStateTransition.RemoveDynamic(this, &UOSAnnouncerSubsystem::OnControlPointStateChanged);
	Point->OnPointStateTransition.AddDynamic(this, &UOSAnnouncerSubsystem::OnControlPointStateChanged);
	
	Point->OnPointCaptured.RemoveDynamic(this, &UOSAnnouncerSubsystem::OnControlPointCapturedReceived);
	Point->OnPointCaptured.AddDynamic(this, &UOSAnnouncerSubsystem::OnControlPointCapturedReceived);
	
	Point->OnPointNeutralized.RemoveDynamic(this, &UOSAnnouncerSubsystem::OnControlPointNeutralizedReceived);
	Point->OnPointNeutralized.AddDynamic(this, &UOSAnnouncerSubsystem::OnControlPointNeutralizedReceived);
	
	BoundControlPoints.Add(Point);
}

void UOSAnnouncerSubsystem::OnControlPointListChanged()
{
	UWorld* World = GetWorld();
	if (!World) return;
	
	AOSGameState_Domination* CPGS = World->GetGameState<AOSGameState_Domination>();
	if (!CPGS) return;
	
	for (AOSControlPoint* Point : CPGS->ControlPoints)
	{
		BindToControlPoint(Point);
	}
}

void UOSAnnouncerSubsystem::OnOneMinuteWarningReceived()
{
	OnOneMinuteWarning.Broadcast(); 
}

void UOSAnnouncerSubsystem::OnControlPointCapturedReceived(AOSControlPoint* Point, int32 CapturingTeamIndex, int32 PreviousOwnerTeamIndex)
{
	if (!IsValid(Point)) return;
	const EAudioTeamRelationship Relationship = ResolveRelationship(this, CapturingTeamIndex);
	OnControlPointCaptured.Broadcast(Relationship, Point->PointName);
}

void UOSAnnouncerSubsystem::OnControlPointNeutralizedReceived(AOSControlPoint* Point, int32 PreviousOwnerTeamIndex, int32 NeutralizingTeamIndex)
{
	if (!IsValid(Point)) return;
	// "Did OUR team just lose this point?" → Friendly if PreviousOwner == local team
	const EAudioTeamRelationship Relationship = ResolveRelationship(this, PreviousOwnerTeamIndex);
	OnControlPointNeutralized.Broadcast(Relationship, Point->PointName);
}

void UOSAnnouncerSubsystem::OnControlPointStateChanged(AOSControlPoint* Point, EControlPointState PreviousState, EControlPointState CurrentState, int32 OwningTeam, int32 CapturingTeam)
{
	if (!IsValid(Point)) return;
	if (PreviousState == EControlPointState::Uninitialized) return; //skips first-spawn weirdness

	const FName PointName = Point->PointName;

	switch (CurrentState)
	{
	case EControlPointState::Capturing:
		OnControlPointCapturing.Broadcast(ResolveRelationship(this, CapturingTeam), PointName);
		break;
	case EControlPointState::Neutralizing:
		OnControlPointNeutralizing.Broadcast(ResolveRelationship(this, OwningTeam), PointName);
		break;
	case EControlPointState::Contested:
		// No single acting team — the point's helper is fine here
		OnControlPointContested.Broadcast(Point->GetLocalClientTeamRelationship(), PointName);
		break;
	}
}

EAudioTeamRelationship UOSAnnouncerSubsystem::GetTeamIndexOccupant(AOSPlayerState* Occupant) const
{
	if (!Occupant) return EAudioTeamRelationship::Friendly;
	
	const APlayerController* LocalPC = UGameplayStatics::GetPlayerController(this, 0);
	if (!LocalPC) return EAudioTeamRelationship::Friendly;
	
	const AOSPlayerState* LocalPS = LocalPC->GetPlayerState<AOSPlayerState>();
	if (!LocalPS) return EAudioTeamRelationship::Friendly;
	
	
	const int32 OccupantTeam = Occupant->GetTeamIndex();
	const int32 LocalTeam = LocalPS->GetTeamIndex();
	
	if (LocalTeam == FGenericTeamId::NoTeam || OccupantTeam == FGenericTeamId::NoTeam)
	{
		return EAudioTeamRelationship::Friendly;
	}
	
	return (LocalTeam != OccupantTeam) ? EAudioTeamRelationship::Enemy : EAudioTeamRelationship::Friendly;
}