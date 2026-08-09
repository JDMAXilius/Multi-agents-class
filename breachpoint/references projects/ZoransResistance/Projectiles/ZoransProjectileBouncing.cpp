// Copyright 2022 Zollpa LLC.


#include "ZoransResistance/Projectiles/ZoransProjectileBouncing.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "ZoransResistance/AbilitySystem/ZoransAbilitySystemComponent.h"
//#include "Zorans/AbilitySystem/ZoransGameplayAbility.h"

void AZoransProjectileBouncing::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(AZoransProjectileBouncing, BouncePath, COND_None, REPNOTIFY_OnChanged);
}

void AZoransProjectileBouncing::BeginPlay()
{
	Super::BeginPlay();
	
	RemainingLifespan = ProjectileLifespan;
	BounceQueryParams.AddIgnoredActor(GetOwner());
	BounceQueryParams.AddIgnoredActor(GetInstigator());
	BounceObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	CurrentSpeed = ProjectileSpeed;
	
	const ENetMode CurrentNetMode = GetNetMode();
	
	if ((GetInstigator() && GetInstigator()->IsLocallyControlled()) || (SpawnSourceType == EZoransProjectileSourceType::World && CurrentNetMode < NM_Client))
	{
		if (BouncePath.IsEmpty())
		{
			BouncePath = GenerateBouncePath();
			SetNextPathSegment();
		}
	}
	else if (CurrentNetMode < NM_Client)		//@todo is this correct?
	{
		SetNextPathSegment();
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
			const float TimeToLocation = SpawnLocationDiscrepancy / CurrentSpeed;
			SyncTime = FMath::Clamp<float>(TimeToLocation + SpawnTimeDiscrepancy * 2.f, 0.f, 0.15f);
			//UE_LOG(LogTemp, Error, TEXT("SyncTime :: %f"), SyncTime);
			bSynced = false;
		}
		else
		{
			bSynced = true;
		}
	}
}

void AZoransProjectileBouncing::InitializeProjectileTraceParameters()
{
	CollisionQueryParams.AddIgnoredActor(GetOwner());
	CollisionQueryParams.AddIgnoredActor(GetInstigator());
	
	if (bTraceEnemy || bTraceFriendly)
	{
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	}

	if (bTraceEnvironment)
	{
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	}
}

void AZoransProjectileBouncing::UpdateProjectile_Server()
{
	if (!GetWorld() || !GetWorld()->GetGameState()) return;
	
	if (!bSynced)
	{
		const float LocalTime = GetLocalProjectileLifetime();
		const float ServerTime = GetServerProjectileLifetime();
		const float LifeAlpha = FMath::Clamp<float>(LocalTime / SyncTime, 0.f, 1.f);
		const float MergedTime = FMath::Lerp<float>(ServerTime, LocalTime, LifeAlpha);
		//UE_LOG(LogTemp, Warning, TEXT("TimeAdjustment %f"), ServerTime - MergedTime);
		TimeAdjustment = ServerTime - MergedTime;
		if (LifeAlpha >= 1.f)
		{
			bSynced = true;
		}
	}
	
	const float CurrentTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds() + TimeAdjustment;
	const float PathTime = CurrentTime - CurrentPathSegment.SegmentStartTime;
	const FVector NewLocation = GetProjectileLocationAtTime(PathTime, true);

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
	
	const FVector NewRotation = (NewLocation - PreviousLocation).GetSafeNormal();
	SetActorLocationAndRotation(NewLocation, NewRotation.Rotation());
	PreviousLocation = NewLocation;

	if (!bHit && ShouldBounce())
	{
		const ENetMode CurrentNetMode = GetNetMode();
		if (SetNextPathSegment())
		{
			if (CurrentNetMode < NM_Client)
			{
				CurrentBounce++;
				ServerBounce(CurrentPathSegment.SegmentStartLocation, CurrentPathSegment.SegmentLaunchDirection);
			}
			if (CurrentNetMode != NM_DedicatedServer)
			{
				if (!CurrentPathSegment.bBounceTriggered)
				{
					//UE_LOG(LogTemp, Error, TEXT("CLIENT BOUNCE !!"));
					ClientBounce(CurrentPathSegment.SegmentStartLocation, CurrentPathSegment.SegmentLaunchDirection);
					CurrentPathSegment.bBounceTriggered = true;
				}
			}
		}
		else
		{
			if (!bHasValidHit)
			{
				if (CurrentNetMode < NM_Client)
				{
					ServerProjectileExpired();
				}
			}
			DeactivateProjectile();
		}
	}
	
	Super::UpdateProjectile_Server();
}

void AZoransProjectileBouncing::UpdateProjectile_Client()
{
	if (!GetWorld() || !GetWorld()->GetGameState()) return;
	
	if (!bSynced)
	{
		const float LocalTime = GetLocalProjectileLifetime();
		const float ServerTime = GetServerProjectileLifetime();
		const float LifeAlpha = FMath::Clamp<float>(LocalTime / SyncTime, 0.f, 1.f);
		const float MergedTime = FMath::Lerp<float>(ServerTime, LocalTime, LifeAlpha);
		//UE_LOG(LogTemp, Warning, TEXT("TimeAdjustment %f"), ServerTime - MergedTime);
		TimeAdjustment = ServerTime - MergedTime;
		if (LifeAlpha >= 1.f)
		{
			TimeAdjustment = 0.f;
			bSynced = true;
		}
	}
	
	const float CurrentTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds() + TimeAdjustment;
	const float PathTime = CurrentTime - CurrentPathSegment.SegmentStartTime;
	const FVector NewLocation = GetProjectileLocationAtTime(PathTime, true);
	const FVector NewRotation = (NewLocation - PreviousLocation).GetSafeNormal();
	
	SetActorLocationAndRotation(NewLocation, NewRotation.Rotation());

	if (ShouldBounce())
	{
		if (SetNextPathSegment())
		{
			if (!CurrentPathSegment.bBounceTriggered)
			{
				ClientBounce(CurrentPathSegment.SegmentStartLocation, CurrentPathSegment.SegmentLaunchDirection);
				CurrentPathSegment.bBounceTriggered = true;
			}
		}
		else
		{
			if (!bHasValidHit)
			{
				ClientProjectileExpired();
			}
			//UE_LOG(LogTemp, Error, TEXT("CLIENT DEACTIVATE"));
			DeactivateProjectile();
		}
	}
	
	Super::UpdateProjectile_Client();
}

bool AZoransProjectileBouncing::PerformProjectileTrace(FHitResult& InHit, const FVector& NewLocation)
{
	bool bHasHit = false;
	
	if (GetWorld())
	{
		const FCollisionShape Sphere = FCollisionShape::MakeSphere(ProjectileRadius);

		if (!IgnoredDynamicComponents.IsEmpty())
		{
			FCollisionQueryParams TempQueryParams;
			TempQueryParams.AddIgnoredActor(GetOwner());
			TempQueryParams.AddIgnoredActor(GetInstigator());
			
			FCollisionObjectQueryParams TempObjectParams;
			TempObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
			
			TArray<FHitResult> TempHits;
			GetWorld()->SweepMultiByObjectType(TempHits, PreviousLocation, NewLocation, FQuat::Identity, TempObjectParams, FCollisionShape::MakeSphere(ProjectileRadius + IntersectionTraceTolerance), TempQueryParams);
			//DrawDebugLine(GetWorld(), PreviousLocation, NewLocation, FColor::Blue, false, 2.f, 0, 4.f);
			if (TempHits.IsEmpty())
			{
				//UE_LOG(LogTemp, Warning, TEXT("REMOVING ALL IGNORED DYNAMIC COMPONENTS"));
				IgnoredDynamicComponents.Empty();
				CollisionQueryParams.ClearIgnoredComponents();
			}
			else
			{
				TArray<UPrimitiveComponent*> NewIgnoredComponents;
				for (auto TempHit : TempHits)
				{
					if (TempHit.GetComponent() && IgnoredDynamicComponents.Contains(TempHit.GetComponent()))
					{
						NewIgnoredComponents.Add(TempHit.GetComponent());
						//UE_LOG(LogTemp, Warning, TEXT("STILL IGNORING DYNAMIC COMPONENT %s"), *GetNameSafe(TempHit.GetComponent()));
					}
				}

				IgnoredDynamicComponents.Empty();
				CollisionQueryParams.ClearIgnoredComponents();
				
				if (!NewIgnoredComponents.IsEmpty())
				{
					IgnoredDynamicComponents.Append(NewIgnoredComponents);
					CollisionQueryParams.AddIgnoredComponents(IgnoredDynamicComponents);
				}
			}
		}

		//TArray<FHitResult> Hits;
		//FHitResult Hit;
		bHasHit = GetWorld()->SweepSingleByObjectType(InHit, PreviousLocation, NewLocation, FQuat::Identity, ObjectQueryParams, Sphere, CollisionQueryParams);
		//bHasHit = GetWorld()->SweepMultiByObjectType(Hits, PreviousLocation, NewLocation, FQuat::Identity, ObjectQueryParams, Sphere, CollisionQueryParams);
		if (bHasHit)
		{
			//InHit = Hits.IsValidIndex(0) ? Hits[0] : FHitResult();
			if (InHit.GetComponent())
			{
				ECollisionChannel HitChannel = InHit.GetComponent()->GetCollisionObjectType();
				if (HitChannel == ECC_WorldDynamic)
				{
					CurrentBounce++;
					if (CurrentBounce > NumberOfBounces)
					{
						//UE_LOG(LogTemp, Error, TEXT("FINAL HIT IS DYNAMIC"));
						bHasValidHit = true;
						ServerHitReceived(InHit);
						SendHitResultToClients(InHit);
		
						if (LocalIndex > -1)
						{
							SendHitResultToAbilitySystemComponent(LocalIndex, InHit);
						}
		
						DeactivateProjectile();
					}
					else
					{
						CollisionQueryParams.AddIgnoredComponent(InHit.GetComponent());
						IgnoredDynamicComponents.Add(InHit.GetComponent());
						//UE_LOG(LogTemp, Warning, TEXT("IGNORING DYNAMIC COMPONENT %s"), *GetNameSafe(InHit.GetComponent()));
						RemainingLifespan = ProjectileLifespan - GetServerProjectileLifetime();
						const FVector TraceDirection = (InHit.TraceEnd - InHit.TraceStart).GetSafeNormal();
						const FVector NewDirection = FMath::GetReflectionVector(TraceDirection, InHit.ImpactNormal);
						BouncePath = RegenerateBouncePath(InHit.Location, NewDirection, NumberOfBounces - CurrentBounce);
						
						if (LocalIndex > -1)
						{
							SendServerBounceAdjustmentToAbilitySystemComponent(LocalIndex, BouncePath);
						}
						
						SetNextPathSegment();
						ClientBounce(CurrentPathSegment.SegmentStartLocation, CurrentPathSegment.SegmentLaunchDirection);
						CurrentPathSegment.bBounceTriggered = true;
						bHasHit = false;
					}
				}
				else if (HitChannel == ECC_Pawn)
				{
					if (InHit.GetActor())
					{
						//UE_LOG(LogTemp, Warning, TEXT("PAWN ACTOR HIT %s"), *GetNameSafe(InHit.GetActor()));
						if (!IsValidTarget(InHit.GetActor()))
						{
							BounceQueryParams.AddIgnoredActor(InHit.GetActor());
							bHasHit = false;
						}
					}
				}
			}
		}
	}
	
	return bHasHit;
}

FVector AZoransProjectileBouncing::GetProjectileLocationAtTime(const float InTime, const bool bServer)
{
	const FVector StartLocation = CurrentPathSegment.SegmentStartLocation;
		
	const float RawDistance = CurrentSpeed * InTime;
	const float Gravity = GetGravityEffectAtTime(InTime);

	const FVector RawDirection = CurrentPathSegment.SegmentLaunchDirection * RawDistance;
	const FVector AdjustedLocation = StartLocation + RawDirection;
	const FVector FinalLocation = AdjustedLocation - FVector(0.f, 0.f, Gravity);
	
	return FinalLocation;
}

float AZoransProjectileBouncing::GetGravityEffectAtTime(const float InTime) const
{
	float Gravity = 0.f;
	
	if (!FMath::IsNearlyZero(GravityModifier, 0.01f))
	{
		Gravity = GravityConstant * GravityModifier * InTime * InTime * 0.5f;
	}
	
	return Gravity;
}

float AZoransProjectileBouncing::GetSpeedFromBounce(const int32 InBounce) const
{
	if (InBounce != 0 && EnergyLossPerBounce != 0.f)
	{
		const float SpeedModifier = InBounce * EnergyLossPerBounce;
		const float MinSpeed = ProjectileSpeed * MinimumSpeedModifier;
		const float MaxSpeed = ProjectileSpeed * MaximumSpeedModifier;
		return FMath::Clamp<float>(ProjectileSpeed - (ProjectileSpeed * SpeedModifier), MinSpeed, MaxSpeed);
	}
	
	return ProjectileSpeed;
}

void AZoransProjectileBouncing::OnRep_BouncePath()
{
	if (GetNetMode() == NM_Client)
	{
		//UE_LOG(LogTemp, Error, TEXT("OnRep_BouncePath"));
		//const float LifetimeAlpha = GetServerProjectileLifetime() / ProjectileLifespan;
		for (auto &x : BouncePath)
		{
			x.bSelected = false;
			x.bBounceTriggered = false;
			//UE_LOG(LogTemp, Warning, TEXT("BOUNCE EVENT TIME %f -> %f"), LifetimeAlpha, x.EndTime);
		}
		SetNextPathSegment();
		if (bProjectileInitialized)
		{
			ClientBounce(CurrentPathSegment.SegmentStartLocation, CurrentPathSegment.SegmentLaunchDirection);
			CurrentPathSegment.bBounceTriggered = true;
		}
		bProjectileInitialized = true;
	}
}

bool AZoransProjectileBouncing::ShouldBounce() const
{
	const float LifetimeAlpha = (GetLocalProjectileLifetime() + TimeAdjustment) / ProjectileLifespan;
	//UE_LOG(LogTemp, Warning, TEXT("ShouldBounceTime :: %f"), GetLocalProjectileLifetime() + TimeAdjustment);
	return LifetimeAlpha >= CurrentPathSegment.EndTime;
}

bool AZoransProjectileBouncing::SetNextPathSegment()
{
	if (!GetWorld() || !GetWorld()->GetGameState()) return false;
	
	float TempTimeCheck = 9999.f;
	FBouncingProjectilePathSegment *SelectedEvent = nullptr;

	//(LogTemp, Warning, TEXT("BouncePath Lenght :: %d"), BouncePath.Num());
			
	for (auto &x : BouncePath)
	{
		if (!x.bSelected && x.EndTime < TempTimeCheck)
		{
			TempTimeCheck = x.EndTime;
			SelectedEvent = &x;
		}
	}

	if (TempTimeCheck > 9.f)
	{
		//UE_LOG(LogTemp, Error, TEXT("NO NEXT PATH SEGMENT"));
		return false;
	}
	
	if (SelectedEvent != nullptr)
	{
		SelectedEvent->bSelected = true;
		CurrentPathSegment = *SelectedEvent;
		CurrentPathSegment.SegmentStartTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds() + TimeAdjustment;
		CurrentSpeed = GetSpeedFromBounce(CurrentPathSegment.SegmentBounceCount);
		//if (CurrentSpeed <= ProjectileSpeed * MinimumSpeedModifier && )
		//UE_LOG(LogTemp, Warning, TEXT("CURRENT SPEED %d :: %f"), CurrentPathSegment.SegmentBounceCount, CurrentSpeed);
		return true;
	}

	//UE_LOG(LogTemp, Error, TEXT("NO NEXT SELECTED PATH"));
	return false;
}

TArray<FBouncingProjectilePathSegment> AZoransProjectileBouncing::GenerateBouncePath()
{
	TArray<FBouncingProjectilePathSegment> Segments;

	float SimulationLifespan = RemainingLifespan;
	FVector EndLocation = SpawnPoint;
	FVector NextBounceDirection = LaunchDirection;
	float TimeRemoved = 0.f;
	float TotalRemoved = 0.f;
	bool bEndTrace = false;
	for (int32 x = 0; x < NumberOfBounces + 1; x++)
	{
		if (!bEndTrace && SimulationLifespan > 0.f)
		{
			//UE_LOG(LogTemp, Warning, TEXT("GENERATE BOUNCE SEGMENT %d"), x);
			FBouncingProjectilePathSegment NewSegment = GenerateBounceSegment(EndLocation, NextBounceDirection, SimulationLifespan, TotalRemoved, EndLocation, NextBounceDirection, TimeRemoved, x);
			if (NewSegment.EndTime >= 9.f)
			{
				bEndTrace = true;
				break;
			}
			else
			{
				SimulationLifespan -= TimeRemoved;
				TotalRemoved += TimeRemoved;
				Segments.Add(NewSegment);
			}
		}
		else
		{
			break;
		}
	}
	//UE_LOG(LogTemp, Error, TEXT("BOUNCE SEGMENTS %d"), Segments.Num());
	return Segments;
}

TArray<FBouncingProjectilePathSegment> AZoransProjectileBouncing::RegenerateBouncePath(const FVector NewLocation, const FVector NewDirection, const int32 RemainingBounces)
{
	TArray<FBouncingProjectilePathSegment> Segments;

	//UE_LOG(LogTemp, Error, TEXT("REMAINING BOUNCES :: %d"), RemainingBounces);

	//DrawDebugSphere(GetWorld(), NewLocation, 32.f, 6, FColor::Purple, false, 3.f, 0, 4.f);
	PreviousLocation = NewLocation;
	float SimulationLifespan = RemainingLifespan;
	FVector EndLocation = NewLocation;
	FVector NextBounceDirection = NewDirection;
	float TimeRemoved = 0.f;
	float TotalRemoved = 0.f;
	bool bEndTrace = false;
	for (int32 x = 0; x <= RemainingBounces; x++)
	{
		if (!bEndTrace && SimulationLifespan > 0.f)
		{
			const int32 ThisBounce = NumberOfBounces - RemainingBounces + x;
			FBouncingProjectilePathSegment NewSegment = GenerateBounceSegment(EndLocation, NextBounceDirection, SimulationLifespan, TotalRemoved, EndLocation, NextBounceDirection, TimeRemoved, ThisBounce);
			if (NewSegment.EndTime >= 9.f)
			{
				bEndTrace = true;
				break;
			}
			else
			{
				SimulationLifespan -= TimeRemoved;
				TotalRemoved += TimeRemoved;
				Segments.Add(NewSegment);
			}
		}
		else
		{
			break;
		}
	}

	return Segments;
}

FBouncingProjectilePathSegment AZoransProjectileBouncing::GenerateBounceSegment(const FVector InStartLocation, const FVector InLaunchDirection, const float InDuration, const float InPreviousTime, FVector &EndLocation, FVector &BounceDirection, float &EndTime, const int32 InBounce)
{
	FBouncingProjectilePathSegment Bounce;
	Bounce.SegmentStartLocation = InStartLocation;
	Bounce.SegmentLaunchDirection = InLaunchDirection;
	Bounce.SegmentBounceCount = InBounce;
	const float CorrectedTime = ProjectileLifespan - RemainingLifespan;

	//UE_LOG(LogTemp, Warning, TEXT("BOUNCE SEGMENT DURATION :: %f"), InDuration);
	//UE_LOG(LogTemp, Warning, TEXT("BOUNCE SEGMENT TOTAL :: %f + %f = %f"), InDuration, InPreviousTime, CorrectedTime + InDuration + InPreviousTime);
	
	constexpr float TraceDuration = 0.1f;
	const int32 MaximumTraces = FMath::RoundToInt(InDuration / TraceDuration);
	//UE_LOG(LogTemp, Error, TEXT("MaximumTraces :: %d"), MaximumTraces);
	if (MaximumTraces > 1)
	{
		Bounce.EndTime = 2.f;
		EndTime = 999.f;
		FVector PreviousTracePoint = InStartLocation;
		for (int32 x = 0; x < MaximumTraces; x++)
		{
			const float SpeedAtBounce = GetSpeedFromBounce(InBounce);
			const float TraceTime = x * TraceDuration;
			const float RawDistance = SpeedAtBounce * TraceTime;
			const float Gravity = GetGravityEffectAtTime(TraceTime);

			const FVector RawDirection = InLaunchDirection * RawDistance;
			const FVector AdjustedLocation = InStartLocation + RawDirection;
			const FVector FinalLocation = AdjustedLocation - FVector(0.f, 0.f, Gravity);
			//DrawDebugLine(GetWorld(), PreviousTracePoint, FinalLocation, FColor::Green, false, 2.f, 0, 2.f);

			if (!IgnoredStaticComponents.IsEmpty())
			{
				FCollisionQueryParams TempQueryParams;
				TempQueryParams.AddIgnoredActor(GetOwner());
				TempQueryParams.AddIgnoredActor(GetInstigator());
				
				TArray<FHitResult> TempHits;
				GetWorld()->SweepMultiByObjectType(TempHits, PreviousTracePoint, FinalLocation, FQuat::Identity, BounceObjectParams, FCollisionShape::MakeSphere(ProjectileRadius + IntersectionTraceTolerance), TempQueryParams);
				//DrawDebugLine(GetWorld(), PreviousTracePoint, FinalLocation, FColor::Orange, false, 2.f, 0, 4.f);
				if (TempHits.IsEmpty())
				{
					//UE_LOG(LogTemp, Warning, TEXT("REMOVING ALL IGNORED STATIC COMPONENTS"));
					IgnoredStaticComponents.Empty();
					BounceQueryParams.ClearIgnoredComponents();
				}
				else
				{
					TArray<UPrimitiveComponent*> NewIgnoredComponents;
					for (auto TempHit : TempHits)
					{
						if (TempHit.GetComponent() && IgnoredStaticComponents.Contains(TempHit.GetComponent()))
						{
							NewIgnoredComponents.Add(TempHit.GetComponent());
							//UE_LOG(LogTemp, Warning, TEXT("STILL IGNORING STATIC COMPONENT %s"), *GetNameSafe(TempHit.GetComponent()));
						}
					}

					IgnoredStaticComponents.Empty();
					BounceQueryParams.ClearIgnoredComponents();
				
					if (!NewIgnoredComponents.IsEmpty())
					{
						IgnoredStaticComponents.Append(NewIgnoredComponents);
						BounceQueryParams.AddIgnoredComponents(IgnoredStaticComponents);
					}
				}
			}
			
			FHitResult Hit;
			const FCollisionShape Sphere = FCollisionShape::MakeSphere(ProjectileRadius);
			if (GetWorld()->SweepSingleByObjectType(Hit, PreviousTracePoint, FinalLocation, FQuat::Identity, BounceObjectParams, Sphere, BounceQueryParams))
			{
				//DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 32.f, 6, FColor::Purple, false, 3.f, 0, 4.f);
				if (Hit.GetComponent())
				{
					//UE_LOG(LogTemp, Warning, TEXT("COMPONENT HIT :: %s"), *GetNameSafe(Hit.GetComponent()));
					IgnoredStaticComponents.Add(Hit.GetComponent());
					BounceQueryParams.AddIgnoredComponent(Hit.GetComponent());
				}
				
				const FVector ProjectileEndLocation = Hit.bBlockingHit ? Hit.Location : Hit.TraceEnd;
				EndLocation = ProjectileEndLocation;
				//UE_LOG(LogTemp, Warning, TEXT("ENDLOCATION = %s"), *EndLocation.ToString());
				
				const FVector ProjectileDirection = (FinalLocation - PreviousTracePoint).GetSafeNormal();
				BounceDirection = FMath::GetReflectionVector(ProjectileDirection, Hit.ImpactNormal);

				const float TraceRemainder = Hit.bBlockingHit ? TraceDuration - UKismetMathLibrary::SafeDivide(FVector::Distance(ProjectileEndLocation, Hit.TraceStart), CurrentSpeed) : 0.f;
				const float AdjustedTraceTime = TraceTime - TraceRemainder;
				EndTime = AdjustedTraceTime;
				
				const float NormalizedEndTime = FMath::Clamp<float>(UKismetMathLibrary::SafeDivide(CorrectedTime + InPreviousTime + AdjustedTraceTime, ProjectileLifespan), 0.f, 1.f);
				//UE_LOG(LogTemp, Warning, TEXT("BOUNCE END TIME :: %f + %f + %f / %f = %f"), CorrectedTime, InPreviousTime, AdjustedTraceTime, ProjectileLifespan, NormalizedEndTime);
				//UE_LOG(LogTemp, Warning, TEXT("Bounce -> InDuration %f"), InDuration);
				Bounce.EndTime = NormalizedEndTime;
				//UE_LOG(LogTemp, Warning, TEXT("Bounce.EndTime = %f"), NormalizedEndTime);
				PreviousTracePoint = ProjectileEndLocation;
				break;
			}
			PreviousTracePoint = FinalLocation;
		}
	}
	else
	{
		Bounce.EndTime = 999.f;
	}
	//UE_LOG(LogTemp, Warning, TEXT("Bounce.EndTime(%f)"), Bounce.EndTime);
	return Bounce;
}

void AZoransProjectileBouncing::ReceiveServerAdjustedBounce(TArray<FBouncingProjectilePathSegment> InNewBouncePath)
{
	TimeAdjustment = 0.f;
	bSynced = true;
	BouncePath = InNewBouncePath;
	if (GetNetMode() == NM_Client)
	{
		//UE_LOG(LogTemp, Error, TEXT("ReceiveServerAdjustedBounce"));
		//const float LifetimeAlpha = GetServerProjectileLifetime() / ProjectileLifespan;
		for (auto &x : BouncePath)
		{
			x.bSelected = false;
			x.bBounceTriggered = false;
			//UE_LOG(LogTemp, Warning, TEXT("BOUNCE EVENT TIME %f -> %f"), LifetimeAlpha, x.EndTime);
		}
		SetNextPathSegment();
		ClientBounce(CurrentPathSegment.SegmentStartLocation, CurrentPathSegment.SegmentLaunchDirection);
		CurrentPathSegment.bBounceTriggered = true;

		//DrawDebugLine(GetWorld(), CurrentPathSegment.SegmentStartLocation, CurrentPathSegment.SegmentStartLocation + (CurrentPathSegment.SegmentLaunchDirection * 1000.f), FColor::Purple, false, 5.f, 0, 6.f);
	}
}

void AZoransProjectileBouncing::SendServerBounceAdjustmentToAbilitySystemComponent(const int32 InProjectileIndex, const TArray<FBouncingProjectilePathSegment>& InBouncePath)
{
	if (InProjectileIndex > -1)
	{
		if (GetOwner() && !InBouncePath.IsEmpty())
		{
			UZoransAbilitySystemComponent* OwningASC = Cast<UZoransAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
			if (IsValid(OwningASC))
			{
				OwningASC->InternalProcessServerProjectileBounce(InProjectileIndex, InBouncePath);
			}
		}
	}
}