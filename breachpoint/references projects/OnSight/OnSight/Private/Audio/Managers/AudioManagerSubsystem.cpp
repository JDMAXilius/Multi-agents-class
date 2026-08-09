// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/Managers/AudioManagerSubsystem.h"
#include "AkGameplayStatics.h"
#include "AkStateValue.h"
#include "AkAudioEvent.h"
#include "AkAudioDevice.h"
#include "Kismet/GameplayStatics.h"


void UAudioManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UE_LOG(LogTemp, Log, TEXT("Audio Manager Initialized"));
}

void UAudioManagerSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("Audio Manager Deinitialized"));
	Super::Deinitialize();
}

void UAudioManagerSubsystem::PostGlobalEvent(UAkAudioEvent* Event)
{
	if (!Event || !IsAudioAllowed()) return;
	
	UAkGameplayStatics::PostEvent(Event, nullptr, 0, FOnAkPostEventCallback(), true);
}

void UAudioManagerSubsystem::PostEventAtLocation(UAkAudioEvent* Event, const FVector& Location, const FRotator& Rotation)
{
	if (!Event || !IsAudioAllowed()) return;
	
	UAkGameplayStatics::PostEventAtLocation(Event, Location, Rotation, GetWorld());
}

void UAudioManagerSubsystem::SetState(UAkStateValue* StateName)
{
	if (!IsAudioAllowed()) return;
	UAkGameplayStatics::SetState(StateName);
	
}

void UAudioManagerSubsystem::SetGlobalRTPC(const UAkRtpc* RTPCName, float Value, int32 InterpTimeMs, AActor* Actor)
{
	if (!IsAudioAllowed()) return;
	
	UAkGameplayStatics::SetRTPCValue(RTPCName, Value, InterpTimeMs, nullptr);
}

bool UAudioManagerSubsystem::IsAudioAllowed() const
{
	if (!GetWorld()) return false;
	
	return GetWorld()->IsNetMode(NM_Client) || GetWorld()->IsNetMode(NM_Standalone);
}


int32 UAudioManagerSubsystem::PostGlobalEventReturnID(UAkAudioEvent* Event)
{
	if (!Event || !IsAudioAllowed())
	{
		
		return AK_INVALID_PLAYING_ID;
	}
	
	AkPlayingID PlayingID = UAkGameplayStatics::PostEvent(Event, nullptr, 0, FOnAkPostEventCallback(), true);
	
	
	
	
	
	
	return static_cast<int32>(PlayingID);
}

void UAudioManagerSubsystem::StopByPlayingID(int32 PlayingID)
{
	if (!IsAudioAllowed()) return;

	if (PlayingID == AK_INVALID_PLAYING_ID)
	{
		return;
	}

	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		AkAudioDevice->StopPlayingID(
			static_cast<AkPlayingID>(PlayingID)
		);
	}
}


