
// Bullet shell actor that simulates ejected casing

#include "Actors/BulletShellBase.h"
#include "Components/StaticMeshComponent.h"


ABulletShellBase::ABulletShellBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// Destroy the shell after 2 seconds
	SetLifeSpan(2.0f);

	// Create and configure shell mesh component
	ShellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	ShellMesh->SetupAttachment(RootComponent);
	ShellMesh->SetMassOverrideInKg(NAME_None, 0.01f, true);
	ShellMesh->SetSimulatePhysics(true);
	ShellMesh->SetLinearDamping(1.0f);
	ShellMesh->SetAngularDamping(0.5f);
	ShellMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ShellMesh->SetCollisionObjectType(ECollisionChannel::ECC_PhysicsBody);
	ShellMesh->SetCollisionProfileName(FName("PhysicsActor"), false);

}

// Applies impulse to the shell for ejection effect
void ABulletShellBase::ShellImpulse(FVector Impulse)
{
	ShellMesh->AddImpulse(Impulse, NAME_None, true);
}

