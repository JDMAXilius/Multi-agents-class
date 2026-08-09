// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Cues/GC_OSHitImpact_Sound.h"

#include "AkGameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/OSHitDamageContext.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"

namespace
{
	static bool PlayWwiseAndVfx(AActor* MyTarget, const FGameplayCueParameters& Parameters, const UAkAudioEvent* AkEvent, const UNiagaraSystem* Vfx, FName DefaultSocketName)
	{
		if (!IsValid(MyTarget))
		{
			return false;
		}

		// Cues can execute on PlayerState (PlayerState-owned ASC). Prefer an in-world avatar when available.
		AActor* VisualTarget = MyTarget;
		if (APlayerState* PS = Cast<APlayerState>(MyTarget))
		{
			if (APawn* Pawn = PS->GetPawn())
			{
				VisualTarget = Pawn;
			}
		}

		UWorld* World = VisualTarget->GetWorld();
		if (!IsValid(World))
		{
			return false;
		}

		static bool bLoggedMissingAssetsOnce = false;
		if (!AkEvent && !Vfx && !bLoggedMissingAssetsOnce)
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("[GC_OSHitImpact_Sound] Cue fired but no HitAkEvent/ImpactVfx is set (this is fine if you only want logic). Target=%s"),
				*GetNameSafe(MyTarget));
			bLoggedMissingAssetsOnce = true;
		}

		FVector Location = VisualTarget->GetActorLocation();
		FRotator Rotation = VisualTarget->GetActorRotation();

		// Socket resolution (best-effort):
		// 1) Explicit socket in our custom effect context (CueSocketName)
		// 2) HitResult bone name (often the skeletal bone hit)
		// 3) None -> fallback to hit impact point or actor location
		FName SocketName = NAME_None;
		const FOSGameplayEffectContext* OSCtx = nullptr;
		if (TryGetOSGameplayEffectContext(Parameters.EffectContext, OSCtx))
		{
			SocketName = OSCtx->GetCueSocketName();
		}

		const FHitResult* HR = Parameters.EffectContext.GetHitResult();
		if (HR && SocketName.IsNone() && HR->BoneName != NAME_None)
		{
			SocketName = HR->BoneName;
		}

		USkeletalMeshComponent* TargetMesh = nullptr;
		if (ACharacter* Char = Cast<ACharacter>(VisualTarget))
		{
			TargetMesh = Char->GetMesh();
		}
		if (!TargetMesh)
		{
			TargetMesh = VisualTarget->FindComponentByClass<USkeletalMeshComponent>();
		}

		// Designer fallback when there is no hit bone/socket (e.g., capsule hits).
		if (TargetMesh && SocketName.IsNone() && DefaultSocketName != NAME_None && TargetMesh->DoesSocketExist(DefaultSocketName))
		{
			SocketName = DefaultSocketName;
		}

		// Default to hit impact point when we have it.
		if (HR)
		{
			Location = HR->ImpactPoint;
			Rotation = HR->ImpactNormal.Rotation();
		}

		// If we have a mesh+socket/bone, use its world transform (correct skeleton).
		if (TargetMesh && !SocketName.IsNone())
		{
			const FTransform SocketXf = TargetMesh->GetSocketTransform(SocketName, RTS_World);
			Location = SocketXf.GetLocation();
			Rotation = SocketXf.GetRotation().Rotator();
		}

		if (AkEvent)
		{
			UAkGameplayStatics::PostEventAtLocation(const_cast<UAkAudioEvent*>(AkEvent), Location, Rotation, World);
		}

		if (Vfx)
		{
			// Attach when possible so the effect follows the skeleton (ragdolls, root motion, etc.).
			if (TargetMesh && !SocketName.IsNone())
			{
				const bool bAutoDestroy = true;
				const bool bAutoActivate = true;
				UNiagaraFunctionLibrary::SpawnSystemAttached(
					const_cast<UNiagaraSystem*>(Vfx),
					TargetMesh,
					SocketName,
					FVector::ZeroVector,
					FRotator::ZeroRotator,
					EAttachLocation::KeepRelativeOffset,
					bAutoDestroy,
					bAutoActivate);
			}
			else
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, const_cast<UNiagaraSystem*>(Vfx), Location, Rotation);
			}
		}

		return true;
	}
}

bool UGC_OSHitImpact_Sound::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	return PlayWwiseAndVfx(MyTarget, Parameters, HitAkEvent, ImpactVfx, DefaultSocketName);
}

bool UGC_OSHitImpact_Sound::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	return PlayWwiseAndVfx(MyTarget, Parameters, HitAkEvent, ImpactVfx, DefaultSocketName);
}

