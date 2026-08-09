// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "LocalPlayerData.generated.h"

class UDA_ZoranAttachment_Skeletal;
class UDA_ZoranAttachment_Static;

USTRUCT(BlueprintType)
struct FPlayerProfileData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName Banner = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName Frame = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName Image = NAME_None;
};

USTRUCT(BlueprintType)
struct FWeaponSkinSet
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName WeaponName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName WeaponSkin = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName WeaponSwatch = NAME_None;
};

USTRUCT(BlueprintType)
struct FZoranComponentData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Zoran Modular Component")
	FName Component_Name = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Zoran Modular Component")
	FName Material_Name = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Zoran Modular Component")
	float Component_Level = 1.f;
};

USTRUCT(BlueprintType)
struct FZoranCharacterComponents
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FZoranComponentData Component_Head = FZoranComponentData();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FZoranComponentData Component_Torso = FZoranComponentData();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FZoranComponentData Component_Arms = FZoranComponentData();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FZoranComponentData Component_Legs = FZoranComponentData();
};

USTRUCT(BlueprintType)
struct FZoranCharacterLoadedAssets
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	USkeletalMesh* Mesh_Merged = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	USkeletalMesh* Component_Head = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	UMaterialInstanceDynamic* Material_Head = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	USkeletalMesh* Component_Torso = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	UMaterialInstanceDynamic* Material_Torso = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	USkeletalMesh* Component_Arms = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	UMaterialInstanceDynamic* Material_Arms = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	USkeletalMesh* Component_Legs = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	UMaterialInstanceDynamic* Material_Legs = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	TArray<UDA_ZoranAttachment_Static*> LoadedAttachments_Static;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	TArray<UDA_ZoranAttachment_Skeletal*> LoadedAttachments_Skeletal;
};

USTRUCT(BlueprintType)
struct FPlayerLoadout
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName PrimaryWeapon = NAME_None;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout", meta=(NoJsonSerialize))
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName SecondaryWeapon = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName Hologram = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName ThrowdownClose = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName ThrowdownRanged = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName ZoranSkin = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName ZoranSwatch = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName CharacterSelection = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FZoranCharacterComponents ZoranCharacterComponents;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, NotReplicated, Category = "Player Loadout")
	FZoranCharacterLoadedAssets ZoranCharacterLoadedAssets;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	TArray<FName> AttachedCosmetics;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	TArray<FWeaponSkinSet> WeaponSkinSets;
};

USTRUCT(BlueprintType)
struct FEmoteList
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName Emote1 = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName Emote2 = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName Emote3 = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName Emote4 = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName Emote5 = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName Emote6 = NAME_None;	
};

USTRUCT(BlueprintType)
struct FPlayerLoadoutContainer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FPlayerLoadout PlayerLoadout1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FPlayerLoadout PlayerLoadout2;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FPlayerLoadout PlayerLoadout3;
};

USTRUCT(BlueprintType)
struct FZoransPlayerData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Data|Profile")
	FPlayerProfileData ProfileData;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Data|Emotes")
	FEmoteList EmoteList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Data|Loadouts")
	FPlayerLoadoutContainer Loadouts;
};

/*

"WeaponSkinSets": [{"WeaponName": "Rifle","WeaponSkin": "GoldRush","WeaponSwatch": "Clear"}, { * }]

*/