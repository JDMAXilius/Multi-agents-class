// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Cues/OSDashTrailCue.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "OSLogCategories.h"

UOSDashTrailCue::UOSDashTrailCue()
	: TrailVFX(nullptr)
	, bAttachToTarget(true)
	, AttachSocketName(NAME_None)
	, EndLocationParamName(NAME_None)
	, LengthParamName(NAME_None)
{
}

bool UOSDashTrailCue::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	UE_LOG(LogOSCombat, Verbose, TEXT("OSDashTrailCue::OnExecute — Target=%s Loc=%s Dir=%s Len=%.1f Attach=%d"),
		*GetNameSafe(MyTarget), *Parameters.Location.ToString(), *Parameters.Normal.ToString(),
		Parameters.RawMagnitude, bAttachToTarget ? 1 : 0);

	if (!TrailVFX)
	{
		UE_LOG(LogOSCombat, Warning, TEXT("OSDashTrailCue::OnExecute — TrailVFX is null on %s."), *GetNameSafe(GetClass()));
		return false;
	}
	if (!MyTarget)
	{
		UE_LOG(LogOSCombat, Warning, TEXT("OSDashTrailCue::OnExecute — MyTarget is null."));
		return false;
	}

	const FVector Direction = Parameters.Normal.IsNearlyZero() ? FVector::ForwardVector : Parameters.Normal.GetSafeNormal();
	const FRotator Rotation = Direction.Rotation();

	UNiagaraComponent* Spawned = nullptr;

	if (bAttachToTarget)
	{
		// Attached path: ribbon emitter follows the actor through the world and leaves a trail naturally.
		USceneComponent* AttachRoot = nullptr;
		if (const ACharacter* AsChar = Cast<ACharacter>(MyTarget))
		{
			AttachRoot = AsChar->GetMesh();
		}
		if (!AttachRoot)
		{
			AttachRoot = MyTarget->GetRootComponent();
		}

		Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(
			TrailVFX, AttachRoot, AttachSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator, FVector(1.f),
			EAttachLocation::SnapToTarget, /*bAutoDestroy*/ true,
			ENCPoolMethod::AutoRelease);

		/* Lock world rotation to the actor's quat rotated 180° around Z (facing opposite the
		   player's forward, so the trail streams *behind* them). Bypasses the mesh's baked
		   -90° yaw offset. Child.World = Parent.World * Child.Relative, so:
		   Relative = Parent.WorldInverse * (ActorQuat * YawPi).
		   Because the mesh stays rigid relative to the actor, this relation holds every frame. */
		if (Spawned)
		{
			const FQuat ParentWorldQuat = AttachSocketName.IsNone()
				? AttachRoot->GetComponentQuat()
				: AttachRoot->GetSocketQuaternion(AttachSocketName);
			const FQuat ActorWorldQuat = MyTarget->GetActorQuat();
			const FQuat YawPi(FVector::UpVector, PI);
			const FQuat RelRot = ParentWorldQuat.Inverse() * (ActorWorldQuat * YawPi);
			Spawned->SetRelativeRotation(RelRot);
		}
	}
	else
	{
		// Fixed-segment path: spawn at world Location, orientation toward End.
		Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			MyTarget->GetWorld(), TrailVFX, Parameters.Location, Rotation, FVector(1.f), /*bAutoDestroy*/ true);
	}

	if (!Spawned)
	{
		UE_LOG(LogOSCombat, Warning, TEXT("OSDashTrailCue::OnExecute — Spawn returned null."));
		return false;
	}

	if (!EndLocationParamName.IsNone())
	{
		const FVector End = Parameters.Location + Direction * Parameters.RawMagnitude;
		Spawned->SetVariableVec3(EndLocationParamName, End);
	}
	if (!LengthParamName.IsNone())
	{
		Spawned->SetVariableFloat(LengthParamName, Parameters.RawMagnitude);
	}

	return true;
}
