// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Data/AbilitySystemData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayAbilitySystemFunctionLibrary.generated.h"

UCLASS()
class ZORANSRESISTANCE_API UGameplayAbilitySystemFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static void GrantAbilitySystemData(const UObject* WorldContextObject, UObject* SourceObject, const AActor* InActor, FGrantedAbilitySystemData AbilitySystemData);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static TArray<FExplosionDamageModifiers> CalculateExplosionDamageResults(const UObject* WorldContextObject, const AActor* OwningActor, const TArray<AActor*> InActors, const FVector InCenter, const float InRadius, const FRuntimeFloatCurve InFalloff, const AActor* OptionalIgnoreTraceActor = nullptr, const float SelfDamage = 0.f, const float InModifier = 1.f);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static FDamageMetricValue ApplyDamageToTargetActorGeneric(AActor* SourceActor, AActor* TargetActor, const FHitResult& HitResult, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount, const FGameplayTag DamageTag, UGameplayAbility* InSourceAbility, AActor* OptionalEffectSource, bool& bValidHit, const float ImpactForce = 1000.f);

	static float GetConeRadiusAtDistance(const float RadiusPerMeter, const float Distance);
	static FVector GetCenterOfConeAtDistance(const FVector& StartPoint, const FVector& ConeDirection, const float Distance);
	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static bool IsInsideCone(const UObject* WorldContextObject, const FVector& ConeStartingPoint, const FVector& ConeDirection, const float ConeHalfAngle, const FVector& TargetLocation, const bool bDebug = false);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static TArray<FExplosionDamageModifiers> CalculateConeExplosionDamageResults(AActor* OwningActor, const FVector& TraceStart, const FVector& TraceDirection, const float TraceLength, const float StartRadius, const float RadiusPerMeter, const FRuntimeFloatCurve& InFalloff, const bool bTraceFriendly = false, const AActor* OptionalIgnoreTraceActor = nullptr, const float SelfDamage = 0.5f, const float InModifier = 0.f, const bool bDebug = false);

	static bool IsValidTarget(AActor* SourceActor, AActor* TargetActor, const bool bTraceFriendly);
};