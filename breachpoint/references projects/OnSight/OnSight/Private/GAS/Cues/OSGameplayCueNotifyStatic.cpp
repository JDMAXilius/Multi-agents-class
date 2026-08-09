// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Cues/OSGameplayCueNotifyStatic.h"

#include "GameFramework/PlayerController.h"

namespace
{
	struct FOSCueRateLimitKey
	{
		TWeakObjectPtr<const AActor> Target;
		FName CueName;

		bool operator==(const FOSCueRateLimitKey& Other) const
		{
			return Target == Other.Target && CueName == Other.CueName;
		}
	};

	uint32 GetTypeHash(const FOSCueRateLimitKey& Key)
	{
		return HashCombine(GetTypeHash(Key.Target), GetTypeHash(Key.CueName));
	}

	static bool GetNearestLocalViewpoint(UWorld* World, const FVector& CueLocation, FVector& OutViewLocation)
	{
		if (!IsValid(World))
		{
			return false;
		}

		bool bFound = false;
		float BestDistSq = TNumericLimits<float>::Max();

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (!IsValid(PC) || !PC->IsLocalController())
			{
				continue;
			}

			FVector ViewLoc;
			FRotator ViewRot;
			PC->GetPlayerViewPoint(ViewLoc, ViewRot);

			const float DistSq = FVector::DistSquared(ViewLoc, CueLocation);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutViewLocation = ViewLoc;
				bFound = true;
			}
		}

		return bFound;
	}
}

void UOSGameplayCueNotifyStatic::HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	if (!IsValid(MyTarget))
	{
		return;
	}

	UWorld* World = MyTarget->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (bSkipOnDedicatedServer && World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Compute a reasonable cue location for distance culling.
	FVector CueLocation = MyTarget->GetActorLocation();
	if (const FHitResult* HR = Parameters.EffectContext.GetHitResult())
	{
		CueLocation = HR->ImpactPoint;
	}

	if (MaxCueDistance > 0.f)
	{
		FVector ViewLoc;
		if (GetNearestLocalViewpoint(World, CueLocation, ViewLoc))
		{
			const float DistSq = FVector::DistSquared(ViewLoc, CueLocation);
			if (DistSq > FMath::Square(MaxCueDistance))
			{
				return;
			}
		}
	}

	if (MinIntervalPerTarget > 0.f)
	{
		static TMap<FOSCueRateLimitKey, float> LastTimeByTarget;

		const float Now = World->GetTimeSeconds();
		const FOSCueRateLimitKey Key{ MyTarget, GameplayCueName };

		if (const float* Last = LastTimeByTarget.Find(Key))
		{
			if ((Now - *Last) < MinIntervalPerTarget)
			{
				return;
			}
		}

		LastTimeByTarget.Add(Key, Now);

		// Opportunistic cleanup (avoid unbounded growth in long sessions).
		if (LastTimeByTarget.Num() > 1024)
		{
			for (auto It = LastTimeByTarget.CreateIterator(); It; ++It)
			{
				if (!It.Key().Target.IsValid())
				{
					It.RemoveCurrent();
				}
			}
		}
	}

	Super::HandleGameplayCue(MyTarget, EventType, Parameters);
}

