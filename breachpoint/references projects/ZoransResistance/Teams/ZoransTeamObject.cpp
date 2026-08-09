// Copyright Zollpa LLC.

#include "ZoransResistance/Teams/ZoransTeamObject.h"
#include "ZoransResistance/Game/ZoransGameState.h"
#include "Data/TeamData.h"
#include "Net/UnrealNetwork.h"

void UZoransTeamObject::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION(UZoransTeamObject, TeamIndex, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UZoransTeamObject, FriendlyTeams, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UZoransTeamObject, AssignedPlayers, COND_None);
	
	DOREPLIFETIME_CONDITION_NOTIFY(UZoransTeamObject, TeamKills, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZoransTeamObject, TeamDeaths, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZoransTeamObject, TeamAssists, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZoransTeamObject, TeamObjectives, COND_None, REPNOTIFY_Always);
}

void UZoransTeamObject::AssignPlayerToTeam(AZoransPlayerState* InPlayerState)
{
	if (GetOwningActor()->GetNetMode() < NM_Client)
	{
		AssignedPlayers.AddUnique(InPlayerState);

		Internal_TeamAssignmentsUpdated();
	}
}

void UZoransTeamObject::RemoveAssignedPlayerFromTeam(AZoransPlayerState* InPlayerState)
{
	if (GetOwningActor()->GetNetMode() < NM_Client)
	{
		AssignedPlayers.Remove(InPlayerState);

		Internal_TeamAssignmentsUpdated();
	}
}

void UZoransTeamObject::UpdateKillScore(const int32 NewScore)
{
	if (GetOwningActor()->GetNetMode() < NM_Client)
	{
		TeamKills = NewScore;

		OnRep_TeamKills();
	}
}

void UZoransTeamObject::UpdateDeathScore(const int32 NewScore)
{
	if (GetOwningActor()->GetNetMode() < NM_Client)
	{
		TeamDeaths = NewScore;

		OnRep_TeamDeaths();
	}
}

void UZoransTeamObject::UpdateAssistScore(const int32 NewScore)
{
	if (GetOwningActor()->GetNetMode() < NM_Client)
	{
		TeamAssists = NewScore;

		OnRep_TeamAssists();
	}
}

void UZoransTeamObject::UpdateObjectiveScore(const int32 NewScore)
{
	if (GetOwningActor()->GetNetMode() < NM_Client)
	{
		TeamObjectives = NewScore;

		OnRep_TeamObjectives();
	}
}

UWorld* UZoransTeamObject::GetWorld() const
{
	if (const UObject* MyOuter = GetOuter())
	{
		return MyOuter->GetWorld();
	}
	
	return nullptr;
}

AActor* UZoransTeamObject::GetOwningActor() const
{
	return GetTypedOuter<AActor>();
}

bool UZoransTeamObject::CallRemoteFunction(UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack)
{
	check(!HasAnyFlags(RF_ClassDefaultObject));

	AActor* Owner = GetOwningActor();

	if (UNetDriver* NetDriver = Owner->GetNetDriver())
	{
		NetDriver->ProcessRemoteFunction(Owner, Function, Parms, OutParms, Stack, this);

		return true;
	}

	return false;
}

void UZoransTeamObject::Internal_TeamAssignmentsUpdated()
{
	if (OnAssignedPlayersChanged.IsBound())
	{
		OnAssignedPlayersChanged.Broadcast();

		return;
	}

	if (!OnAssignedPlayersChanged.IsBound())
	{
		if (AZoransGameState* GameState = Cast<AZoransGameState>(GetOwningActor()))
		{
			OnAssignedPlayersChanged.AddUniqueDynamic(GameState, &AZoransGameState::Internal_OnTeamAssignmentsUpdated);

			OnAssignedPlayersChanged.Broadcast();
		}
	}
}

void UZoransTeamObject::OnRep_TeamKills()
{
	if (OnTeamStatsChange.IsBound())
	{
		OnTeamStatsChange.Broadcast(this, EScoreStat::Kills);

		return;
	}

	if (!OnTeamStatsChange.IsBound())
	{
		if (AZoransGameState* GameState = Cast<AZoransGameState>(GetOwningActor()))
		{
			OnTeamStatsChange.AddUniqueDynamic(GameState, &AZoransGameState::Internal_OnRegisteredTeamStatsChanged);

			OnTeamStatsChange.Broadcast(this, EScoreStat::Kills);
		}
	}
}

void UZoransTeamObject::OnRep_TeamDeaths()
{
	if (OnTeamStatsChange.IsBound())
	{
		OnTeamStatsChange.Broadcast(this, EScoreStat::Deaths);

		return;
	}

	if (!OnTeamStatsChange.IsBound())
	{
		if (AZoransGameState* GameState = Cast<AZoransGameState>(GetOwningActor()))
		{
			OnTeamStatsChange.AddUniqueDynamic(GameState, &AZoransGameState::Internal_OnRegisteredTeamStatsChanged);

			OnTeamStatsChange.Broadcast(this, EScoreStat::Deaths);
		}
	}
}

void UZoransTeamObject::OnRep_TeamAssists()
{
	if (OnTeamStatsChange.IsBound())
	{
		OnTeamStatsChange.Broadcast(this, EScoreStat::Assists);

		return;
	}

	if (!OnTeamStatsChange.IsBound())
	{
		if (AZoransGameState* GameState = Cast<AZoransGameState>(GetOwningActor()))
		{
			OnTeamStatsChange.AddUniqueDynamic(GameState, &AZoransGameState::Internal_OnRegisteredTeamStatsChanged);

			OnTeamStatsChange.Broadcast(this, EScoreStat::Assists);
		}
	}
}

void UZoransTeamObject::OnRep_TeamObjectives()
{
	if (OnTeamStatsChange.IsBound())
	{
		OnTeamStatsChange.Broadcast(this, EScoreStat::Objective);

		return;
	}

	if (!OnTeamStatsChange.IsBound())
	{
		if (AZoransGameState* GameState = Cast<AZoransGameState>(GetOwningActor()))
		{
			OnTeamStatsChange.AddUniqueDynamic(GameState, &AZoransGameState::Internal_OnRegisteredTeamStatsChanged);

			OnTeamStatsChange.Broadcast(this, EScoreStat::Objective);
		}
	}
}