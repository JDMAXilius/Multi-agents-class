// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"

class SaveAsyncTask : public FNonAbandonableTask
{
	const UWorld* world;
	const TArray<const AActor*>* const LevelActors;
	AActor* GameMode;
	const bool IgnoreHostPawn;

public:

	SaveAsyncTask(const UWorld* W, AActor* GM, const bool ihp, const TArray<const AActor*>* const Actors) : world(W), LevelActors(Actors), GameMode(GM), IgnoreHostPawn(ihp)
	{

	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(SaveAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
	}
	void DoWork();
};
