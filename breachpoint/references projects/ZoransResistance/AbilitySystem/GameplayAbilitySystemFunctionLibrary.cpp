// Copyright Zollpa LLC.


#include "GameplayAbilitySystemFunctionLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ZoransGameplayEffect.h"
#include "ZoransGameplayAbility.h"
#include "Attributes/ZoransAttributeSet.h"
#include "Data/ZoransGameplayTags.h"
#include "Kismet/KismetMathLibrary.h"
#include "ZoransResistance/Characters/ZoransCharacterBase.h"
#include "ZoransResistance/Interfaces/LocationMagnitudeInterface.h"
#include "ZoransResistance/Teams/TeamStateFunctionLibrary.h"
#include "ZoransResistance/Teams/Data/TeamData.h"

void UGameplayAbilitySystemFunctionLibrary::GrantAbilitySystemData(const UObject* WorldContextObject, UObject* SourceObject, const AActor* InActor, FGrantedAbilitySystemData AbilitySystemData)
{
	check(InActor);
	
	if (InActor->GetNetMode() == NM_Client)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InActor))
	{
		for (const TSubclassOf<UZoransAttributeSet>& AttributeSet : AbilitySystemData.GrantedAttributes)
		{
			UAttributeSet* NewAttributeSet = NewObject<UZoransAttributeSet>(SourceObject, AttributeSet);

			AbilitySystemComponent->AddSpawnedAttribute(NewAttributeSet);
		}
		
		for (const TSubclassOf<UZoransGameplayEffect> GameplayEffect : AbilitySystemData.GrantedGameplayEffects)
		{
			FGameplayEffectContextHandle EffectContextHandle = AbilitySystemComponent->MakeEffectContext();
		
			EffectContextHandle.AddSourceObject(SourceObject);
		
			if (const FGameplayEffectSpecHandle GameplayEffectSpecHandle = AbilitySystemComponent->MakeOutgoingSpec(GameplayEffect, 1, EffectContextHandle); GameplayEffectSpecHandle.IsValid())
			{
				GameplayEffectSpecHandle.Data->AddDynamicAssetTag(GameplayTag_CharacterAbility);
				AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*GameplayEffectSpecHandle.Data.Get());
			}
		}
		
		for (const TSubclassOf<UZoransGameplayAbility>& GameplayAbility : AbilitySystemData.GrantedAbilities)
		{
			FGameplayAbilitySpec AbilitySpec = AbilitySystemComponent->BuildAbilitySpecFromClass(GameplayAbility);
			AbilitySpec.Ability->AbilityTags.AddTag(GameplayTag_CharacterAbility);
			AbilitySystemComponent->GiveAbility(AbilitySpec);
		}
		
		if (const TSubclassOf<UZoransGameplayAbility>& MobilityAction = AbilitySystemData.MobilityAction)
		{
			FGameplayAbilitySpec AbilitySpec = AbilitySystemComponent->BuildAbilitySpecFromClass(MobilityAction);
			AbilitySpec.Ability->AbilityTags.AddTag(GameplayTag_CharacterAbility);
			AbilitySystemComponent->GiveAbility(AbilitySpec);
		}
		
		if (const TSubclassOf<UZoransGameplayAbility>& OffensiveAction = AbilitySystemData.OffensiveAction)
		{
			FGameplayAbilitySpec AbilitySpec = AbilitySystemComponent->BuildAbilitySpecFromClass(OffensiveAction);
			AbilitySpec.Ability->AbilityTags.AddTag(GameplayTag_CharacterAbility);
			AbilitySystemComponent->GiveAbility(AbilitySpec);
		}
		
		if (const TSubclassOf<UZoransGameplayAbility>& JumpAction = AbilitySystemData.JumpAction)
		{
			FGameplayAbilitySpec AbilitySpec = AbilitySystemComponent->BuildAbilitySpecFromClass(JumpAction);
			AbilitySpec.Ability->AbilityTags.AddTag(GameplayTag_CharacterAbility);
			AbilitySystemComponent->GiveAbility(AbilitySpec);
		}
		
		if (const TSubclassOf<UZoransGameplayAbility>& ClassAbility1 = AbilitySystemData.ClassAbility1)
		{
			FGameplayAbilitySpec AbilitySpec = AbilitySystemComponent->BuildAbilitySpecFromClass(ClassAbility1);
			AbilitySpec.Ability->AbilityTags.AddTag(GameplayTag_CharacterAbility);
			AbilitySystemComponent->GiveAbility(AbilitySpec);
		}
		
		if (const TSubclassOf<UZoransGameplayAbility>& ClassAbility2 = AbilitySystemData.ClassAbility2)
		{
			FGameplayAbilitySpec AbilitySpec = AbilitySystemComponent->BuildAbilitySpecFromClass(ClassAbility2);
			AbilitySpec.Ability->AbilityTags.AddTag(GameplayTag_CharacterAbility);
			AbilitySystemComponent->GiveAbility(AbilitySpec);
		}
		
		if (const TSubclassOf<UZoransGameplayAbility>& ClassAbility3 = AbilitySystemData.ClassAbility3)
		{
			FGameplayAbilitySpec AbilitySpec = AbilitySystemComponent->BuildAbilitySpecFromClass(ClassAbility3);
			AbilitySpec.Ability->AbilityTags.AddTag(GameplayTag_CharacterAbility);
			AbilitySystemComponent->GiveAbility(AbilitySpec);
		}
		
		if (const TSubclassOf<UZoransGameplayAbility>& ClassAbility4 = AbilitySystemData.ClassAbility4)
		{
			FGameplayAbilitySpec AbilitySpec = AbilitySystemComponent->BuildAbilitySpecFromClass(ClassAbility4);
			AbilitySpec.Ability->AbilityTags.AddTag(GameplayTag_CharacterAbility);
			AbilitySystemComponent->GiveAbility(AbilitySpec);
		}
	}
}

TArray<FExplosionDamageModifiers> UGameplayAbilitySystemFunctionLibrary::CalculateExplosionDamageResults(
	const UObject* WorldContextObject, const AActor* OwningActor, const TArray<AActor*> InActors, const FVector InCenter, const float InRadius,
	const FRuntimeFloatCurve InFalloff, const AActor* OptionalIgnoreTraceActor, const float SelfDamage, const float InModifier)
{
	TArray<FExplosionDamageModifiers> OutHits;

	FCollisionObjectQueryParams ObjQueryParams;
	ObjQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	
	FCollisionQueryParams QueryParams;
	if (IsValid(OwningActor) && OwningActor->GetClass()->ImplementsInterface(ULocationMagnitudeInterface::StaticClass()))
	{
		QueryParams.AddIgnoredActor(OwningActor);
	}
	if (IsValid(OptionalIgnoreTraceActor))
	{
		QueryParams.AddIgnoredActor(OptionalIgnoreTraceActor);
	}

	const float CoreOffsetDistance = InRadius * 0.33f;

	for (auto x : InActors)
	{
		if (IsValid(x))
		{
			float TotalMod = 0.f;
			if (AZoransCharacterBase* ZChar = Cast<AZoransCharacterBase>(x))
			{
				const FVector CharOrigin = ZChar->GetActorLocation();
				if (USkeletalMeshComponent* CharMesh = ZChar->GetMesh())
				{
					for (auto y : ZChar->GetBoneSections())
					{
						const FVector PointLocation = CharMesh->GetSocketLocation(y);
						const float PointOffsetDistance = FMath::Clamp<float>(FVector::Distance(CharOrigin, PointLocation), 0.f, CoreOffsetDistance);
						const FVector PointOffsetDirection = (PointLocation - CharOrigin).GetSafeNormal();
						const FVector CoreOffset = PointOffsetDirection * PointOffsetDistance;
						const FVector OffsetLocation = InCenter + CoreOffset;
						FHitResult OffsetHit;
						//DrawDebugLine(WorldContextObject->GetWorld(), InCenter, OffsetLocation, FColor::Yellow, false, 2.f, 0, 3.f);
						const bool bOffsetBlocked = WorldContextObject->GetWorld()->LineTraceSingleByObjectType(OffsetHit, InCenter, OffsetLocation, ObjQueryParams, QueryParams);
						
						const FVector TraceStart = bOffsetBlocked ? InCenter : OffsetLocation;
						FHitResult TestHit;
						//DrawDebugLine(WorldContextObject->GetWorld(), TraceStart, PointLocation, bOffsetBlocked ? FColor::Orange : FColor::Blue, false, 2.f, 0, 3.f);
						if (!WorldContextObject->GetWorld()->LineTraceSingleByObjectType(TestHit, TraceStart, PointLocation, ObjQueryParams, QueryParams))
						{
							const float DistanceToPoint = FVector::Distance(InCenter, PointLocation);
							if (DistanceToPoint <= InRadius)
							{
								const float DistanceAlpha = UKismetMathLibrary::SafeDivide(DistanceToPoint, InRadius);
								const float Falloff = InFalloff.EditorCurveData.Eval(DistanceAlpha);
								const float SectionMod = ZChar->GetDamageModifierFromSection(y).DamageModifier;
								TotalMod += Falloff * SectionMod * InModifier;
							}
						}
					}
				}
			}
			else
			{
				const FVector ActorLocation = x->GetActorLocation();
				FVector HitActorOrigin;
				FVector HitActorBounds;
				x->GetActorBounds(true, HitActorOrigin, HitActorBounds, false);
				const float BoundsOffset = HitActorBounds.X;
				FCollisionObjectQueryParams DynamicObjQueryParams;
				DynamicObjQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
				
				FHitResult TestHit;
				//DrawDebugLine(x->GetWorld(), InCenter, ActorLocation, FColor::Purple, false, 2.f, 0, 3.f);
				if (!WorldContextObject->GetWorld()->LineTraceSingleByObjectType(TestHit, InCenter, ActorLocation, DynamicObjQueryParams, QueryParams))
				{
					const float DistanceToPoint = FVector::Distance(InCenter, ActorLocation) - BoundsOffset;
					const float DistanceAlpha = UKismetMathLibrary::SafeDivide(DistanceToPoint, InRadius);
					const float Falloff = InFalloff.EditorCurveData.Eval(DistanceAlpha);
					TotalMod += Falloff * InModifier;
				}
				else
				{
					//DrawDebugSphere(x->GetWorld(), TestHit.Location, 32.f, 6, FColor::Red, false, 3.f, 0, 2.f);
				}
			}

			if (TotalMod > 0.f)
			{				
				FExplosionDamageModifiers ExplosionMod;
				ExplosionMod.HitActor = x;

				if (SelfDamage > 0.f && IsValid(OwningActor) && ExplosionMod.HitActor == OwningActor)
				{
					TotalMod *= SelfDamage;
				}
				
				ExplosionMod.TotalModifier = TotalMod;
				OutHits.Add(ExplosionMod);
			}
		}
	}
	
	return OutHits;
}

FDamageMetricValue UGameplayAbilitySystemFunctionLibrary::ApplyDamageToTargetActorGeneric(AActor* SourceActor, AActor* TargetActor,
	const FHitResult& HitResult, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount,
	const FGameplayTag DamageTag, UGameplayAbility* InSourceAbility, AActor* OptionalEffectSource, bool& bValidHit, const float ImpactForce)
{
	if (SourceActor && TargetActor)
	{
		if (TargetActor->GetClass()->ImplementsInterface(ULocationMagnitudeInterface::StaticClass()))
		{
			ETeamComparisonResult TeamComparisonResult = ETeamComparisonResult::Invalid;
			UTeamStateFunctionLibrary::GetTeamDispositionFromActors(SourceActor, SourceActor, TargetActor, TeamComparisonResult);
			if (TeamComparisonResult == ETeamComparisonResult::Hostile)
			{
				FLocationMagnitudeEvent LocationMagnitudeEvent;
				LocationMagnitudeEvent.SourceActor = SourceActor;
				LocationMagnitudeEvent.HitComponent = HitResult.GetComponent();
				LocationMagnitudeEvent.RelativeHitLocation = HitResult.Location;
				LocationMagnitudeEvent.WorldHitLocation = HitResult.Location;
				LocationMagnitudeEvent.EventMagnitude = DamageAmount;
			
				ILocationMagnitudeInterface::Execute_LocationMagnitudeEvent(TargetActor, LocationMagnitudeEvent);
				bValidHit = true;
			}
		}

		if (IsValid(DamageEffectClass))
		{
			if (UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(SourceActor))
			{
				if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
				{
					FGameplayEffectContextHandle EffectContextHandle = SourceASC->MakeEffectContext();
					EffectContextHandle.AddInstigator(SourceActor, OptionalEffectSource);
					EffectContextHandle.AddSourceObject(InSourceAbility);
					EffectContextHandle.AddHitResult(HitResult);

					const FGameplayEffectSpecHandle EffectSpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, ImpactForce, EffectContextHandle);
			
					EffectSpecHandle.Data->SetSetByCallerMagnitude(GameplayTag_Damage.GetTag(), DamageAmount);
					EffectSpecHandle.Data->AddDynamicAssetTag(DamageTag);

					SourceASC->ApplyGameplayEffectSpecToTarget(*EffectSpecHandle.Data.Get(), TargetASC);
					bValidHit = true;
				}
			}
		}
	}

	FDamageMetricValue DamageMetric = FDamageMetricValue();
	DamageMetric.Damage += DamageAmount;
	return DamageMetric;
}

float UGameplayAbilitySystemFunctionLibrary::GetConeRadiusAtDistance(const float RadiusPerMeter, const float Distance)
{
	return RadiusPerMeter * (Distance * 0.01f);
}

FVector UGameplayAbilitySystemFunctionLibrary::GetCenterOfConeAtDistance(const FVector& StartPoint,
	const FVector& ConeDirection, const float Distance)
{
	return StartPoint + ConeDirection * Distance;
}

bool UGameplayAbilitySystemFunctionLibrary::IsInsideCone(const UObject* WorldContextObject, const FVector& ConeStartingPoint,
	const FVector& ConeDirection, const float ConeHalfAngle, const FVector& TargetLocation,	const bool bDebug)
{
	if (bDebug && IsValid(WorldContextObject))
	{
		DrawDebugLine(WorldContextObject->GetWorld(), TargetLocation, TargetLocation, FColor::Red, false, 2.f, 0, 2.f);
	}
		
	const FVector TargetDirection = (TargetLocation - ConeStartingPoint).GetSafeNormal();
	const float AngleDot = FVector::DotProduct(ConeDirection, TargetDirection);
	const float AngleRadians = FMath::Acos(AngleDot);
	const float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);
	
	return AngleDegrees <= ConeHalfAngle;
}

TArray<FExplosionDamageModifiers> UGameplayAbilitySystemFunctionLibrary::CalculateConeExplosionDamageResults(AActor* OwningActor, const FVector& TraceStart,
	const FVector& TraceDirection, const float TraceLength, const float StartRadius, const float RadiusPerMeter, const FRuntimeFloatCurve& InFalloff, const bool bTraceFriendly,
	const AActor* OptionalIgnoreTraceActor, const float SelfDamage, const float InModifier, const bool bDebug)
{
	TArray<FExplosionDamageModifiers> DamageModifiers;
	if (!IsValid(OwningActor)) return DamageModifiers;
	
	const FVector TraceCenter = TraceStart + TraceDirection * (TraceLength * 0.5f);
	const FVector TraceEnd = TraceStart + TraceDirection * TraceLength;
	
	float EndRadius = RadiusPerMeter * (TraceLength * 0.01f);
	
	if (EndRadius <= StartRadius + 5.f)
	{
		EndRadius = StartRadius + 10.f;
	}

	const float OriginalTraceLength = FVector::Distance(TraceStart, TraceEnd);
	const float BackTraceLength = (StartRadius * OriginalTraceLength) / (EndRadius - StartRadius);
	const float OppositeByAdjacent = EndRadius / (OriginalTraceLength + BackTraceLength);
	const float ConeRadians = FMath::Atan(OppositeByAdjacent);

	const FVector ConeDirection = (TraceEnd - TraceStart).GetSafeNormal();
	const float ConeInAngle = FMath::RadiansToDegrees(ConeRadians);
	const FVector ConeStart = TraceStart + ConeDirection * -BackTraceLength;

	FCollisionQueryParams ConeParams(SCENE_QUERY_STAT(MakeTargetDataFromConeTraceData), false);
	ConeParams.bReturnPhysicalMaterial = false;
	if (IsValid(OptionalIgnoreTraceActor))
	{
		ConeParams.AddIgnoredActor(OptionalIgnoreTraceActor);
	}
	
	FCollisionObjectQueryParams ConeObjParams;
	ConeObjParams.AddObjectTypesToQuery(ECC_Pawn);
	ConeObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
					
	TArray<FOverlapResult> SphereOverlaps;
	OwningActor->GetWorld()->OverlapMultiByObjectType(SphereOverlaps, TraceCenter, FQuat::Identity, ConeObjParams, FCollisionShape::MakeSphere(TraceLength * 0.5f), ConeParams);

	TArray<AActor*> FilteredOverlaps;
	for (const auto &Overlap : SphereOverlaps)
	{
		if (IsValid(Overlap.GetActor()))
		{
			FilteredOverlaps.AddUnique(Overlap.GetActor());
		}
	}

	if (bDebug)
	{
		DrawDebugSphere(OwningActor->GetWorld(), TraceCenter, TraceLength * 0.5f, 16, FColor::Blue, false, 2.f, 0, 3.f);
		DrawDebugCone(OwningActor->GetWorld(), ConeStart, ConeDirection, OriginalTraceLength + BackTraceLength, ConeRadians, ConeRadians, 16, FColor::Yellow, false, 2.f, 0, 2.f);
	}

	for (const auto &x : FilteredOverlaps)
	{
		if (IsValid(x) && IsValidTarget(OwningActor, x, bTraceFriendly))
		{
			if (AZoransCharacterBase* HitCharacter = Cast<AZoransCharacterBase>(x))
			{
				const FVector CharOrigin = HitCharacter->GetActorLocation();
				if (USkeletalMeshComponent* CharMesh = HitCharacter->GetMesh())
				{
					float TotalMod = 0.f;
					for (const auto &y : HitCharacter->GetBoneSections())
					{
						const FVector PointLocation = CharMesh->GetSocketLocation(y);
						if (IsInsideCone(OwningActor, ConeStart, ConeDirection, ConeInAngle, PointLocation, bDebug))
						{
							const float CoreOffsetDistance = GetConeRadiusAtDistance(RadiusPerMeter, FVector::Distance(HitCharacter->GetActorLocation(), TraceStart)) * 0.25f;
							const float PointOffsetDistance = FMath::Clamp<float>(FVector::Distance(CharOrigin, PointLocation), 0.f, CoreOffsetDistance);
							const FVector PointOffsetDirection = (PointLocation - CharOrigin).GetSafeNormal();
							const FVector CoreOffset = PointOffsetDirection * PointOffsetDistance;
							const FVector OffsetLocation = TraceStart + CoreOffset;

							FCollisionQueryParams QueryParams;
							QueryParams.AddIgnoredActor(OptionalIgnoreTraceActor);
							
							FCollisionObjectQueryParams ObjQueryParams;
							ObjQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
							ObjQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

							FHitResult OffsetHit;
							const bool bOffsetBlocked = OwningActor->GetWorld()->LineTraceSingleByObjectType(OffsetHit, TraceStart, OffsetLocation, ObjQueryParams, QueryParams);
						
							const FVector SightTraceStart = bOffsetBlocked ? TraceStart : OffsetLocation;
							FHitResult TestHit;
							if (!OwningActor->GetWorld()->LineTraceSingleByObjectType(TestHit, SightTraceStart, PointLocation, ObjQueryParams, QueryParams))
							{
								const float DistanceToPoint = FVector::Distance(TraceStart, PointLocation);
								if (DistanceToPoint <= TraceLength)
								{
									const float DistanceAlpha = UKismetMathLibrary::SafeDivide(DistanceToPoint, TraceLength);
									const float Falloff = InFalloff.EditorCurveData.Eval(DistanceAlpha);
									const float SectionMod = HitCharacter->GetDamageModifierFromSection(y).DamageModifier;
									TotalMod += Falloff * SectionMod * InModifier;
								}
							}
						}
					}
					if (TotalMod > 0.f)
					{				
						FExplosionDamageModifiers ExplosionMod;
						ExplosionMod.HitActor = HitCharacter;

						if (SelfDamage > 0.f && ExplosionMod.HitActor == OwningActor)
						{
							TotalMod *= SelfDamage;
						}
				
						ExplosionMod.TotalModifier = TotalMod;
						DamageModifiers.Add(ExplosionMod);
					}
				}
			}
		}
	}
	return DamageModifiers;
}

bool UGameplayAbilitySystemFunctionLibrary::IsValidTarget(AActor* SourceActor, AActor* TargetActor, const bool bTraceFriendly)
{
	if (IsValid(SourceActor) && IsValid(TargetActor))
	{
		if (TargetActor->GetClass()->ImplementsInterface(ULocationMagnitudeInterface::StaticClass()))
		{
			return true;
		}
		
		ETeamComparisonResult TeamComparisonResult = ETeamComparisonResult::Invalid;
		UTeamStateFunctionLibrary::GetTeamDispositionFromActors(SourceActor, SourceActor, TargetActor, TeamComparisonResult);
		switch (TeamComparisonResult)
		{
		case ETeamComparisonResult::Invalid:
			return false;
		case ETeamComparisonResult::Hostile:
			if (!bTraceFriendly) return true;
		case ETeamComparisonResult::Friendly:
			if (bTraceFriendly) return true;
		}
	}
	
	return false;
}