// Copyright Zollpa LLC


#include "ZoransBeaconCapturePointBase.h"

#include "Components/SphereComponent.h"
#include "ZoransResistance/Characters/ZoransCharacterBase.h"
#include "ZoransResistance/Characters/Components/ZoransHealthComponent.h"
#include "ZoransResistance/Characters/Components/ZoransWeaponComponent.h"


AZoransBeaconCapturePointBase::AZoransBeaconCapturePointBase()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->Mobility = EComponentMobility::Movable;
	RootComponent = SceneRoot;
	
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionObjectType(ECC_Destructible);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CollisionShape = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionShape"));
	CollisionShape->SetupAttachment(SceneRoot);
}

void AZoransBeaconCapturePointBase::BeginPlay()
{
	Super::BeginPlay();
	if(HasAuthority() && IsValid(CollisionShape))
	{
		CollisionShape->OnComponentBeginOverlap.AddUniqueDynamic(this, &AZoransBeaconCapturePointBase::OnCollisionOverlap);
	}
}

bool AZoransBeaconCapturePointBase::IsPlayerBeaconHolder(const AZoransCharacterBase* ZoransCharacterBase) const
{
	const UZoransHealthComponent* ZoransHealthComponent = ZoransCharacterBase->GetHealthComponent();
	if (IsValid(ZoransHealthComponent) && ZoransHealthComponent->IsAlive())
	{
		if(ZoransCharacterBase->GetWeaponComponent()->HasObjectiveWeaponEquipped())
		{
			if(ZoransCharacterBase->GetTeamAssignment() == TeamIndex)
			{
				return true;
			}
		}
	}
	return false;
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
// ReSharper disable once CppMemberFunctionMayBeConst
void AZoransBeaconCapturePointBase::OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if(!bActive)
		return;
	
	if(!HasAuthority())
		return;
	
	if(OverlappedComponent != CollisionShape || !IsValid(OtherActor))
		return;
	
	if (AZoransCharacterBase* ZoransCharacterBase = Cast<AZoransCharacterBase>(OtherActor))
	{
		if(IsPlayerBeaconHolder(ZoransCharacterBase))
		{
			OnBeaconHolderOverlap.Broadcast(ZoransCharacterBase);
		}
	}
}
