// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundCue.h"
#include "StaticGameData.generated.h"

namespace Zorans_StaticGameData
{
	namespace CharacterAttachSocketNames
	{
		static inline FName RightHand = "Hand_R";
		static inline FName LeftHand = "Hand_L";
	}
	
	namespace PartSocketNames
	{
		static inline FName Head = "Head";
		static inline FName Torso = "TorsoUpper";
		static inline FName ArmRight = "Shoulder_R";
		static inline FName ArmLeft = "Shoulder_L";
	}

	namespace AccessorySocketNames
	{
		static inline FName Head = "HeadAcc";
		static inline FName TorsoUpper = "TorsoUpperAcc";
		static inline FName TorsoLower = "TorsoLowerAcc";
		static inline FName ArmRight = "Shoulder_R_Acc";
		static inline FName ArmLeft = "Shoulder_L_Acc";
		static inline FName ElbowRight = "Elbow_R_Acc";
		static inline FName ElbowLeft = "Elbow_L_Acc";
		static inline FName KneeRight = "Knee_R_Acc";
		static inline FName KneeLeft = "Knee_L_Acc";
		static inline FName LegRight = "Leg_R_Acc";
		static inline FName LegLeft = "Leg_L_Acc";
	}

	namespace WeaponSocketNames
	{
		static inline FName Muzzle = "Muzzle";
		static inline FName	Magazine = "Magazine";
		static inline FName ShellEject = "ShellEject";
		static inline FName	OffhandLocation = "OffhandLocation";
		static inline FName TracePoint = "TracePoint";
	}

	namespace BodyPoints
	{
		static inline FName Point_Head = "Point_Head";
		static inline FName Point_Torso = "Point_Torso";
		static inline FName Point_Arm_Left = "Point_Arm_Left";
		static inline FName Point_Arm_Right = "Point_Arm_Right";
		static inline FName Point_Leg_Left = "Point_Leg_Left";
		static inline FName Point_Leg_Right = "Point_Leg_Right";
	}

	namespace BodySections
	{
		static inline FName Section_Head = "head";
		static inline FName Section_Torso = "spine_03";
		static inline FName Section_Arm_Left = "upperarm_l";
		static inline FName Section_Arm_Right = "upperarm_r";
		static inline FName Section_Leg_Left = "thigh_l";
		static inline FName Section_Leg_Right = "thigh_r";
	}

	namespace WeaponNames
	{
		static inline FName Unarmed = "Unarmed";
	}
}

UENUM(BlueprintType)
enum class EZoranDirection4 : uint8
{
	Forward				UMETA(DisplayName = "Forward"),
	Left				UMETA(DisplayName = "Left"),
	Right				UMETA(DisplayName = "Right"),
	Backward			UMETA(DisplayName = "Backward")
};

UENUM(BlueprintType)
enum class EZoranDirection6 : uint8
{
	Forward				UMETA(DisplayName = "Forward"),
	Left				UMETA(DisplayName = "Left"),
	Right				UMETA(DisplayName = "Right"),
	Backward			UMETA(DisplayName = "Backward"),
	Up					UMETA(DisplayName = "Up"),
	Down				UMETA(DisplayName = "Down")
};

USTRUCT(BlueprintType)
struct FAIStatsInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float AggressionLevel = 1;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float AccurayLevel = 2;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float ReactionTime = 1;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float MaxDistanceTrace = 10000;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool TakesCover = true;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool Flanks = true;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool HeadShots = true;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool PickWeaponAtStart = false;

	//Abilities

	//Level 1 - aiming reflects struggles
	//factor weapon recaoil
	//keep tracks of player

	//Level 2 - focus on player headshots
	//Level 3 - factoring playermovement and recoil
	//Level 4 - have no real issiues with aim
	
};

UENUM(BlueprintType)
enum class EAIDificulTyLevel : uint8
{
	Easy				UMETA(DisplayName = "Easy"),
	Medium				UMETA(DisplayName = "Medium"),
	Hard				UMETA(DisplayName = "Hard"),
	Insane				UMETA(DisplayName = "Insane")
};

USTRUCT(BlueprintType)
struct FCharacterRewindData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float Timestamp = 0.f;

	UPROPERTY(BlueprintReadOnly)
	FTransform CharacterTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly)
	FVector CharacterVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FRotator CharacterControlRotation = FRotator::ZeroRotator;
};

UENUM(BlueprintType)
enum class EZoranBodySection : uint8
{
	None				UMETA(DisplayName = "None"),
	Full				UMETA(DisplayName = "Full"),
	Head				UMETA(DisplayName = "Head"),
	Arms				UMETA(DisplayName = "Arms"),
	Front				UMETA(DisplayName = "Front"),
	Back				UMETA(DisplayName = "Back"),
	Legs				UMETA(DisplayName = "Legs")
};

static TMap<EZoranBodySection, FName> BodySectionTagMap =
{
	{EZoranBodySection::Full, "ZoranAttachment.Full"},
	{EZoranBodySection::Head, "ZoranAttachment.Head"},
	{EZoranBodySection::Arms, "ZoranAttachment.Arms"},
	{EZoranBodySection::Front, "ZoranAttachment.Front"},
	{EZoranBodySection::Back, "ZoranAttachment.Back"},
	{EZoranBodySection::Legs, "ZoranAttachment.Legs"}
};

UENUM(BlueprintType)
enum class EModularZoranComponent : uint8
{
	None				UMETA(DisplayName = "None"),
	Head				UMETA(DisplayName = "Head"),
	Torso				UMETA(DisplayName = "Torso"),
	Arms				UMETA(DisplayName = "Arms"),
	Legs				UMETA(DisplayName = "Legs")
};

UENUM(BlueprintType)
enum class EZoranRarity : uint8
{
	None				UMETA(DisplayName = "None"), // light gray
	Common				UMETA(DisplayName = "Common"), // white
	Uncommon			UMETA(DisplayName = "Uncommon"), // green
	Rare				UMETA(DisplayName = "Rare"), // blue
	SuperRare			UMETA(DisplayName = "SuperRare"), // purple
	Legendary			UMETA(DisplayName = "Legendary"), // gold
	Exotic				UMETA(DisplayName = "Exotic") // orangish/red
};

USTRUCT(BlueprintType)
struct FGameEventAnnouncement
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "Game Event Announcement")
	FString EventText;

	UPROPERTY(BlueprintReadWrite, Category = "Game Event Announcement")
	TSoftObjectPtr<USoundCue> EventSound;

	// To use ColorOverride, use a Team Index of -2
	UPROPERTY(BlueprintReadWrite, Category = "Game Event Announcement")
	int32 TeamIndex = -1;

	// Only used if the Team Index is -2
	UPROPERTY(BlueprintReadWrite, Category = "Game Event Announcement")
	FLinearColor ColorOverride = FLinearColor::Black;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

inline bool FGameEventAnnouncement::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	Ar << EventText;
	Ar << EventSound;
	Ar << TeamIndex;

	if (TeamIndex < -1)
	{
		Ar << ColorOverride;
	}

	bOutSuccess = true;
	return true;
}