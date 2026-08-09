// Copyright Zollpa LLC


#include "PlayerMetricsSubsystem.h"
#include "PlayFabJsonHelpers.h"
#include "PlayFabJsonObject.h"
#include "ZoransResistance/Player/ZoransPlayerState.h"

static TSet<EWeaponMetric> StandardWeaponMetrics = {EWeaponMetric::Shots, EWeaponMetric::Damage, EWeaponMetric::WeakPoint, EWeaponMetric::Hits, EWeaponMetric::Kills};

static TMap<FName, TSet<EWeaponMetric>> AvailableMetricsForWeapon =
{
	{FName("Pistol"), StandardWeaponMetrics},
	{FName("Rifle"), StandardWeaponMetrics},
	{FName("Sniper"), StandardWeaponMetrics},
	{FName("Shotgun"), StandardWeaponMetrics},
	{FName("NanoSwarm"), StandardWeaponMetrics},
	{FName("GrenadeLauncher"), StandardWeaponMetrics},
	{FName("RocketLauncher"), StandardWeaponMetrics},
	{FName("Melee1"), StandardWeaponMetrics},
	{FName("Melee2"), StandardWeaponMetrics},
	{FName("Melee3"), StandardWeaponMetrics}
};

static TMap<FName, TSet<EAbilityMetric>> AvailableMetricsForAbility =
{
	{FName("Shield"), {EAbilityMetric::Activations, EAbilityMetric::Duration, EAbilityMetric::Mitigation}},
	{FName("ChargeDash"), {EAbilityMetric::Activations}},
	{FName("Grenade"), {EAbilityMetric::Activations, EAbilityMetric::Damage, EAbilityMetric::Kills}},
	{FName("AerialAttack"), {EAbilityMetric::Activations, EAbilityMetric::Kills}},
	{FName("ProximityMine"), {EAbilityMetric::Activations, EAbilityMetric::Kills}},
	{FName("Dash"), {EAbilityMetric::Activations}},
	{FName("DoubleJump"), {EAbilityMetric::Activations}},
	{FName("Throwdown_BeamAttack"), {EAbilityMetric::Activations, EAbilityMetric::Damage, EAbilityMetric::Kills}},
	{FName("Throwdown_BallisticProjectile"), {EAbilityMetric::Activations, EAbilityMetric::Damage, EAbilityMetric::Kills}},
	{FName("Throwdown_Cannonball"), {EAbilityMetric::Activations, EAbilityMetric::Damage, EAbilityMetric::Kills}}
};

const FPlayerMetrics& UPlayerMetricsSubsystem::GetPlayerMetrics(const AZoransPlayerState* Player)
{
	if (PlayerMetrics.Contains(Player))
	{
		return PlayerMetrics[Player];
	}
	
	return EmptyMetrics;
}

bool UPlayerMetricsSubsystem::IsMetricAvailableForWeapon(const FName& WeaponName, const EWeaponMetric Metric)
{
	if(AvailableMetricsForWeapon.Contains(WeaponName))
	{
		return AvailableMetricsForWeapon[WeaponName].Contains(Metric);
	}
	return false;
}

bool UPlayerMetricsSubsystem::IsMetricAvailableForAbility(const FName& AbilityName, const EAbilityMetric Metric)
{
	if(AvailableMetricsForAbility.Contains(AbilityName))
	{
		return AvailableMetricsForAbility[AbilityName].Contains(Metric);
	}
	return false;
}

void UPlayerMetricsSubsystem::RegisterWeaponMetrics(AZoransPlayerState* Player, const FName WeaponName, const TMap<EWeaponMetric, float>& WeaponMetricsIn)
{
	if(!IsValid(Player) || Player->IsABot()) return;
	
	if (!WeaponMetricsIn.IsEmpty() && WeaponName != NAME_None)
	{
		FPlayerMetrics& PlayerMetric = PlayerMetrics.FindOrAdd(Player);
		auto& [WeaponMetric] = PlayerMetric.WeaponMetrics.FindOrAdd(WeaponName);
		
		for (const auto& WeaponMetricEntry : WeaponMetricsIn) 
		{
			const float CurrentValue = WeaponMetric.FindRef(WeaponMetricEntry.Key);
			WeaponMetric.Add(WeaponMetricEntry.Key, WeaponMetricEntry.Value + CurrentValue);
			OnWeaponMetricRegistered.Broadcast(Player, WeaponName, WeaponMetricEntry.Key, WeaponMetricEntry.Value);
		}
	}
}

void UPlayerMetricsSubsystem::RegisterAbilityMetrics(AZoransPlayerState* Player, const FName AbilityName, const TMap<EAbilityMetric, float>& AbilityMetricsIn)
{
	if(!IsValid(Player) || Player->IsABot()) return;
	
	if (!AbilityMetricsIn.IsEmpty() && AbilityName != NAME_None)
	{
		FPlayerMetrics& PlayerMetric = PlayerMetrics.FindOrAdd(Player);
		auto& [AbilityMetric] = PlayerMetric.AbilityMetrics.FindOrAdd(AbilityName);
		for (const auto& AbilityMetricEntry : AbilityMetricsIn)
		{
			const float CurrentValue = AbilityMetric.FindRef(AbilityMetricEntry.Key);
			AbilityMetric.Add(AbilityMetricEntry.Key, AbilityMetricEntry.Value + CurrentValue);
			OnAbilityMetricRegistered.Broadcast(Player, AbilityName, AbilityMetricEntry.Key, AbilityMetricEntry.Value);
		}
	}
}

#define WEAPON_METRICS_FIELD_NAME "WeaponMetrics"
#define ABILITY_METRICS_FIELD_NAME "AbilityMetrics"

UPlayFabJsonObject* UPlayerMetricsSubsystem::WeaponMetricToJson(UObject* WorldContextObject, const FWeaponMetrics& Metrics)
{
	UPlayFabJsonObject* Object = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
	for (const auto& MetricEntry : Metrics.Metrics)
	{
		Object->SetNumberField(UEnum::GetValueAsName<EWeaponMetric>(MetricEntry.Key).ToString().RightChop(15), MetricEntry.Value);
	}
	return Object;
}

UPlayFabJsonObject* UPlayerMetricsSubsystem::AbilityMetricToJson(UObject* WorldContextObject, const FAbilityMetrics& Metrics)
{
	UPlayFabJsonObject* Object = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
	for (const auto& MetricEntry : Metrics.Metrics)
	{
		Object->SetNumberField(UEnum::GetValueAsName<EAbilityMetric>(MetricEntry.Key).ToString().RightChop(16), MetricEntry.Value);
	}
	return Object;
}

UPlayFabJsonObject* UPlayerMetricsSubsystem::PlayerMetricToJson(UObject* WorldContextObject, const AZoransPlayerState* Player, TMap<FString, int32>& OutStats)
{
	FPlayerMetrics& Metrics = PlayerMetrics.Contains(Player) ? PlayerMetrics[Player] : EmptyMetrics;
	UPlayFabJsonObject* PlayerObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
	
	UPlayFabJsonObject* WeaponObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
	for (const auto& WeaponMetricEntry : Metrics.WeaponMetrics)
	{
		WeaponObject->SetObjectField(WeaponMetricEntry.Key.ToString(), WeaponMetricToJson(WorldContextObject, WeaponMetricEntry.Value));

		for (const auto& WeaponStats : WeaponMetricEntry.Value.Metrics)
		{
			const int32 WeaponStatValue = FMath::RoundToInt32(WeaponStats.Value);
			if (WeaponStatValue > 0)
			{
				const FString WeaponStatString = "Weapon_" + WeaponMetricEntry.Key.ToString() + "_" + UEnum::GetValueAsName<EWeaponMetric>(WeaponStats.Key).ToString().RightChop(15);
				OutStats.Add(WeaponStatString, WeaponStatValue);
			}
		}
	}
	
	UPlayFabJsonObject* AbilityObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
	for (const auto& AbilityMetricEntry : Metrics.AbilityMetrics)
	{
		AbilityObject->SetObjectField(AbilityMetricEntry.Key.ToString(), AbilityMetricToJson(WorldContextObject, AbilityMetricEntry.Value));

		for (const auto& AbilityStats : AbilityMetricEntry.Value.Metrics)
		{
			const int32 AbilityStatValue = FMath::RoundToInt32(AbilityStats.Value);
			if (AbilityStatValue > 0)
			{
				const FString AbilityStatString = "Ability_" + AbilityMetricEntry.Key.ToString() + "_" + UEnum::GetValueAsName<EAbilityMetric>(AbilityStats.Key).ToString().RightChop(16);
				OutStats.Add(AbilityStatString, AbilityStatValue);
			}
		}
	}

	PlayerObject->SetObjectField(WEAPON_METRICS_FIELD_NAME, WeaponObject);
	PlayerObject->SetObjectField(ABILITY_METRICS_FIELD_NAME, AbilityObject);

	return PlayerObject;
}

FWeaponMetrics UPlayerMetricsSubsystem::WeaponMetricFromJson(UPlayFabJsonObject* Object)
{
	FWeaponMetrics Metrics;
	for (const FString& FieldName : Object->GetFieldNames())
	{
		const UEnum* WeaponMetric = StaticEnum<EWeaponMetric>();
		Metrics.Metrics.Add(static_cast<EWeaponMetric>(WeaponMetric->GetIndexByNameString(FieldName)), Object->GetNumberField(FieldName));
	}
	return Metrics;
}

FAbilityMetrics UPlayerMetricsSubsystem::AbilityMetricFromJson(UPlayFabJsonObject* Object)
{
	FAbilityMetrics Metrics;
	for (const FString& FieldName : Object->GetFieldNames())
	{
		const UEnum* WeaponMetric = StaticEnum<EAbilityMetric>();
		Metrics.Metrics.Add(static_cast<EAbilityMetric>(WeaponMetric->GetIndexByNameString(FieldName)), Object->GetNumberField(FieldName));
	}
	return Metrics;
}

FPlayerMetrics UPlayerMetricsSubsystem::PlayerMetricFromJson(const UPlayFabJsonObject* Object)
{
	FPlayerMetrics PlayerMetricsOut;
	
	UPlayFabJsonObject* WeaponObject = Object->GetObjectField(WEAPON_METRICS_FIELD_NAME);
	for (const FString& FieldName : WeaponObject->GetFieldNames())
	{
		FWeaponMetrics WeaponMetrics = WeaponMetricFromJson(WeaponObject->GetObjectField(FieldName));
		PlayerMetricsOut.WeaponMetrics.Add(FName(*FieldName), WeaponMetrics);
	}
	
	UPlayFabJsonObject* AbilityObject = Object->GetObjectField(ABILITY_METRICS_FIELD_NAME);
	for (const FString& FieldName : AbilityObject->GetFieldNames())
	{
		FAbilityMetrics AbilityMetrics = AbilityMetricFromJson(AbilityObject->GetObjectField(FieldName));
		PlayerMetricsOut.AbilityMetrics.Add(FName(*FieldName), AbilityMetrics);
	}
	
	return PlayerMetricsOut;
}
