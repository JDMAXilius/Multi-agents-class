#include "UI/BNFrontEndDisplay.h"
#include "Components/SkeletalMeshComponent.h"

ABNFrontEndDisplay::ABNFrontEndDisplay()
{
	// Law 4: no gameplay Tick. Nothing here moves except the animation the mesh plays.
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// A menu prop must animate with no pawn and no world relevance driving it.
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Weapon = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Weapon"));
	// THE WHOLE POINT: a socketed attachment, which no property write can express.
	// WeaponSocket's in-class initialiser has already run by the time we get here.
	Weapon->SetupAttachment(Body, WeaponSocket);
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABNFrontEndDisplay::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// The constructor socketed against the DEFAULT socket name. If an instance retypes
	// WeaponSocket (a different skeleton, a left-hand carry), re-seat against the new one —
	// otherwise the details-panel field would silently do nothing after the first build.
	if (Weapon && Body)
	{
		Weapon->AttachToComponent(Body,
			FAttachmentTransformRules::SnapToTargetIncludingScale, WeaponSocket);
	}
}
