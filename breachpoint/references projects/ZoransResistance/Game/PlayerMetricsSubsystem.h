// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "ZoransResistance/Game/Data/PlayerMetrics.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/OnlineStatsInterface.h"
#include "ZoransResistance/Player/ZoransPlayerState.h"
#include "PlayerMetricsSubsystem.generated.h"

class UPlayFabJsonObject;
class AZoransPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnWeaponMetricRegistered, AZoransPlayerState*, Player, const FName, WeaponName, EWeaponMetric, Metric, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnAbilityMetricRegistered, AZoransPlayerState*, Player, const FName, WeaponName, EAbilityMetric, Metric, float, Value);

UCLASS()
class ZORANSRESISTANCE_API UPlayerMetricsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player Metrics|Weapon")
	void RegisterWeaponMetrics(AZoransPlayerState* Player, const FName WeaponName, const TMap<EWeaponMetric, float>& WeaponMetricsIn);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player Metrics|Ability")
	void RegisterAbilityMetrics(AZoransPlayerState* Player, const FName AbilityName, const TMap<EAbilityMetric, float>& AbilityMetricsIn);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player Metrics")
	const FPlayerMetrics& GetPlayerMetrics(const AZoransPlayerState* Player);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Metrics")
	static bool IsMetricAvailableForWeapon(const FName& WeaponName, const EWeaponMetric Metric);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Metrics")
	static bool IsMetricAvailableForAbility(const FName& AbilityName, const EAbilityMetric Metric);

	UFUNCTION(BlueprintCallable, BlueprintPure, BlueprintAuthorityOnly, meta=(WorldContext=WorldContextObject), Category = "Player Metrics")
	UPlayFabJsonObject* PlayerMetricToJson(UObject* WorldContextObject, const AZoransPlayerState* Player, TMap<FString, int32>& OutStats);
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, meta=(WorldContext=WorldContextObject), Category = "Player Metrics")
	FPlayerMetrics PlayerMetricFromJson(const UPlayFabJsonObject* Object);

	UPROPERTY(BlueprintAssignable)
	FOnWeaponMetricRegistered OnWeaponMetricRegistered;

	UPROPERTY(BlueprintAssignable)
	FOnAbilityMetricRegistered OnAbilityMetricRegistered;
	
private:
	UPROPERTY()
	TMap<AZoransPlayerState*, FPlayerMetrics> PlayerMetrics;

	UPROPERTY()
	FPlayerMetrics EmptyMetrics;

	static UPlayFabJsonObject* WeaponMetricToJson(UObject* WorldContextObject, const FWeaponMetrics& Metrics);
	static UPlayFabJsonObject* AbilityMetricToJson(UObject* WorldContextObject, const FAbilityMetrics& Metrics);
	
	static FWeaponMetrics WeaponMetricFromJson(UPlayFabJsonObject* Object);
	static FAbilityMetrics AbilityMetricFromJson(UPlayFabJsonObject* Object);
};
