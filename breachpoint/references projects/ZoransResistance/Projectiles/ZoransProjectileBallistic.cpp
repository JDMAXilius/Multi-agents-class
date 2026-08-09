// Copyright 2022 Zollpa LLC.


#include "ZoransProjectileBallistic.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetMathLibrary.h"


AZoransProjectileBallistic::AZoransProjectileBallistic()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	SetReplicatingMovement(false);

	TArray<FRichCurveKey> Keys;
	const FRichCurveKey OriginKey = FRichCurveKey(0.f, 0.f);
	const FRichCurveKey EndKey = FRichCurveKey(1.f, 1.f);
	Keys.Add(OriginKey);
	Keys.Add(EndKey);
	
	ProjectileForwardModifier.EditorCurveData.SetKeys(Keys);
}

void AZoransProjectileBallistic::BeginPlay()
{
	Super::BeginPlay();

	const ENetMode CurrentNetMode = GetNetMode();
	if (CurrentNetMode < NM_Client && ProjectileLifetimeBasedEvents.Num() > 0)
	{
		SetNextLifetimeEvent();
		bHasLifetimeEvent = true;
	}

	if (CurrentNetMode == NM_DedicatedServer)
	{
		bSynced = true;
	}
	else
	{
		const float SpawnTimeDiscrepancy = FMath::Abs(LocalSpawnTime - SpawnTime);
		const float SpawnLocationDiscrepancy = FVector::Distance(SpawnPoint, LocalSpawnLocation);
		
		if (SpawnTimeDiscrepancy >= 0.025f || SpawnLocationDiscrepancy >= 10.f)
		{
			const float TimeToLocation = SpawnLocationDiscrepancy / ProjectileSpeed;
			SyncTime = FMath::Clamp<float>(TimeToLocation + SpawnTimeDiscrepancy * 2.f, 0.f, MaximumSyncTime);
			bSynced = false;
		}
		else
		{
			bSynced = true;
		}
	}
}

void AZoransProjectileBallistic::InitializeProjectileTraceParameters()
{
	CollisionQueryParams.AddIgnoredActor(GetOwner());
	CollisionQueryParams.AddIgnoredActor(GetInstigator());
			
	if (bTraceEnemy || bTraceFriendly)
	{
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	}

	if (bTraceEnvironment)
	{
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	}
}

void AZoransProjectileBallistic::UpdateProjectile_Server()
{
	PreviousLocation = GetActorLocation();
	FVector NewLocation = FVector::ZeroVector;
	const FVector TrueLocation = GetProjectileLocationAtTime(GetServerProjectileLifetime(), true);
	if (bSynced)
	{
		NewLocation = TrueLocation;
	}
	else
	{
		const float LocalLifetime = GetLocalProjectileLifetime();
		const float SyncAlpha = FMath::Clamp<float>(LocalLifetime / SyncTime, 0.f, 1.f);
		const FVector LocalLocation = GetProjectileLocationAtTime(LocalLifetime, false);
		NewLocation = FMath::Lerp(LocalLocation, TrueLocation, SyncAlpha);
		
		//DrawDebugSphere(GetWorld(), LocalLocation, 16.f, 6, FColor::Blue, false, 2.f, 0, 3.f);
		//DrawDebugSphere(GetWorld(), TrueLocation, 16.f, 6, FColor::Red, false, 2.f, 0, 3.f);
		//DrawDebugSphere(GetWorld(), NewLocation, 16.f, 6, FColor::Yellow, false, 2.f, 0, 3.f);
		if (SyncAlpha >= 1.f)
		{
			//DrawDebugSphere(GetWorld(), NewLocation, 32.f, 6, FColor::Green, false, 2.f, 0, 3.f);
			//UE_LOG(LogTemp, Error, TEXT("SYNCED SERVER :: %f"), TrueLifetime);
			bSynced = true;
		}
	}

	FHitResult Hit;
	const bool bHit = PerformProjectileTrace(Hit, NewLocation);
	
	if (bHit)
	{
		bHasValidHit = true;
		ServerHitReceived(Hit);
		SendHitResultToClients(Hit);
		
		if (LocalIndex > -1)
		{
			SendHitResultToAbilitySystemComponent(LocalIndex, Hit);
		}
		
		DeactivateProjectile();
	}
	else
	{
		if (FMath::IsNearlyZero(FVector::Distance(NewLocation, PreviousLocation), 0.1f))
		{
			SetActorLocation(NewLocation);
		}
		else
		{
			const FVector NewRotation = (NewLocation - PreviousLocation).GetSafeNormal();
			SetActorLocationAndRotation(NewLocation, NewRotation.Rotation());
		}

		if (bHasLifetimeEvent)
		{
			const float TrueLifetime = GetLocalProjectileLifetime();
			if (ShouldTriggerLifetimeEvent(TrueLifetime))
			{
				Internal_ProjectileLifetimeEventTriggered();
			}
		}
	
		Super::UpdateProjectile_Server();
	}
}

void AZoransProjectileBallistic::UpdateProjectile_Client()
{
	PreviousLocation = GetActorLocation();
	if (bSynced)
	{
		const FVector NewLocation = GetProjectileLocationAtTime(GetServerProjectileLifetime(), true);
		const FVector NewRotation = (NewLocation - PreviousLocation).GetSafeNormal();
	
		SetActorLocationAndRotation(NewLocation, NewRotation.Rotation());
	}
	else
	{
		const float LocalLifetime = GetLocalProjectileLifetime();
		const float SyncAlpha = FMath::Clamp<float>(LocalLifetime / SyncTime, 0.f, 1.f);
		const FVector ServerLocation = GetProjectileLocationAtTime(GetServerProjectileLifetime(), true);
		const FVector LocalLocation = GetProjectileLocationAtTime(LocalLifetime, false);
		const FVector MergedLocation = FMath::Lerp(LocalLocation, ServerLocation, SyncAlpha);
		const FVector NewRotation = (MergedLocation - PreviousLocation).GetSafeNormal();
		SetActorLocationAndRotation(MergedLocation, NewRotation.Rotation());
		
		//DrawDebugSphere(GetWorld(), LocalLocation, 16.f, 6, FColor::Blue, false, 2.f, 0, 3.f);
		//DrawDebugSphere(GetWorld(), ServerLocation, 16.f, 6, FColor::Red, false, 2.f, 0, 3.f);
		//DrawDebugSphere(GetWorld(), MergedLocation, 16.f, 6, FColor::Yellow, false, 2.f, 0, 3.f);


		if (SyncAlpha >= 1.f)
		{
			//UE_LOG(LogTemp, Error, TEXT("SYNCED CLIENT :: %f"), LocalLifetime);
			//DrawDebugSphere(GetWorld(), ServerLocation, 32.f, 6, FColor::Green, false, 2.f, 0, 3.f);
			bSynced = true;
		}
	}
	
	Super::UpdateProjectile_Client();
}

bool AZoransProjectileBallistic::PerformProjectileTrace(FHitResult& InHit, const FVector& NewLocation)
{
	bool bHasHit = false;
	
	if (GetWorld())
	{
		TArray<FHitResult> Hits;
		const FCollisionShape Sphere = FCollisionShape::MakeSphere(ProjectileRadius);
		bHasHit = GetWorld()->SweepMultiByObjectType(Hits, PreviousLocation, NewLocation, FQuat::Identity, ObjectQueryParams, Sphere, CollisionQueryParams);
		if (bHasHit)
		{
			InHit = Hits.IsValidIndex(0) ? Hits[0] : FHitResult();
			if (InHit.GetActor())
			{
				if (!IsValidTarget(InHit.GetActor()))
				{
					bHasHit = false;
				}
			}
		}
	}
	
	return bHasHit;
}

bool AZoransProjectileBallistic::ShouldTriggerLifetimeEvent(const float InTime) const
{
	const float LifetimeAlpha = InTime / ProjectileLifespan;
	return LifetimeAlpha >= NextLifetimeEvent.PointInLife;
}

FVector AZoransProjectileBallistic::GetProjectileLocationAtTime(const float InTime, const bool bServer)
{
	const FVector StartLocation = bServer ? SpawnPoint : LocalSpawnLocation;
	
	const float Gravity = GetGravityEffectAtTime(InTime);
	const float LifeAlpha = FMath::Clamp<float>(UKismetMathLibrary::SafeDivide(InTime, ProjectileLifespan), 0.f, 1.f);
	const float MaximumRange = ProjectileSpeed * ProjectileLifespan;
	
	const float RawDistance = ProjectileForwardModifier.EditorCurveData.Eval(LifeAlpha) * MaximumRange;

	const FVector RawDirection = LaunchDirection * RawDistance;
	const FVector AdjustedLocation = StartLocation + RawDirection;
	const FVector FinalLocation = AdjustedLocation - FVector(0.f, 0.f, Gravity);
	
	return FinalLocation;
}

float AZoransProjectileBallistic::GetGravityEffectAtTime(const float InTime) const
{
	float Gravity = 0.f;
	
	if (!FMath::IsNearlyZero(GravityModifier, 0.01f))
	{
		float GravityTime = InTime;
		
		if (GravityDelay > 0.f)
		{
			GravityTime = FMath::Clamp<float>(InTime - GravityDelay * ProjectileLifespan, 0.f, ProjectileLifespan);
		}
		
		Gravity = GravityConstant * GravityModifier * GravityTime * GravityTime * 0.5f;
	}
	
	return Gravity;
}

void AZoransProjectileBallistic::SetNextLifetimeEvent()
{
	float TempTimeCheck = 9999.f;
	FProjectileLifetimeBasedEvent *SelectedEvent = nullptr;
		
	for (auto &x : ProjectileLifetimeBasedEvents)
	{
		if (!x.bTriggered && x.PointInLife < TempTimeCheck)
		{
			TempTimeCheck = x.PointInLife;
			SelectedEvent = &x;
		}
	}

	if (TempTimeCheck > 1.f)
	{
		bHasLifetimeEvent = false;
	}
	else if (SelectedEvent)
	{
		SelectedEvent->bTriggered = true;
		NextLifetimeEvent = *SelectedEvent;
	}
}

void AZoransProjectileBallistic::Internal_ProjectileLifetimeEventTriggered()
{
	const float LifetimeAlpha = NextLifetimeEvent.PointInLife * ProjectileLifespan;
	const FVector EventLocation = GetProjectileLocationAtTime(LifetimeAlpha, true);
	ProjectileLifetimeEventTriggered(NextLifetimeEvent, EventLocation);
	SetNextLifetimeEvent();
}

void AZoransProjectileBallistic::ProjectileLifetimeEventTriggered_Implementation(FProjectileLifetimeBasedEvent TriggeredEvent, const FVector EventLocation)
{
	UE_LOG(LogTemp, Error, TEXT("PROJECTILE -> LIFETIME EVENT TRIGGERED MUST BE IMPLEMENTED IN BLUEPRINT"));
}
