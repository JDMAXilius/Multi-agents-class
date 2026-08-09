// Copyright Zollpa LLC


#include "CosmeticWeapon.h"
#include "AbilitySystemGlobals.h"
#include "Data/ActorData.h"
#include "ZoransResistance/ZoransResistance.h"
#include "ZoransResistance/AbilitySystem/Data/ZoransGameplayTags.h"
#include "ZoransResistance/Characters/ZoransCharacterBase.h"

ACosmeticWeapon::ACosmeticWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;

	Tags.AddUnique(ActorTag_CosmeticWeapon);
}

void ACosmeticWeapon::BeginPlay()
{	
	if (GetOwner())
	{
		if (AZoransCharacterBase* AttachedCharacter = Cast<AZoransCharacterBase>(GetOwner()))
		{
			OwningCharacter = AttachedCharacter;
		}
	}

	if (IsValid(GetOwningCharacter()))
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningCharacter()))
		{
			if (UZoransAbilitySystemComponent* ZASC = Cast<UZoransAbilitySystemComponent>(ASC))
			{
				OwningAbilitySystemComponent = ZASC;
			}
		}

		if (const UMotionWarpingComponent* MotionWarpingComponent = GetOwningCharacter()->GetMotionWarpingComponent())
		{
			if (const FMotionWarpingTarget* WarpTarget = MotionWarpingComponent->FindWarpTarget("Location"))
			{
				if (const USceneComponent* WarpComponent = WarpTarget->Component.Get())
				{
					if (AActor* WarpActor = WarpComponent->GetOwner())
					{
						if (AZoransCharacterBase* WarpTargetCharacter = Cast<AZoransCharacterBase>(WarpActor))
						{
							TargetCharacter = WarpTargetCharacter;
						}
					}
				}
			}
		}
	}

	if (IsValid(GetOwningAbilitySystemComponent()))
	{
		FGameplayEventMulticastDelegate CosmeticEventDelegate;
		CosmeticEventDelegate.AddUObject(this, &ACosmeticWeapon::Internal_CosmeticWeaponEvent);
		GetOwningAbilitySystemComponent()->GenericGameplayEventCallbacks.Add(GameplayTag_CosmeticWeaponEvent, CosmeticEventDelegate);
	}

	Super::BeginPlay();
}

void ACosmeticWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ACosmeticWeapon::Internal_CosmeticWeaponEvent(const FGameplayEventData* InEventData)
{
	if (InEventData != nullptr)
	{
		const float EventFloat = InEventData->EventMagnitude;
		const int32 EventInt = FMath::RoundToInt32(EventFloat);
		CosmeticWeaponEvent(EventInt);
	}
}

void ACosmeticWeapon::RemoveCosmeticWeapon()
{
	if (GetWorld())
	{
		FTimerHandle CleanupTimer;
		FTimerDelegate CleanupDelegate;
		CleanupDelegate.BindLambda([this] { Destroy(); });
		GetWorld()->GetTimerManager().SetTimer(CleanupTimer, CleanupDelegate, 2.f, false);
	}

	OnCosmeticWeaponRemoved();
}