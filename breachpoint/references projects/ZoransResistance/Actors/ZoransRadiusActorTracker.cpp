// Copyright Zollpa LLC


#include "ZoransRadiusActorTracker.h"
#include "ZoransResistance/Teams/TeamStateInterface.h"


AZoransRadiusActorTracker::AZoransRadiusActorTracker()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;

	DetectionSphere = CreateDefaultSubobject<USphereComponent>("DetectionSphere");
	DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionSphere->SetCollisionObjectType(ECC_Destructible);
	DetectionSphere->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);

	RootComponent = DetectionSphere;
}

void AZoransRadiusActorTracker::BeginPlay()
{
	OverrideOwningTeam();
	
	if (GetNetMode() < NM_Client)
	{
		IgnoredActors.Add(this);
		OnActorBeginOverlap.AddDynamic(this, &AZoransRadiusActorTracker::DetectionSphereBeginOverlap);
		OnActorEndOverlap.AddDynamic(this, &AZoransRadiusActorTracker::DetectionSphereEndOverlap);
		
		DetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		DetectionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		DetectionSphere->SetCollisionEnabled(ECollisionEnabled::Type::QueryOnly);
	}

	Super::BeginPlay();
}

void AZoransRadiusActorTracker::OverrideOwningTeam_Implementation()
{
	// do nothing
}

FVector AZoransRadiusActorTracker::GetTraceStartLocation_Implementation() const
{
	return GetActorLocation();
}

void AZoransRadiusActorTracker::DetectionSphereBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (IsValid(OtherActor) && !IgnoredActors.Contains(OtherActor))
	{
		if (const ITeamStateInterface* TeamStateInterface = Cast<ITeamStateInterface>(OtherActor))
		{
			const int32 OtherTeam = TeamStateInterface->GetTeamAssignment();
			if (OtherTeam < 0)
			{
				if (OtherActor != GetOwner())
				{
					const bool bInitialEmpty = EnemyActors.IsEmpty();
					EnemyActors.Add(OtherActor);
					NewActorDetected(OtherActor, false);
					if (bInitialEmpty)
					{
						EnemyActorsNotEmpty();
					}
				}
			}
			else if (bTrackFriendly && OwningTeam == OtherTeam)
			{
				const bool bInitialEmpty = FriendlyActors.IsEmpty();
				FriendlyActors.Add(OtherActor);
				NewActorDetected(OtherActor, true);
				if (bInitialEmpty)
				{
					FriendlyActorsNotEmpty();
				}
				
				if (OtherActor == OwningActorReference)
				{
					Internal_OwnerEnteredRadius(OwningActorReference);
				}
			}
			else if (bTrackEnemy && OwningTeam != OtherTeam)
			{
				if (OtherActor != GetOwner())
				{
					const bool bInitialEmpty = EnemyActors.IsEmpty();
					EnemyActors.Add(OtherActor);
					NewActorDetected(OtherActor, false);
					if (bInitialEmpty)
					{
						EnemyActorsNotEmpty();
					}
				}
			}
		}
		else
		{
			IgnoredActors.Add(OtherActor);
		}
	}
}

inline void AZoransRadiusActorTracker::DetectionSphereEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (IsValid(OtherActor))
	{
		if (FriendlyActors.Contains(OtherActor))
		{
			FriendlyActors.Remove(OtherActor);
			ActorDetectionLost(OtherActor, true);
			if (FriendlyActors.IsEmpty())
			{
				FriendlyActorsEmpty();	
			}
			
			if (OtherActor == OwningActorReference)
			{
				Internal_OwnerExitedRadius(OwningActorReference);
			}
		}
		else if (EnemyActors.Contains(OtherActor))
		{
			EnemyActors.Remove(OtherActor);
			ActorDetectionLost(OtherActor, false);
			if (EnemyActors.IsEmpty())
			{
				EnemyActorsEmpty();
			}
		}
	}
}

AActor* AZoransRadiusActorTracker::GetClosestActor(const bool bFriendly, const bool bLineOfSight, const float InFilterByAngle)
{
	TArray<AActor*> PotentialTargets;
	const FVector TraceStart = GetTraceStartLocation();
	
	if (bFriendly)
	{
		if (!FriendlyActors.IsEmpty())
		{
			for (auto const &Friendly : FriendlyActors)
			{
				bool bPotentialTarget = true;
				if (InFilterByAngle > 0.f && !FilterByAngle(Friendly->GetActorLocation(), InFilterByAngle))
				{
					bPotentialTarget = false;
				}
				
				if (bPotentialTarget)
				{
					if (bLineOfSight && !HasLineOfSight(Friendly->GetActorLocation()))
					{
						bPotentialTarget = false;
					}
				}

				if (bPotentialTarget)
				{
					PotentialTargets.AddUnique(Friendly);
				}
			}
		}
	}
	else if (!EnemyActors.IsEmpty())
	{
		for (auto const &Enemy : EnemyActors)
		{
			bool bPotentialTarget = true;
			if (InFilterByAngle > 0.f && !FilterByAngle(Enemy->GetActorLocation(), InFilterByAngle))
			{
				bPotentialTarget = false;
			}
			
			if (bPotentialTarget)
			{
				if (bLineOfSight && !HasLineOfSight(Enemy->GetActorLocation()))
				{
					bPotentialTarget = false;
				}
			}

			if (bPotentialTarget)
			{
				PotentialTargets.AddUnique(Enemy);
			}
		}
	}

	float ClosestTargetDistance = 9999999999.f;
	AActor* ClosestTarget = nullptr;
	for (auto const & TestActor : PotentialTargets)
	{
		const float TestDistance = FVector::DistSquared(TraceStart, TestActor->GetActorLocation());
		if (TestDistance < ClosestTargetDistance)
		{
			ClosestTargetDistance = TestDistance;
			ClosestTarget = TestActor;
		}
	}

	return ClosestTarget;
}

/*//
Distance
weapon range

score value = Distance/weapon range

float CalculateBaseScore(APlayerCharacter* Player, AAICharacter* AI, float WeaponRange)
{
	float Distance = FVector::Dist(Player->GetActorLocation(), AI->GetActorLocation());
	float DistanceScore = FMath::Clamp((WeaponRange - Distance) / WeaponRange, 0.0f, 1.0f);

    float BaseScore = DistanceScore;
	float HatredValue = AI->GetHatredValueForPlayer(Player);

	// Assuming HatredValue is normalized between 0 and 1
	return BaseScore * (1 + HatredValue); // Amplify score based on hatred
}

APlayerCharacter* SelectTarget(AAICharacter* AI, const TArray<APlayerCharacter*>& PotentialTargets, float WeaponRange)
{
	APlayerCharacter* BestTarget = nullptr;
	float HighestScore = 0.0f;

	for (APlayerCharacter* Player : PotentialTargets)
	{
		float Score = CalculateHatredScore(Player, AI, WeaponRange);
		if (Score > HighestScore)
		{
			HighestScore = Score;
			BestTarget = Player;
		}
	}

	return BestTarget;
}

void AICharacter::UpdateHatredValue(APlayerCharacter* Player, float DeltaValue)
{
	if (PlayerHatredMap.Contains(Player))
	{
		PlayerHatredMap[Player] = FMath::Clamp(PlayerHatredMap[Player] + DeltaValue, 0.0f, 1.0f);
	}
	else
	{
		PlayerHatredMap.Add(Player, DeltaValue);
	}
}

   
//*/

TArray<AActor*> AZoransRadiusActorTracker::GetFilteredActors(const bool bFriendly, const bool bLineOfSight,	const float InFilterByAngle)
{
	TArray<AActor*> PotentialTargets = bFriendly ? FriendlyActors : EnemyActors;
	TArray<AActor*> FilteredTargets;
	
	if (InFilterByAngle > 0)
	{
		for (auto const &x : PotentialTargets)
		{
			const FVector TargetLocation = x->GetActorLocation();
			if (FilterByAngle(TargetLocation, InFilterByAngle))
			{
				if (bLineOfSight)
				{
					if (HasLineOfSight(TargetLocation))
					{
						FilteredTargets.AddUnique(x);
					}
				}
				else
				{
					FilteredTargets.AddUnique(x);
				}
			}
		}
	}
	else if (bLineOfSight)
	{
		for (auto const &y : PotentialTargets)
		{
			if (HasLineOfSight(y->GetActorLocation()))
			{
				FilteredTargets.AddUnique(y);
			}
		}
	}
	else
	{
		FilteredTargets = PotentialTargets;
	}
	
	return FilteredTargets;
}

bool AZoransRadiusActorTracker::HasLineOfSight(const FVector& InTargetVector) const
{
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	FCollisionObjectQueryParams ObjQueryParams;
	ObjQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FHitResult TestHit;
	return !GetWorld()->LineTraceSingleByObjectType(TestHit, GetTraceStartLocation(), InTargetVector, ObjQueryParams, QueryParams);
}

bool AZoransRadiusActorTracker::FilterByAngle(const FVector& InTargetVector, const float InAngle) const
{
	const FVector ConeDirection = GetActorForwardVector();
	const FVector TargetDirection = (InTargetVector - GetActorLocation()).GetSafeNormal();
	const float DirectionDot = FVector::DotProduct(ConeDirection, TargetDirection);
	const float DirectionRadians = FMath::Acos(DirectionDot);
	const float DirectionDegrees = FMath::RadiansToDegrees(DirectionRadians);
	
	return DirectionDegrees <= InAngle;
}

void AZoransRadiusActorTracker::Internal_OwnerEnteredRadius(AActor* OwningActor)
{
	bOwnerWithinRadius = true;
	OwnerEnteredRadius(OwningActor);
}

void AZoransRadiusActorTracker::Internal_OwnerExitedRadius(AActor* OwningActor)
{
	bOwnerWithinRadius = false;
	OwnerExitedRadius(OwningActor);
}