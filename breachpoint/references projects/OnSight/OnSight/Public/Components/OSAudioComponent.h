// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "OSAudioComponent.generated.h"

class UAkAudioEvent;
class UAkSwitchValue;
class UAkRtpc;
class AOSPlayer;


UENUM(BlueprintType)
enum class EPlayerAudioDistanceBucket : uint8
{
	Near	UMETA(DisplayName = "Near"),
	Mid		UMETA(DisplayName = "Mid"),
	Far		UMETA(DisplayName = "Far"),
	VeryFar UMETA(DisplayName = "VeryFar"),
};

UENUM(BlueprintType)
enum class EAudioTeamRelationship : uint8
{
	Friendly UMETA(DisplayName = "Friendly"),
	Enemy UMETA(DisplayName = "Enemy")
};


UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ONSIGHT_API UOSAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSAudioComponent(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	//==========Distance Bucket============//
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Optimization")
	float NearDistance = 500.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Optimization")
	float MidDistance = 1000.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Optimization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NearAttenuationFraction = 0.25f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Optimization", meta = (ClampMin = "0.0", ClampMax = "1.0") )
	float MidAttenuationFraction = 0.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Optimization", meta = (ClampMin = "0.0", ClampMax = "1.0") )
	float FarAttenuationFraction = 0.75f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Optimization")
	float FarDistance = 1500.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Optimization|RTPC")
	TObjectPtr<UAkRtpc> PlayerToDistRTPC = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Optimization|RTPC")
	TObjectPtr<UAkRtpc> PlayerDistanceBucketRTPC = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Priority")
	TObjectPtr<UAkRtpc> TeamRelationshipRTPC = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Optimization")
	bool bOnlySendOnBucketChange = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Optimization")
	bool bSendExactDistanceForVeryFar = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDebugDistanceBuckets = false;
	
	UFUNCTION(BlueprintPure, Category = "Audio Distance")
	EPlayerAudioDistanceBucket GetCurrentAudioDistanceBucket() const
	{
		return CurrentAudioDistanceBucket;
	}

	/** Called from OSGameplayCueNotifyStatic when a GC fires on a remote pawn.
	 *  Computes the distance between the local player and RemotePawn and pushes
	 *  the Wwise distance RTPCs for that pawn only. Replaces the old timer-based sweep. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Distance")
	EPlayerAudioDistanceBucket UpdateSingleRemotePlayerDistance(APawn* RemotePawn, UAkAudioEvent* AkEvent = nullptr);
	
	

	//========Base Functions by Juan=========//


	/** Convenience for BP: returns the owning player (castable to BP_OSPlayerR, etc.). */
	UFUNCTION(BlueprintPure, Category = "Owner")
	AOSPlayer* GetPlayerOwner() const;
	// Replicate footsteps to other clients (mirrors old USoundBase multicast behavior). 
	
	UFUNCTION(BlueprintCallable, Category = "Audio|Team Relationship")
	void PushTeamRelationshipToAllComponents(APawn* RemotePawn);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	bool bReplicateToOtherClients = true;

private:
	void UpdatePlayerSpeedRtpc();
	FVector GetListenerReferenceLocation() const;
	void PushWwiseDistanceValues(AActor* TargetActor, EPlayerAudioDistanceBucket Bucket, float DistanceMeters, EAudioTeamRelationship Relationship) const;
	float BucketToRTPCValue(EPlayerAudioDistanceBucket Bucket) const;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Audio|Optimization|Debug", meta = (AllowPrivateAccess = "true"))
	EPlayerAudioDistanceBucket CurrentAudioDistanceBucket = EPlayerAudioDistanceBucket::Near;
	
	TMap<TWeakObjectPtr<APawn>, EAudioTeamRelationship> CachedTeamRelationship;
	EAudioTeamRelationship GetTeamRelationship(APawn* LocalPawn, APawn* RemotePawn) const;
	float TeamRelationshipToRTPCValue(EAudioTeamRelationship Relationship) const;

	TMap<TWeakObjectPtr<APawn>, EPlayerAudioDistanceBucket> CachedBuckets;
	
	void PostFootstep_Internal(UAkAudioEvent* EventToPost, const FVector& Location, const FRotator& Rotation, UAkSwitchValue* SurfaceSwitchValue) const;

	FTimerHandle PlayerSpeedRtpcTimerHandle;
	
	
	
};


