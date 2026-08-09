// Copyright Zollpa LLC


#include "ZoransThrowdownBallisticBase.h"
#include "GameFramework/GameNetworkManager.h"


AZoransThrowdownBallisticBase::AZoransThrowdownBallisticBase()
{
	ProjectileSpeed = 4500.f;
	ProjectileLifespan = 4.f;
	ProjectileRadius = 16.f;
	GravityModifier = 0.f;
	ImpactForce = 20000.f;
	SpawnSourceType = EZoransProjectileSourceType::Character;

	bCanModify = false;
}

bool AZoransThrowdownBallisticBase::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	if (bAlwaysRelevant || IsOwnedBy(ViewTarget) || IsOwnedBy(RealViewer) || this == ViewTarget || ViewTarget == GetInstigator())
	{
		return true;
	}
	if (bNetUseOwnerRelevancy && Owner)
	{
		return Owner->IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
	}
	if (bOnlyRelevantToOwner)
	{
		return false;
	}
	if (RootComponent && RootComponent->GetAttachParent() && RootComponent->GetAttachParent()->GetOwner() && (Cast<USkeletalMeshComponent>(RootComponent->GetAttachParent()) || (RootComponent->GetAttachParent()->GetOwner() == Owner)))
	{
		return RootComponent->GetAttachParent()->GetOwner()->IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
	}
	if(IsHidden() && (!RootComponent || !RootComponent->IsCollisionEnabled()))
	{
		return false;
	}

	if (!RootComponent)
	{
		return false;
	}

	return !GetDefault<AGameNetworkManager>()->bUseDistanceBasedRelevancy || IsWithinNetRelevancyDistance(SrcLocation);
}


