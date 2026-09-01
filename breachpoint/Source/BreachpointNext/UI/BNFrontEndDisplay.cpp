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
	// Covers a socket retyped in the details panel, or a mesh swapped on the instance.
	AttachWeaponToSocket();
}

void ABNFrontEndDisplay::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	// The level's saved Body mesh is present by now, which is the earliest the skeleton -
	// and therefore the socket - actually exists. The constructor's SetupAttachment ran long
	// before that and could not resolve it.
	AttachWeaponToSocket();
}

void ABNFrontEndDisplay::AttachWeaponToSocket()
{
	if (!Weapon || !Body)
	{
		return;
	}
	// DoesSocketExist is the load-bearing check, not politeness: AttachToComponent treats a
	// missing socket as "attach to the origin" and drops the name, so attaching too early
	// does not fail loudly - it quietly parks the weapon at the hero's feet forever.
	if (!Body->GetSkeletalMeshAsset() || !Body->DoesSocketExist(WeaponSocket))
	{
		return;
	}
	Weapon->AttachToComponent(Body,
		FAttachmentTransformRules::SnapToTargetIncludingScale, WeaponSocket);
}
