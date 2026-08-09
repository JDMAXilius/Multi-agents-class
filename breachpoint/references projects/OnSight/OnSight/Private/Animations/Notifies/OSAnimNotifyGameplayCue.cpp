// Fill out your copyright notice in the Description page of Project Settings.


#include "Animations/Notifies/OSAnimNotifyGameplayCue.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Characters/Player/OSPlayer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/OSHitDamageContext.h"

void UOSAnimNotifyGameplayCue::Notify( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                       const FAnimNotifyEventReference& EventReference )
{
	if (!MeshComp || !CueTag.IsValid()) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
	if (!ASC) return;

	FGameplayCueParameters Params;

	Params.EffectContext = ASC->MakeEffectContext();
	FOSGameplayEffectContext* Ctx = nullptr;
	if (TryGetOSGameplayEffectContext(Params.EffectContext, Ctx))
	{
		Ctx->CueSocketName = SocketName;
	}

	Params.SourceObject = MeshComp;
	Params.Instigator = Owner;
	Params.OriginalTag = CueTag;
	ASC->ExecuteGameplayCue(CueTag, Params);
}