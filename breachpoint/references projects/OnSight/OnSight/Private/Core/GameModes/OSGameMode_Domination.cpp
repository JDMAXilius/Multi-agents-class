#include "Core/GameModes/OSGameMode_Domination.h"

#include "OSLogCategories.h"
#include "Actors/OSControlPoint.h"
#include "Core/OSPlayerController.h"
#include "Core/OSPlayerState.h"
#include "Core/GameStates/OSGameState_Domination.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "TDM/OSTDMPlayerStart.h"
#include "Engine/PlayerStartPIE.h"
#include "EngineUtils.h"

AOSGameMode_Domination::AOSGameMode_Domination(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AOSGameState_Domination::StaticClass();
}

// ------------------------------------------------------------------
// Init
// ------------------------------------------------------------------

void AOSGameMode_Domination::InitGameState()
{
	Super::InitGameState();

	auto* GS = GetDomGameState();
	if (!GS) return;

	if (ScoreLimit <= 0)
		UE_LOG(LogOSDomination, Warning, TEXT("[GM_DOM][INIT] ScoreLimit is %d — match will never end via score!"), ScoreLimit);
	if (TimeLimitSeconds <= 0 && ScoreLimit <= 0)
		UE_LOG(LogOSDomination, Error, TEXT("[GM_DOM][INIT] Both ScoreLimit and TimeLimitSeconds are <= 0 — match will NEVER end!"));

	GS->DomScoreLimit = ScoreLimit;
	GS->DomTimeLimitSeconds = TimeLimitSeconds;
	// Note: ModeLayerClasses already pushed by Super::InitGameState()

	// Listen for player spawns/respawns to check for spawning inside control points
	OnPlayerStateReady.AddDynamic(this, &AOSGameMode_Domination::OnPlayerReady);

	if (TimeLimitSeconds > 0)
	{
		MatchDurationSeconds = TimeLimitSeconds;
		GS->InitMatchTimer(TimeLimitSeconds);
		GS->StartMatchTimer();
	}
}

FString AOSGameMode_Domination::InitNewPlayer(APlayerController* NewPlayerController,
	const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	auto* GS = GetDomGameState();
	if (!GS || !NewPlayerController) return Result;

	auto* PS = NewPlayerController->GetPlayerState<AOSPlayerState>();
	if (!PS) return Result;

	const int32 TeamIndex = GS->GetLowestPopulationTeamIndex();
	PS->SetGenericTeamId(FGenericTeamId(TeamIndex));

	UE_LOG(LogOSDomination, Log, TEXT("[GM_DOM][TEAM] %s assigned to Team %d"), *PS->GetPlayerName(), TeamIndex);
	return Result;
}

void AOSGameMode_Domination::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	for (auto& Pair : RegisteredPoints)
	{
		if (Pair.Value && Pair.Value->IsPointValid())
			Pair.Value->InitializePoint();
	}
}

void AOSGameMode_Domination::ConfigureWinCondition(AOSGameState* GS)
{
	if (!GS) return;
	GS->UpdateWinCondition_Server(FOSWinConditionType::SCORE, ScoreLimit);
}

// ------------------------------------------------------------------
// Point Registration
// ------------------------------------------------------------------

void AOSGameMode_Domination::RegisterControlPoint(AOSControlPoint* Point)
{
	if (!HasAuthority()) return;
	if (!IsValid(Point) || !Point->IsPointValid()) return;

	auto* GS = GetDomGameState();
	if (!GS) return;

	FName Name = Point->PointName;
	if (Name.IsNone())
	{
		int32 i = 0;
		do { Name = FName(*FString::Printf(TEXT("ControlPoint_%d"), i++)); }
		while (GS->ControlPointByName.Contains(Name));
	}
	else if (GS->ControlPointByName.Contains(Name))
	{
		const FName Base = Name;
		int32 i = 1;
		do { Name = FName(*FString::Printf(TEXT("%s_%d"), *Base.ToString(), i++)); }
		while (GS->ControlPointByName.Contains(Name));
	}

	RegisteredPoints.Add(Name, Point);

	FString Label = AOSControlPoint::NormalizeLabelString(Point->GetPointLabelText());
	if (!AOSControlPoint::IsLabelValid(Label) || UsedLabels.Contains(Label))
	{
		do { Label = AOSControlPoint::IndexToAlphaLabel(NextPointLabelIndex++); }
		while (UsedLabels.Contains(Label));
		Point->SetPointLabel(FText::FromString(Label));
	}
	else
	{
		Point->SetPointLabel(FText::FromString(Label));
	}
	UsedLabels.Add(Label);

	Point->SetMatchConfig(CaptureRate, DecayRate, MaxCap, CaptureInterval, BaseScoreInterval, TeamPresenceCaptureScaling, MaxTeamPresenceMultiplier);

	GS->RegisterPoint(Point, Name);
	Point->Register(Name);

	// Defer overlap check to next frame — colliders were just activated in Register()
	// and physics hasn't processed them yet, so IsOverlappingActor returns false this frame.
	TWeakObjectPtr<AOSControlPoint> WeakPoint = Point;
	GetWorldTimerManager().SetTimerForNextTick([this, WeakPoint]()
	{
		if (!WeakPoint.IsValid()) return;
		AOSControlPoint* CP = WeakPoint.Get();

		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			auto* PC = Cast<AOSPlayerController>(It->Get());
			if (!PC) continue;
			APawn* Pawn = PC->GetPawn();
			if (!Pawn) continue;
			if (Pawn->IsOverlappingActor(CP))
			{
				CP->Server_PlayerEntered_Implementation(PC);
			}
		}
	});
}

void AOSGameMode_Domination::UnregisterControlPoint(const FName& Name)
{
	if (!HasAuthority()) return;

	auto* GS = GetDomGameState();
	if (GS) GS->UnregisterPoint(Name);

	AOSControlPoint* Point = nullptr;
	if (AOSControlPoint** Found = RegisteredPoints.Find(Name))
		Point = *Found;

	RegisteredPoints.Remove(Name);
	if (IsValid(Point))
		Point->Unregister_Internal();
}

// ------------------------------------------------------------------
// Scoring & Win Condition
// ------------------------------------------------------------------

void AOSGameMode_Domination::OnScoreTick(AOSControlPoint* Point, int32 OwningTeamIndex)
{
	if (!Point || bGameEnded) return;

	auto* GS = GetDomGameState();
	if (!GS || GS->IsMatchOver()) return;

	GS->AddTeamScore(OwningTeamIndex, ScoreRate);
	UpdateWinCondition();
	CheckDomWinCondition(OwningTeamIndex);
}

void AOSGameMode_Domination::CheckDomWinCondition(int32 TeamIndex)
{
	auto* GS = GetDomGameState();
	if (!GS || GS->IsMatchOver()) return;
	if (ScoreLimit <= 0) return;

	if (GS->GetTeamScore(TeamIndex) >= ScoreLimit)
		EndDominationMatch(TeamIndex);
}

void AOSGameMode_Domination::OnMatchTimeExpired()
{
	auto* GS = GetDomGameState();
	if (!GS || GS->IsMatchOver()) return;

	int32 WinningTeam = INDEX_NONE;
	int32 HighScore = INDEX_NONE;
	bool bTied = false;

	for (int32 i = 0; i < AOSGameState::MaxTDMTeams; ++i)
	{
		const int32 Score = GS->GetTeamScore(i);
		if (Score > HighScore)
		{
			HighScore = Score;
			WinningTeam = i;
			bTied = false;
		}
		else if (Score == HighScore)
		{
			bTied = true;
		}
	}

	EndDominationMatch(bTied ? -1 : WinningTeam);
}

void AOSGameMode_Domination::EndDominationMatch(int32 WinningTeamIndex)
{
	auto* GS = GetDomGameState();
	if (!GS || GS->IsMatchOver()) return;

	GS->StopMatchTimer();
	GS->bMatchInProgress = false;
	CancelAllPlayerAbilities();

	// WinningTeamIndex lives on parent AOSGameState (replicated but no OnRep).
	// Multicast fires OnDomMatchResult on all machines so BP win/lose UI works on clients.
	GS->WinningTeamIndex = WinningTeamIndex;
	GS->Multicast_DomMatchResult(WinningTeamIndex);
	GS->ForceNetUpdate();

	// Pass only WinningTeamIndex — don't override WinnerValue. HUD reads the score directly
	// via GetTeamScore() based on GameState type. Passing score as override would break the
	// WinnerValue == TeamIndex contract and show "Draw" for scores outside [0, MaxTDMTeams).
	GS->NotifyMatchWon(WinningTeamIndex);
	bGameEnded = true;

	const int32 WinnerScore = (WinningTeamIndex >= 0) ? GS->GetTeamScore(WinningTeamIndex) : 0;
	UE_LOG(LogOSDomination, Log, TEXT("[GM_DOM][MATCH_END] WinningTeam=%d Score=%d"),
		WinningTeamIndex, WinnerScore);

	GetWorldTimerManager().ClearTimer(DomRematchHandle);
	GetWorldTimerManager().SetTimer(DomRematchHandle,
		FTimerDelegate::CreateLambda([this]() { RestartCurrentMatch(); }),
		PostMatchRematchDelaySeconds, false);
}

bool AOSGameMode_Domination::QueryWinCondition(AOSPlayerState*& OutWinner) const
{
	OutWinner = nullptr;
	return false;
}

void AOSGameMode_Domination::BuildMatchResult(AOSGameState* GS, AOSPlayerState* PS)
{
	if (PS)
		Super::BuildMatchResult(GS, PS);
}

// ------------------------------------------------------------------
// Player Death & Disconnect
// ------------------------------------------------------------------

void AOSGameMode_Domination::HandlePlayerDeath(AController* VictimController, const FOSDeathEventInfo& DeathEvent)
{
	if (!HasAuthority() || !IsValid(VictimController))
	{
		return;
	}

	RemovePlayerFromAllPoints(VictimController);

	// Team death tracking lives in ReportKill — not here, to avoid double-counting
	if (!DeathEvent.bIsManualRespawn)
	{
		AController* KillerController = DeathEvent.GetInstigatorController(GetWorld());
		ReportKill(KillerController, VictimController, nullptr);

		if (AOSGameState* GS = GetGameState<AOSGameState>())
		{
			GS->AddKillfeed_Server(DeathEvent);
		}
	}

	if (bGameEnded)
	{
		return;
	}

	RequestRespawn(VictimController);
}

void AOSGameMode_Domination::Logout(AController* Exiting)
{
	RemovePlayerFromAllPoints(Exiting);
	Super::Logout(Exiting);
}

void AOSGameMode_Domination::RemovePlayerFromAllPoints(AController* Controller)
{
	if (!Controller) return;
	auto* PC = Cast<AOSPlayerController>(Controller);
	if (!PC) return;

	for (auto& Pair : RegisteredPoints)
	{
		if (!Pair.Value || !Pair.Value->IsPointValid()) continue;
		Pair.Value->Server_PlayerExited_Implementation(PC);
	}
}

void AOSGameMode_Domination::CancelAllPlayerAbilities()
{
	UWorld* World = GetWorld();
	if (!World) return;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (auto* PC = It->Get())
		{
			if (auto* PS = PC->GetPlayerState<AOSPlayerState>())
			{
				if (auto* ASC = Cast<UOSAbilitySystemComponent>(PS->GetAbilitySystemComponent()))
					ASC->CancelAllAbilities();
			}
		}
	}
}

void AOSGameMode_Domination::ReportKill(AController* KillerController, AController* VictimController, AController* AssistController)
{
	if (!HasAuthority()) return;

	AOSGameState* GS = GetWorld() ? GetWorld()->GetGameState<AOSGameState>() : nullptr;
	if (!IsValid(GS) || GS->IsMatchOver()) return;

	AOSPlayerState* VictimPS = IsValid(VictimController) ? VictimController->GetPlayerState<AOSPlayerState>() : nullptr;
	AOSPlayerState* KillerPS = IsValid(KillerController) ? KillerController->GetPlayerState<AOSPlayerState>() : nullptr;
	AOSPlayerState* AssistPS = IsValid(AssistController) ? AssistController->GetPlayerState<AOSPlayerState>() : nullptr;

	const int32 VictimTeam = VictimPS ? VictimPS->GetTeamIndex() : -1;
	const int32 KillerTeam = KillerPS ? KillerPS->GetTeamIndex() : -1;

	if (IsValid(VictimController) && VictimPS)
	{
		VictimPS->AddDeath();
		if (VictimTeam >= 0) GS->AddTeamDeath(VictimTeam);
	}

	// Mirror TDM semantics: only enemy kills award kill + team kill.
	const bool bHasKiller = IsValid(KillerPS);
	const bool bValidEnemyKill = bHasKiller && KillerPS != VictimPS && KillerTeam >= 0 && VictimTeam >= 0 && KillerTeam != VictimTeam;
	if (bValidEnemyKill)
	{
		KillerPS->AddKill();
		GS->AddTeamKill(KillerTeam);

		if (IsValid(AssistPS) && AssistPS != KillerPS && AssistPS != VictimPS)
		{
			AssistPS->AddAssist();
		}
	}
}

void AOSGameMode_Domination::RequestRespawn(AController* VictimController)
{
	if (!IsValid(VictimController)) return;

	AOSGameState* GS = GetGameState<AOSGameState>();
	if (GS && GS->IsMatchOver()) return;

	if (APawn* DeadPawn = VictimController->GetPawn())
	{
		VictimController->UnPossess();
		DeadPawn->SetLifeSpan(2.0f);
	}

	QueueRespawn(VictimController);
}

void AOSGameMode_Domination::OnPlayerReady(AOSPlayerState* PS)
{
	if (!PS) return;
	APawn* Pawn = PS->GetPawn();
	if (!Pawn) return;

	auto* PC = Cast<AOSPlayerController>(PS->GetOwningController());
	if (!PC) return;

	// Check if the spawned pawn is already inside any control point.
	// NotifyActorBeginOverlap doesn't fire for actors that spawn already overlapping,
	// so we manually check and register.
	for (auto& Pair : RegisteredPoints)
	{
		if (!Pair.Value || !Pair.Value->IsPointValid()) continue;
		if (Pawn->IsOverlappingActor(Pair.Value))
		{
			Pair.Value->Server_PlayerEntered_Implementation(PC);
		}
	}
}

void AOSGameMode_Domination::OnPointStateChanged(AOSControlPoint* Point)
{
	if (!Point) return;
	UE_LOG(LogOSDomination, Log, TEXT("[GM_DOM][POINT_STATE] Point=%s State=%d"),
		*Point->GetPointName(), (int32)Point->GetControlPointState());
	RecalculateScoreIntervals();
}

void AOSGameMode_Domination::RecalculateScoreIntervals()
{
	// Count captured points per team
	TMap<int32, int32> TeamPointCounts;
	for (const auto& Pair : RegisteredPoints)
	{
		if (!Pair.Value || !Pair.Value->IsCaptured()) continue;
		TeamPointCounts.FindOrAdd(Pair.Value->GetOwningTeamIndex())++;
	}

	// Update each captured point's score interval based on how many points its team owns
	for (const auto& Pair : RegisteredPoints)
	{
		if (!Pair.Value || !Pair.Value->IsCaptured()) continue;
		const int32 NumPoints = TeamPointCounts.FindRef(Pair.Value->GetOwningTeamIndex());
		const float Interval = BaseScoreInterval / (1.0f + (NumPoints - 1) * PointScoreMultiplier);
		Pair.Value->UpdateScoreInterval(Interval);
	}
}

void AOSGameMode_Domination::SetScoreLimit(int32 NewLimit)
{
	ScoreLimit = FMath::Max(1, NewLimit);
	if (auto* GS = GetDomGameState())
	{
		GS->DomScoreLimit = ScoreLimit;
		GS->ForceNetUpdate();
	}
}

AActor* AOSGameMode_Domination::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!IsValid(Player))
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	const AOSPlayerState* PS = Player->GetPlayerState<AOSPlayerState>();
	const int32 PlayerTeam = PS ? PS->GetTeamIndex() : -1;

	const UClass* PawnClass = GetDefaultPawnClassForController(Player);
	const APawn* PawnToFit = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;

	AOSTDMPlayerStart* FoundStart = nullptr;
	AOSTDMPlayerStart* HighestScoreStart = nullptr;
	float HighestScoreValue = -1.f;

	TArray<AOSTDMPlayerStart*> HighScoreStarts;
	TArray<AOSTDMPlayerStart*> OccupiedStarts;

	for (TActorIterator<AOSTDMPlayerStart> It(World); It; ++It)
	{
		AOSTDMPlayerStart* Start = *It;
		if (!IsValid(Start))
		{
			continue;
		}

		if (Start->IsA<APlayerStartPIE>())
		{
			FoundStart = Start;
			break;
		}

		if (Start->TeamIndex != -1 && Start->TeamIndex != PlayerTeam)
		{
			continue;
		}

		FVector SpawnLoc = Start->GetActorLocation();
		const FRotator SpawnRot = Start->GetActorRotation();

		if (!PawnToFit || !World->EncroachingBlockingGeometry(PawnToFit, SpawnLoc, SpawnRot))
		{
			const float Score = Start->GetSpawnScore(Player);
			if (Score > (1700.f * 1700.f))
			{
				HighScoreStarts.Add(Start);
			}

			if (!IsValid(HighestScoreStart) || Score > HighestScoreValue)
			{
				HighestScoreStart = Start;
				HighestScoreValue = Score;
			}
		}
		else if (PawnToFit && World->FindTeleportSpot(const_cast<APawn*>(PawnToFit), SpawnLoc, SpawnRot))
		{
			OccupiedStarts.Add(Start);
		}
	}

	if (!IsValid(FoundStart))
	{
		if (!HighScoreStarts.IsEmpty())
		{
			FoundStart = HighScoreStarts[FMath::RandRange(0, HighScoreStarts.Num() - 1)];
		}
		else if (IsValid(HighestScoreStart))
		{
			FoundStart = HighestScoreStart;
		}
		else if (!OccupiedStarts.IsEmpty())
		{
			FoundStart = OccupiedStarts[FMath::RandRange(0, OccupiedStarts.Num() - 1)];
		}
	}

	if (IsValid(FoundStart))
	{
		return FoundStart;
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

void AOSGameMode_Domination::RestartPlayer(AController* NewPlayer)
{
	if (!HasAuthority() || !IsValid(NewPlayer))
	{
		return;
	}

	if (AActor* Start = ChoosePlayerStart(NewPlayer))
	{
		RestartPlayerAtPlayerStart(NewPlayer, Start);
		return;
	}

	Super::RestartPlayer(NewPlayer);
}

AOSGameState_Domination* AOSGameMode_Domination::GetDomGameState() const
{
	return GetGameState<AOSGameState_Domination>();
}
