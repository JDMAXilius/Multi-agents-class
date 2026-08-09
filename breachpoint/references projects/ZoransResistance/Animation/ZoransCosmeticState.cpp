// Copyright Zollpa LLC


#include "ZoransResistance/Animation/ZoransCosmeticState.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

void UZoransCosmeticState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (IsValid(MeshComp))
	{
		if (const AActor* NotifyOwner = MeshComp->GetOwner())
		{
			const FTransform SocketTransform = MeshComp->GetSocketTransform(CosmeticData.MeshSocketName);
			const FVector CosmeticEventLocation = SocketTransform.TransformPosition(EventLocation);
			
			if (IsValid(CosmeticData.NiagaraSystem))
			{
				if (CosmeticData.bAttachEffectToMesh)
				{
					SpawnedNiagaraEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(CosmeticData.NiagaraSystem, MeshComp, CosmeticData.MeshSocketName, CosmeticEventLocation, SocketTransform.Rotator(), EAttachLocation::KeepWorldPosition, true, true, ENCPoolMethod::AutoRelease, true);
				}
				else
				{
					SpawnedNiagaraEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(NotifyOwner, CosmeticData.NiagaraSystem, CosmeticEventLocation, SocketTransform.Rotator(), FVector(1.f), true, true, ENCPoolMethod::AutoRelease, true);
				}
			}

			if (IsValid(CosmeticData.SoundEffect))
			{
				if (CosmeticData.bAttachSoundToMesh)
				{
					SpawnedSoundEffect = UGameplayStatics::SpawnSoundAttached(CosmeticData.SoundEffect, MeshComp, CosmeticData.MeshSocketName, CosmeticEventLocation, SocketTransform.Rotator(), EAttachLocation::KeepWorldPosition);
				}
				else
				{
					SpawnedSoundEffect = UGameplayStatics::SpawnSoundAtLocation(NotifyOwner, CosmeticData.SoundEffect, CosmeticEventLocation, SocketTransform.Rotator());
				}
			}
		}
	}
}

void UZoransCosmeticState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (IsValid(SpawnedNiagaraEffect))
	{
		SpawnedNiagaraEffect->Deactivate();
	}

	if (IsValid(SpawnedSoundEffect))
	{
		SpawnedSoundEffect->Deactivate();
	}
}
