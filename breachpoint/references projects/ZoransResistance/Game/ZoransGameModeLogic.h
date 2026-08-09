// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ZoransGameModeLogic.generated.h"

enum class EGamePhase : uint8;
class AZoransGameState;
class AZoransPlayerState;

UENUM(BlueprintType)
enum class EWeaponPickupOverride : uint8
{
	None				UMETA(DisplayName = "None"),
	ReplaceWithAmmo		UMETA(DisplayName = "Replace With Ammo")
	
};

USTRUCT(BlueprintType)
struct FGameModeSettings
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	bool bKillsCountAsObjective = true;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	bool bFreeForAll = false;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	bool bHasCustomLoadout = false;
};

UCLASS(Blueprintable, BlueprintType)
class ZORANSRESISTANCE_API UZoransGameModeLogic : public UActorComponent
{
	GENERATED_BODY()

public:
	
	UPROPERTY(BlueprintReadOnly)
	AZoransGameState* ZoransGameState;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FGameModeSettings Settings;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	EWeaponPickupOverride WeaponPickupOverride = EWeaponPickupOverride::None;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnKill(AZoransPlayerState* KillerPlayerState, AZoransPlayerState* KilledPlayerState, FTransform KilledTransform, AActor* DeathCauseActor);

	/** 
	 * Initialize the loadout of the specified player. Does not run during warmup or if HasCustomLoadout is false.
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void InitLoadout(const AZoransPlayerState* ZoransPlayerState, FName& FirstWeapon, FName& SecondWeapon);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnGamePhaseChange(EGamePhase NewPhase);
};
