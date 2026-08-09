#include "Core/GameStates/OSGameState_Domination.h"

#include "OSLogCategories.h"
#include "Actors/OSControlPoint.h"
#include "Net/UnrealNetwork.h"

void AOSGameState_Domination::EnsureScoreArraySize()
{
	while (TeamObjectiveScores.Num() < MaxTDMTeams)
		TeamObjectiveScores.Add(0);
}

void AOSGameState_Domination::RegisterPoint(AOSControlPoint* Point, FName Name)
{
	if (!HasAuthority() || !IsValid(Point) || !Point->IsPointValid()) return;

	ControlPointByName.Add(Name, Point);

	if (!ControlPoints.Contains(Point))
		ControlPoints.Add(Point);

	UE_LOG(LogOSDomination, Log,
		TEXT("[GS_DOM][REGISTER] Name=%s TotalMap=%d TotalArray=%d"),
		*Name.ToString(), ControlPointByName.Num(), ControlPoints.Num());

	OnControlPointsChanged.Broadcast();
	ForceNetUpdate();
}

void AOSGameState_Domination::UnregisterPoint(FName Name)
{
	if (!HasAuthority()) return;

	AOSControlPoint* Point = nullptr;
	if (TObjectPtr<AOSControlPoint>* Found = ControlPointByName.Find(Name))
		Point = Found->Get();

	ControlPointByName.Remove(Name);
	if (Point)
		ControlPoints.Remove(Point);

	UE_LOG(LogOSDomination, Log,
		TEXT("[GS_DOM][UNREGISTER] Name=%s TotalMap=%d TotalArray=%d"),
		*Name.ToString(), ControlPointByName.Num(), ControlPoints.Num());

	OnControlPointsChanged.Broadcast();
	ForceNetUpdate();
}

int32 AOSGameState_Domination::AddTeamScore(int32 TeamIndex, int32 Amount)
{
	if (!HasAuthority()) return 0;
	EnsureScoreArraySize();
	if (!TeamObjectiveScores.IsValidIndex(TeamIndex)) return 0;

	TeamObjectiveScores[TeamIndex] += Amount;

	UE_LOG(LogOSDomination, Log,
		TEXT("[GS_DOM][SCORE] Team=%d +%d Total=%d"),
		TeamIndex, Amount, TeamObjectiveScores[TeamIndex]);

	// Broadcast only the changed team on server (OnRep_TeamObjectiveScores broadcasts all for clients)
	OnTeamObjectiveScoreChanged.Broadcast(TeamIndex);
	ForceNetUpdate();
	return TeamObjectiveScores[TeamIndex];
}

int32 AOSGameState_Domination::GetTeamScore(int32 TeamIndex) const
{
	return TeamObjectiveScores.IsValidIndex(TeamIndex) ? TeamObjectiveScores[TeamIndex] : 0;
}

int32 AOSGameState_Domination::GetTotalOccupantsOnObjectives() const
{
	int32 Total = 0;
	for (const auto& Point : ControlPoints)
	{
		if (IsValid(Point))
			Total += Point->GetTotalPlayerCount();
	}
	return Total;
}

int32 AOSGameState_Domination::GetTeamOccupantsOnObjectives(int32 TeamIndex) const
{
	int32 Total = 0;
	for (const auto& Point : ControlPoints)
	{
		if (IsValid(Point))
			Total += Point->GetTeamPlayerCount(TeamIndex);
	}
	return Total;
}

int32 AOSGameState_Domination::GetOccupiedPointCount(int32 TeamIndex) const
{
	int32 Count = 0;
	for (const auto& Point : ControlPoints)
	{
		if (IsValid(Point) && Point->GetTeamPlayerCount(TeamIndex) > 0)
			Count++;
	}
	return Count;
}

void AOSGameState_Domination::OnRep_ControlPoints()
{
	OnControlPointsChanged.Broadcast();
}

void AOSGameState_Domination::OnRep_TeamObjectiveScores()
{
	EnsureScoreArraySize();
	for (int32 i = 0; i < TeamObjectiveScores.Num(); ++i)
		OnTeamObjectiveScoreChanged.Broadcast(i);
}

void AOSGameState_Domination::Multicast_DomMatchResult_Implementation(int32 InWinningTeamIndex)
{
	OnDomMatchResult.Broadcast(InWinningTeamIndex);
}

void AOSGameState_Domination::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AOSGameState_Domination, ControlPoints);
	DOREPLIFETIME(AOSGameState_Domination, TeamObjectiveScores);
	DOREPLIFETIME(AOSGameState_Domination, DomScoreLimit);
	DOREPLIFETIME(AOSGameState_Domination, DomTimeLimitSeconds);
}
