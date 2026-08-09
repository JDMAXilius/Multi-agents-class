// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/Components/AudioPlayerDistanceBuckets.h"

#include "AkComponent.h"
#include "AkGameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// Sets default values for this component's properties
UAudioPlayerDistanceBuckets::UAudioPlayerDistanceBuckets()
{
	PrimaryComponentTick.bCanEverTick = false; //I am using a timer so turning off the tick
}

void UAudioPlayerDistanceBuckets::BeginPlay()
{
	Super::BeginPlay();

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}
	
	if (!OwnerPawn->IsLocallyControlled())
	{
		return;
	}
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DistanceUpdateTimerHandle, this, &UAudioPlayerDistanceBuckets::UpdateRemotePlayerDistances, UpdateIntervalSeconds, true);
	}
}

void UAudioPlayerDistanceBuckets::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DistanceUpdateTimerHandle);
	}
	
	CachedBuckets.Empty();
	
	Super::EndPlay(EndPlayReason);
}

void UAudioPlayerDistanceBuckets::UpdateRemotePlayerDistances()
{
	APawn* LocalPawn = Cast<APawn>(GetOwner());
	if (!LocalPawn||!LocalPawn->IsLocallyControlled())
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (!World) return;
	
	const FVector ListenerLocation = GetListenerReferenceLocation();
	
	const float NearSq = FMath::Square(NearDistance);
	const float MidSq = FMath::Square(MidDistance);
	const float FarSq = FMath::Square(FarDistance);
	
	TArray<AActor*> FoundPawns;
	UGameplayStatics::GetAllActorsOfClass(World, APawn::StaticClass(), FoundPawns);
	
	for (AActor* Actor : FoundPawns)
	{
		APawn* RemotePawn = Cast<APawn>(Actor);
		if (!RemotePawn || RemotePawn == LocalPawn)
		{
			continue;
		}
		
		const float DistSq = FVector::DistSquared(ListenerLocation, RemotePawn->GetActorLocation());
		
		EAudioPlayerDistanceBucket Bucket = EAudioPlayerDistanceBucket::VeryFar;
		
		if (DistSq <= NearSq)
		{
			Bucket = EAudioPlayerDistanceBucket::Near;
		}
		
		else if (DistSq <= MidSq)
		{
			Bucket = EAudioPlayerDistanceBucket::Mid;
		}
		
		else if (DistSq <= FarSq)
		{
			Bucket = EAudioPlayerDistanceBucket::Far;
		}
		
		
		if (UAudioPlayerDistanceBuckets* RemoteDistanceComp = RemotePawn->FindComponentByClass<UAudioPlayerDistanceBuckets>())
		{
			RemoteDistanceComp->CurrentAudioDistanceBucket = Bucket;
		}
		bool bShouldSend = true;
		if (bOnlySendOnBucketChange)
		{
			if (EAudioPlayerDistanceBucket* PreviousBucket = CachedBuckets.Find(RemotePawn))
			{
				if (*PreviousBucket == Bucket)
				{
					bShouldSend = false;
				}
			}
		}
		
		float DistanceMeters = 0.f;
		
		const bool bNeedsExactDistance = (Bucket != EAudioPlayerDistanceBucket::VeryFar) || bSendExactDistanceVeryFar;
		
		if (bNeedsExactDistance)
		{
			DistanceMeters = FMath::Sqrt(DistSq)*0.01f; //cm->m (Remove SquareRoot)
		}
		
		if (bShouldSend || !bOnlySendOnBucketChange)
		{
			PushWwiseDistanceValues(RemotePawn, Bucket, DistanceMeters);
			CachedBuckets.Add(RemotePawn, Bucket);
			
#if !UE_BUILD_SHIPPING
			if (bDebugPrint)
			{
				const FString Msg = FString::Printf(TEXT("Remote %s -> Bucket %d, Dist %.1fm"), *RemotePawn->GetName(), static_cast<int32>(Bucket), DistanceMeters);

				if (GEngine)
				{
					const int32 DebugKey = 1001;
					GEngine->AddOnScreenDebugMessage(DebugKey, 9999.f, FColor::Cyan, Msg);
				}
			}
#endif
		}
	}
	
	for (auto It = CachedBuckets.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

FVector UAudioPlayerDistanceBuckets::GetListenerReferenceLocation() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return FVector::ZeroVector;
	}
	
	return OwnerPawn->GetActorLocation();
}

UAkComponent* UAudioPlayerDistanceBuckets::FindAKComponentOnPawn(APawn* Pawn) const
{
	if (!Pawn)
	{
		return nullptr;
	}
	
	return Pawn->FindComponentByClass<UAkComponent>();
}

void UAudioPlayerDistanceBuckets::PushWwiseDistanceValues(APawn* RemotePawn, EAudioPlayerDistanceBucket Bucket, float DistanceMeters) const
{
	if (!RemotePawn)
	{
		return;
	}
	
	UAkGameplayStatics::SetRTPCValue(DistanceBucketRTPC, BucketToRTPCValue(Bucket), 0, RemotePawn);
	
	UAkGameplayStatics::SetRTPCValue(DistanceMetersRTPC, DistanceMeters, 0, RemotePawn);
	
}

float UAudioPlayerDistanceBuckets::BucketToRTPCValue(EAudioPlayerDistanceBucket Bucket) const
{
	switch (Bucket)
	{
	case EAudioPlayerDistanceBucket::Near: return 0.0f;
	case EAudioPlayerDistanceBucket::Mid: return 1.0f;
	case EAudioPlayerDistanceBucket::Far: return 2.0f;
	case EAudioPlayerDistanceBucket::VeryFar: return 3.0f;
	default: return 3.0f;
		
	}
}

// Called every frame
void UAudioPlayerDistanceBuckets::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

