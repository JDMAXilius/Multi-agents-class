#include "Animations/Notifies/ANS_OSAttackPhase.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Data/OSGameplayTags.h"

static UAbilitySystemComponent* GetASCFromMesh(const USkeletalMeshComponent* MeshComp)
{
	return MeshComp ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner()) : nullptr;
}

UANS_OSAttackPhase::UANS_OSAttackPhase()
{
	DefaultName = "Attack Phase";
}

void UANS_OSAttackPhase::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UAbilitySystemComponent* ASC = GetASCFromMesh(MeshComp);
	if (!ASC) return;

	FGameplayEventData Payload;
	ASC->HandleGameplayEvent(FOSGameplayTags::Get().Event_Attack_Phase_Active, &Payload);
}

void UANS_OSAttackPhase::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	UAbilitySystemComponent* ASC = GetASCFromMesh(MeshComp);
	if (!ASC) return;

	FGameplayEventData Payload;
	ASC->HandleGameplayEvent(FOSGameplayTags::Get().Event_Attack_Phase_Recovery, &Payload);
}
