// Copyright Zollpa LLC

#include "ZoransResistance/Animation/AN_Footstep.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "ZoransResistance/Characters/ZoransCharacterBase.h"

UAN_Footstep::UAN_Footstep()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(100, 200, 255);
#endif
}

void UAN_Footstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	// In multiplayer, only process on server or locally controlled character
	// This prevents duplicate sounds from anim notifies running on all clients
	if (Owner->GetNetMode() == NM_Client && !Owner->IsLocallyControlled())
	{
		return;
	}

	// Get foot socket location
	FName SocketName = FootSocketName;
	if (SocketName == NAME_None)
	{
		// Default to foot sockets based on footstep type
		SocketName = (FootstepType == EFootstepType::LeftFoot) ? FName("foot_l") : FName("foot_r");
	}

	if (!MeshComp->DoesSocketExist(SocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("AN_Footstep: Socket '%s' does not exist on mesh '%s'"), *SocketName.ToString(), *MeshComp->GetName());
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

	USoundBase* SoundToPlay = DefaultSound;
	FVector SoundLocation = SocketLocation;
	FRotator SoundRotation = SocketTransform.Rotator();

	if (bHit && HitResult.PhysMaterial.IsValid())
	{
		// Get sound based on physical material
		SoundToPlay = GetSoundForSurface(HitResult.PhysMaterial.Get());
		SoundLocation = HitResult.ImpactPoint;
		SoundRotation = HitResult.ImpactNormal.Rotation();
	}

	if (IsValid(SoundToPlay))
	{
		// Get volume and pitch multipliers for this surface
		float FinalVolume = VolumeMultiplier;
		float FinalPitch = PitchMultiplier;

		if (bHit && HitResult.PhysMaterial.IsValid())
		{
			for (const FFootstepSoundData& SoundData : SurfaceSounds)
			{
				if (SoundData.PhysicalMaterial.Get() == HitResult.PhysMaterial.Get())
				{
					FinalVolume = SoundData.VolumeMultiplier;
					FinalPitch = SoundData.PitchMultiplier;
					break;
				}
			}
		}

		// Try to use character's networked footstep function for multiplayer support
		if (AZoransCharacterBase* Character = Cast<AZoransCharacterBase>(Owner))
		{
			Character->PlayFootstepSound(SoundToPlay, SoundLocation, SoundRotation, FinalVolume, FinalPitch);
		}
		else
		{
			// Fallback to direct sound playback if not a ZoransCharacterBase
			PlayFootstepSound(MeshComp, SoundToPlay, SoundLocation, SoundRotation, FinalVolume, FinalPitch);
		}
	}
}

USoundBase* UAN_Footstep::GetSoundForSurface(UPhysicalMaterial* PhysicalMaterial) const
{
	if (!IsValid(PhysicalMaterial))
	{
		return DefaultSound;
	}

	// Find matching surface sound
	for (const FFootstepSoundData& SoundData : SurfaceSounds)
	{
		if (SoundData.PhysicalMaterial.Get() == PhysicalMaterial)
		{
			return SoundData.Sound;
		}
	}

	return DefaultSound;
}

void UAN_Footstep::PlayFootstepSound(USkeletalMeshComponent* MeshComp, USoundBase* Sound, const FVector& Location, const FRotator& Rotation, float InVolumeMultiplier, float InPitchMultiplier) const
{
	if (!IsValid(Sound) || !IsValid(MeshComp))
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	if (bAttachToMesh)
	{
		FName SocketName = FootSocketName;
		if (SocketName == NAME_None)
		{
			SocketName = (FootstepType == EFootstepType::LeftFoot) ? FName("foot_l") : FName("foot_r");
		}

		UAudioComponent* AudioComponent = UGameplayStatics::SpawnSoundAttached(
			Sound,
			MeshComp,
			SocketName,
			Location,
			Rotation,
			EAttachLocation::KeepWorldPosition,
			false,
			InVolumeMultiplier,
			InPitchMultiplier
		);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(
			Owner,
			Sound,
			Location,
			Rotation,
			InVolumeMultiplier,
			InPitchMultiplier
		);
	}
}

