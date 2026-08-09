// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AkAudioEvent.h"
#include "AudioManagerSubsystem.generated.h" 


class UAkAudioEvent;


/**
 * 
 */
UCLASS()
class ONSIGHT_API UAudioManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	//Wwise Events
	
	UFUNCTION(BlueprintCallable, Category = "Audio|Events")
	void PostGlobalEvent(UAkAudioEvent* Event);
	
	UFUNCTION(BlueprintCallable, Category = "Audio|Events")
	int32 PostGlobalEventReturnID(UAkAudioEvent* Event);
	
	UFUNCTION(BlueprintCallable, Category = "Audio|Stop")
	void StopByPlayingID(int32 PlayingID);
	
		
	UFUNCTION(BlueprintCallable, Category = "Audio|Events")
	void PostEventAtLocation(UAkAudioEvent* Event, const FVector& Location, const FRotator& Rotation);
	
	
	
	//Wwise States
	UFUNCTION(BlueprintCallable, Category = "Audio|States")
	void SetState(UAkStateValue* StateName);
	
	UFUNCTION(BlueprintCallable, Category = "Audio|RTPCs")
	void SetGlobalRTPC(const UAkRtpc* RTPCName, float Value, int32 InterpTimeMs, AActor* Actor);

	
private:
	bool IsAudioAllowed() const;
	
	
};
