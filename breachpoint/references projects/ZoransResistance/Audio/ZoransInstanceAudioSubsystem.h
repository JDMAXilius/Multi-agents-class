// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Sound/SoundEffectSubmix.h"
#include "Data/MusicData.h"
#include "ZoransInstanceAudioSubsystem.generated.h"

class UZoransInstanceAudioSubsystemConfig;
class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlaylistTrackChange, const FName&, TrackName);

UCLASS()
class ZORANSRESISTANCE_API UZoransInstanceAudioSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY()
	UAudioComponent* PrimaryMusicAudioComponent = nullptr;

	UPROPERTY()
	UAudioComponent* SecondaryMusicAudioComponent = nullptr;
	
	UPROPERTY()
	TObjectPtr<UAudioComponent> ConditionalAudioComponent = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<USoundEffectSubmixPreset> MasterSubmix = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UZoransInstanceAudioSubsystemConfig> Config;

	// API Start

	UPROPERTY(BlueprintAssignable)
	FOnPlaylistTrackChange OnPlaylistTrackChange;
	
	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void FadeToAnotherTrackAndChangeLoop(const FName& NextTrackName, const ELoopName& LoopName, const float FadeTime);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void FadeToAnotherTrack(const FName& NextTrackName, const float FadeTime);
	
	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void ChangeTrackLoop(const ELoopName LoopName, const bool bImmediate = false);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void StopAll();

	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void Pause() const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, BlueprintPure)
	bool IsPaused() const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void Play() const;

	UPROPERTY(BlueprintReadOnly, DisplayName=Volume)
	float VolumeMultiplier = 1;
	
	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void SetVolume(float Volume);

	UPROPERTY(BlueprintReadWrite)
	bool bLooping = false;

	UPROPERTY(BlueprintReadWrite)
	bool bShuffle = false;

	UPROPERTY(BlueprintReadOnly)
	int32 PlaylistIndex;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void SetPlaylist(FMusicPlaylist Playlist);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void NextPlaylistTrack(const float FadeTime) {ChangePlaylistTrack(PlaylistIndex + 1, FadeTime);}

	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void PreviousPlaylistTrack(const float FadeTime) {ChangePlaylistTrack(PlaylistIndex - 1, FadeTime);}

	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void ChangePlaylistTrack(int32 TrackIndex, const float FadeTime);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	FName GetCurrentPlaylistTrack();
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UAudioComponent* GetActiveMusicComponent() const { return bIsPrimaryAudioComponent ? PrimaryMusicAudioComponent : SecondaryMusicAudioComponent; }

	UFUNCTION(BlueprintCallable, BlueprintPure)
	UAudioComponent* GetInactiveMusicComponent() const { return !bIsPrimaryAudioComponent ? PrimaryMusicAudioComponent : SecondaryMusicAudioComponent; }
	
	// API End

	UFUNCTION()
	void InitializeWorld(UWorld* World);
	
	virtual void Tick(const float DeltaTime) override;
	
	virtual bool IsAllowedToTick() const override;

	virtual TStatId GetStatId() const override;
private:

	bool bIsPrimaryAudioComponent = true;
	
	float CurrentFadeProgress = 0;

	float TimeToFade = 0;

	FMusicPlaylist CurrentPlaylist;
	bool bIsPlaylistSet = false;
	bool bIsCurrentlyUsingPlaylist = false;

	bool bIsFading = false;

	ELoopName CurrentLoopName = ELoopName::Full;

	const FMusicData* CurrentTrack;

	TMap<ELoopName, int32> TrackLoopMap;

	TMap<FName, float> ComponentPlayProgress;

	void FadeToAnotherTrackInternal(const FName& NextTrackName, const float FadeTime, const bool bPartOfPlaylist);
	
	UAudioComponent* CreateSubsystemAudio(FName Name, UWorld* World);
	
	void UpdateLoopCache();
	
	const FMusicLoop& GetTrackLoop(const ELoopName& LoopName) const;

	UFUNCTION()
	void OnAudioFinished();
};	