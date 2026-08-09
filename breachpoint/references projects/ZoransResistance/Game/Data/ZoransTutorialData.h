// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "ZoransTutorialData.generated.h"


USTRUCT(BlueprintType)
struct FTutorialDialogueAssetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FPrimaryAssetId TutorialDialogueAsset;
};

UENUM(BlueprintType)
enum class ETutorialDialogueCharacter : uint8
{
	None		UMETA(DisplayName = "None"),
	Jive		UMETA(DisplayName = "JIVE")
};

USTRUCT(BlueprintType)
struct FTutorialDialogueLine
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText Text = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TSoftObjectPtr<USoundWave> Audio = nullptr;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	ETutorialDialogueCharacter Character = ETutorialDialogueCharacter::Jive;
};

UCLASS(BlueprintType)
class ZORANSRESISTANCE_API UTutorialDialogue : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FTutorialDialogueLine> Lines;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("DA_TutorialDialogue", GetFName());
	}
};

UENUM(BlueprintType)
enum class ETutorialObjectiveType : uint8
{
	None				UMETA(DisplayName = "None"),
	Move				UMETA(DisplayName = "Move"),
	Aim					UMETA(DisplayName = "Aim"),
	Emote				UMETA(DisplayName = "Emote"),
	DoubleJump			UMETA(DisplayName = "DoubleJump"),
	ShootZoran			UMETA(DisplayName = "ShootZoran"),
	Dash				UMETA(DisplayName = "Dash"),
	Grenade				UMETA(DisplayName = "Grenade"),
	Throwdown			UMETA(DisplayName = "Throwdown"),
	ThrowdownRanged		UMETA(DisplayName = "ThrowdownRanged"),
	AerialAttack		UMETA(DisplayName = "AerialAttack"),
	AerialAttackKill	UMETA(DisplayName = "AerialAttackKill"),
};

USTRUCT(BlueprintType)
struct FTutorialObjective
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	ETutorialObjectiveType ObjectiveType = ETutorialObjectiveType::None;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	int32 Goal = 0;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	bool bShowProgressNumber = false;
};

USTRUCT(BlueprintType)
struct FTutorialLesson : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FPrimaryAssetId BeginDialogueAsset;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TArray<UDataLayerAsset*> DataLayers;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TArray<FTutorialObjective> Objectives = {};

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FName NextLesson = NAME_None;
};