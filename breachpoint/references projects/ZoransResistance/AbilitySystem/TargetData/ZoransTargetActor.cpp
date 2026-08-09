// Copyright 2022 Zollpa LLC.


#include "ZoransResistance/AbilitySystem/TargetData/ZoransTargetActor.h"
#include "ZoransResistance/AbilitySystem/ZoransAbilitySystemComponent.h"


AZoransTargetActor::AZoransTargetActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	ShouldProduceTargetDataOnServer = false;
	bDestroyOnConfirmation = false;
	bReplicates = false;
}

void AZoransTargetActor::BeginPlay()
{
	Super::BeginPlay();

}

void AZoransTargetActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//ABILITY_LOG(Warning, TEXT("TargetActor Is Being Destroyed :: %s"), *GetName());

	Super::EndPlay(EndPlayReason);
}

void AZoransTargetActor::StartTargeting(UGameplayAbility* InAbility)
{
	OwningAbility = InAbility;
	SourceActor = InAbility->GetCurrentActorInfo()->AvatarActor.Get();

	if (SourceActor)
	{

	}
}

void AZoransTargetActor::CancelTargeting()
{
	const FGameplayAbilityActorInfo* ActorInfo = (OwningAbility ? OwningAbility->GetCurrentActorInfo() : nullptr);
	UAbilitySystemComponent* ASC = (ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr);
	if (ASC)
	{
		ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::GenericCancel, OwningAbility->GetCurrentAbilitySpecHandle(), OwningAbility->GetCurrentActivationInfo().GetActivationPredictionKey()).Remove(GenericCancelHandle);
	}

	CanceledDelegate.Broadcast(FGameplayAbilityTargetDataHandle());
}

void AZoransTargetActor::ConfirmTargeting()
{
	Super::ConfirmTargeting();

}

//this is where we tell it what trace params to use
void AZoransTargetActor::ConfirmTargetingAndContinue()
{
	check(ShouldProduceTargetData());
	if (SourceActor)
	{
		switch (ZoransTargetDataType)
		{
		case EZoransTargetDataType::None:
			TargetDataReadyDelegate.Broadcast(MakeTargetDataFromGenericInput());
			break;
		case EZoransTargetDataType::Hitscan:
			TargetDataReadyDelegate.Broadcast(MakeTargetDataFromHitscanArray());
			break;
		case EZoransTargetDataType::Projectile_Ballistic:
			TargetDataReadyDelegate.Broadcast(MakeTargetDataFromBallisticProjectileSpawnData());
			break;
		case EZoransTargetDataType::Projectile_Bouncing:
			TargetDataReadyDelegate.Broadcast(MakeTargetDataFromBouncingProjectileSpawnData());
			break;
		}
	}
}

void AZoransTargetActor::SetGenericTargetData()
{
	ZoransTargetDataType = EZoransTargetDataType::None;
}

void AZoransTargetActor::SetHitscanResults(TArray<FHitscanData> InHits)
{
	ZoransTargetDataType = EZoransTargetDataType::Hitscan;
	HitScanResults = InHits;
}

void AZoransTargetActor::SetBallisticProjectileSpawnParams(FBaseProjectileSpawnParams InBaseProjectileSpawnParams)
{
	ZoransTargetDataType = EZoransTargetDataType::Projectile_Ballistic;
	BaseProjectileSpawnParams = InBaseProjectileSpawnParams;
}

void AZoransTargetActor::SetBouncingProjectileSpawnParams(FBaseProjectileSpawnParams InBaseProjectileSpawnParams, TArray<FBouncingProjectilePathSegment> InBouncingProjectilePath)
{
	ZoransTargetDataType = EZoransTargetDataType::Projectile_Bouncing;
	BaseProjectileSpawnParams = InBaseProjectileSpawnParams;
	BouncingProjectilePath = InBouncingProjectilePath;
}

FGameplayAbilityTargetDataHandle AZoransTargetActor::MakeTargetDataFromGenericInput() const
{
	FGameplayAbilityTargetDataHandle GenericInputTargetData;

	//
	
	return GenericInputTargetData;
}

FGameplayAbilityTargetDataHandle AZoransTargetActor::MakeTargetDataFromHitscanArray() const
{
	FGameplayAbilityTargetDataHandle HitscanTargetData;

	for (auto &x : HitScanResults)
	{
		FHitscanTargetData* HitData = new FHitscanTargetData();
		HitData->HitActor = x.HitActor;
		HitData->HitComponent = x.HitComponent;
		HitData->SourceActor = x.SourceActor;
		HitData->HitBone = x.BoneName;
		HitData->RelativeHitLocation = x.RelativeHitLocation;
		HitData->SurfaceNormal = x.SurfaceNormal;
		HitData->TargetHitType = x.TargetHitType;

		HitscanTargetData.Append(HitData);
	}
	
	return HitscanTargetData;
}

FGameplayAbilityTargetDataHandle AZoransTargetActor::MakeTargetDataFromBallisticProjectileSpawnData() const
{
	FGameplayAbilityTargetDataHandle SpawnDataHandle;

	FBaseProjectileSpawnTargetData* SpawnData = new FBaseProjectileSpawnTargetData();
	SpawnData->SpawnLocation = BaseProjectileSpawnParams.SpawnPoint;
	SpawnData->SpawnDirection = BaseProjectileSpawnParams.LaunchDirection;
	SpawnData->LocalSpawnTime = BaseProjectileSpawnParams.SpawnTime;
	SpawnData->ProjectileIndex = BaseProjectileSpawnParams.LocalIndex;
	SpawnData->SpawnSocketOverride = BaseProjectileSpawnParams.SpawnSocketOverride;
	SpawnDataHandle.Add(SpawnData);

	return SpawnDataHandle;
}

FGameplayAbilityTargetDataHandle AZoransTargetActor::MakeTargetDataFromBouncingProjectileSpawnData() const
{
	FGameplayAbilityTargetDataHandle SpawnDataHandle;

	FBouncingProjectileSpawnTargetData* SpawnData = new FBouncingProjectileSpawnTargetData();
	SpawnData->SpawnLocation = BaseProjectileSpawnParams.SpawnPoint;
	SpawnData->SpawnDirection = BaseProjectileSpawnParams.LaunchDirection;
	SpawnData->LocalSpawnTime = BaseProjectileSpawnParams.SpawnTime;
	SpawnData->ProjectileIndex = BaseProjectileSpawnParams.LocalIndex;
	SpawnData->SpawnSocketOverride = BaseProjectileSpawnParams.SpawnSocketOverride;

	SpawnData->ProjectileBouncePath = BouncingProjectilePath;
	SpawnDataHandle.Add(SpawnData);

	return SpawnDataHandle;
}