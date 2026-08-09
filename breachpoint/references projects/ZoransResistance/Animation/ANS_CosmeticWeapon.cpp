// Copyright Zollpa LLC


#include "ANS_CosmeticWeapon.h"
#include "ZoransResistance/Actors/Data/ActorData.h"

void UANS_CosmeticWeapon::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (CosmeticWeaponClass != nullptr && IsValid(MeshComp))
	{
		if (UWorld* CurrentWorld = MeshComp->GetWorld())
		{
			if (AActor* OwningActor = MeshComp->GetOwner())
			{
				FActorSpawnParameters CosmeticWeaponSpawnParams;
				CosmeticWeaponSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				CosmeticWeaponSpawnParams.Owner = OwningActor;
				const FVector SpawnLocation = OwningActor->GetActorLocation();
				const FRotator SpawnRotation = OwningActor->GetActorRotation();
				if (AActor* SpawnedWeapon = CurrentWorld->SpawnActor(CosmeticWeaponClass, &SpawnLocation, &SpawnRotation, CosmeticWeaponSpawnParams))
				{
					const FAttachmentTransformRules AttachRules = FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true);
					SpawnedWeapon->AttachToActor(OwningActor, AttachRules);
				}
			}
		}		
	}
}

void UANS_CosmeticWeapon::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (IsValid(MeshComp))
	{
		if (const AActor* OwningActor = MeshComp->GetOwner())
		{
			TArray<AActor*> AttachedActors;
			OwningActor->GetAttachedActors(AttachedActors);
			if (!AttachedActors.IsEmpty())
			{
				for (auto const &x : AttachedActors)
				{
					if (x->ActorHasTag(ActorTag_CosmeticWeapon))
					{
						if (ACosmeticWeapon* AttachedCosmeticWeapon = Cast<ACosmeticWeapon>(x))
						{
							AttachedCosmeticWeapon->RemoveCosmeticWeapon();
						}
					}
				}
			}
		}
	}
}
