// Copyright Zollpa LLC


#include "ZoransTutorialController.h"

#include "ZoransTutorialBotSpawner.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "ZoransResistance/Characters/ZoransPlayerCharacter.h"
#include "ZoransResistance/Game/ZoransWorldSubsystem.h"
#include "ZoransResistance/Player/ZoransPlayerState.h"


AZoransTutorialController::AZoransTutorialController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AZoransTutorialController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransTutorialController, CurrentLessonName, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransTutorialController, LessonProgress, COND_None, REPNOTIFY_Always);
}

void AZoransTutorialController::PostRepNotifies()
{
	Super::PostRepNotifies();
	if(bLessonNameChanged)
	{
		GetLessonFromName(CurrentLessonName, CurrentLesson);
		OnLessonReplicated();
	}
	else if(bLessonProgressChanged)
	{
		OnProgressReplicated();
	}
	bLessonNameChanged = false;
	bLessonProgressChanged = false;
}

void AZoransTutorialController::AddTutorialBotSpawner(const AZoransTutorialBotSpawner* Spawner) const
{
	if(const UZoransWorldSubsystem* WorldSubsystem = GetWorld()->GetSubsystem<UZoransWorldSubsystem>())
	{
		if(const AZoransGameState* GameState = WorldSubsystem->GetZoransGameState())
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GetWorld()->SpawnActor(GameState->AISpawnerClass, nullptr, nullptr, SpawnParams);
		}
	}
}

void AZoransTutorialController::SetActiveLesson(const FName LessonNameIn)
{
	if(!IsValid(PlayerState))
			return;
	bIsPendingNextLesson = false;
	CurrentLessonName = LessonNameIn;
	GetLessonFromName(CurrentLessonName, CurrentLesson);
	LessonProgress = {};
	LessonProgress.Init(0, CurrentLesson.Objectives.Num());

	if(UDataLayerSubsystem* DataLayerSubsystem = GetWorld()->GetSubsystem<UDataLayerSubsystem>())
	{
		for (const UDataLayerAsset* DataLayer : CurrentLesson.DataLayers)
		{
			DataLayerSubsystem->SetDataLayerInstanceRuntimeState(DataLayer, EDataLayerRuntimeState::Activated, false);
		}
	}
	bLessonNameChanged = true;
	bLessonProgressChanged = true;
	PostRepNotifies();
}

void AZoransTutorialController::UpdateObjectiveProgress(const int32 Index, const int32 Progress)
{
	if(!LessonProgress.IsValidIndex(Index))
		return;
	if(bIsPendingNextLesson)
		return;
	
	const int32 MaxProgress = CurrentLesson.Objectives[Index].Goal;
	
	int32& CurrentProgress = LessonProgress[Index];
	int32 NewProgress = CurrentProgress + Progress;
	if(NewProgress > MaxProgress)
	{
		NewProgress = MaxProgress;
	}
	if(NewProgress != CurrentProgress)
	{
		CurrentProgress = NewProgress;
		bLessonProgressChanged = true;
	}

	// Check if all objectives are complete
	if(NewProgress == MaxProgress)
	{
		bool bAllComplete = true;
		for (int32 IterIndex = 0; IterIndex < CurrentLesson.Objectives.Num(); ++IterIndex)
		{
			if(CurrentLesson.Objectives[IterIndex].Goal != LessonProgress[IterIndex])
			{
				bAllComplete = false;
				break;
			}
		}

		if(bAllComplete)
		{
			GetWorldTimerManager().SetTimer(LessonChangeTimerHandle, this, &AZoransTutorialController::BeginNextLesson, 3.f);
			bIsPendingNextLesson = true;
		}
	}

	if(bLessonProgressChanged)
	{
		PostRepNotifies();
	}
}

void AZoransTutorialController::TryUpdateProgress(const ETutorialObjectiveType ObjectiveType, int32 Progress)
{
	for (int32 Index = 0; Index < CurrentLesson.Objectives.Num(); ++Index)
	{
		const FTutorialObjective& Objective = CurrentLesson.Objectives[Index];
		if(Objective.ObjectiveType == ObjectiveType)
		{
			UpdateObjectiveProgress(Index, Progress);
		}
	}
}

void AZoransTutorialController::BeginNextLesson()
{
	if(UDataLayerSubsystem* DataLayerSubsystem = GetWorld()->GetSubsystem<UDataLayerSubsystem>())
	{
		for (const UDataLayerAsset* DataLayer : CurrentLesson.DataLayers)
		{
			DataLayerSubsystem->SetDataLayerInstanceRuntimeState(DataLayer, EDataLayerRuntimeState::Unloaded, false);
		}
	}

	TArray<AActor*> PlayerActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AZoransPlayerCharacter::StaticClass(), PlayerActors);
	for (AActor* ActorBase : PlayerActors)
	{
		AZoransPlayerCharacter* PlayerActor = static_cast<AZoransPlayerCharacter*>(ActorBase);
		if(PlayerActor->GetPlayerState()->IsABot())
		{
			PlayerActor->GetPlayerState()->Destroy();
			PlayerActor->GetController()->Destroy();
			PlayerActor->Destroy();
		}
	}
	
	if(CurrentLesson.NextLesson != NAME_None)
	{
		SetActiveLesson(CurrentLesson.NextLesson);
	}
	else
	{
		if (EndTutorial.IsBound())
		{
			EndTutorial.Broadcast();
		}
	}
}

