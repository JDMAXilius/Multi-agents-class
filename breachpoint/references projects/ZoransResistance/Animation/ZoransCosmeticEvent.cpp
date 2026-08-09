// Copyright Zollpa LLC


#include "ZoransResistance/Animation/ZoransCosmeticEvent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"

UZoransCosmeticEvent::UZoransCosmeticEvent()
{
	
}

void UZoransCosmeticEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

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
					UNiagaraFunctionLibrary::SpawnSystemAttached(CosmeticData.NiagaraSystem, MeshComp, CosmeticData.MeshSocketName, CosmeticEventLocation, SocketTransform.Rotator(), EAttachLocation::KeepWorldPosition, true, true, ENCPoolMethod::AutoRelease, true);
				}
				else
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(NotifyOwner, CosmeticData.NiagaraSystem, CosmeticEventLocation, SocketTransform.Rotator(), FVector(1.f), true, true, ENCPoolMethod::AutoRelease, true);
				}
			}

			if (IsValid(CosmeticData.SoundEffect))
			{
				if (CosmeticData.bAttachSoundToMesh)
				{
					UGameplayStatics::SpawnSoundAttached(CosmeticData.SoundEffect, MeshComp, CosmeticData.MeshSocketName, CosmeticEventLocation, SocketTransform.Rotator(), EAttachLocation::KeepWorldPosition);
				}
				else
				{
					UGameplayStatics::PlaySoundAtLocation(NotifyOwner, CosmeticData.SoundEffect, CosmeticEventLocation, SocketTransform.Rotator());
				}
			}
		}
	}
}
