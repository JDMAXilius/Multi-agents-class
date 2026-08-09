
#include "Pickups/PickupBase.h"
#include "Main/PlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"


APickupBase::APickupBase()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(FName("BlockAllDynamic"), false);
	Mesh->SetGenerateOverlapEvents(false);

	PickupData.InteractionTime = 1.2f;

	InteractionArea = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	InteractionArea->SetupAttachment(Mesh);
	InteractionArea->InitSphereRadius(90.0f);
	InteractionArea->SetCollisionProfileName(FName("OverlapAllDynamic"), true);
}


void APickupBase::BeginPlay()
{
	Super::BeginPlay();
}

void APickupBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APickupBase::PlayPickupMontage()
{
}

void APickupBase::InteractionAreaEntered_Implementation(APlayerCharacter* Player)
{
	PlayerCharacter = Player;
	Mesh->SetRenderCustomDepth(true);
	SetActorTickEnabled(false);
}

void APickupBase::InteractedToPickup_Implementation(APlayerCharacter* Player)
{
	Mesh->SetRenderCustomDepth(false);
	UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
}

void APickupBase::InteractionAreaExited_Implementation()
{
	Mesh->SetRenderCustomDepth(false);
}

