// Fill out your copyright notice in the Description page of Project Settings.

#include "Animations/Notifies/OSAnimNotify_Footstep.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Components/OSAudioComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

UOSAnimNotify_Footstep::UOSAnimNotify_Footstep()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(100, 200, 255);
#endif
}

void UOSAnimNotify_Footstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;

	// In multiplayer, only process on server or locally controlled character
	// This prevents duplicate sounds from anim notifies running on all clients
	if (!IsValid(Owner))
	{
		return;
	}

	if (Owner->GetNetMode() == NM_Client)
	{
		if (const APawn* OwnerPawn = Cast<APawn>(Owner))
		{
			if (!OwnerPawn->IsLocallyControlled())
			{
				return;
			}
		}
	}

	// Get audio component (contains footstep helpers)
	UOSAudioComponent* FootstepComponent = Owner->FindComponentByClass<UOSAudioComponent>();
	if (!IsValid(FootstepComponent))
	{
		return;
	}

	// Get foot socket location
	const FName SocketName = (FootSocketName != NAME_None)
		? FootSocketName
		: ((FootstepType == EOSFootstepType::LeftFoot) ? FName("foot_l") : FName("foot_r"));
	if (!MeshComp->DoesSocketExist(SocketName))
	{
		return;
	}

	const FTransform SocketTransform = MeshComp->GetSocketTransform(SocketName);
	const FVector SocketLocation = SocketTransform.GetLocation();
	const FVector TraceStart = SocketLocation;
	const FVector TraceEnd = SocketLocation - FVector(0.0f, 0.0f, TraceDistance);

	// Perform line trace to detect surface
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.AddIgnoredActor(Owner);

	UWorld* World = Owner->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	const FVector FootstepLocation = (bHit) ? HitResult.ImpactPoint : SocketLocation;
	const FRotator FootstepRotation = (bHit) ? HitResult.ImpactNormal.Rotation() : SocketTransform.Rotator();
	UPhysicalMaterial* PhysMat = (bHit && HitResult.PhysMaterial.IsValid()) ? HitResult.PhysMaterial.Get() : nullptr;
	
}

