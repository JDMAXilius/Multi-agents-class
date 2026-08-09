// Copyright Zollpa LLC


#include "ZoransInstanceAudioSubsystem.h"
#include "ZoransInstanceAudioSubsystemConfig.h"
#include "Components/AudioComponent.h"
#include "Data/MusicData.h"
#include "Engine/DataTable.h"
#include "ZoransResistance/ZoransResistance.h"

UAudioComponent* UZoransInstanceAudioSubsystem::CreateSubsystemAudio(const FName Name, UWorld* World)
{
	UAudioComponent* AudioComponent = NewObject<UAudioComponent>(World, Name);
	AudioComponent->bAutoActivate = false;
	AudioComponent->AttenuationSettings = nullptr;
	AudioComponent->bAllowSpatialization = false;
	AudioComponent->bIsUISound = true;
	AudioComponent->bAutoDestroy = false;
	AudioComponent->bIgnoreForFlushing = false;
	AudioComponent->bStopWhenOwnerDestroyed = true;
	AudioComponent->bIsMusic = true;
	AudioComponent->bAlwaysPlay = true;

	//Keep track of how far we are in the current song
	AudioComponent->OnAudioPlaybackPercentNative.AddLambda(
		[this](const UAudioComponent* Component, const USoundWave* SoundWave, const float Percent)
	{
			ComponentPlayProgress.Add(Component->GetFName(), SoundWave->GetDuration()*Percent);
	});
	return AudioComponent;
}

void UZoransInstanceAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UE_LOG(ZoransLog, Log, TEXT("ZoransInstanceAudioSubsystem: Initialized"));
}

void UZoransInstanceAudioSubsystem::InitializeWorld(UWorld* World)
{
	PrimaryMusicAudioComponent = CreateSubsystemAudio(FName("PrimaryAudio"), World);
	SecondaryMusicAudioComponent = CreateSubsystemAudio(FName("SecondaryAudio"), World);
		
	PrimaryMusicAudioComponent->RegisterComponentWithWorld(World);
	SecondaryMusicAudioComponent->RegisterComponentWithWorld(World);

	//If the active component was previously playing music, continue to play where we left off
	UAudioComponent* ActiveComponent = bIsFading ? GetInactiveMusicComponent() : GetActiveMusicComponent();
	if(ComponentPlayProgress.Contains(ActiveComponent->GetFName()) && CurrentTrack)
	{
		ActiveComponent->SetSound(GetTrackLoop(CurrentLoopName).Wav);
		ActiveComponent->SetVolumeMultiplier(Config->GetFadeVolume(bIsFading ? CurrentFadeProgress : 1, true, VolumeMultiplier));
		ActiveComponent->Play(ComponentPlayProgress.FindRef(ActiveComponent->GetFName()));
		UE_LOG(ZoransLog, Log, TEXT("ZoransInstanceAudioSubsystem: Continuing audio"));
	}
	UE_LOG(ZoransLog, Log, TEXT("ZoransInstanceAudioSubsystem: World Initialized"));
}

void UZoransInstanceAudioSubsystem::FadeToAnotherTrackAndChangeLoop(const FName& NextTrackName, const ELoopName& LoopName, const float FadeTime)
{
	CurrentLoopName = LoopName;
	FadeToAnotherTrack(NextTrackName, FadeTime);
}

void UZoransInstanceAudioSubsystem::FadeToAnotherTrack(const FName& NextTrackName, const float FadeTime)
{
	FadeToAnotherTrackInternal(NextTrackName, FadeTime, false);
}

void UZoransInstanceAudioSubsystem::FadeToAnotherTrackInternal(const FName& NextTrackName, const float FadeTime, const bool bPartOfPlaylist)
{
	UAudioComponent* InactiveComponent = GetInactiveMusicComponent();
	if(!IsValid(InactiveComponent))
	{
		UE_LOG(ZoransLog, Error, TEXT("UZoransInstanceAudioSubsystem::FadeToAnotherTrack : InactiveComponent is not valid!"));
		return;
	}
	const FMusicData* NextTrack = Config->MusicTable->FindRow<FMusicData>(NextTrackName, "");
	if(NextTrack || NextTrackName.IsNone())
	{
		CurrentTrack = NextTrack;
		UpdateLoopCache();
		ComponentPlayProgress.Add(InactiveComponent->GetFName(), 0.f);
		InactiveComponent->SetSound(CurrentTrack ? GetTrackLoop(CurrentLoopName).Wav : nullptr);
		TimeToFade = FadeTime;
		CurrentFadeProgress = 0.f;
		bIsFading = true;

		if(bPartOfPlaylist)
		{
			OnPlaylistTrackChange.Broadcast(NextTrackName);
		}
		bIsCurrentlyUsingPlaylist = bPartOfPlaylist;
	}else
	{
		UE_LOG(ZoransLog, Error, TEXT("UZoransInstanceAudioSubsystem::FadeToAnotherTrack : Unable to find music track: %s"), *NextTrackName.ToString());
	}
}

void UZoransInstanceAudioSubsystem::ChangeTrackLoop(const ELoopName LoopName, const bool bImmediate)
{
	CurrentLoopName = LoopName;
	UAudioComponent* ActiveComponent = GetActiveMusicComponent();
	if(ActiveComponent->IsPlaying() && !bImmediate)
	{
		ActiveComponent->OnAudioFinished.Clear();
		ActiveComponent->OnAudioFinished.AddUniqueDynamic(this, &UZoransInstanceAudioSubsystem::OnAudioFinished);
	}else
	{
		OnAudioFinished();
	}
}

void UZoransInstanceAudioSubsystem::StopAll()
{
	PrimaryMusicAudioComponent->OnAudioFinished.Clear();
	SecondaryMusicAudioComponent->OnAudioFinished.Clear();
	
	PrimaryMusicAudioComponent->Stop();
	SecondaryMusicAudioComponent->Stop();
	
	bIsFading = false;
}

void UZoransInstanceAudioSubsystem::Pause() const
{
	PrimaryMusicAudioComponent->SetPaused(true);
	SecondaryMusicAudioComponent->SetPaused(true);
}

bool UZoransInstanceAudioSubsystem::IsPaused() const
{
	return PrimaryMusicAudioComponent->bIsPaused;
}

void UZoransInstanceAudioSubsystem::Play() const
{
	if(PrimaryMusicAudioComponent->bIsPaused)
	{
		PrimaryMusicAudioComponent->SetPaused(false);
		SecondaryMusicAudioComponent->SetPaused(false);
	}else
	{
		PrimaryMusicAudioComponent->Play();
		SecondaryMusicAudioComponent->Play();
	}
}

void UZoransInstanceAudioSubsystem::SetVolume(const float Volume)
{
	VolumeMultiplier = FMath::Max(Volume, 0.001f);
	if(!bIsFading)
	{
		GetActiveMusicComponent()->SetVolumeMultiplier(Config->GetFadeVolume(1, true, VolumeMultiplier));
	}
}

void UZoransInstanceAudioSubsystem::SetPlaylist(const FMusicPlaylist Playlist)
{
	CurrentPlaylist = Playlist;
	bIsPlaylistSet = true;
	if(CurrentPlaylist.Playlist.Num() > 0)
	{
		ChangePlaylistTrack(0, 0);
	}
}

void UZoransInstanceAudioSubsystem::ChangePlaylistTrack(int32 TrackIndex, const float FadeTime)
{
	if(!bIsPlaylistSet)
	{
		UE_LOG(ZoransLog, Error, TEXT("UZoransInstanceAudioSubsystem::ChangePlaylistTrack : Playlist is not set!"));
		return;
	}
	const int32 PlaylistSize = CurrentPlaylist.Playlist.Num();
	if(bShuffle)
	{
		TrackIndex = FMath::RandRange(0, PlaylistSize - 2);
		if(TrackIndex >= PlaylistIndex)
		{
			TrackIndex++;
		}
	}
	if(TrackIndex < 0)
	{
		PlaylistIndex = PlaylistSize - 1;
	}else if(TrackIndex >= PlaylistSize)
	{
		PlaylistIndex = 0;
	}else
	{
		PlaylistIndex = TrackIndex;
	}
	FadeToAnotherTrackInternal(CurrentPlaylist.Playlist[PlaylistIndex], FadeTime, true);
}

FName UZoransInstanceAudioSubsystem::GetCurrentPlaylistTrack()
{
	if(!bIsPlaylistSet)
	{
		UE_LOG(ZoransLog, Error, TEXT("UZoransInstanceAudioSubsystem::GetCurrentPlaylistTrack : No playlist set!"));
		return FName();
	}
	return CurrentPlaylist.Playlist[PlaylistIndex];
}

void UZoransInstanceAudioSubsystem::Tick(float DeltaTime)
{
	// Want to make transition sound smooth so if the game freezes for any reason (i.e. level transition
	// the transition will continue to sound smooth
	if(DeltaTime > 0.5f)
	{
		DeltaTime = 0.1f;
	}
	UAudioComponent* ActiveComponent = GetActiveMusicComponent();
	UAudioComponent* InactiveComponent = GetInactiveMusicComponent();
	if(!IsValid(ActiveComponent) || !IsValid(InactiveComponent))
	{
		UE_LOG(ZoransLog, Error, TEXT("UZoransInstanceAudioSubsystem::Tick : Music Components are not valid!"));
		return;
	}
	
	const float ProgressDelta = DeltaTime/TimeToFade;
	CurrentFadeProgress = FMath::Clamp(CurrentFadeProgress + ProgressDelta, 0.f, 1.f);

	//Change volume
	ActiveComponent->SetVolumeMultiplier(Config->GetFadeVolume(CurrentFadeProgress, false, VolumeMultiplier));
	InactiveComponent->SetVolumeMultiplier(Config->GetFadeVolume(CurrentFadeProgress, true, VolumeMultiplier));
	
	if(!InactiveComponent->IsPlaying())
	{
		InactiveComponent->OnAudioFinished.AddUniqueDynamic(this, &UZoransInstanceAudioSubsystem::OnAudioFinished);
		InactiveComponent->Play(ComponentPlayProgress.FindRef(InactiveComponent->GetFName()));
	}
	
	if(CurrentFadeProgress >= 1.f)
	{
		//Fade is over
		CurrentFadeProgress = 0.f;
		bIsFading = false;
		bIsPrimaryAudioComponent = !bIsPrimaryAudioComponent;
		ActiveComponent->OnAudioFinished.Clear();
		ActiveComponent->Stop();
	}
}

bool UZoransInstanceAudioSubsystem::IsAllowedToTick() const
{
	return Config && bIsFading;
}

TStatId UZoransInstanceAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZoransInstanceAudioSubsystem, STATGROUP_Tickables);
}

void UZoransInstanceAudioSubsystem::UpdateLoopCache()
{
	TrackLoopMap.Reset();
	if(CurrentTrack)
	{
		for(int32 i = 0; i < CurrentTrack->Loops.Num(); i++)
		{
			TrackLoopMap.Add(CurrentTrack->Loops[i].LoopName, i);
		}
	}
}

const FMusicLoop& UZoransInstanceAudioSubsystem::GetTrackLoop(const ELoopName& LoopName) const
{
	check(CurrentTrack != nullptr)
	return CurrentTrack->Loops[TrackLoopMap[LoopName]];
}

// ReSharper disable once CppMemberFunctionMayBeConst
void UZoransInstanceAudioSubsystem::OnAudioFinished()
{
	if(!CurrentTrack)return;
	if(bLooping && bIsCurrentlyUsingPlaylist)
	{
		FadeToAnotherTrackInternal(CurrentPlaylist.Playlist[PlaylistIndex], 0.f, true);
		return;
	}
	NextPlaylistTrack(0.f);
}
