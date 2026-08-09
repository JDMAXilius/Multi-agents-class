// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AkRtpc.h"
#include "AudioPlayerDistanceBuckets.generated.h"

class UAkComponent;

UENUM(BlueprintType)
enum class EAudioPlayerDistanceBucket : uint8
{
	Near	UMETA(DisplayName = "Near"),
	Mid		UMETA(DisplayName = "Mid"),
	Far		UMETA(DisplayName = "Far"),
	VeryFar UMETA(DisplayName = "VeryFar"),
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ONSIGHT_API UAudioPlayerDistanceBuckets : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UAudioPlayerDistanceBuckets();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	//Timer Callback
	void UpdateRemotePlayerDistances();
	
	//Get listener reference position for distance checks
	FVector GetListenerReferenceLocation() const;
	
	//Fallback helper for distance checks if target pawn check fails for whatever reason
	UAkComponent* FindAKComponentOnPawn(APawn* Pawn) const;
	
	void PushWwiseDistanceValues(APawn* RemotePawn, EAudioPlayerDistanceBucket Bucket, float DistanceMeters) const;
	
	float BucketToRTPCValue(EAudioPlayerDistanceBucket Bucket) const;
	
	UPROPERTY(BlueprintReadOnly, Category = "Audio|Optimization|Debug")
	EAudioPlayerDistanceBucket CurrentAudioDistanceBucket = EAudioPlayerDistanceBucket::Near;
	
	UFUNCTION(BlueprintPure, Category = "Audio|Optimization|Debug")
	EAudioPlayerDistanceBucket GetCurrentAudioBucket() const
	{
		return CurrentAudioDistanceBucket;
	}
	
	UPROPERTY(EditAnywhere, Category = "Audio|Optimization")
	float UpdateIntervalSeconds = 0.1f;
	
	UPROPERTY(EditAnywhere, Category = "Audio|Optimization")
	float NearDistance = 1500.f; //15m
	
	UPROPERTY(EditAnywhere, Category = "Audio|Optimization")
	float MidDistance = 3500.f; //35m
	
	UPROPERTY(EditAnywhere, Category = "Audio|Optimization")
	float FarDistance = 6000.f; //60m
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Optimization")
	TObjectPtr<UAkRtpc> DistanceMetersRTPC = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Optimization")
	TObjectPtr<UAkRtpc> DistanceBucketRTPC = nullptr;
	
	UPROPERTY(EditAnywhere, Category = "Audio|Optimization")
	bool bOnlySendOnBucketChange = true;
	
	UPROPERTY(EditAnywhere, Category = "Audio|Optimization")
	bool bSendExactDistanceVeryFar = false;
	
	UPROPERTY(EditAnywhere, Category = "Audio|Optimization|Debug")
	bool bDebugPrint = false;

private:
	FTimerHandle DistanceUpdateTimerHandle;
	
	TMap<TWeakObjectPtr<APawn>, EAudioPlayerDistanceBucket> CachedBuckets;
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
