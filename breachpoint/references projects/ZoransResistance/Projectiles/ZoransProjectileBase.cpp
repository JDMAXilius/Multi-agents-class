// Copyright 2022 Zollpa LLC.


#include "ZoransProjectileBase.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "ZoransResistance/AbilitySystem/ZoransAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Kismet/KismetMathLibrary.h"
#include "ZoransResistance/ZoransResistance.h"
#include "ZoransResistance/AbilitySystem/ZoransGameplayAbility.h"
#include "ZoransResistance/AbilitySystem/Data/ZoransGameplayTags.h"
#include "ZoransResistance/Characters/Components/ComponentInterface.h"
#include "ZoransResistance/Characters/Components/ZoransWeaponComponent.h"
#include "ZoransResistance/Interfaces/LocationMagnitudeInterface.h"
#include "ZoransResistance/InventorySystem/Weapons/ZoransWeaponActor.h"
#include "ZoransResistance/Teams/TeamStateFunctionLibrary.h"


AZoransProjectileBase::AZoransProjectileBase()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetReplicatingMovement(false);
	
	ProjectileRoot = CreateDefaultSubobject<USceneComponent>("ProjectileRoot");
	if (ProjectileRoot)
	{
		RootComponent = ProjectileRoot;
	}
}

void AZoransProjectileBase::BeginPlay()
{
	PreviousLocation = GetActorLocation();

	switch (SpawnSourceType)
	{
	case EZoransProjectileSourceType::World:
		LocalSpawnLocation = PreviousLocation;
		SetActorLocation(LocalSpawnLocation);
		break;
	case EZoransProjectileSourceType::Weapon:
		if (GetInstigator())
		{
			if (const IComponentInterface* ComponentInterface = Cast<IComponentInterface>(GetInstigator()))
			{
				if (const UZoransWeaponComponent* WeaponComponent = ComponentInterface->GetWeaponComponent())
				{
					if (AZoransWeaponActor* WeaponActor = WeaponComponent->GetCurrentEquippedWeapon())
					{
						OwningWeaponActor = WeaponActor;
						
						if (GetNetMode() < NM_Client)
						{
							PreviousLocation = SpawnPoint;
							LocalSpawnLocation = PreviousLocation;
							SetActorLocation(SpawnPoint);
						}
						else
						{
							const FTransform MuzzleTransform = WeaponActor->GetWeaponMuzzleTransform();
							PreviousLocation = MuzzleTransform.GetLocation();
							LocalSpawnLocation = PreviousLocation;
							SetActorTransform(MuzzleTransform);
						}
					}
				}
			}
		}
		break;
	case EZoransProjectileSourceType::Character:
		if (GetInstigator())
		{
			if (const ACharacter* OwningCharacter = Cast<ACharacter>(GetInstigator()))
			{
				if (OwningCharacter->GetMesh())
				{
					if (GetNetMode() < NM_Client)
					{
						PreviousLocation = SpawnPoint;
						LocalSpawnLocation = PreviousLocation;
						SetActorLocation(LocalSpawnLocation);
					}
					else
					{
						PreviousLocation = OwningCharacter->GetMesh()->GetSocketLocation(SpawnSocketOverride != NAME_None ? SpawnSocketOverride : SpawnSocketName);
						LocalSpawnLocation = PreviousLocation;
						SetActorLocation(LocalSpawnLocation);
					}
				}
			}
		}
		break;
	}
	
	if (GetWorld())
	{
		if (GetWorld()->GetGameState())
		{
			LocalSpawnTime = GetWorld()->GetGameState() ? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : -1.f;
		}
		else
		{
			LocalSpawnTime = SpawnTime;	
		}
		
		if (GetNetMode() < NM_Client)
		{
			InitializeProjectileTraceParameters();
			
			ProjectileUpdateDelegate.BindUObject(this, &AZoransProjectileBase::UpdateProjectile_Server);
			GetWorld()->GetTimerManager().SetTimerForNextTick(ProjectileUpdateDelegate);
		}
		else
		{
			ProjectileUpdateDelegate.BindUObject(this, &AZoransProjectileBase::UpdateProjectile_Client);
			GetWorld()->GetTimerManager().SetTimerForNextTick(ProjectileUpdateDelegate);
		}
	}

	Super::BeginPlay();
}

void AZoransProjectileBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetNetMode() == NM_Client)
	{
		ProjectileExpiredDelegate.ExecuteIfBound(LocalIndex);
	}
	
	Super::EndPlay(EndPlayReason);
}

bool AZoransProjectileBase::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	switch (GetNetMode())
	{
	case NM_DedicatedServer :

		if (IsOwnedBy(ViewTarget)) return false;
		
	case NM_ListenServer :

		if (LocalIndex > 0 && GetInstigator() == ViewTarget) return false;

		return true;
	}

	return false;
	//return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

bool AZoransProjectileBase::PerformProjectileTrace(FHitResult& InHit, const FVector& NewLocation)
{
	return false;
}

void AZoransProjectileBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AZoransProjectileBase, SpawnPoint, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AZoransProjectileBase, LaunchDirection, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AZoransProjectileBase, SpawnTime, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AZoransProjectileBase, SpawnSocketOverride, COND_InitialOnly);
}

void AZoransProjectileBase::UpdateProjectile_Server()
{
	const float CurrentLifetime = GetServerProjectileLifetime();

	if (CurrentLifetime >= ProjectileLifespan)
	{
		if (!bHasValidHit)
		{
			ServerProjectileExpired();
		}
		DeactivateProjectile();
	}
	
	if (bIsActive)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(ProjectileUpdateDelegate);
	}
}

void AZoransProjectileBase::UpdateProjectile_Client()
{
	const float CurrentLifetime = GetServerProjectileLifetime();

	if (CurrentLifetime >= ProjectileLifespan)
	{
		if (!bHasValidHit)
		{
			ClientProjectileExpired();
		}
		DeactivateProjectile();
	}
	
	if (bIsActive)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(ProjectileUpdateDelegate);
	}
}

void AZoransProjectileBase::DeactivateProjectile()
{
	bIsActive = false;
		
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(DestroyProjectileTimer, this, &AZoransProjectileBase::DestroyProjectile, 2.f, false);
	}
}

void AZoransProjectileBase::DestroyProjectile()
{
	Destroy();
}

void AZoransProjectileBase::ServerProjectileExpired_Implementation()
{
	return;
}

void AZoransProjectileBase::ClientProjectileExpired_Implementation()
{
	return;
}

bool AZoransProjectileBase::IsValidTarget(AActor* InActor)
{
	if (APawn* OwningPawn = GetInstigator())
	{
		if (IsValid(InActor))
		{
			if (InActor->GetClass()->ImplementsInterface(ULocationMagnitudeInterface::StaticClass()))
			{
				return true;
			}
			
			ETeamComparisonResult TeamComparisonResult = ETeamComparisonResult::Invalid;
			UTeamStateFunctionLibrary::GetTeamDispositionFromActors(OwningPawn, InActor, OwningPawn, TeamComparisonResult);
			switch (TeamComparisonResult)
			{
			case ETeamComparisonResult::Invalid:
				if (bTraceEnvironment) return true;
				break;
			case ETeamComparisonResult::Hostile:
				if (bTraceEnemy) return true;
				break;
			case ETeamComparisonResult::Friendly:
				if (bTraceFriendly) return true;
				break;
			}
		}
	}
	
	return false;
}

FName AZoransProjectileBase::GetClosestBoneToHit(const FHitResult& InHit) const
{
	FName ClosestBone = InHit.BoneName;

	if (ClosestBone == NAME_None && InHit.GetComponent())
	{
		if (AActor* HitActor = InHit.GetActor())
		{
			if (const ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
			{
				if (const USkeletalMeshComponent* CharacterMesh = HitCharacter->GetMesh())
				{
					ClosestBone = CharacterMesh->FindClosestBone(InHit.ImpactPoint, nullptr, 0, true);
				}
			}
		}
	}
	
	return ClosestBone;
}

FVector AZoransProjectileBase::GetProjectileLocationAtTime(const float InTime, const bool bServer)
{
	return FVector::ZeroVector;
}

float AZoransProjectileBase::GetServerProjectileLifetime() const
{
	float ProjectileLifetime = -1.f;
	if (GetWorld())
	{
		const float CurrentTime = GetWorld()->GetGameState() ? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : -1.f;
		ProjectileLifetime = CurrentTime - SpawnTime;
	}
	
	return ProjectileLifetime;
}

float AZoransProjectileBase::GetLocalProjectileLifetime() const
{
	float ProjectileLifetime = -1.f;
	if (GetWorld())
	{
		const float CurrentTime = GetWorld()->GetGameState() ? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : -1.f;
		ProjectileLifetime = CurrentTime - LocalSpawnTime;
	}
	
	return ProjectileLifetime;
}

void AZoransProjectileBase::InitializeProjectileTraceParameters()
{
	return;
}

void AZoransProjectileBase::ServerHitReceived_Implementation(const FHitResult& HitResult)
{
	UE_LOG(LogTemp, Error, TEXT("PROJECTILE -> SERVER HIT RECEIVED MUST BE IMPLEMENTED"));
}

void AZoransProjectileBase::Internal_ClientHitReceived(const FHitResult& InHitResult)
{
	bHasValidHit = true;
	DeactivateProjectile();
	ClientHitReceived(InHitResult);
}

void AZoransProjectileBase::ClientHitReceived_Implementation(const FHitResult& InHitResult)
{
	UE_LOG(LogTemp, Error, TEXT("PROJECTILE -> CLIENT HIT RECEIVED MUST BE IMPLEMENTED"));
}

void AZoransProjectileBase::SendHitResultToClients_Implementation(const FHitResult& InHitResult)
{
	Internal_ClientHitReceived(InHitResult);
}

void AZoransProjectileBase::SendHitResultToAbilitySystemComponent(const int32 InProjectileIndex, const FHitResult& InHitResult)
{
	if (InProjectileIndex > -1)
	{
		if (GetOwner())
		{
			UZoransAbilitySystemComponent* OwningASC = Cast<UZoransAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
			if (IsValid(OwningASC))
			{
				OwningASC->InternalProcessServerHitResult(InProjectileIndex, InHitResult);
			}
		}
	}
}

FDamageMetricValue AZoransProjectileBase::ApplyDamageToTarget(AActor* SourceActor, AActor* TargetActor, const FHitResult& HitResult, TSubclassOf<UGameplayEffect> DamageEffectClass, const float DamageAmount, const float AppliedImpactForce, const FGameplayTag DamageTag, bool& bValidHit)
{
	bValidHit = false;
	FDamageMetricValue DamageMetric;
	if (SourceActor && TargetActor)
	{
		if (TargetActor->GetClass()->ImplementsInterface(ULocationMagnitudeInterface::StaticClass()))
		{
			FLocationMagnitudeEvent LocationMagnitudeEvent;
			LocationMagnitudeEvent.SourceActor = SourceActor;
			LocationMagnitudeEvent.HitComponent = HitResult.GetComponent();
			LocationMagnitudeEvent.RelativeHitLocation = HitResult.Location;
			LocationMagnitudeEvent.WorldHitLocation = HitResult.Location;
			LocationMagnitudeEvent.EventMagnitude = DamageAmount;

			bValidHit = true;
			DamageMetric.Damage += DamageAmount;
			
			ILocationMagnitudeInterface::Execute_LocationMagnitudeEvent(TargetActor, LocationMagnitudeEvent);
		}
		else if (IsValid(DamageEffectClass))
		{
			if (UAbilitySystemComponent* InstigatorAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(SourceActor))
			{
				if (UAbilitySystemComponent* TargetAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
				{
					FGameplayEffectContextHandle EffectContextHandle = InstigatorAbilitySystemComponent->MakeEffectContext();
					EffectContextHandle.AddInstigator(SourceActor, SpawnSourceType == EZoransProjectileSourceType::Weapon ? GetOwningWeapon() : SourceActor);
					EffectContextHandle.AddSourceObject(OwningAbility);
					EffectContextHandle.AddHitResult(HitResult);
					
					const FGameplayEffectSpecHandle EffectSpecHandle = InstigatorAbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, AppliedImpactForce, EffectContextHandle);
			
					EffectSpecHandle.Data->SetSetByCallerMagnitude(GameplayTag_Damage.GetTag(), DamageAmount);
					EffectSpecHandle.Data->AddDynamicAssetTag(DamageTag);
					
					bValidHit = true;
					DamageMetric.Damage += DamageAmount;

					InstigatorAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*EffectSpecHandle.Data.Get(), TargetAbilitySystemComponent);
				}
			}
		}
	}

	return DamageMetric;
}

float AZoransProjectileBase::CalculateImpactForce(const float Modifier) const
{
	return ImpactForce * Modifier;
}
