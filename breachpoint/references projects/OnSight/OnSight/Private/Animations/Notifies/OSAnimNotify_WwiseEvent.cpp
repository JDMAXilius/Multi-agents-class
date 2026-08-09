// Fill out your copyright notice in the Description page of Project Settings.

#include "Animations/Notifies/OSAnimNotify_WwiseEvent.h"

#include "AkAudioEvent.h"
#include "AkGameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	struct FOSWwiseRateLimitKey
	{
		TWeakObjectPtr<AActor> Owner;
		TWeakObjectPtr<UAkAudioEvent> Event;

		bool operator==(const FOSWwiseRateLimitKey& Other) const
		{
			return Owner == Other.Owner && Event == Other.Event;
		}
	};

	uint32 GetTypeHash(const FOSWwiseRateLimitKey& Key)
	{
		return HashCombine(GetTypeHash(Key.Owner), GetTypeHash(Key.Event));
	}

	static bool GetNearestLocalViewpoint_Wwise(UWorld* World, const FVector& Location, FVector& OutViewLocation)
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

			const float DistSq = FVector::DistSquared(ViewLoc, Location);
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

void UOSAnimNotify_WwiseEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	
	if (!IsValid(MeshComp) || !AkEventPair.IsValid())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	// Attacker-local swing sound (recommended).
	APawn* PawnOwner = Cast<APawn>(Owner);
	if (!IsValid(PawnOwner)) return;
	
	TObjectPtr<UAkAudioEvent> AkEvent = ResolveEvent(PawnOwner);
	if (!IsValid(AkEvent)) return;

	UWorld* World = Owner->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (bSkipOnDedicatedServer && World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	FVector Location = Owner->GetActorLocation();
	FRotator Rotation = Owner->GetActorRotation();

	if (SocketName != NAME_None && MeshComp->DoesSocketExist(SocketName))
	{
		Location = MeshComp->GetSocketLocation(SocketName);
		Rotation = MeshComp->GetSocketRotation(SocketName);
	}

	// NOTE: We intentionally do perf culling/rate limiting *before* posting the event,
	// to avoid Wwise voice spikes on bursty swing notifies.
	if (MaxDistance > 0.f)
	{
		FVector ViewLoc;
		if (GetNearestLocalViewpoint_Wwise(World, Location, ViewLoc))
		{
			if (FVector::DistSquared(ViewLoc, Location) > FMath::Square(MaxDistance))
			{
				return;
			}
		}
	}

	if (MinIntervalPerOwner > 0.f)
	{
		static TMap<FOSWwiseRateLimitKey, float> LastTimeByOwner;

		const float Now = World->GetTimeSeconds();
		const FOSWwiseRateLimitKey Key{ Owner, AkEvent.Get() };

		if (const float* Last = LastTimeByOwner.Find(Key))
		{
			if ((Now - *Last) < MinIntervalPerOwner)
			{
				return;
			}
		}

		LastTimeByOwner.Add(Key, Now);

		if (LastTimeByOwner.Num() > 512)
		{
			for (auto It = LastTimeByOwner.CreateIterator(); It; ++It)
			{
				if (!It.Key().Owner.IsValid() || !It.Key().Event.IsValid())
				{
					It.RemoveCurrent();
				}
			}
		}
	}

	UAkGameplayStatics::PostEventAtLocation(AkEvent, Location, Rotation, World);
}

UAkAudioEvent* UOSAnimNotify_WwiseEvent::ResolveEvent(APawn* Owner)
{
	if (!Owner) return nullptr;
	switch (PlayerContext)
	{
	case EOSFXPlayerContext::LOCAL:
		return AkEventPair.Local;
		case EOSFXPlayerContext::REMOTE:
		return AkEventPair.Remote;
	case EOSFXPlayerContext::AUTO:
		if (Owner->IsLocallyControlled())
			return AkEventPair.Local;
		else
			return AkEventPair.Remote;
	}
	
	return nullptr;
}

