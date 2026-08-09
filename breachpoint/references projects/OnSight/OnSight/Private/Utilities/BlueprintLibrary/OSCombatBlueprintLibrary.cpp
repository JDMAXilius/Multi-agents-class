#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Characters/OSCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/OSCharacterMovementComponent.h"
#include "Engine/OverlapResult.h"
#include "Data/OSCombatTypes.h"
#include "Data/OSGameplayTags.h"
#include "Data/OSHitDamageContext.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimationAsset.h"
#include "Animations/Notifies/OSAnimNotifyState_OSTrackMotionWarpTarget.h"
#include "AnimNotifyState_MotionWarping.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"
#include "Core/OSPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameModeBase.h"
#include "GAS/Effects/GE_OSApplyDamage.h"
#include "GenericTeamAgentInterface.h"
#include "OSLogCategories.h"

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

namespace
{
/** Fills FOSGameplayEffectContext fields when the engine context wraps our custom type. No-op if not OS context. */
static void TryPopulateFOSDamageContext(
	FGameplayEffectContextHandle& EffectContext,
	AOSCharacter* Instigator,
	AActor* VictimActor,
	EOSAttackType AttackType,
	const FVector& HitLocation,
	FName ImpactCueSocketName)
{
	FOSGameplayEffectContext* OSCtx = nullptr;
	if (!TryGetOSGameplayEffectContext(EffectContext, OSCtx))
	{
		return;
	}

	OSCtx->HitInfo.HitLocation = HitLocation;
	OSCtx->HitInfo.AttackInfo.Type = AttackType;
	if (ImpactCueSocketName != NAME_None)
	{
		OSCtx->CueSocketName = ImpactCueSocketName;
	}

	if (APawn* InstigatorPawn = Cast<APawn>(Instigator))
	{
		if (APlayerState* PS = InstigatorPawn->GetPlayerState())
		{
			OSCtx->HitInfo.AttackInfo.InstigatorPlayerStateUniqueId = PS->GetUniqueId();
		}
	}

	if (APawn* VictimPawn = Cast<APawn>(VictimActor))
	{
		if (APlayerState* PS = VictimPawn->GetPlayerState())
		{
			OSCtx->HitInfo.AttackInfo.VictimPlayerStateUniqueId = PS->GetUniqueId();
		}
	}
}

static bool ApplyOutgoingDamageSpec(
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* VictimASC,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	float Damage,
	const FGameplayEffectContextHandle& EffectContext)
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	if (!Tags.Data_Damage.IsValid())
	{
		static bool bLoggedMissingDamageTag = false;
		if (!bLoggedMissingDamageTag)
		{
			UE_LOG(LogOSCombat, Warning, TEXT("ApplyOutgoingDamageSpec: Tags.Data_Damage is invalid; check DefaultGameplayTags.ini / OSGameplayTags."));
			bLoggedMissingDamageTag = true;
		}
		return false;
	}

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Damage, Damage);
	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), VictimASC);
	return true;
}
} // namespace

#if !UE_BUILD_SHIPPING
// --- Debug: Rotation Visualization CVar ---
// Not static: referenced by OSHUD.cpp via extern for debug overlay drawing.
bool GDebugRotationViz = false;
static FAutoConsoleVariableRef CVarRotationViz(
	TEXT("os.debug.RotationViz"),
	GDebugRotationViz,
	TEXT("Draw rotation system debug visualization (MW/CMC arrows, auto-target cone, HUD text)"),
	ECVF_Default);
#endif

// ========================================
// TARGET ACQUISITION
// ========================================

AOSCharacter* UOSCombatBlueprintLibrary::BoxTraceForTarget(
	const AActor* Instigator,
	const FGameplayTagContainer& ExcludeTags,
	float HalfExtent,
	float ForwardDistance)
{
	if (!IsValid(Instigator)) return nullptr;

	const FVector Start = Instigator->GetActorLocation();
	const FVector End = Start + Instigator->GetActorForwardVector() * ForwardDistance;
	const FVector Half(HalfExtent);

	FHitResult Hit;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Instigator);

	if (!Instigator->GetWorld()->SweepSingleByObjectType(
		Hit, Start, End, FQuat::Identity,
		ObjParams, FCollisionShape::MakeBox(Half), QueryParams))
	{
		return nullptr;
	}

	AOSCharacter* Target = Cast<AOSCharacter>(Hit.GetActor());
	if (!IsValid(Target)) return nullptr;

	// Reject targets behind the instigator (box sweep starts at center, catches all directions)
	FVector ToTarget = Target->GetActorLocation() - Start;
	ToTarget.Z = 0.f;
	if (FVector::DotProduct(Instigator->GetActorForwardVector().GetSafeNormal2D(),
		ToTarget.GetSafeNormal()) < 0.f)
	{
		return nullptr;
	}

	// Reject targets with any excluded tag
	if (ExcludeTags.Num() > 0)
	{
		if (UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent())
		{
			if (TargetASC->HasAnyMatchingGameplayTags(ExcludeTags))
			{
				return nullptr;
			}
		}
	}

	// Team filter: reject same-team targets so grab (GA_OSGrab) doesn't lock onto teammates.
	// BoxTraceForTarget is the sole target acquisition path for grabs — filtering here
	// prevents the entire grab sequence (teleport, collision disable, victim lockout).
	// No-op in FFA (AreActorsFriendly returns false when either actor has NoTeam).
	if (AreActorsFriendly(Instigator, Target)) return nullptr;

	return Target;
}

AOSCharacter* UOSCombatBlueprintLibrary::FindNearestTarget(
	const AActor* Origin,
	const FGameplayTagContainer& ExcludeTags,
	float Radius)
{
	if (!IsValid(Origin)) return nullptr;

	const UWorld* World = Origin->GetWorld();
	if (!World) return nullptr;

	const FVector Center = Origin->GetActorLocation();

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Origin);

	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
		ObjParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	AOSCharacter* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AOSCharacter* Candidate = Cast<AOSCharacter>(Overlap.GetActor());
		if (!IsValid(Candidate)) continue;

		if (ExcludeTags.Num() > 0)
		{
			if (UAbilitySystemComponent* CandidateASC = Candidate->GetAbilitySystemComponent())
			{
				if (CandidateASC->HasAnyMatchingGameplayTags(ExcludeTags))
				{
					continue;
				}
			}
		}

		// Team filter: standalone targeting path (doesn't use GatherTargetCandidates).
		// BlueprintPure — BP designers may call this directly, so it must be team-safe.
		if (AreActorsFriendly(Origin, Candidate)) continue;

		const float DistSq = FVector::DistSquared(Center, Candidate->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Candidate;
		}
	}

	return Nearest;
}

// Composes: GatherTargetCandidates → cone filter → HasLineOfSightToTarget → ScoreTargetCandidate.
// Cone filter is the only step not shared with FindBestScoredTarget.
AOSCharacter* UOSCombatBlueprintLibrary::FindBestTargetInCone(
	const AActor* Origin,
	const FVector& Direction2D,
	const FGameplayTagContainer& ExcludeTags,
	float ConeAngleDegrees,
	float Radius)
{
	if (!IsValid(Origin)) return nullptr;

	const FVector Dir2D = Direction2D.GetSafeNormal2D();
	if (Dir2D.IsNearlyZero()) return nullptr;

	const float ConeHalfCos = FMath::Cos(FMath::DegreesToRadians(ConeAngleDegrees * 0.5f));
	const FVector Center = Origin->GetActorLocation();

	// Stage 1: Sphere overlap + tag exclusion (shared building block)
	TArray<AOSCharacter*> Candidates = GatherTargetCandidates(Origin, ExcludeTags, Radius);

	AOSCharacter* Best = nullptr;
	float BestScore = 0.f;

	for (AOSCharacter* Candidate : Candidates)
	{
		// Stage 2: Cone angle filter (unique to this function)
		FVector ToCandidate = Candidate->GetActorLocation() - Center;
		ToCandidate.Z = 0.f;
		const FVector ToCandidateDir = ToCandidate.GetSafeNormal();
		if (ToCandidateDir.IsNearlyZero()) continue;

		if (FVector::DotProduct(Dir2D, ToCandidateDir) < ConeHalfCos) continue;

		// Stage 3: LOS check (shared building block)
		if (!HasLineOfSightToTarget(Origin, Candidate)) continue;

		// Stage 4: Distance + angle scoring (shared building block)
		const float Score = ScoreTargetCandidate(Origin, Dir2D, Candidate, Radius);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	return Best;
}


TArray<AOSCharacter*> UOSCombatBlueprintLibrary::GatherTargetCandidates(
	const AActor* Origin,
	const FGameplayTagContainer& ExcludeTags,
	float Radius)
{
	TArray<AOSCharacter*> Result;
	if (!IsValid(Origin)) return Result;

	const UWorld* World = Origin->GetWorld();
	if (!World) return Result;

	const FVector Center = Origin->GetActorLocation();

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Origin);

	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
		ObjParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AOSCharacter* Candidate = Cast<AOSCharacter>(Overlap.GetActor());
		if (!IsValid(Candidate)) continue;

		if (ExcludeTags.Num() > 0)
		{
			if (UAbilitySystemComponent* CandidateASC = Candidate->GetAbilitySystemComponent())
			{
				if (CandidateASC->HasAnyMatchingGameplayTags(ExcludeTags))
				{
					continue;
				}
			}
		}

		// Team filter: this is the shared building block for all scored/cone targeting.
		// Filtering here cascades to: FindBestTargetInCone, FindBestScoredTarget,
		// ResolveAttackDirectionInCone (soft targeting), and all motion warp target selection.
		// Without this, soft targeting locks onto teammates and motion warping lunges at them.
		if (AreActorsFriendly(Origin, Candidate)) continue;

		Result.Add(Candidate);
	}

	return Result;
}

bool UOSCombatBlueprintLibrary::HasLineOfSightToTarget(
	const AActor* Origin,
	const AActor* Target,
	float HeightOffset)
{
	if (!IsValid(Origin) || !IsValid(Target)) return false;

	const UWorld* World = Origin->GetWorld();
	if (!World) return false;

	FVector Start = Origin->GetActorLocation();
	Start.Z += HeightOffset;

	FVector End = Target->GetActorLocation();
	End.Z += HeightOffset;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Origin);
	QueryParams.AddIgnoredActor(Target);

	return !World->LineTraceTestByChannel(Start, End, ECC_Visibility, QueryParams);
}

float UOSCombatBlueprintLibrary::ScoreTargetCandidate(
	const AActor* Origin,
	const FVector& ReferenceDirection2D,
	const AActor* Candidate,
	float MaxRange,
	float DistanceWeight,
	float AngleWeight)
{
	if (!IsValid(Origin) || !IsValid(Candidate) || MaxRange <= 0.f) return 0.f;

	FVector ToCandidate = Candidate->GetActorLocation() - Origin->GetActorLocation();
	ToCandidate.Z = 0.f;

	const float Distance2D = ToCandidate.Size();
	if (Distance2D > MaxRange) return 0.f;

	const float DistanceScore = FMath::Clamp(1.f - (Distance2D / MaxRange), 0.f, 1.f);

	const FVector ToCandidateDir = ToCandidate.GetSafeNormal();
	const FVector RefDir = ReferenceDirection2D.GetSafeNormal2D();
	const float AngleScore = RefDir.IsNearlyZero()
		? 0.f
		: FMath::Max(0.f, FVector::DotProduct(RefDir, ToCandidateDir));

	return DistanceScore * DistanceWeight + AngleScore * AngleWeight;
}

AOSCharacter* UOSCombatBlueprintLibrary::FindBestScoredTarget(
	const AActor* Origin,
	const FVector& ReferenceDirection2D,
	const FGameplayTagContainer& ExcludeTags,
	float MaxRange,
	float DistanceWeight,
	float AngleWeight,
	bool bRequireLOS,
	float LOSHeightOffset)
{
	TArray<AOSCharacter*> Candidates = GatherTargetCandidates(Origin, ExcludeTags, MaxRange);

	AOSCharacter* Best = nullptr;
	float BestScore = 0.f;

	for (AOSCharacter* Candidate : Candidates)
	{
		if (bRequireLOS && !HasLineOfSightToTarget(Origin, Candidate, LOSHeightOffset))
		{
			continue;
		}

		const float Score = ScoreTargetCandidate(Origin, ReferenceDirection2D, Candidate, MaxRange, DistanceWeight, AngleWeight);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	return Best;
}

bool UOSCombatBlueprintLibrary::IsValidCombatTarget(const AActor* Origin, const AActor* Candidate)
{
	if (!Candidate)
		return false;

	if (!Origin || Candidate == Origin)
		return false;

	if (!Candidate->IsA<APawn>())
		return false;

	if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate))
	{
		const FOSGameplayTags& Tags = FOSGameplayTags::Get();
		if (Tags.IsDead.IsValid() && TargetASC->HasMatchingGameplayTag(Tags.IsDead))
		{
			return false;
		}
	}

	// Team filter: covers FindBestSoftTarget (used by OSAnimNotifyState_OSTrackMotionWarpTarget
	// for mid-animation fallback target re-acquisition). Without this, the MW notify could
	// re-acquire a teammate as the warp target during an attack animation.
	if (AreActorsFriendly(Origin, Candidate)) return false;

	return true;
}

namespace
{
	constexpr float SoftTargetPointZOffset = 90.0f;
	constexpr float SoftScoreDistanceWeight = 1.0f;
	constexpr float SoftScoreAngleWeight = 2.0f;

	FVector GetSoftTargetPoint(const AActor* Target)
	{
		return Target ? (Target->GetActorLocation() + FVector(0, 0, SoftTargetPointZOffset)) : FVector::ZeroVector;
	}
}

AActor* UOSCombatBlueprintLibrary::FindBestSoftTarget(
	const AActor* Origin,
	float MaxRange,
	float SoftLockArcDeg,
	bool bDrawDebug)
{
	if (!IsValid(Origin) || MaxRange <= 0.f)
		return nullptr;

	UWorld* World = Origin->GetWorld();
	if (!World)
		return nullptr;

	FVector ViewStart = Origin->GetActorLocation();
	FRotator ViewRot = Origin->GetActorRotation();
	if (const APawn* Pawn = Cast<APawn>(Origin))
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			PC->GetPlayerViewPoint(ViewStart, ViewRot);
		}
	}

	const FVector ViewForward = ViewRot.Vector().GetSafeNormal();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(SoftLockArcDeg * 0.5f));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(OS_BPFL_SoftTarget), false);
	Params.AddIgnoredActor(Origin);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(
		Overlaps,
		Origin->GetActorLocation(),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(MaxRange),
		Params
	);

	AActor* BestTarget = nullptr;
	float BestScore = -FLT_MAX;
	FVector BestTargetPoint = FVector::ZeroVector;

	for (const FOverlapResult& R : Overlaps)
	{
		AActor* Candidate = R.GetActor();
		if (!IsValidCombatTarget(Origin, Candidate))
			continue;

		const FVector TargetPoint = GetSoftTargetPoint(Candidate);
		const FVector ToTarget = (TargetPoint - ViewStart);
		const float DistSq = ToTarget.SizeSquared();
		if (DistSq <= KINDA_SMALL_NUMBER)
			continue;

		const float Dist = FMath::Sqrt(DistSq);
		if (Dist > MaxRange)
			continue;

		const FVector Dir = ToTarget / Dist;
		const float Dot = FVector::DotProduct(ViewForward, Dir);
		if (Dot < MinDot)
			continue;

		FHitResult Hit;
		const bool bHitSomething = World->LineTraceSingleByChannel(Hit, ViewStart, TargetPoint, ECC_Visibility, Params);
		if (bHitSomething && Hit.GetActor() != Candidate)
			continue;

		const float DistanceScore = FMath::Clamp(1.0f - (Dist / MaxRange), 0.0f, 1.0f);
		const float AngleScore = (Dot + 1.0f) * 0.5f;
		const float Score = (DistanceScore * SoftScoreDistanceWeight) + (AngleScore * SoftScoreAngleWeight);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
			BestTargetPoint = TargetPoint;
		}
	}

#if !UE_BUILD_SHIPPING
	if (bDrawDebug)
	{
		constexpr float DebugDuration = 2.0f;
		constexpr float DebugLineThickness = 2.0f;

		DrawDebugSphere(World, Origin->GetActorLocation(), MaxRange, 24, FColor::Cyan, false, DebugDuration, 0, 1.0f);

		const FColor BaseColor = BestTarget ? FColor::Green : FColor::Red;
		DrawDebugSphere(World, ViewStart, 6.0f, 8, BaseColor, false, DebugDuration, 0, 1.5f);

		const FVector LineEnd = BestTarget ? BestTargetPoint : (ViewStart + ViewForward * MaxRange);
		DrawDebugLine(World, ViewStart, LineEnd, BaseColor, false, DebugDuration, 0, DebugLineThickness);
		DrawDebugDirectionalArrow(World, ViewStart, LineEnd, 30.0f, BaseColor, false, DebugDuration, 0, DebugLineThickness);

		if (BestTarget)
		{
			const FVector SphereLoc = GetSoftTargetPoint(BestTarget);
			DrawDebugSphere(World, SphereLoc, 35.0f, 16, FColor::Green, false, DebugDuration, 0, 2.0f);
			DrawDebugString(World, SphereLoc + FVector(0, 0, 25.0f),
				FString::Printf(TEXT("Soft Target: %s"), *GetNameSafe(BestTarget)), nullptr, FColor::White, DebugDuration, false);
		}
	}
#endif

	return BestTarget;
}

// ========================================
// DIRECTION & GEOMETRY
// ========================================

FTransform UOSCombatBlueprintLibrary::ComputeGrabPlacement(
	const AOSCharacter* Attacker,
	const AOSCharacter* Victim,
	float CenterDistance)
{
	if (!IsValid(Attacker) || !IsValid(Victim))
	{
		return FTransform::Identity;
	}

	CenterDistance = FMath::Max(0.f, CenterDistance);

	const FVector AttackerLoc = Attacker->GetActorLocation();
	FVector PlaceDir = (Victim->GetActorLocation() - AttackerLoc);
	PlaceDir.Z = 0.f;
	PlaceDir = PlaceDir.GetSafeNormal();
	if (PlaceDir.IsNearlyZero())
	{
		PlaceDir = Attacker->GetActorForwardVector().GetSafeNormal2D();
	}

	FVector VictimLoc = AttackerLoc + PlaceDir * CenterDistance;
	VictimLoc.Z = Victim->GetActorLocation().Z;

	const float PlaceYaw = PlaceDir.Rotation().Yaw;
	const FRotator VictimRot(0.f, PlaceYaw + 180.f, 0.f);

	return FTransform(VictimRot, VictimLoc);
}

FVector UOSCombatBlueprintLibrary::ComputeApproachDirection2D(const AActor* From, const AActor* To)
{
	if (!IsValid(From) || !IsValid(To))
	{
		return FVector::ForwardVector;
	}

	FVector Dir = To->GetActorLocation() - From->GetActorLocation();
	Dir.Z = 0.f;
	return Dir.GetSafeNormal();
}

EOSDirection UOSCombatBlueprintLibrary::ComputeDirection4Way(const AActor* Target, const FVector& ImpactPoint)
{
	if (!IsValid(Target)) return EOSDirection::FRONT;

	FVector ToImpact = ImpactPoint - Target->GetActorLocation();
	ToImpact.Z = 0.f;

	if (ToImpact.IsNearlyZero()) return EOSDirection::FRONT;
	ToImpact.Normalize();

	const FVector Fwd = Target->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Target->GetActorRightVector().GetSafeNormal2D();

	const float DotFwd = FVector::DotProduct(Fwd, ToImpact);
	const float DotRight = FVector::DotProduct(Right, ToImpact);

	// Prefer LEFT/RIGHT when impact is clearly from the side (so side hits don't collapse to FRONT/BACK).
	// Factor > 1: use front/back only when impact is more frontal than lateral (wider side cones).
	static const float FrontBackBias = 1.15f;
	if (FMath::Abs(DotFwd) >= FrontBackBias * FMath::Abs(DotRight))
	{
		return (DotFwd >= 0.f) ? EOSDirection::FRONT : EOSDirection::BACK;
	}
	return (DotRight >= 0.f) ? EOSDirection::RIGHT : EOSDirection::LEFT;
}

EOSDirection UOSCombatBlueprintLibrary::ComputeDirection4WayFromHit(const AActor* Target, const FHitResult& Hit)
{
	return ComputeDirection4Way(Target, Hit.ImpactPoint);
}

bool UOSCombatBlueprintLibrary::IsActorWithinAngle(const AActor* Origin, const AActor* Other, float AngleDegrees)
{
	if (!IsValid(Origin) || !IsValid(Other)) return false;

	// Project to 2D: elevation differences should not affect the cone check.
	FVector ToOther = Other->GetActorLocation() - Origin->GetActorLocation();
	ToOther.Z = 0.f;
	ToOther = ToOther.GetSafeNormal();

	FVector Forward = Origin->GetActorForwardVector();
	Forward.Z = 0.f;
	Forward = Forward.GetSafeNormal();

	if (ToOther.IsNearlyZero() || Forward.IsNearlyZero()) return false;

	const float HalfRad = FMath::DegreesToRadians(FMath::Clamp(AngleDegrees, 0.f, 360.f) * 0.5f);
	const float MinDot = FMath::Cos(HalfRad);
	return FVector::DotProduct(Forward, ToOther) >= MinDot;
}

float UOSCombatBlueprintLibrary::YawFromDirection2D(const FVector& Direction)
{
	FVector Dir2D(Direction.X, Direction.Y, 0.f);
	if (Dir2D.IsNearlyZero()) return 0.f;
	return Dir2D.Rotation().Yaw;
}

FVector UOSCombatBlueprintLibrary::GetCameraForwardDirection2D(const ACharacter* Character)
{
	if (!IsValid(Character)) return FVector::ZeroVector;

	// Only player controllers have a meaningful camera/control rotation.
	// AI controllers return stale or zero control rotation, so fall through to actor forward.
	if (const APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		const float ControlYaw = PC->GetControlRotation().Yaw;
		return FRotator(0.f, ControlYaw, 0.f).Vector();
	}

	FVector Fwd = Character->GetActorForwardVector();
	Fwd.Z = 0.f;
	return Fwd.GetSafeNormal();
}

FVector UOSCombatBlueprintLibrary::GetAimDirection2D(const ACharacter* Character)
{
	if (!IsValid(Character))
	{
		return FVector::ForwardVector;
	}

	FVector Dir = GetInputDirection2D(Character);
	if (!Dir.IsNearlyZero()) return Dir;

	Dir = GetCameraForwardDirection2D(Character);
	if (!Dir.IsNearlyZero()) return Dir;

	Dir = Character->GetActorForwardVector();
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();
	return Dir.IsNearlyZero() ? FVector::ForwardVector : Dir;
}

FVector UOSCombatBlueprintLibrary::GetLastMovementDirection2D(const ACharacter* Character)
{
	if (!IsValid(Character))
	{
		return FVector::ForwardVector;
	}

	// Server sync fix: Use GetInputDirection2D which correctly falls back to GetCurrentAcceleration()
	// since GetLastInputVector() is always zero on the server for autonomous proxies.
	FVector InputDir = GetInputDirection2D(Character);
	if (!InputDir.IsNearlyZero())
	{
		return InputDir;
	}

	// Player wasn't moving — fall back to camera forward > actor forward.
	return GetCameraForwardDirection2D(Character);
}

FRotator UOSCombatBlueprintLibrary::ComputeAttackDirection(const ACharacter* Character)
{
	if (!IsValid(Character)) return FRotator::ZeroRotator;

	// AI / unpossessed pawns: use actor forward (no camera to blend with)
	if (!Cast<APlayerController>(Character->GetController()))
	{
		FVector Fwd = Character->GetActorForwardVector();
		Fwd.Z = 0.f;
		return Fwd.IsNearlyZero() ? FRotator::ZeroRotator : FRotator(0.f, Fwd.Rotation().Yaw, 0.f);
	}

	// Camera forward (yaw only — no pitch in melee attacks)
	const FRotator YawOnly(0.f, Character->GetControlRotation().Yaw, 0.f);

	// Check for stick input
	const FVector InputVector = Character->GetCharacterMovement()->GetLastInputVector();
	if (InputVector.SizeSquared() <= 0.01f)
	{
		// No stick input — attack where camera is looking
		return YawOnly;
	}

	// GoW-style blend: 60% camera forward + 40% stick direction
	// Prevents wild off-angle swings while still respecting intentional inputs
	const FVector CameraForward = FRotationMatrix(YawOnly).GetScaledAxis(EAxis::X);
	const FVector CameraRight = FRotationMatrix(YawOnly).GetScaledAxis(EAxis::Y);

	FVector WorldInput = CameraForward * InputVector.X + CameraRight * InputVector.Y;
	WorldInput.Z = 0.f;
	WorldInput.Normalize();

	FVector BlendedDir = FMath::Lerp(CameraForward, WorldInput, 0.4f);
	BlendedDir.Normalize();

	return BlendedDir.Rotation();
}

FVector UOSCombatBlueprintLibrary::BlendDirectionWithInput(
	const FVector& TargetDir,
	const FVector& InputDir,
	float InputInfluence)
{
	if (InputDir.IsNearlyZero() || InputInfluence <= KINDA_SMALL_NUMBER)
	{
		return TargetDir;
	}

	// Angular interpolation: rotate TargetDir toward InputDir by a fraction of the angle between them.
	// Component-wise Lerp on direction vectors is non-linear in angle space and snaps at higher values.
	const FVector T = TargetDir.GetSafeNormal2D();
	const FVector I = InputDir.GetSafeNormal2D();
	if (T.IsNearlyZero() || I.IsNearlyZero()) return TargetDir;

	const float AngleDeg = FMath::Acos(FMath::Clamp(FVector::DotProduct(T, I), -1.f, 1.f)) * (180.f / UE_PI);
	if (AngleDeg < KINDA_SMALL_NUMBER) return TargetDir;

	const float RotateDeg = AngleDeg * FMath::Clamp(InputInfluence, 0.f, 1.f);
	const FVector Cross = FVector::CrossProduct(T, I);
	const float Sign = (Cross.Z >= 0.f) ? 1.f : -1.f;
	const FRotator Rot(0.f, Sign * RotateDeg, 0.f);
	return Rot.RotateVector(T).GetSafeNormal2D();
}

FVector UOSCombatBlueprintLibrary::BlendDirectionMagnetic(
	const FVector& TargetDir,
	const FVector& InputDir,
	float Stickiness,
	float BreakawayAngleDegrees)
{
	if (InputDir.IsNearlyZero()) return TargetDir;

	const FVector TDir = TargetDir.GetSafeNormal2D();
	const FVector IDir = InputDir.GetSafeNormal2D();
	if (TDir.IsNearlyZero() || IDir.IsNearlyZero()) return TargetDir;

	const float Dot = FMath::Clamp(FVector::DotProduct(TDir, IDir), -1.f, 1.f);
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));

	FVector Result;
	if (AngleDeg < BreakawayAngleDegrees)
	{
		// Within breakaway zone: magnet pulls toward target
		const float MagnetAlpha = FMath::Clamp(Stickiness, 0.f, 1.f) * (1.f - AngleDeg / FMath::Max(BreakawayAngleDegrees, KINDA_SMALL_NUMBER));
		const FQuat QTarget = TDir.ToOrientationQuat();
		const FQuat QInput = IDir.ToOrientationQuat();
		Result = FQuat::Slerp(QInput, QTarget, MagnetAlpha).GetForwardVector();
	}
	else
	{
		// Beyond breakaway: smooth release over 30-degree transition zone
		const float OvershootAlpha = FMath::Clamp((AngleDeg - BreakawayAngleDegrees) / 30.f, 0.f, 1.f);
		const FQuat QTarget = TDir.ToOrientationQuat();
		const FQuat QInput = IDir.ToOrientationQuat();
		Result = FQuat::Slerp(QTarget, QInput, OvershootAlpha).GetForwardVector();
	}

	Result.Z = 0.f;
	const FVector Final = Result.GetSafeNormal();
	return Final.IsNearlyZero() ? TargetDir : Final;
}

FVector UOSCombatBlueprintLibrary::BlendDirectionWeightedAngle(
	const FVector& TargetDir,
	const FVector& InputDir,
	float DeadZoneAngleDegrees,
	float FullControlAngleDegrees,
	float CurveExponent)
{
	if (InputDir.IsNearlyZero()) return TargetDir;

	const FVector TDir = TargetDir.GetSafeNormal2D();
	const FVector IDir = InputDir.GetSafeNormal2D();
	if (TDir.IsNearlyZero() || IDir.IsNearlyZero()) return TargetDir;

	const float Dot = FMath::Clamp(FVector::DotProduct(TDir, IDir), -1.f, 1.f);
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));

	if (AngleDeg <= DeadZoneAngleDegrees) return TDir;
	if (AngleDeg >= FullControlAngleDegrees) return IDir;

	const float Range = FMath::Max(FullControlAngleDegrees - DeadZoneAngleDegrees, KINDA_SMALL_NUMBER);
	const float T = (AngleDeg - DeadZoneAngleDegrees) / Range;
	const float Alpha = FMath::Pow(T, FMath::Max(CurveExponent, KINDA_SMALL_NUMBER));

	const FQuat QTarget = TDir.ToOrientationQuat();
	const FQuat QInput = IDir.ToOrientationQuat();
	FVector Result = FQuat::Slerp(QTarget, QInput, Alpha).GetForwardVector();
	Result.Z = 0.f;
	const FVector Final = Result.GetSafeNormal();
	return Final.IsNearlyZero() ? TargetDir : Final;
}

FVector UOSCombatBlueprintLibrary::BlendDirectionConeClamp(
	const FVector& TargetDir,
	const FVector& InputDir,
	float MaxDeviationAngleDegrees,
	float InputWeight)
{
	if (InputDir.IsNearlyZero()) return TargetDir;

	const FVector TDir = TargetDir.GetSafeNormal2D();
	const FVector IDir = InputDir.GetSafeNormal2D();
	if (TDir.IsNearlyZero() || IDir.IsNearlyZero()) return TargetDir;

	// Blend input direction with target based on InputWeight
	const FQuat QTarget = TDir.ToOrientationQuat();
	const FQuat QInput = IDir.ToOrientationQuat();
	FVector DesiredDir = FQuat::Slerp(QTarget, QInput, FMath::Clamp(InputWeight, 0.f, 1.f)).GetForwardVector();
	DesiredDir.Z = 0.f;
	DesiredDir = DesiredDir.GetSafeNormal();
	if (DesiredDir.IsNearlyZero()) return TDir;

	// Check if desired direction is within cone
	const float Dot = FMath::Clamp(FVector::DotProduct(TDir, DesiredDir), -1.f, 1.f);
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));

	if (AngleDeg <= MaxDeviationAngleDegrees) return DesiredDir;

	// Clamp: rotate target toward desired by MaxDeviation only
	const float ClampAlpha = MaxDeviationAngleDegrees / FMath::Max(AngleDeg, KINDA_SMALL_NUMBER);
	FVector Clamped = FQuat::Slerp(QTarget, DesiredDir.ToOrientationQuat(), ClampAlpha).GetForwardVector();
	Clamped.Z = 0.f;
	const FVector Final = Clamped.GetSafeNormal();
	return Final.IsNearlyZero() ? TDir : Final;
}

FVector UOSCombatBlueprintLibrary::DispatchBlendAlgorithm(
	const FVector& BaseDir,
	const FVector& InputDir,
	const FOSAttackDirectionParams& Params)
{
	switch (Params.BlendAlgorithm)
	{
	case EOSBlendAlgorithm::LinearLerp:
		return BlendDirectionWithInput(BaseDir, InputDir, Params.InputInfluence);
	case EOSBlendAlgorithm::Magnetic:
		return BlendDirectionMagnetic(BaseDir, InputDir, Params.MagneticStickiness, Params.MagneticBreakawayAngle);
	case EOSBlendAlgorithm::WeightedAngle:
		return BlendDirectionWeightedAngle(BaseDir, InputDir, Params.DeadZoneAngle, Params.FullControlAngle, Params.BlendCurveExponent);
	case EOSBlendAlgorithm::ConeClamp:
		return BlendDirectionConeClamp(BaseDir, InputDir, Params.MaxDeviationAngle, Params.ConeInputWeight);
	default:
		return BaseDir;
	}
}

bool UOSCombatBlueprintLibrary::ResolveAttackDirectionInCone(
	AOSCharacter* Character,
	const FOSAttackDirectionParams& Params,
	FVector& OutFinalDir2D,
	AOSCharacter*& OutResolvedTarget)
{
	OutResolvedTarget = nullptr;
	if (!IsValid(Character) || IsIncapacitated(Character))
	{
		OutFinalDir2D = GetAimDirection2D(Character);
		return false;
	}

	const FVector AimDir = GetAimDirection2D(Character);
	const FVector InputDir = GetInputDirection2D(Character);

	AOSCharacter* ResolvedTarget = FindBestTargetInCone(
		Character, AimDir, Params.ExcludeTags, Params.ConeAngleDegrees, Params.MaxRange);

	// Character-forward fallback: when camera-cone missed and no stick input,
	// retry from character forward to catch targets the camera isn't facing.
	if (!ResolvedTarget && InputDir.IsNearlyZero())
	{
		FVector CharFwd = Character->GetActorForwardVector();
		CharFwd.Z = 0.f;
		CharFwd = CharFwd.GetSafeNormal();

		if (!CharFwd.IsNearlyZero() && FVector::DotProduct(AimDir, CharFwd) < 0.99f)
		{
			ResolvedTarget = FindBestTargetInCone(
				Character, CharFwd, Params.ExcludeTags, Params.ConeAngleDegrees, Params.MaxRange);
		}
	}

	if (!ResolvedTarget)
	{
		OutFinalDir2D = AimDir;
		return false;
	}

	const FVector ToTarget = ComputeApproachDirection2D(Character, ResolvedTarget);
	FVector FinalDir = DispatchBlendAlgorithm(ToTarget, InputDir, Params);

	FinalDir.Z = 0.f;
	OutFinalDir2D = FinalDir.GetSafeNormal();
	if (OutFinalDir2D.IsNearlyZero())
	{
		OutFinalDir2D = AimDir;
	}
	OutResolvedTarget = ResolvedTarget;
	return true;
}

// ========================================
// POSITION WARP PLACEMENT
// ========================================

float UOSCombatBlueprintLibrary::GetCapsuleStopDistance(const ACharacter* A, const ACharacter* B)
{
	if (!IsValid(A) || !IsValid(B)) return 0.f;
	return A->GetCapsuleComponent()->GetScaledCapsuleRadius()
		 + B->GetCapsuleComponent()->GetScaledCapsuleRadius();
}


float UOSCombatBlueprintLibrary::SweepWarpPath(
	const ACharacter* Instigator,
	const FVector& Start,
	const FVector& Direction,
	float Distance,
	const AActor* TargetToIgnore)
{
	if (!IsValid(Instigator) || Distance <= 0.f) return 0.f;

	const float CapsuleRadius = Instigator->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float CapsuleHalf = Instigator->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalf);

	const FVector End = Start + Direction * Distance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Instigator);
	if (IsValid(TargetToIgnore))
	{
		Params.AddIgnoredActor(TargetToIgnore);
	}

	FHitResult Hit;
	if (Instigator->GetWorld()->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity, ECC_Pawn, Capsule, Params))
	{
		// Shorten to hit point (pull back slightly to avoid clipping).
		return FMath::Max(0.f, Hit.Distance - 1.f);
	}

	return Distance;
}

float UOSCombatBlueprintLibrary::SweepBlindWarpPath(
	const ACharacter* Instigator,
	const FVector& Start,
	const FVector& Direction,
	float Distance)
{
	return SweepWarpPath(Instigator, Start, Direction, Distance, nullptr);
}

float UOSCombatBlueprintLibrary::SweepWarpPathMultiChannel(
	const ACharacter* Instigator,
	const FVector& Start,
	const FVector& Direction,
	float Distance,
	const AActor* TargetToIgnore)
{
	if (!IsValid(Instigator) || Distance <= 0.f) return 0.f;

	const float CapsuleRadius = Instigator->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float CapsuleHalf = Instigator->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalf);

	const FVector End = Start + Direction * Distance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Instigator);
	if (IsValid(TargetToIgnore))
	{
		Params.AddIgnoredActor(TargetToIgnore);
		
		// Also ignore everything attached to the target (weapons, shields, cosmetic parts)
		// so the sweep is purely for world obstacles or other pawns.
		TArray<AActor*> Attached;
		TargetToIgnore->GetAttachedActors(Attached, true);
		for (AActor* A : Attached) { Params.AddIgnoredActor(A); }
	}

	float SafeDist = Distance;

	// Sweep ECC_Pawn (other characters)
	FHitResult Hit;
	if (Instigator->GetWorld()->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity, ECC_Pawn, Capsule, Params))
	{
		SafeDist = FMath::Min(SafeDist, FMath::Max(0.f, Hit.Distance - 1.f));
	}

	// Sweep ECC_WorldStatic (walls, environment geometry)
	if (Instigator->GetWorld()->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity, ECC_WorldStatic, Capsule, Params))
	{
		SafeDist = FMath::Min(SafeDist, FMath::Max(0.f, Hit.Distance - 1.f));
	}

	return SafeDist;
}

float UOSCombatBlueprintLibrary::SweepBlindWarpPathMultiChannel(
	const ACharacter* Instigator,
	const FVector& Start,
	const FVector& Direction,
	float Distance)
{
	return SweepWarpPathMultiChannel(Instigator, Start, Direction, Distance, nullptr);
}

// ========================================
// ROOT MOTION ANALYSIS
// ========================================

bool UOSCombatBlueprintLibrary::GetSectionTimeRange(
	const UAnimMontage* Montage,
	FName SectionName,
	float& OutStartTime,
	float& OutEndTime)
{
	OutStartTime = 0.f;
	OutEndTime = 0.f;
	if (!Montage) return false;

	const int32 Idx = Montage->GetSectionIndex(SectionName);
	if (Idx == INDEX_NONE) return false;

	Montage->GetSectionStartAndEndTime(Idx, OutStartTime, OutEndTime);
	return true;
}

FTransform UOSCombatBlueprintLibrary::ExtractSectionRootMotion(
	const UAnimMontage* Montage,
	FName SectionName)
{
	float Start = 0.f, End = 0.f;
	if (!GetSectionTimeRange(Montage, SectionName, Start, End)) return FTransform::Identity;
	if (FMath::IsNearlyEqual(Start, End)) return FTransform::Identity;

	const FAnimExtractContext Context;
	return Montage->ExtractRootMotionFromTrackRange(Start, End, Context);
}

float UOSCombatBlueprintLibrary::GetSectionForwardDistance(
	const UAnimMontage* Montage,
	FName SectionName,
	const FVector& ForwardDir)
{
	const FTransform RM = ExtractSectionRootMotion(Montage, SectionName);
	const FVector Dir2D = ForwardDir.GetSafeNormal2D();
	if (Dir2D.IsNearlyZero()) return 0.f;

	FVector Translation = RM.GetTranslation();
	Translation.Z = 0.f;
	return FVector::DotProduct(Translation, Dir2D);
}

bool UOSCombatBlueprintLibrary::GetWarpWindowTimeRange(
	const UAnimMontage* Montage,
	FName WarpTargetName,
	float& OutStartTime,
	float& OutEndTime)
{
	OutStartTime = 0.f;
	OutEndTime = 0.f;
	if (!Montage || WarpTargetName.IsNone()) return false;

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const UAnimNotifyState_MotionWarping* MWNotify =
			Cast<UAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass);
		if (!MWNotify) continue;

		bool bMatch = false;

		// Check OS merged notify first (dynamic target names)
		if (const UOSAnimNotifyState_OSTrackMotionWarpTarget* OSNotify =
			Cast<UOSAnimNotifyState_OSTrackMotionWarpTarget>(MWNotify))
		{
			bMatch = (OSNotify->WarpTargetName == WarpTargetName);
		}
		else
		{
			// Vanilla MW: check template's WarpTargetName
			const URootMotionModifier_Warp* WarpMod =
				Cast<URootMotionModifier_Warp>(MWNotify->RootMotionModifier);
			bMatch = (WarpMod && WarpMod->WarpTargetName == WarpTargetName);
		}

		if (bMatch)
		{
			OutStartTime = NotifyEvent.GetTime();
			OutEndTime = OutStartTime + NotifyEvent.Duration;
			return true;
		}
	}

	return false;
}

FTransform UOSCombatBlueprintLibrary::ExtractWarpWindowRootMotion(
	const UAnimMontage* Montage,
	FName WarpTargetName)
{
	float Start = 0.f, End = 0.f;
	if (!GetWarpWindowTimeRange(Montage, WarpTargetName, Start, End)) return FTransform::Identity;
	if (FMath::IsNearlyEqual(Start, End)) return FTransform::Identity;

	const FAnimExtractContext Context;
	return Montage->ExtractRootMotionFromTrackRange(Start, End, Context);
}

bool UOSCombatBlueprintLibrary::HasAnyRootMotion(const UAnimInstance* AnimInstance)
{
	if (!IsValid(AnimInstance)) return false;
	return AnimInstance->GetRootMotionMontageInstance() != nullptr;
}

// ========================================
// DAMAGE APPLICATION
// ========================================

// Central team-awareness check used by all combat systems (targeting, traces, damage, projectiles).
// Replaces the previous ResolveDamageGameplayEffectClass + UGE_OSTDMApplyDamage parallel pipeline.
//
// Returns true ONLY when both actors have a real team AND are on the same team.
// Returns false (= allow interaction) for: null, self, no interface, NoTeam, or different teams.
//
// CRITICAL — NoTeam guard:
//   In FFA mode, no team is assigned → GetGenericTeamId() returns NoTeam (ID 255) for everyone.
//   The default attitude solver treats same-ID as Friendly: GetAttitude(255, 255) = Friendly.
//   Without the NoTeam guard, FFA would block ALL damage and targeting (everyone "friendly").
//   The guard short-circuits to false when either actor has NoTeam, so FFA is unaffected.
//
// AOSCharacter implements IGenericTeamAgentInterface, delegating to PlayerState->TeamId.
// TeamId is replicated (DOREPLIFETIME, COND_None) so clients have correct team info.
// Server-side checks (ExecCalc, targeting, traces) use authoritative TeamId directly.
bool UOSCombatBlueprintLibrary::AreActorsFriendly(const AActor* A, const AActor* B)
{
	if (!A || !B || A == B) return false;
	const IGenericTeamAgentInterface* TeamA = Cast<IGenericTeamAgentInterface>(A);
	const IGenericTeamAgentInterface* TeamB = Cast<IGenericTeamAgentInterface>(B);
	if (!TeamA || !TeamB) return false;
	const FGenericTeamId IdA = TeamA->GetGenericTeamId();
	const FGenericTeamId IdB = TeamB->GetGenericTeamId();
	if (IdA == FGenericTeamId::NoTeam || IdB == FGenericTeamId::NoTeam) return false;
	return FGenericTeamId::GetAttitude(IdA, IdB) == ETeamAttitude::Friendly;
}

bool UOSCombatBlueprintLibrary::ApplyDirectDamage(
	AOSCharacter* Instigator, AOSCharacter* Victim,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	float Damage, EOSAttackType AttackType)
{
	if (!IsValid(Instigator) || !IsValid(Victim)) return false;
	// Team filter: block GE application to friendlies BEFORE the effect is created.
	// This is the primary defense for ApplyDirectDamage callers (GA_OSFireCone, GA_OSGrab).
	// Must happen here (not just in ExecCalc) because the GE's dummy modifier + GameplayCue
	// fire even when ExecCalc returns early — blocking here prevents the hit sound cue entirely.
	if (AreActorsFriendly(Instigator, Victim)) return false;
	if (!DamageEffectClass) DamageEffectClass = UGE_OSApplyDamage::StaticClass();
	if (!DamageEffectClass) return false;

	UAbilitySystemComponent* SourceASC = Instigator->GetAbilitySystemComponent();
	UAbilitySystemComponent* VictimASC = Victim->GetAbilitySystemComponent();
	if (!SourceASC || !VictimASC) return false;

	// Reject damage on invincible or dead targets. Mirrors ApplyDamageFromHitResult —
	// both canonical damage functions must enforce these guards so melee, grab, and projectile
	// paths that go through ApplyDirectDamage don't bypass dodge i-frames or post-death damage.
	const FOSGameplayTags& GuardTags = FOSGameplayTags::Get();
	if (GuardTags.State_Invincibility.IsValid() && VictimASC->HasMatchingGameplayTag(GuardTags.State_Invincibility))
	{
		return false;
	}
	if (GuardTags.IsDead.IsValid() && VictimASC->HasMatchingGameplayTag(GuardTags.IsDead))
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(Instigator);
	TryPopulateFOSDamageContext(
		EffectContext, Instigator, Victim, AttackType, Victim->GetActorLocation(), NAME_None);

	return ApplyOutgoingDamageSpec(SourceASC, VictimASC, DamageEffectClass, Damage, EffectContext);
}

bool UOSCombatBlueprintLibrary::ApplyDamageFromHitResult(
	AOSCharacter* Instigator,
	AActor* HitActor,
	const FHitResult& HitResult,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	float Damage,
	EOSAttackType AttackType,
	FName ImpactCueSocketName,
	bool bDispatchAttackHitEvent)
{
	if (!IsValid(Instigator) || !IsValid(HitActor)) return false;
	if (AreActorsFriendly(Instigator, HitActor)) return false;

	UAbilitySystemComponent* SourceASC = Instigator->GetAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!SourceASC || !TargetASC) return false;

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	if (Tags.State_Invincibility.IsValid() && TargetASC->HasMatchingGameplayTag(Tags.State_Invincibility))
	{
		return false;
	}
	// Reject damage on dead targets. Prevents post-death damage cues/hits from firing when a
	// target has already entered the death state but damage events are still in flight.
	if (Tags.IsDead.IsValid() && TargetASC->HasMatchingGameplayTag(Tags.IsDead))
	{
		return false;
	}

	if (!DamageEffectClass) DamageEffectClass = UGE_OSApplyDamage::StaticClass();
	if (!DamageEffectClass) return false;

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(Instigator);
	EffectContext.AddHitResult(HitResult);
	TryPopulateFOSDamageContext(
		EffectContext, Instigator, HitActor, AttackType, HitResult.ImpactPoint, ImpactCueSocketName);

	if (!ApplyOutgoingDamageSpec(SourceASC, TargetASC, DamageEffectClass, Damage, EffectContext))
	{
		return false;
	}

	if (bDispatchAttackHitEvent && Tags.Event_AttackHit.IsValid())
	{
		FGameplayEventData EventData;
		EventData.Instigator = Instigator;
		EventData.Target = HitActor;
		EventData.ContextHandle = EffectContext;
		TargetASC->HandleGameplayEvent(Tags.Event_AttackHit, &EventData);
	}

	return true;
}

// ========================================
// MOTION WARPING
// ========================================

UMotionWarpingComponent* UOSCombatBlueprintLibrary::GetMotionWarpComponent(const AOSCharacter* Character)
{
	if (!IsValid(Character)) return nullptr;
	return Character->GetMotionWarpingComponent();
}

bool UOSCombatBlueprintLibrary::SetupRotationWarp(AOSCharacter* Character, FName TargetName, float TargetYaw)
{
	UMotionWarpingComponent* MWC = GetMotionWarpComponent(Character);
	if (!MWC) return false;

	// Place the target far away in the desired direction so root motion translation
	// during the warp window doesn't shift the angle. If the target were at the
	// character's feet, even small forward root motion would flip the facing direction.
	const FVector TargetDir = FRotationMatrix(FRotator(0.f, TargetYaw, 0.f)).GetUnitAxis(EAxis::X);
	const FVector FarPoint = Character->GetActorLocation() + TargetDir * 10000.f;
	const FTransform WarpTarget(FRotator(0.f, TargetYaw, 0.f), FarPoint);
	MWC->AddOrUpdateWarpTargetFromTransform(TargetName, WarpTarget);

#if !UE_BUILD_SHIPPING
	if (GDebugRotationViz)
	{
		UE_LOG(LogOSCombat, Log, TEXT("[RotViz] SetupRotationWarp: Target=%s Yaw=%.1f CharLoc=(%.0f,%.0f,%.0f) FarPoint=(%.0f,%.0f,%.0f)"),
			*TargetName.ToString(), TargetYaw,
			Character->GetActorLocation().X, Character->GetActorLocation().Y, Character->GetActorLocation().Z,
			FarPoint.X, FarPoint.Y, FarPoint.Z);
	}
#endif

	return true;
}

bool UOSCombatBlueprintLibrary::SetupPositionWarp(AOSCharacter* Character, FName TargetName, const FTransform& Target)
{
	UMotionWarpingComponent* MWC = GetMotionWarpComponent(Character);
	if (!MWC) return false;

	MWC->AddOrUpdateWarpTargetFromTransform(TargetName, Target);
	return true;
}

void UOSCombatBlueprintLibrary::ClearWarpTarget(AOSCharacter* Character, FName TargetName)
{
	UMotionWarpingComponent* MWC = GetMotionWarpComponent(Character);
	if (!MWC) return;

	// Just remove the named target. With per-hit indexed warp names ("AttackTarget_0", "_1", etc.),
	// old modifiers can't accidentally read new targets. The engine auto-disables modifiers whose
	// target name no longer exists (URootMotionModifier_Warp::Update line 319). No DisableAll
	// needed — that was causing a 1-frame gap where raw root motion played, producing the slingshot.
	MWC->RemoveWarpTarget(TargetName);

	// Also clear the replicated warp target so proxies remove it from their MWC.
	// HasAuthority check is inside ClearReplicatedWarpTarget — safe to call on all machines.
	Character->ClearReplicatedWarpTarget();
}

bool UOSCombatBlueprintLibrary::SetupComponentWarp(
	ACharacter* Character,
	FName WarpTargetName,
	const USceneComponent* TargetComponent,
	FName BoneName,
	bool bFollowComponent,
	FVector LocationOffset,
	FRotator RotationOffset,
	EWarpTargetLocationOffsetDirection OffsetDirection)
{
	if (!IsValid(Character) || !IsValid(TargetComponent)) return false;

	UMotionWarpingComponent* MWC = Character->FindComponentByClass<UMotionWarpingComponent>();
	if (!MWC) return false;

	MWC->AddOrUpdateWarpTargetFromComponent(
		WarpTargetName, TargetComponent, BoneName, bFollowComponent,
		OffsetDirection, LocationOffset, RotationOffset);
	return true;
}

bool UOSCombatBlueprintLibrary::MontageHasWarpWindow(const UAnimMontage* Montage, FName WarpTargetName)
{
	if (!Montage || WarpTargetName.IsNone()) return false;

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const UAnimNotifyState_MotionWarping* MWNotify =
			Cast<UAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass);
		if (!MWNotify) continue;

		// Check OS merged notify first — it has dynamic target names
		if (const UOSAnimNotifyState_OSTrackMotionWarpTarget* OSNotify =
			Cast<UOSAnimNotifyState_OSTrackMotionWarpTarget>(MWNotify))
		{
			if (OSNotify->WarpTargetName == WarpTargetName)
			{
				return true;
			}
			continue;
		}

		// Vanilla MW: check template's WarpTargetName
		const URootMotionModifier_Warp* WarpModifier =
			Cast<URootMotionModifier_Warp>(MWNotify->RootMotionModifier);
		if (WarpModifier && WarpModifier->WarpTargetName == WarpTargetName)
		{
			return true;
		}
	}

	return false;
}

TArray<FName> UOSCombatBlueprintLibrary::GetWarpWindowNames(const UAnimMontage* Montage)
{
	TArray<FName> Names;
	if (!Montage) return Names;

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const UAnimNotifyState_MotionWarping* MWNotify =
			Cast<UAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass);
		if (!MWNotify) continue;

		// OS merged notify has dynamic target names
		if (const UOSAnimNotifyState_OSTrackMotionWarpTarget* OSNotify =
			Cast<UOSAnimNotifyState_OSTrackMotionWarpTarget>(MWNotify))
		{
			if (!OSNotify->WarpTargetName.IsNone())
				Names.AddUnique(OSNotify->WarpTargetName);
			continue;
		}

		// Vanilla MW
		const URootMotionModifier_Warp* WarpModifier =
			Cast<URootMotionModifier_Warp>(MWNotify->RootMotionModifier);
		if (WarpModifier && !WarpModifier->WarpTargetName.IsNone())
		{
			Names.AddUnique(WarpModifier->WarpTargetName);
		}
	}

	return Names;
}

int32 UOSCombatBlueprintLibrary::CountWarpWindows(const UAnimMontage* Montage)
{
	if (!Montage) return 0;

	int32 Count = 0;
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (Cast<UAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass))
		{
			++Count;
		}
	}

	return Count;
}

bool UOSCombatBlueprintLibrary::MontageHasOSWarpNotify(const UAnimMontage* Montage,
	FName InWarpTargetName)
{
	if (!Montage || InWarpTargetName.IsNone()) return false;

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const UOSAnimNotifyState_OSTrackMotionWarpTarget* OSNotify =
			Cast<UOSAnimNotifyState_OSTrackMotionWarpTarget>(NotifyEvent.NotifyStateClass);
		if (!OSNotify) continue;

		if (OSNotify->WarpTargetName == InWarpTargetName)
		{
			return true;
		}
	}

	return false;
}

// ========================================
// CHARACTER ALIGNMENT UTILITIES
// ========================================

/** Read raw stick input from Enhanced Input, converted to a world-space direction.
 *  Returns ZeroVector if there's no player controller, no MoveAction, or the stick is idle. */
static FVector ReadRawStickWorldDirection(const ACharacter* Character)
{
	const APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return FVector::ZeroVector;

	const AOSPlayerController* OSPC = Cast<AOSPlayerController>(PC);
	if (!OSPC) return FVector::ZeroVector;

	const UInputAction* MA = OSPC->GetMoveAction();
	if (!MA) return FVector::ZeroVector;

	const UEnhancedInputLocalPlayerSubsystem* EIS =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!EIS || !EIS->GetPlayerInput()) return FVector::ZeroVector;

	const FVector2D Stick = EIS->GetPlayerInput()->GetActionValue(MA).Get<FVector2D>();
	if (Stick.IsNearlyZero()) return FVector::ZeroVector;

	// Convert camera-relative stick to world direction (same math as OSPlayerController::Move).
	const FRotator YawRot(0.f, PC->GetControlRotation().Yaw, 0.f);
	const FVector Fwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	return Fwd * Stick.Y + Right * Stick.X;
}

FVector UOSCombatBlueprintLibrary::GetInputDirection2D(const ACharacter* Character)
{
	if (!IsValid(Character)) return FVector::ZeroVector;

	const UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
	if (!CMC) return FVector::ZeroVector;

	// Priority: raw Enhanced Input stick > CMC last input > CMC acceleration (AI/punching bag).
	FVector Input = ReadRawStickWorldDirection(Character);
	if (Input.IsNearlyZero())
	{
		Input = CMC->GetLastInputVector();
	}
	if (Input.IsNearlyZero())
	{
		Input = CMC->GetCurrentAcceleration();
	}
	Input.Z = 0.f;
	return Input.GetSafeNormal();
}

bool UOSCombatBlueprintLibrary::IsIncapacitated(const AActor* Actor)
{
	if (!IsValid(Actor)) return true;

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC) return false;

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	return ASC->HasMatchingGameplayTag(Tags.IsDead) || ASC->HasMatchingGameplayTag(Tags.IsStunned);
}

bool UOSCombatBlueprintLibrary::AlignCharacterToDirection(
	AOSCharacter* Character,
	const FVector& WorldDir2D,
	UAnimMontage* Montage,
	FName WarpTargetName)
{
	if (!IsValid(Character) || IsIncapacitated(Character)) return false;

	FVector Dir(WorldDir2D.X, WorldDir2D.Y, 0.f);
	if (Dir.IsNearlyZero()) return false;

	const float TargetYaw = YawFromDirection2D(Dir);

	// MW-only: montage must have a matching warp window
	const bool bHasMWWindow = Montage && (MontageHasOSWarpNotify(Montage, WarpTargetName)
	                                   || MontageHasWarpWindow(Montage, WarpTargetName));

#if !UE_BUILD_SHIPPING
	if (GDebugRotationViz)
	{
		UE_LOG(LogOSCombat, Log, TEXT("[RotViz] AlignCharacterToDirection: Dir=(%.2f,%.2f) Yaw=%.1f Montage=%s WarpTarget=%s HasMW=%s"),
			Dir.X, Dir.Y, TargetYaw,
			Montage ? *Montage->GetName() : TEXT("NONE"),
			*WarpTargetName.ToString(),
			bHasMWWindow ? TEXT("YES") : TEXT("NO"));
	}
#endif

	if (bHasMWWindow)
	{
		SetupRotationWarp(Character, WarpTargetName, TargetYaw);
		return true;
	}

	return false;
}

bool UOSCombatBlueprintLibrary::AlignCharacterToDirectionCMC(
	AOSCharacter* Character,
	const FVector& WorldDir2D,
	float RotationRate,
	float MaxAngle)
{
	// Removed: CMC target rotation system no longer exists. Use Motion Warping for rotation.
	return false;
}

void UOSCombatBlueprintLibrary::ClearAlignmentState(AOSCharacter* Character, FName WarpTargetName)
{
	if (!IsValid(Character)) return;

	ClearWarpTarget(Character, WarpTargetName);
}

void UOSCombatBlueprintLibrary::GetAimOffsetYawPitch(const ACharacter* Character, float& OutYaw, float& OutPitch)
{
	OutYaw = 0.f;
	OutPitch = 0.f;

	if (!IsValid(Character)) return;

	const APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return;

	const FRotator AimRot = PC->GetControlRotation();
	const FRotator ActorRot = Character->GetActorRotation();
	const FRotator Delta = (AimRot - ActorRot).GetNormalized();

	OutYaw = FMath::Clamp(Delta.Yaw, -180.f, 180.f);
	OutPitch = FMath::Clamp(Delta.Pitch, -90.f, 90.f);
}
