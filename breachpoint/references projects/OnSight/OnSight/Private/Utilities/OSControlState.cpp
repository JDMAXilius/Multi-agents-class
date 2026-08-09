
#include "Utilities/OSControlState.h"

#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UOSControlState::SetTraceCollision(ACharacter* character, ECollisionChannel channel, ECollisionResponse newResponse)
{
	if (!IsValid(character)) return;
	
	if (USkeletalMeshComponent* Mesh = character->GetMesh())
	{
		Mesh->SetCollisionResponseToChannel(channel, newResponse);
	}
	if (UCapsuleComponent* Capsule = character->GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(channel, newResponse);
	}
}
