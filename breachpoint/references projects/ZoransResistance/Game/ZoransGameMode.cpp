// Copyright Zollpa LLC.


#include "ZoransGameMode.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemGlobals.h"
#include "EngineUtils.h"
#include "ZoransGameState.h"
#include "Online.h"
#include "OnlineSessionSettings.h"
#include "ZoransGameModeLogic.h"
#include "Engine/PlayerStartPIE.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "ZoransResistance/ZoransResistance.h"
#include "ZoransResistance/AbilitySystem/GameplayAbilitySystemFunctionLibrary.h"
#include "ZoransResistance/AbilitySystem/Data/ZoransGameplayTags.h"
#include "ZoransResistance/Audio/ZoransInstanceAudioSubsystem.h"
#include "ZoransResistance/Characters/ZoransCharacterBase.h"
#include "ZoransResistance/Characters/Data/CharacterData.h"
#include "ZoransResistance/Player/ZoransAIController.h"
#include "ZoransResistance/Player/ZoransPlayerState.h"
#include "ZoransResistance/Teams/ZoransTeamSpawner.h"

AZoransGameMode::AZoransGameMode()
{
	bUseSeamlessTravel = true;
	MaxInactivePlayers = 0;
	InactivePlayerStateLifeSpan = 0.f;
}

void AZoransGameMode::BeginPlay()
{
	if (BotDataTable)
	{
		TArray<FName> RowNames = BotDataTable->GetRowNames();

		for (FName BotName : RowNames)
		{
			AvailableBotNames.Add(BotName.ToString());
		}
	}
	
	if (const IOnlineSessionPtr SessionInterface = Online::GetSessionInterface())
	{
		if (const FOnlineSessionSettings* CurrentSettings = SessionInterface->GetSessionSettings("ZoransSession"))
		{
			GameSession->MaxPlayers = FMath::Clamp<int32>(CurrentSettings->NumPublicConnections, 2, 16);
		}
	}

	if (UZoransInstanceAudioSubsystem* ZoransInstanceAudioSubsystem = GetGameInstance()->GetSubsystem<UZoransInstanceAudioSubsystem>())
	{
		ZoransInstanceAudioSubsystem->InitializeWorld(GetWorld());
	}
	
	Super::BeginPlay();
}

void AZoransGameMode::AddInactivePlayer(APlayerState* PlayerState, APlayerController* PC)
{
	// do nothing
}

bool AZoransGameMode::FindInactivePlayer(APlayerController* PC)
{
	return false;
}

void AZoransGameMode::RequestRespawn(AController* InController, const bool bFirstSpawn, APawn* OldPawn, const float RespawnTimeOverride)
{
	bool bAIController = false;

	if (IsValid(InController))
	{
		bAIController = Cast<AAIController>(InController) ? true : false;
	}
	
	if (bFirstSpawn)
	{
		Internal_HandleRespawn(InController, bAIController, bFirstSpawn, OldPawn);
	}
	else
	{
		if (RespawnTimeOverride < -0.5f)
		{
			Internal_HandleRespawn(InController, false, false, OldPawn);
			return;
		}

		bool bPreTeamInit = false;
		
		if (const AZoransGameState* ZoransGameState = GetGameState<AZoransGameState>())
		{
			if (!ZoransGameState->bInitialTeamAssignmentsFinished)
			{
				bPreTeamInit = true;
			}
		}
		
		const float RespawnTime = RespawnTimeOverride > 0.0f ? RespawnTimeOverride : RespawnDelay;
		FTimerHandle RespawnTimer;
		const FTimerDelegate RespawnDelegate = FTimerDelegate::CreateUObject(this, &AZoransGameMode::Internal_HandleRespawn, InController, bAIController, bFirstSpawn, OldPawn, bPreTeamInit);
		
		GetWorldTimerManager().SetTimer(RespawnTimer, RespawnDelegate, RespawnTime, false);
	}
}

void AZoransGameMode::Internal_HandleRespawn(AController* InController, const bool bAIController, const bool bFirstSpawn, APawn* OldPawn, const bool bPreTeamInit)
{
	if (bPreTeamInit)
	{
		if (GetWorld() && GetWorld()->GetGameState())
		{
			if (const AZoransGameState* ZoransGameState = GetGameState<AZoransGameState>())
			{
				

				if (ZoransGameState->bInitialTeamAssignmentsFinished)
				{
					if (OldPawn)
					{
						OldPawn->Destroy();
					}
				
					return;
				}
			}
		}
	}	
	
	const AZoransGameState* ZoransGS = GetGameState<AZoransGameState>();
	if (ZoransGS && ZoransGS->GetCurrentPhase() == EGamePhase::Cooldown) { return; }
	
	if (IsValid(InController))
	{
		UClass* PawnClass = nullptr;
		
		if (const AZoransGameState* ZoransGameState = GetGameState<AZoransGameState>())
		{
			PawnClass = bAIController ? ZoransGameState->DefaultAIClass : ZoransGameState->DefaultPlayerClass;
		}
		
		const AActor* SpawnPoint = ChoosePlayerStart(InController);

		if (IsValid(SpawnPoint))
		{
			const FTransform SpawnTransform = SpawnPoint->GetActorTransform();
		
			if (APawn* NewPawn = GetWorld()->SpawnActorDeferred<APawn>(PawnClass, SpawnTransform, InController, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
			{
				if (AZoransCharacterBase* NewCharacter = Cast<AZoransCharacterBase>(NewPawn))
				{
					if (AZoransPlayerState* ZoransPlayerState = InController->GetPlayerState<AZoransPlayerState>())
					{
						ZoransPlayerState->ClearPlayerAssists();
						NewCharacter->TeamIndex = ZoransPlayerState->GetTeamAssignment();

						if (ZoransPlayerState->IsABot())
						{
							ZoransPlayerState->SetLoadoutIndex(UKismetMathLibrary::RandomIntegerInRange(0, 2));
							if (bFirstSpawn)
							{
								if (AvailableBotNames.Num() > 0)
								{
									const int32 Index = FMath::RandRange(0, AvailableBotNames.Num() - 1);
									ZoransPlayerState->SetPlayerName(AvailableBotNames[Index]);
									AvailableBotNames.RemoveAt(Index);

									if (IsValid(BotDataTable) && BotDataTable->GetRowNames().Contains(FName(ZoransPlayerState->GetPlayerName())))
									{
										if (const FBotData* BotData = BotDataTable->FindRow<FBotData>(FName(ZoransPlayerState->GetPlayerName()), "", false))
										{
											ZoransPlayerState->SetWeaponLoadouts(FPlayerProfileData(), FEmoteList(), BotData->BotLoadout1, BotData->BotLoadout2, BotData->BotLoadout3);
										}
									}
								}
							}
						}
						
						if (UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ZoransPlayerState))
						{
							if (UZoransAbilitySystemComponent* ZASC = Cast<UZoransAbilitySystemComponent>(AbilitySystemComponent))
							{
								ZASC->CheckAttributeSets();
								
								FGameplayTagContainer DeathEffectContainer;
								DeathEffectContainer.AddTag(GameplayTag_Dead);
								DeathEffectContainer.AddTag(GameplayTag_RemoveOnDeath);
								ZASC->RemoveActiveEffectsWithTags(DeathEffectContainer);

								if (ZoransPlayerState->HasCharacterSelectionChanged())
								{
									TArray<FGameplayAbilitySpecHandle> AbilitiesToRemove;
									ZASC->FindAllAbilitiesWithTags(AbilitiesToRemove, FGameplayTagContainer(GameplayTag_CharacterAbility), true);

									for (auto &AbilityToRemove : AbilitiesToRemove)
									{
										ZASC->ClearAbility(AbilityToRemove);
									}
								
									FGameplayTagContainer CharacterSelectionEffectsContainer;
									CharacterSelectionEffectsContainer.AddTag(GameplayTag_CharacterAbility);
									ZASC->RemoveActiveEffectsWithTags(CharacterSelectionEffectsContainer);
									
									ZoransPlayerState->GrantModularCharacterAbilities();
								}
							}
						}
												
						ZoransPlayerState->InitializeCharacter();
					}
			
					NewCharacter->bIsRespawn = !bFirstSpawn;
					NewCharacter->FinishSpawning(SpawnTransform);
					InController->SetControlRotation(SpawnTransform.GetRotation().Rotator());
					InController->Possess(NewCharacter);
					InController->SetPawn(NewCharacter);
				
					if (AZoransAIController* ZoransAIController = Cast<AZoransAIController>(InController))
					{				
						ZoransAIController->SetOwningAI(NewCharacter);
					}
				}
			}
		}

		if (OldPawn)
		{
			OldPawn->Destroy();
		}
	}
}

FString AZoransGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	if (GetWorld()->IsEditorWorld())
	{
		return Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	}

	UE_LOG(ZoransLog, Warning, TEXT("ZoransGameMode->InitNewPlayer->Options = %s"), *Options);
	
	if (AZoransPlayerState* ZoransPlayerState = NewPlayerController->GetPlayerState<AZoransPlayerState>())
	{
		TArray<FString> OptionsArray;
		Options.ParseIntoArray(OptionsArray, TEXT("?"), true);

		for (auto String : OptionsArray)
		{
			UE_LOG(ZoransLog, Warning, TEXT("ZoransGameMode->InitNewPlayer->String : OptionsArray = %s"), *String);
			if (String.StartsWith(TEXT("Name=")))
			{
				ZoransPlayerState->SetPlayerName(String.RightChop(5));
			}
			else if (String.StartsWith(TEXT("InPlayfabId=")))
			{
				UE_LOG(ZoransLog, Warning, TEXT("ZoransGameMode->InitNewPlayer->InPlayfabId = %s"), *String.RightChop(12));
				ZoransPlayerState->PlayfabId = String.RightChop(12);
				ZoransPlayerState->OnPlayfabIdSet(ZoransPlayerState->PlayfabId);
			}
		}
	}
	
	return Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
}

void AZoransGameMode::GetSeamlessTravelActorList(const bool bToTransition, TArray<AActor*>& ActorList)
{
	TArray<APlayerState*> TempPlayerStateArray;
		
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (!PlayerState->IsABot())
		{
			TempPlayerStateArray.AddUnique(PlayerState);
		}
	}

	const int32 ActorsToAddCount = TempPlayerStateArray.Num() + (bToTransition ? 3 : 0);

	ActorList.Reserve(ActorsToAddCount);
	ActorList.Append(TempPlayerStateArray);

	if (bToTransition)
	{
		TArray<APlayerState*> PlayerArrayCopy = GameState->PlayerArray;
	
		for (APlayerState* PlayerState : PlayerArrayCopy)
		{
			if (PlayerState->IsABot())
			{
				if (AAIController* AIController = Cast<AAIController>(PlayerState->GetOwningController()))
				{
					AIController->Destroy(true);

					GameState->RemovePlayerState(PlayerState);
				}
			}	
		}
		
		ActorList.Add(this);
		ActorList.Add(GameState);
		ActorList.Add(GameSession);
	}
}

AActor* AZoransGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (const AZoransGameState* ZoransGameState = GetGameState<AZoransGameState>())
	{
		if (ZoransGameState->bUseTeamSpawns)
		{
			if (IsValid(Player) && IsValid(Player->GetPlayerState<AZoransPlayerState>()))
			{
				if (const AZoransPlayerState* ZoransPlayerState = Player->GetPlayerState<AZoransPlayerState>())
				{
					const int32 PlayerTeam = ZoransGameState->GetGameModeSettings().bFreeForAll ? -1 : ZoransPlayerState->GetTeamAssignment();

					AZoransTeamSpawner* FoundPlayerStart = nullptr;

					const UClass* PawnClass = ZoransGameState->DefaultPlayerClass;
					const APawn* PawnToFit = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;

					TArray<AZoransTeamSpawner*> UnOccupiedStartPointsHigh;
					TArray<AZoransTeamSpawner*> OccupiedStartPoints;
					
					AZoransTeamSpawner* HighestScoreSpawner = nullptr;
					float HighestScoreValue = 0;

					UWorld* World = GetWorld();

					for (TActorIterator<AZoransTeamSpawner> It(World); It; ++It)
					{
						AZoransTeamSpawner* PlayerStart = *It;

						if (PlayerStart->IsA<APlayerStartPIE>())
						{
							FoundPlayerStart = PlayerStart;
							break;
						}

						if (PlayerStart->TeamIndex == PlayerTeam)
						{
							FVector ActorLocation = PlayerStart->GetActorLocation();
							const FRotator ActorRotation = PlayerStart->GetActorRotation();

							if (!World->EncroachingBlockingGeometry(PawnToFit, ActorLocation, ActorRotation))
							{
								const float Score = PlayerStart->GetSpawnScore();
								if(Score > 1700*1700)
								{
									UnOccupiedStartPointsHigh.Add(PlayerStart);
								}
								if(!IsValid(HighestScoreSpawner) || HighestScoreValue < Score)
								{
									HighestScoreSpawner = PlayerStart;
									HighestScoreValue = Score;
								}
							}
							else if (World->FindTeleportSpot(PawnToFit, ActorLocation, ActorRotation))
							{
								OccupiedStartPoints.Add(PlayerStart);
							}
						}
					}
			
					if (FoundPlayerStart == nullptr)
					{
						if(!UnOccupiedStartPointsHigh.IsEmpty())
						{
							FoundPlayerStart = UnOccupiedStartPointsHigh[FMath::RandRange(0, UnOccupiedStartPointsHigh.Num() - 1)];
						}else if(IsValid(HighestScoreSpawner))
						{
							FoundPlayerStart = HighestScoreSpawner;
						}else if(!OccupiedStartPoints.IsEmpty())
						{
							FoundPlayerStart = OccupiedStartPoints[FMath::RandRange(0, OccupiedStartPoints.Num() - 1)];
						}
					}
			
					if (FoundPlayerStart != nullptr)
					{
						return FoundPlayerStart;
					}
				}
			}
		}
	}
	
	return Super::ChoosePlayerStart_Implementation(Player);
}