// Copyright Zollpa LLC.


#include "ZoransGameState.h"
#include "ZoransGameMode.h"
#include "ZoransGameModeLogic.h"
#include "ZoransGameInstance.h"
#include "ZoransWorldSubsystem.h"
#include "Data/StaticGameData.h"
#include "GameFramework/GameSession.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "ZoransResistance/ZoransResistance.h"
#include "ZoransResistance/Audio/ZoransInstanceAudioSubsystem.h"
#include "ZoransResistance/Player/ZoransPlayerState.h"
#include "ZoransResistance/Teams/ZoransTeamObject.h"
#include "ZoransResistance/Teams/Data/TeamData.h"

AZoransGameState::AZoransGameState()
{
	bReplicateUsingRegisteredSubObjectList = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval = 0.2f;
}

void AZoransGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(AZoransGameState, CurrentGamePhase, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransGameState, RegisteredTeams, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION(AZoransGameState, RegisteredFreeForAllTeam, COND_None);
	DOREPLIFETIME_CONDITION(AZoransGameState, GameModeOverride, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AZoransGameState, bInitialTeamAssignmentsFinished, COND_None);
	DOREPLIFETIME_CONDITION(AZoransGameState, PhaseEndTime, COND_None);
	DOREPLIFETIME_CONDITION(AZoransGameState, GameModeInfo, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AZoransGameState, GameModeSettings, COND_InitialOnly);
}

void AZoransGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);

	if (CurrentGamePhase == EGamePhase::Waiting)
	{
		if (ExitWaitingPhase())
		{
			SetGamePhase(EGamePhase::Warmup);
		}
	}

	if (bAutoAssignToTeams)
	{
		if (ITeamStateInterface* TeamStateInterface = Cast<ITeamStateInterface>(PlayerState))
		{
			if (TeamStateInterface->GetTeamAssignment() < 0)
			{
				const bool bIsJoiningBot = !PlayerState->GetPlayerController();
				
				if (const UZoransTeamObject* LowestPopulationTeam = GetLowestPopulationTeam(bIsJoiningBot))
				{
					const int32 LowestTeamIndex = GameModeOverride == EGameModeOverride::Tutorial ? (bIsJoiningBot ? 1 : 0) : LowestPopulationTeam->TeamIndex;
					TeamStateInterface->SetTeamAssignment(LowestTeamIndex);

					if (bSpawnAI && !bIsJoiningBot && bBotsInit)
					{
						for (auto const &ZoransPlayerState : LowestPopulationTeam->AssignedPlayers)
						{
							if (IsValid(ZoransPlayerState) && ZoransPlayerState->IsABot())
							{
								if (AController* Controller = ZoransPlayerState->GetOwningController())
								{
									ZoransPlayerState->SetTeamAssignment(-1);
									Controller->Destroy();
									break;
								}
							}
						}
					}
				}
			}
		}
	}
	
	Internal_OnPlayerJoin(PlayerState);
}

void AZoransGameState::RemovePlayerState(APlayerState* PlayerState)
{
	bool bHadTeam = false;
	
	AZoransPlayerState* ZoransPlayerState = Cast<AZoransPlayerState>(PlayerState);

	if (IsValid(ZoransPlayerState))
	{
		if (ZoransPlayerState->GetTeamAssignment() >= 0)
		{
			bHadTeam = true;
			ZoransPlayerState->SetTeamAssignment(-1);
		}
	}
	else
	{
		return;
	}
		
	if (CurrentGamePhase != EGamePhase::Ended && bSpawnAI && bAutoAssignToTeams && bHadTeam && !ZoransPlayerState->IsABot() && IsValid(AISpawnerClass))
	{
		if (const UZoransTeamObject* LowestPopulationTeam = GetLowestPopulationTeam(true))
		{
			if (DoesTeamHaveEmptySlot(LowestPopulationTeam))
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				GetWorld()->SpawnActor(AISpawnerClass, nullptr, nullptr, SpawnParams);
			}
		}
	}

	if (PlayerState->IsABot())
	{
		if (IsValid(GetZoransGameMode()))
		{
			GetZoransGameMode()->AvailableBotNames.AddUnique(PlayerState->GetPlayerName());
		}
	}
	
	Internal_OnPlayerLeave(PlayerState);
	
	Super::RemovePlayerState(PlayerState);
}

void AZoransGameState::Internal_OnPlayerJoin_Implementation(APlayerState* InPlayerState)
{
	if (IsValid(InPlayerState) && OnPlayerJoinDelegate.IsBound())
	{
		OnPlayerJoinDelegate.Broadcast(InPlayerState);
	}
}

void AZoransGameState::Internal_OnPlayerLeave_Implementation(APlayerState* InPlayerState)
{
	if (IsValid(InPlayerState) && OnPlayerLeaveDelegate.IsBound())
	{
		OnPlayerLeaveDelegate.Broadcast(InPlayerState);
	}
}

void AZoransGameState::SetGamePhase(const EGamePhase NewPhase, const bool UseCustomTime, const float TimeOverride)
{
	if (CurrentGamePhase != NewPhase)
	{
		CurrentGamePhase = NewPhase;

		if (CurrentGamePhase != EGamePhase::Ended)
		{
			if (AGameModeBase* GameModeBase = AuthorityGameMode.Get())
			{
				if (IsValid(GetZoransGameMode()))
				{
					const FGameModeDetails GameModeDetails = GetZoransGameMode()->GameModeDetails;
					const float CurrentTime = GetServerWorldTimeSeconds();

					switch (CurrentGamePhase)
					{
					case EGamePhase::Waiting :
						{
							PhaseEndTime = UseCustomTime ? CurrentTime + TimeOverride : CurrentTime + GameModeDetails.WaitingTimeLimit;
							break;
						}
					case EGamePhase::Warmup :
						{
							PhaseEndTime = UseCustomTime ? CurrentTime + TimeOverride : CurrentTime + GameModeDetails.WarmupTimeLimit;
							break;
						}
					case EGamePhase::InProgress :
						{
							bAutoAssignToTeams = true;
							
							AssignCurrentPlayersToTeams();
		
							UZoransGameInstance* ZoransInstance = nullptr;
							if (GetWorld())
							{
								ZoransInstance = Cast<UZoransGameInstance>(GetWorld()->GetGameInstance());
							}
							if (ZoransInstance && ZoransInstance->MatchTimeOverrideFloat > 1.f)
							{
								UE_LOG(LogTemp, Warning, TEXT("We are using the game instance env var MATCHTIME_OVERRIDE"));
								float MatchTimeOverride = ZoransInstance->MatchTimeOverrideFloat;
								PhaseEndTime = CurrentTime + MatchTimeOverride;
								break;
							}

							PhaseEndTime = UseCustomTime ? CurrentTime + TimeOverride : CurrentTime + GameModeDetails.RoundTimeLimit;
							break;
						}
					case EGamePhase::Cooldown :
						{
							PhaseEndTime = UseCustomTime ? CurrentTime + TimeOverride : CurrentTime + GameModeDetails.CooldownTimeLimit;
							break;
						}
					case EGamePhase::Ended :
						{
							break;
						}
					case EGamePhase::Loading :
						{
							PhaseEndTime = 9999.f;
							break;
						}
					case EGamePhase::ServerReady : 
						{
							PhaseEndTime = 3600.f;
							break;
						}
					default: break;
					}
				}
			}
		}
			
		OnRep_GamePhaseChanged();
	}
}

UZoransTeamObject* AZoransGameState::GetTeamObjectByTeamIndex(const int32 InIndex) const
{
	if (RegisteredTeams.IsValidIndex(InIndex))
	{
		return RegisteredTeams[InIndex];
	}

	return nullptr;
}

void AZoransGameState::AssignPlayerStateToTeam(AZoransPlayerState* InPlayerState, const int32 TeamIndex)
{
	if (GetNetMode() < NM_Client && IsValid(InPlayerState))
	{
		if (RegisteredTeams.IsValidIndex(TeamIndex))
		{
			RegisteredTeams[TeamIndex]->AssignPlayerToTeam(InPlayerState);
			
			for (int Index = 0; Index < RegisteredTeams.Num(); ++Index)
			{
				if (Index != TeamIndex && RegisteredTeams[Index]->AssignedPlayers.Contains(InPlayerState))
				{
					RegisteredTeams[Index]->RemoveAssignedPlayerFromTeam(InPlayerState);
				}
			}
		}
		else
		{
			for (int Index = 0; Index < RegisteredTeams.Num(); ++Index)
			{
				if (Index != TeamIndex && RegisteredTeams[Index]->AssignedPlayers.Contains(InPlayerState))
				{
					RegisteredTeams[Index]->RemoveAssignedPlayerFromTeam(InPlayerState);
				}
			}
		}
	}
}

void AZoransGameState::UpdatePlayerScore(AZoransPlayerState* InPlayerState, const EScoreStat StatToChange, const int32 Amount, const EStatChangeOperation StatChangeOperation, const bool bContributeToTeamScore)
{
	if (GetNetMode() == NM_Client || (Amount == 0 && StatChangeOperation != EStatChangeOperation::Override) || CurrentGamePhase != EGamePhase::InProgress)
	{
		return;
	}

	ensure(InPlayerState);
	
	switch (StatToChange)
	{
	case EScoreStat::Kills :
		{
			const int32 CurrentPlayerScore = InPlayerState->PlayerKills;
			
			const int32 NewPlayerScore = CalculateScoreChange(CurrentPlayerScore, Amount, StatChangeOperation);

			InPlayerState->UpdateKillScore(NewPlayerScore);

			if (const int32 TeamAssignment = InPlayerState->GetTeamAssignment(); TeamAssignment >= 0)
			{
				UpdateScoreForTeam(TeamAssignment, EScoreStat::Kills, Amount, StatChangeOperation);

				if (GameModeSettings.bKillsCountAsObjective)
				{
					const int32 CurrentPlayerObjectiveScore = InPlayerState->PlayerObjectives;
			
					const int32 NewPlayerObjectiveScore = CalculateScoreChange(CurrentPlayerObjectiveScore, Amount, StatChangeOperation);

					InPlayerState->UpdateObjectiveScore(NewPlayerObjectiveScore);

					UpdateScoreForTeam(TeamAssignment, EScoreStat::Objective, Amount, StatChangeOperation);
				}
			}
						
			return;
		}
	case EScoreStat::Assists :
		{
			const int32 CurrentPlayerScore = InPlayerState->PlayerAssists;
			
			const int32 NewPlayerScore = CalculateScoreChange(CurrentPlayerScore, Amount, StatChangeOperation);

			InPlayerState->UpdateAssistScore(NewPlayerScore);

			if (const int32 TeamAssignment = InPlayerState->GetTeamAssignment(); TeamAssignment >= 0)
			{
				UpdateScoreForTeam(TeamAssignment, EScoreStat::Assists, Amount, StatChangeOperation);
			}
			
			return;
		}
	case EScoreStat::Deaths :
		{
			const int32 CurrentPlayerScore = InPlayerState->PlayerDeaths;
			
			const int32 NewPlayerScore = CalculateScoreChange(CurrentPlayerScore, Amount, StatChangeOperation);

			InPlayerState->UpdateDeathScore(NewPlayerScore);

			if (const int32 TeamAssignment = InPlayerState->GetTeamAssignment(); TeamAssignment >= 0)
			{
				UpdateScoreForTeam(TeamAssignment, EScoreStat::Deaths, Amount, StatChangeOperation);
			}
			
			return;
		}
	case EScoreStat::Objective :
		{
			const int32 CurrentPlayerScore = InPlayerState->PlayerObjectives;
			
			const int32 NewPlayerScore = CalculateScoreChange(CurrentPlayerScore, Amount, StatChangeOperation);

			InPlayerState->UpdateObjectiveScore(NewPlayerScore);

			if (bContributeToTeamScore)
			{
				if (const int32 TeamAssignment = InPlayerState->GetTeamAssignment(); TeamAssignment >= 0)
				{
					UpdateScoreForTeam(TeamAssignment, EScoreStat::Objective, Amount, StatChangeOperation);
				}
			}
		}	
	}
}

void AZoransGameState::UpdateTeamScore(const int32 InTeamIndex, const EScoreStat StatToChange, const int32 Amount, const EStatChangeOperation StatChangeOperation)
{
	if (GetNetMode() == NM_Client || (Amount == 0 && StatChangeOperation != EStatChangeOperation::Override) || CurrentGamePhase != EGamePhase::InProgress)
	{
		return;
	}
	
	if (UZoransTeamObject* TeamObject = GetTeamObjectByTeamIndex(InTeamIndex))
	{
		switch (StatToChange)
		{
		case EScoreStat::Kills :
			{
				const int32 CurrentTeamScore = TeamObject->TeamKills;
			
				const int32 NewTeamScore = CalculateScoreChange(CurrentTeamScore, Amount, StatChangeOperation);

				TeamObject->UpdateKillScore(NewTeamScore);
			
				return;
			}
		case EScoreStat::Assists :
			{
				const int32 CurrentTeamScore = TeamObject->TeamAssists;
			
				const int32 NewTeamScore = CalculateScoreChange(CurrentTeamScore, Amount, StatChangeOperation);

				TeamObject->UpdateAssistScore(NewTeamScore);
			
				return;
			}
		case EScoreStat::Deaths :
			{
				const int32 CurrentTeamScore = TeamObject->TeamDeaths;
			
				const int32 NewTeamScore = CalculateScoreChange(CurrentTeamScore, Amount, StatChangeOperation);

				TeamObject->UpdateDeathScore(NewTeamScore);
			
				return;
			}
		case EScoreStat::Objective :
			{
				const int32 CurrentTeamScore = TeamObject->TeamObjectives;
			
				const int32 NewTeamScore = CalculateScoreChange(CurrentTeamScore, Amount, StatChangeOperation);

				TeamObject->UpdateObjectiveScore(NewTeamScore);
			}	
		}
	}
}

void AZoransGameState::Internal_OnRegisteredTeamStatsChanged(UZoransTeamObject* TeamObject, const EScoreStat ChangedStat)
{
	TeamStatsUpdated(TeamObject, ChangedStat);
	
	if (IsValid(TeamObject) && OnRegisteredTeamStatsUpdated.IsBound())
	{
		OnRegisteredTeamStatsUpdated.Broadcast(TeamObject, ChangedStat);
	}
}

void AZoransGameState::AssignCurrentPlayersToTeams()
{
	if (!PlayerArray.IsEmpty())
	{
		bool bFirstAssignment = true;
		bool bFirstAssignmentSuccessful = false;
		
		for (auto &PlayerState : PlayerArray)
		{
			if (ITeamStateInterface* TeamStateInterface = Cast<ITeamStateInterface>(PlayerState))
			{
				if (TeamStateInterface->GetTeamAssignment() < 0)
				{
					if(GameModeSettings.bFreeForAll)
					{
						if(RegisteredFreeForAllTeam)
						{
							TeamStateInterface->SetTeamAssignment(RegisteredFreeForAllTeam->TeamIndex);
							APawn* PlayerPawn = PlayerState->GetPawn();
							if (IsValid(PlayerPawn) && PlayerPawn->GetClass() != GetZoransGameMode()->DefaultPawnClass)
							{
								GetZoransGameMode()->RequestRespawn(PlayerState->GetPlayerController(), false, PlayerPawn, -1.f);
							}
						}
					
					}else if (bFirstAssignment)
					{
						const int32 MaxTeamIndex = RegisteredFreeForAllTeam ? RegisteredFreeForAllTeam->TeamIndex : RegisteredTeams.Num();
						const int32 RandomTeamIndex = GameModeOverride == EGameModeOverride::Tutorial ? 0 : FMath::RandRange(0, MaxTeamIndex -1);
						if (RegisteredTeams.IsValidIndex(RandomTeamIndex))
						{
							if (const UZoransTeamObject* FirstTeam = RegisteredTeams[RandomTeamIndex])
							{
								TeamStateInterface->SetTeamAssignment(FirstTeam->TeamIndex);
								bFirstAssignment = false;
								bFirstAssignmentSuccessful = true;

								APawn* PlayerPawn = PlayerState->GetPawn();
								if (IsValid(PlayerPawn) && PlayerPawn->GetClass() != GetZoransGameMode()->DefaultPawnClass)
								{
									GetZoransGameMode()->RequestRespawn(PlayerState->GetPlayerController(), false, PlayerPawn, -1.f);
								}
							}
						}
					}
					else if (const UZoransTeamObject* ZoransTeamObject = GetLowestPopulationTeam())
					{
						TeamStateInterface->SetTeamAssignment(ZoransTeamObject->TeamIndex);

						if (PlayerState->GetPawn()->GetClass() != GetZoransGameMode()->DefaultPawnClass)
						{
							GetZoransGameMode()->RequestRespawn(PlayerState->GetPlayerController(), false, PlayerState->GetPawn(), -1.f);
						}
					}

					if (bFirstAssignment && !bFirstAssignmentSuccessful)
					{
						if (const UZoransTeamObject* ZoransTeamObject = GetLowestPopulationTeam())
						{
							TeamStateInterface->SetTeamAssignment(ZoransTeamObject->TeamIndex);

							if (PlayerState->GetPawn()->GetClass() != GetZoransGameMode()->DefaultPawnClass)
							{
								GetZoransGameMode()->RequestRespawn(PlayerState->GetPlayerController(), false, PlayerState->GetPawn(), -1.f);
							}
						}
					}
				}
			}
		}
	}

	if (bSpawnAI && IsValid(AISpawnerClass))
	{
		if (const AGameModeBase* GameModeBase = AuthorityGameMode.Get())
		{
			int32 MaxPlayers = GameModeBase->GameSession->MaxPlayers;
			if (GIsPlayInEditorWorld)
			{
				if (ZoransGameMode.Get())
				{
					MaxPlayers = ZoransGameMode->GameModeDetails.MaximumTeamSize * 2;
				}
			}


			
			int32 OccupiedSlots = 0;
		
			for (auto const &TeamObject : RegisteredTeams)
			{
				OccupiedSlots += TeamObject->AssignedPlayers.Num();
			}
		
			if (MaxPlayers > OccupiedSlots)
			{
				const int32 SpawnCount = MaxPlayers - OccupiedSlots;
				// if (GIsPlayInEditorWorld)
				// {
				// 	const AZoransGameMode* ZoranGameModeBase = Cast<AZoransGameMode>(GameModeBase);
				// 	SpawnCount = ZoranGameModeBase->GameModeDetails.MaximumTeamSize*2;
				// {
					
		
				if (SpawnCount > 0)
				{
					for (int32 z = 0; z < SpawnCount; z++)
					{
						FActorSpawnParameters SpawnParams;
						SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						GetWorld()->SpawnActor(AISpawnerClass, nullptr, nullptr, SpawnParams);
					}
				}
			}
		}

		bBotsInit = true;
	}

	bInitialTeamAssignmentsFinished = true;
	
	const FTimerDelegate NextTickTimer = FTimerDelegate::CreateUObject(this, &AZoransGameState::Internal_OnInitialTeamAssignmentsFinished);
	GetWorldTimerManager().SetTimerForNextTick(NextTickTimer);
}

void AZoransGameState::Internal_OnTeamAssignmentsUpdated_Implementation()
{
	if (OnTeamAssignmentsUpdated.IsBound())
	{
		OnTeamAssignmentsUpdated.Broadcast();
	}
}

void AZoransGameState::Internal_OnInitialTeamAssignmentsFinished_Implementation()
{
	if (OnInitialTeamAssignmentsCompleted.IsBound())
	{
		OnInitialTeamAssignmentsCompleted.Broadcast();
	}
}

void AZoransGameState::HandleKillfeedEvent_Implementation(APlayerState* SourcePlayerState, const APlayerState* TargetPlayerState, const UTexture2D* KillfeedIcon, const bool bWeakPoint)
{
	if (OnKillfeedEvent.IsBound())
	{
		bool bIsFinalKill = false;
		if (IsValid(GetZoransGameMode()) && !GameModeSettings.bFreeForAll && !RegisteredTeams.IsEmpty())
		{
			if (const AZoransPlayerState* ZoransPlayerState = Cast<AZoransPlayerState>(SourcePlayerState))
			{
				if (const int32 SourceTeamIndex = ZoransPlayerState->GetTeamAssignment(); SourceTeamIndex >= 0)
				{
					for (auto const &x : RegisteredTeams)
					{
						if (SourceTeamIndex == x->TeamIndex)
						{
							bIsFinalKill = (x->TeamKills == FinalScoreTarget); 
							break;
						}
					}
				}
			}
		}
		
		OnKillfeedEvent.Broadcast(SourcePlayerState, TargetPlayerState, KillfeedIcon, bWeakPoint, bIsFinalKill);
	}
}

int32 AZoransGameState::GetNumberOfBotsOnTeam(const UZoransTeamObject* InTeamObject)
{
	int32 Bots = 0;

	if (IsValid(InTeamObject))
	{
		for (auto const &ZoransPlayerState : InTeamObject->AssignedPlayers)
		{
			if (ZoransPlayerState->IsABot())
			{
				Bots++;
			}
		}
	}
	
	return Bots;
}

float AZoransGameState::GetPhaseTimeRemaining() const
{
	return PhaseEndTime != 0.0f ? PhaseEndTime - GetServerWorldTimeSeconds() : 0.0f;
}

bool AZoransGameState::DoesTeamHaveEmptySlot(const UZoransTeamObject* InTeamObject) const
{
	if (IsValid(InTeamObject) && IsValid(GetZoransGameMode()))
	{
		const int32 MaxTeamSize = GetZoransGameMode()->GameModeDetails.MaximumTeamSize;
		const int32 CurrentTeamSize = InTeamObject->AssignedPlayers.Num();

		return CurrentTeamSize < MaxTeamSize;
	}

	return false;
}

UZoransTeamObject* AZoransGameState::GetLowestPopulationTeam(const bool bIncludeBots)
{
	UZoransTeamObject* LowestTeam = nullptr;
	
	if (!RegisteredTeams.IsEmpty())
	{
		if(GameModeSettings.bFreeForAll && RegisteredFreeForAllTeam)
		{
			return RegisteredFreeForAllTeam;
		}
		
		int32 LowestTeamCount = 9999;
		for (auto const &TeamObject : RegisteredTeams)
		{
			if (IsValid(TeamObject) && !TeamDefinitions[TeamObject->TeamIndex].bFreeForAllTeam)
			{
				if (bIncludeBots)
				{
					const int32 TeamNum = TeamObject->AssignedPlayers.Num();
					if (TeamNum < LowestTeamCount)
					{
						LowestTeamCount = TeamNum;
						LowestTeam = TeamObject;
					}
				}
				else
				{
					int32 PlayerNum = 0;
					for (auto const &y : TeamObject->AssignedPlayers)
					{
						if (!y->IsABot())
						{
							PlayerNum++;
						}
					}
					
					if (PlayerNum < LowestTeamCount)
					{
						LowestTeamCount = PlayerNum;
						LowestTeam = TeamObject;
					}
				}
			}
		}
	}

	return LowestTeam;
}

UZoransTeamObject* AZoransGameState::GetTeamWithMostBots()
{
	UZoransTeamObject* BotTeam = nullptr;
	
	if (!RegisteredTeams.IsEmpty())
	{
		int32 BotCount = -1;
		for (auto const &x : RegisteredTeams)
		{
			const int32 BotsOnTeam = GetNumberOfBotsOnTeam(x);
			if (BotsOnTeam > BotCount)
			{
				BotCount = BotsOnTeam;
				BotTeam = x;
			}
		}
	}
	
	return BotTeam;
}

bool AZoransGameState::ExitWaitingPhase() const
{
	if (!RegisteredTeams.IsEmpty())
	{
		if (GetZoransGameMode())
		{
			const FGameModeDetails GameModeDetails = GetZoransGameMode()->GameModeDetails;

			return PlayerArray.Num() >= GameModeDetails.MinimumTeamSize * RegisteredTeams.Num();
		}
	}
	
	return false;
}

void AZoransGameState::CleanupGameState()
{
	for (UZoransTeamObject* TeamObject : RegisteredTeams)
	{
		TeamObject->OnAssignedPlayersChanged.Clear();
		TeamObject->OnTeamStatsChange.Clear();
		TeamObject->AssignedPlayers.Empty();

		TeamObject->RemoveFromRoot();
		TeamObject->ConditionalBeginDestroy();
	}

	RegisteredTeams.Empty();
}

void AZoransGameState::CheckForPhaseChange()
{
	if (bSetPhaseAutomatically && CurrentGamePhase < EGamePhase::Ended && GetPhaseTimeRemaining() <= 0.f)
	{
		switch (CurrentGamePhase)
		{
		case EGamePhase::Waiting :
			{
				SetGamePhase(EGamePhase::Warmup);

				return;
			}
		case EGamePhase::Warmup :
			{
				SetGamePhase(EGamePhase::InProgress);

				return;
			}
		case EGamePhase::InProgress :
			{
				SetGamePhase(EGamePhase::Cooldown);

				return;
			}
		case EGamePhase::Cooldown :
			{
				SetGamePhase(EGamePhase::Ended);

				CleanupGameState();

				return;
			}
		case EGamePhase::Ended :
			{
				return;
			}
		case EGamePhase::Loading :
			{
				return;
			}
		}
	}
}

void AZoransGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetNetMode() < NM_Client)
	{
		CheckForPhaseChange();
	}
}

int32 AZoransGameState::CalculateScoreChange(const int32 CurrentAmount, const int32 ChangeAmount, const EStatChangeOperation StatChangeOperation)
{
	switch (StatChangeOperation)
	{
	case EStatChangeOperation::Add :
		{
			return CurrentAmount + ChangeAmount;
		}
	case EStatChangeOperation::Subtract :
		{
			return CurrentAmount - ChangeAmount;
		}
	case EStatChangeOperation::Multiply :
		{
			return CurrentAmount * ChangeAmount;
		}	
	case EStatChangeOperation::Divide :
		{
			return CurrentAmount / ChangeAmount;
		}
	case EStatChangeOperation::Override :
		{
			return ChangeAmount;
		}
	default :
		{
			return CurrentAmount;
		}
	}
}

void AZoransGameState::UpdateScoreForTeam(const int32 TeamIndex, const EScoreStat StatToChange, const int32 Amount, const EStatChangeOperation StatChangeOperation) const
{
	if (UZoransTeamObject* TeamObject = GetTeamObjectByTeamIndex(TeamIndex); IsValid(TeamObject))
	{
		switch (StatToChange)
		{
		case EScoreStat::Kills :
			{
				const int32 CurrentTeamScore = TeamObject->TeamKills;
			
				const int32 NewTeamScore = CalculateScoreChange(CurrentTeamScore, Amount, StatChangeOperation);

				TeamObject->UpdateKillScore(NewTeamScore);
				
				return;
			}
		case EScoreStat::Assists :
			{
				const int32 CurrentTeamScore = TeamObject->TeamAssists;
			
				const int32 NewTeamScore = CalculateScoreChange(CurrentTeamScore, Amount, StatChangeOperation);

				TeamObject->UpdateAssistScore(NewTeamScore);
				
				return;
			}
		case EScoreStat::Deaths :
			{
				const int32 CurrentTeamScore = TeamObject->TeamDeaths;
			
				const int32 NewTeamScore = CalculateScoreChange(CurrentTeamScore, Amount, StatChangeOperation);

				TeamObject->UpdateDeathScore(NewTeamScore);
			
				return;
			}
		case EScoreStat::Objective :
			{
				const int32 CurrentTeamScore = TeamObject->TeamObjectives;
			
				const int32 NewTeamScore = CalculateScoreChange(CurrentTeamScore, Amount, StatChangeOperation);

				TeamObject->UpdateObjectiveScore(NewTeamScore);
			}	
		}
	}
}	

void AZoransGameState::BeginPlay()
{
	if (GIsPlayInEditorWorld)
	{
		GameModeOverride = DefaultGameModeOverride;

		if (ExperienceDataTable)
		{
			const FString MapName = GetWorld()->GetMapName();
			const FString ActualMapName = MapName.RightChop(9);
		
			const FString GameModeOverrideString = UEnum::GetDisplayValueAsText(DefaultGameModeOverride).ToString();

			if (ExperienceDataTable->GetRowMap().Contains(*ActualMapName))
			{
				ExperienceData = *ExperienceDataTable->FindRow<FExperienceData>(*ActualMapName, "", false);

				if (ExperienceData.AvailableGameModes.Contains(GameModeOverrideString))
				{
					GameModeInfo = *ExperienceData.AvailableGameModes.Find(GameModeOverrideString);
					
					FinalScoreTarget = GameModeInfo.ScoreLimit;
				}
			}
		
		}
	
		Super::BeginPlay();
	}
	else
	{
		Super::BeginPlay();
	
		TArray<FString> OptionsArray;
		const FString WorldURL = GetWorld()->URL.ToString();
		WorldURL.ParseIntoArray(OptionsArray, TEXT("?"), true);
		FString GameModeOverrideString = "Default";
	
		for (auto String : OptionsArray)
		{
			if (String.StartsWith(TEXT("GameModeOverride=")))
			{
				GameModeOverrideString = String.RightChop(17);
				GameModeOverrideString.RemoveSpacesInline();
			}
		}
	
		if (GameModeMap.Contains(GameModeOverrideString))
		{
			GameModeOverride = *GameModeMap.Find(GameModeOverrideString);
		}
		
		if (IsValid(ExperienceDataTable))
		{
			const FString MapString = GetWorld()->GetMapName();
			const FName MapName = FName(MapString);
		
			if (ExperienceDataTable->GetRowNames().Contains(MapName))
			{
				ExperienceData = *ExperienceDataTable->FindRow<FExperienceData>(MapName, "", false);

				for (const auto &AvailableGameMode : ExperienceData.AvailableGameModes)
				{
					FString ModifiedGameModeString = AvailableGameMode.Key;
					
					ModifiedGameModeString.RemoveSpacesInline();
					
					if (ModifiedGameModeString == GameModeOverrideString)
					{
						GameModeInfo = AvailableGameMode.Value;
						FinalScoreTarget = GameModeInfo.ScoreLimit;

						UE_LOG(ZoransLog, Warning, TEXT("GameModeInfo Found -> %s"), *ModifiedGameModeString);
					
						break;
					}
				}
			}
		}
	}
	
	if (GetNetMode() < NM_Client)
	{
		FinalScoreTarget = GameModeInfo.ScoreLimit;
		
		ZoransGameMode = Cast<AZoransGameMode>(AuthorityGameMode);

		if (!GetGameModeInfo().GameModeRelatedDataLayers.IsEmpty())
		{
			UE_LOG(ZoransLog, Warning, TEXT("SetDataLayerRuntimeState -> GameMode Has Related Data Layers (%d)"), GetGameModeInfo().GameModeRelatedDataLayers.Num());

			if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
			{
				for (const UDataLayerAsset* DataLayer : GetGameModeInfo().GameModeRelatedDataLayers)
				{
					if (const UDataLayerInstance* DataLayerInstance = DataLayerSubsystem->GetDataLayerInstance(DataLayer))
					{
						if (DataLayerSubsystem->GetDataLayerRuntimeState(DataLayerInstance) != EDataLayerRuntimeState::Activated)
						{
							DataLayerSubsystem->SetDataLayerRuntimeState(DataLayerInstance, EDataLayerRuntimeState::Activated, false);
						}
					}
				}
			}
		}

		if(GameModeLogicMap.Contains(GetGameModeOverride()))
		{
			GameModeLogic = NewObject<UZoransGameModeLogic>(this, GameModeLogicMap[GetGameModeOverride()]);
		}else
		{
			GameModeLogic = NewObject<UZoransGameModeLogic>(this, DefaultGameModeLogic);
		}
		GameModeLogic->ZoransGameState = this;
		GameModeSettings = GameModeLogic->Settings;
		
		int32 TeamIndex = 0;
		RegisteredTeams.Empty();
		
		for (const FTeamDefinition& TeamDefinition : TeamDefinitions)
		{
			UZoransTeamObject* NewTeamObject = NewObject<UZoransTeamObject>(this);

			NewTeamObject->TeamIndex = TeamIndex;
			NewTeamObject->FriendlyTeams = TeamDefinition.FriendlyTeams;

			AddReplicatedSubObject(NewTeamObject);
			
			RegisteredTeams.Add(NewTeamObject);

			if(TeamDefinition.bFreeForAllTeam)
			{
				RegisteredFreeForAllTeam = NewTeamObject;
			}
			
			TeamIndex++;
		}
				
		OnRep_RegisteredTeams();
		
		if (ExitWaitingPhase())
		{
			SetGamePhase(EGamePhase::Warmup);
		}
		else
		{
			if (GetWorld()->GetGameInstance()->IsDedicatedServerInstance() && !GetWorld()->IsPlayInEditor())
			{
				auto GameMode = UGameplayStatics::GetGameMode(GetWorld());
				if (GameMode && GameMode->GetNumPlayers() < 1)
				{
					SetGamePhase(EGamePhase::ServerReady);
				}
				else
				{
					SetGamePhase(EGamePhase::Waiting);
				}
			}
			else
			{
				SetGamePhase(EGamePhase::Waiting);
			}
		}
	}

	if (UZoransWorldSubsystem* ZoransWorldSubsystem = GetWorld()->GetSubsystem<UZoransWorldSubsystem>())
	{
		ZoransWorldSubsystem->SetGameState(this);
	}

	if (UZoransInstanceAudioSubsystem* ZoransInstanceAudioSubsystem = GetGameInstance()->GetSubsystem<UZoransInstanceAudioSubsystem>())
	{
		if (GetNetMode() == NM_Client)
		{
			ZoransInstanceAudioSubsystem->InitializeWorld(GetWorld());
		}
		ZoransInstanceAudioSubsystem->FadeToAnotherTrack(GameModeInfo.LevelMusic, 1.0f);
	}
}

void AZoransGameState::OnRep_GamePhaseChanged()
{
	OnGamePhaseChanged(CurrentGamePhase);

	if (GamePhaseChangeDelegate.IsBound())
	{
		GamePhaseChangeDelegate.Broadcast(CurrentGamePhase);
	}
}

void AZoransGameState::OnRep_RegisteredTeams()
{
	if (RegisteredTeams.Num() == TeamDefinitions.Num())
	{
		UE_LOG(ZoransLog, Log, TEXT("Zorans Game State: Finished team registration. %d total teams registered."), RegisteredTeams.Num())
		bTeamRegistrationFinished = true;
		OnTeamRegistrationFinished.Broadcast();
	}
}

void AZoransGameState::BroadcastGameEvent(const FGameEventAnnouncement& InGameEvent)
{
	MTC_BroadcastGameEvent(InGameEvent);
}

void AZoransGameState::DebugPrintTags() const
{
	
	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(this, 0);
	const int32 NumPlayerStates = UGameplayStatics::GetNumPlayerStates(this);
	for (int32 Index = 0; Index < NumPlayerStates; Index++)
	{
		const AZoransPlayerState* CurrentPS = static_cast<AZoransPlayerState*>(UGameplayStatics::GetPlayerState(this, Index));
		if(CurrentPS->IsABot())
		{
			continue;
		}
		FString CurrentName = CurrentPS->GetPlayerName();
		FString TagString = "";
		for (const FName& DataTag : CurrentPS->DataTags)
		{
			TagString.Append(DataTag.ToString()).Append(" | ");
		}
		LocalPC->ClientMessage(CurrentName + ": " + TagString, NAME_None, 5);
	}
}

void AZoransGameState::MTC_BroadcastGameEvent_Implementation(const FGameEventAnnouncement& InGameEvent)
{
	if (GameEventNotification.IsBound() && (!InGameEvent.EventText.IsEmpty() || InGameEvent.EventSound != nullptr))
	{
		GameEventNotification.Broadcast(InGameEvent);
	}
}
