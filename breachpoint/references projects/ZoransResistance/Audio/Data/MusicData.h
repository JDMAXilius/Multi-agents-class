// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/Object.h"
#include "MusicData.generated.h"


UENUM(BlueprintType)
enum class EGenre : uint8
{
	Edm		UMETA(DisplayName = "EDM"),
	Rock		UMETA(DisplayName = "Rock"),
	Metal		UMETA(DisplayName = "Metal"),
	Country		UMETA(DisplayName = "Country"),
};

UENUM(BlueprintType)
enum class EMusicKeys : uint8
{
	A		UMETA(DisplayName = "A"),
	Bb		UMETA(DisplayName = "Bb"),
	B		UMETA(DisplayName = "B"),
	C		UMETA(DisplayName = "C"),
	Db		UMETA(DisplayName = "Db"),
	D		UMETA(DisplayName = "D"),
	Eb		UMETA(DisplayName = "Eb"),
	E		UMETA(DisplayName = "E"),
	F		UMETA(DisplayName = "F"),
	Gb		UMETA(DisplayName = "Gb"),
	G		UMETA(DisplayName = "G"),
	Ab		UMETA(DisplayName = "Ab"),
};

UENUM(BlueprintType)
enum class ELoopName : uint8
{
	Full		UMETA(DisplayName = "Full"),
	Intro		UMETA(DisplayName = "Intro"),
	Verse		UMETA(DisplayName = "Verse"),
	Chorus		UMETA(DisplayName = "Chorus"),
	Outro		UMETA(DisplayName = "Outro")
	
};

USTRUCT(BlueprintType)
struct FMusicLoop
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	ELoopName LoopName = ELoopName::Full;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float LoopTime = 0.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 LoopBars = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, DisplayName="WAV")
	TObjectPtr<USoundWave> Wav;
};


USTRUCT(BlueprintType)
struct FMusicPlaylist : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FName> Playlist;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName PlaylistName = FName("None");
};

USTRUCT(BlueprintType)
struct FMusicBundle : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName BundleName = FName("None");

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FName> BundleTracks;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float BundleCost = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName BudleGenre = FName("None");

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGenre Genre = EGenre::Edm;
};


USTRUCT(BlueprintType)
struct FMusicData : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName TrackName = FName("None");

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float TrackTime = 0.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 TotalBars = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGenre Genre = EGenre::Edm;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, DisplayName="BPM")
	float Bpm = 0.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EMusicKeys Key = EMusicKeys::A;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<EMusicKeys> AdjacentKeys;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FMusicLoop> Loops;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName TrackArtist = FName("None");
};