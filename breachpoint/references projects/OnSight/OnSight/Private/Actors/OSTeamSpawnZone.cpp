#include "Actors/OSTeamSpawnZone.h"
#include "Components/BoxComponent.h"
#include "Core/OSPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "OSLogCategories.h"

AOSTeamSpawnZone::AOSTeamSpawnZone()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = false;

	SpawnVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnVolume"));
	SpawnVolume->SetBoxExtent(FVector(500.f, 500.f, 100.f));
	SpawnVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(SpawnVolume);
}

bool AOSTeamSpawnZone::ServesTeam(int32 InTeamIndex) const
{
	return TeamIndex == -1 || TeamIndex == InTeamIndex;
}

FTransform AOSTeamSpawnZone::GetSpawnTransform() const
{
	if (!SpawnVolume) return GetActorTransform();

	const UWorld* World = GetWorld();
	if (!World) return GetActorTransform();

	const FVector Extent = SpawnVolume->GetScaledBoxExtent();
	const FTransform VolumeXf = SpawnVolume->GetComponentTransform();

	// Default capsule half-height and radius for overlap checks.
	// Matches AOSCharacter default capsule (42 radius, 96 half-height).
	constexpr float TestRadius = 42.f;
	constexpr float TestHalfHeight = 96.f;
	constexpr float MinPlayerSeparation = 200.f;
	constexpr int32 MaxAttempts = 10;

	// Collect existing pawn locations for overlap avoidance
	TArray<FVector> OccupiedLocations;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const AController* PC = It->Get())
		{
			if (const APawn* Pawn = PC->GetPawn())
			{
				OccupiedLocations.Add(Pawn->GetActorLocation());
			}
		}
	}

	FVector BestPoint = VolumeXf.GetLocation(); // Fallback: zone center
	bool bFoundValid = false;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		// Random offset in LOCAL space, then transform to world
		const FVector LocalOffset(
			FMath::FRandRange(-Extent.X, Extent.X),
			FMath::FRandRange(-Extent.Y, Extent.Y),
			0.f);
		FVector CandidatePoint = VolumeXf.TransformPosition(LocalOffset);

		// Floor trace: find solid ground below the candidate point.
		// Prevents spawning in mid-air or underground.
		FHitResult FloorHit;
		const FVector TraceStart = CandidatePoint + FVector(0.f, 0.f, TestHalfHeight * 2.f);
		const FVector TraceEnd = CandidatePoint - FVector(0.f, 0.f, 500.f);
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(SpawnFloorTrace), false);

		if (World->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
		{
			CandidatePoint = FloorHit.ImpactPoint + FVector(0.f, 0.f, TestHalfHeight + 1.f);
		}

		// Capsule overlap check: ensure no blocking geometry at spawn point
		FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(SpawnOverlap), false);
		const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(TestRadius, TestHalfHeight);
		if (World->OverlapBlockingTestByChannel(CandidatePoint, FQuat::Identity, ECC_Pawn, CapsuleShape, OverlapParams))
		{
			continue; // Blocked by geometry, try again
		}

		// Player proximity check: avoid spawning on top of another player
		bool bTooCloseToPlayer = false;
		for (const FVector& Occupied : OccupiedLocations)
		{
			if (FVector::Dist2D(CandidatePoint, Occupied) < MinPlayerSeparation)
			{
				bTooCloseToPlayer = true;
				break;
			}
		}
		if (bTooCloseToPlayer) continue;

		// Valid point found
		BestPoint = CandidatePoint;
		bFoundValid = true;
		break;
	}

	if (!bFoundValid)
	{
		UE_LOG(LogOSSpawn, Warning, TEXT("[SpawnZone] %s: failed to find clear spawn point after %d attempts, using best available"),
			*GetName(), MaxAttempts);
	}

	return FTransform(GetActorRotation(), BestPoint);
}

float AOSTeamSpawnZone::ScoreForPlayer(AController* Player) const
{
	if (!Player || !GetWorld()) return 0.f;

	const FVector ZoneCenter = GetActorLocation();
	float MinEnemyDist = MAX_FLT;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AController* Other = It->Get();
		if (!IsValid(Other) || Other == Player) continue;

		const AOSPlayerState* OtherPS = Other->GetPlayerState<AOSPlayerState>();
		const AOSPlayerState* PlayerPS = Player->GetPlayerState<AOSPlayerState>();
		if (!OtherPS || !PlayerPS) continue;

		if (OtherPS->GetTeamIndex() == PlayerPS->GetTeamIndex()) continue;

		if (const APawn* OtherPawn = Other->GetPawn())
		{
			const float Dist = FVector::Dist(ZoneCenter, OtherPawn->GetActorLocation());
			MinEnemyDist = FMath::Min(MinEnemyDist, Dist);
		}
	}

	if (MinEnemyDist == MAX_FLT) return 1.f;
	return FMath::Clamp(MinEnemyDist / FMath::Max(1.f, PreferredEnemyDistance), 0.f, 1.f);
}
