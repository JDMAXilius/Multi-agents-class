// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/OSAudioComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "AkGameplayStatics.h"
#include "Characters/Player/OSPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Core/OSPlayerState.h"
#include "AkComponent.h"
#include "AkAudioEvent.h"

UOSAudioComponent::UOSAudioComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UOSAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || Owner->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
}

void UOSAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerSpeedRtpcTimerHandle);
	}
	
	CachedBuckets.Empty();
	CachedTeamRelationship.Empty();

	Super::EndPlay(EndPlayReason);
}

AOSPlayer* UOSAudioComponent::GetPlayerOwner() const
{
	return Cast<AOSPlayer>(GetOwner());
}

void UOSAudioComponent::PushTeamRelationshipToAllComponents(APawn* RemotePawn)
{
	if (!RemotePawn || !TeamRelationshipRTPC) return;
	
	APawn* LocalPawn = GetOwner<APawn>();
	if (!IsValid(LocalPawn)) return;
	
	const float Value = TeamRelationshipToRTPCValue(GetTeamRelationship(LocalPawn, RemotePawn));
	
	TArray<UAkComponent*> AkComponents;
	RemotePawn->GetComponents(AkComponents);
	
	for (UAkComponent* Comp : AkComponents)
	{
		if (IsValid(Comp))
		{
			Comp->SetRTPCValue(TeamRelationshipRTPC, Value, 0, FString());
			
		}
	}
}


void UOSAudioComponent::PostFootstep_Internal(UAkAudioEvent* EventToPost, const FVector& Location, const FRotator& Rotation, UAkSwitchValue* SurfaceSwitchValue) const
{
	if (EventToPost == nullptr)
	{
		return;
	}

	// Keep this function "old-file simple": set switch + post event on the owning actor.
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || Owner->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (SurfaceSwitchValue != nullptr)
	{
		UAkGameplayStatics::SetSwitch(SurfaceSwitchValue, Owner);
	}

	UAkGameplayStatics::PostEvent(EventToPost, Owner, /*CallbackMask*/ 0, FOnAkPostEventCallback(), /*bStopWhenAttachedToDestroyed*/ true);
}

//====================Distance Buckets Section====================//

FVector UOSAudioComponent::GetListenerReferenceLocation() const
{
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		return OwnerPawn->GetActorLocation();
	}
	
	return FVector::ZeroVector;
}

float UOSAudioComponent::BucketToRTPCValue(EPlayerAudioDistanceBucket Bucket) const
{
	switch (Bucket)
	{
	case EPlayerAudioDistanceBucket::Near:
		return 0.f;
	case EPlayerAudioDistanceBucket::Mid:
		return 1.0f;
	case EPlayerAudioDistanceBucket::Far:
		return 2.0f;
	case EPlayerAudioDistanceBucket::VeryFar:
		return 3.0f;
	default:
		return 3.0f;
	}
}



void UOSAudioComponent::PushWwiseDistanceValues(AActor* TargetActor, EPlayerAudioDistanceBucket Bucket, float DistanceMeters, EAudioTeamRelationship Relationship) const
{
	if (!IsValid(TargetActor) || TargetActor->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	
	if (PlayerDistanceBucketRTPC)
	{
		UAkGameplayStatics::SetRTPCValue(PlayerDistanceBucketRTPC, BucketToRTPCValue(Bucket), 0, TargetActor);
	}
	
	if (PlayerToDistRTPC)
	{
		UAkGameplayStatics::SetRTPCValue(PlayerToDistRTPC, DistanceMeters, 0, TargetActor);
	}
	
	if (TeamRelationshipRTPC)
	{
		UAkGameplayStatics::SetRTPCValue(TeamRelationshipRTPC, TeamRelationshipToRTPCValue(Relationship), 0, TargetActor);
	}
}

EPlayerAudioDistanceBucket UOSAudioComponent::UpdateSingleRemotePlayerDistance(APawn* RemotePawn, UAkAudioEvent* AkEvent)
{
	// Guard: only the locally controlled pawn's component should drive this
	APawn* LocalPawn = Cast<APawn>(GetOwner());
	if (!LocalPawn || !LocalPawn->IsLocallyControlled())
	{
		return EPlayerAudioDistanceBucket::VeryFar;
	}
	if (!RemotePawn || RemotePawn == LocalPawn)
	{
		return EPlayerAudioDistanceBucket::VeryFar;
	}
	if (GetOwner()->GetNetMode() == NM_DedicatedServer)
	{
		return EPlayerAudioDistanceBucket::VeryFar;
	}

	const FVector ListenerLocation = GetListenerReferenceLocation();
	const float DistSq = FVector::DistSquared(ListenerLocation, RemotePawn->GetActorLocation());

	float EffectiveNear = NearDistance;
	float EffectiveMid = MidDistance;
	float EffectiveFar = FarDistance;
	
	if (AkEvent && AkEvent->MaxAttenuationRadius > 0.f)
	{
		EffectiveNear = AkEvent->MaxAttenuationRadius * NearAttenuationFraction;
		EffectiveMid = AkEvent->MaxAttenuationRadius * MidAttenuationFraction;
		EffectiveFar = AkEvent->MaxAttenuationRadius * FarAttenuationFraction;
	}
	
	const float NearSq = FMath::Square(EffectiveNear);
	const float MidSq = FMath::Square(EffectiveMid);
	const float FarSq = FMath::Square(EffectiveFar);

	EPlayerAudioDistanceBucket Bucket = EPlayerAudioDistanceBucket::VeryFar;
	if      (DistSq < NearSq) Bucket = EPlayerAudioDistanceBucket::Near;
	else if (DistSq < MidSq)  Bucket = EPlayerAudioDistanceBucket::Mid;
	else if (DistSq < FarSq)  Bucket = EPlayerAudioDistanceBucket::Far;

	if (UOSAudioComponent* RemoteAudioComp = RemotePawn->FindComponentByClass<UOSAudioComponent>())
	{
		RemoteAudioComp->CurrentAudioDistanceBucket = Bucket;
	}
	const EAudioTeamRelationship Relationship = GetTeamRelationship(LocalPawn, RemotePawn);
	
	bool bShouldSend = true;
	if (bOnlySendOnBucketChange)
	{
		const bool bBucketSame = CachedBuckets.Contains(RemotePawn) && CachedBuckets[RemotePawn] == Bucket;
		const bool bRelationshipSame = CachedTeamRelationship.Contains(RemotePawn) && CachedTeamRelationship[RemotePawn] == Relationship;
		bShouldSend = !bBucketSame || !bRelationshipSame;

	}

	if (bShouldSend)
	{
		const bool bNeedsExactDistance = (Bucket != EPlayerAudioDistanceBucket::VeryFar) || bSendExactDistanceForVeryFar;
		const float DistanceMeters = bNeedsExactDistance ? (FMath::Sqrt(DistSq) * 0.01f) : 0.f;
		PushWwiseDistanceValues(RemotePawn, Bucket, DistanceMeters, Relationship);
		CachedBuckets.Add(RemotePawn, Bucket);
		CachedTeamRelationship.Add(RemotePawn, Relationship);

		if (bDebugDistanceBuckets && GEngine)
		{
			FColor DebugColor = FColor::Purple;
			switch (Bucket)
			{
				case EPlayerAudioDistanceBucket::Near:    DebugColor = FColor::Green;           break;
				case EPlayerAudioDistanceBucket::Mid:     DebugColor = FColor::Yellow;          break;
				case EPlayerAudioDistanceBucket::Far:     DebugColor = FColor(255, 165, 0);     break;
				case EPlayerAudioDistanceBucket::VeryFar: DebugColor = FColor::Red;             break;
				default: break;
			}
			const FString BucketName =
				(Bucket == EPlayerAudioDistanceBucket::Near)    ? TEXT("Near") :
				(Bucket == EPlayerAudioDistanceBucket::Mid)     ? TEXT("Mid")  :
				(Bucket == EPlayerAudioDistanceBucket::Far)     ? TEXT("Far")  :
				TEXT("VeryFar");
			
			const FString DistStr = (Bucket == EPlayerAudioDistanceBucket::VeryFar && !bSendExactDistanceForVeryFar)
			? FString::Printf(TEXT(">%.0fm"), FarDistance * 0.01f) : FString::Printf(TEXT("%.1fm"), DistanceMeters);

			const FString RelationshipName = (Relationship == EAudioTeamRelationship::Enemy) ? TEXT("Enemy") : TEXT("Friendly");
			const FString Msg = FString::Printf(TEXT("[DistBucket|GC] %s | %s | %s | %s"),
				*RemotePawn->GetName(), *BucketName, *RelationshipName, *DistStr);
			
			const int32 DebugKey = static_cast<int32>(2000 + RemotePawn->GetUniqueID());
			GEngine->AddOnScreenDebugMessage(DebugKey, 3.f, DebugColor, Msg);
		}
	}

	// Opportunistic stale-entry cleanup
	for (auto It = CachedBuckets.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	
	for (auto It = CachedTeamRelationship.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	
	return Bucket;
}

EAudioTeamRelationship UOSAudioComponent::GetTeamRelationship(APawn* LocalPawn, APawn* RemotePawn) const
{
	const AOSPlayerState* LocalPS = LocalPawn ? LocalPawn->GetPlayerState<AOSPlayerState>() : nullptr;
	const AOSPlayerState* RemotePS = RemotePawn ? RemotePawn->GetPlayerState<AOSPlayerState>() : nullptr;
	
	
	if (!LocalPS || !RemotePS)
	{
		return EAudioTeamRelationship::Friendly;
	}
	
	const int32 LocalTeam = LocalPS->GetTeamIndex();
	const int32 RemoteTeam = RemotePS->GetTeamIndex();
	// 255 = FGenericTeamID::NoTeam - not assigned yet, treat as non-threat
	if (LocalTeam == 255 || RemoteTeam == 255)
	{
		return EAudioTeamRelationship::Friendly;
	}
	return (LocalTeam != RemoteTeam) ? EAudioTeamRelationship::Enemy : EAudioTeamRelationship::Friendly;
}

float UOSAudioComponent::TeamRelationshipToRTPCValue(EAudioTeamRelationship Relationship) const
{
	return (Relationship == EAudioTeamRelationship::Enemy) ? 1.f : 0.f;
}
