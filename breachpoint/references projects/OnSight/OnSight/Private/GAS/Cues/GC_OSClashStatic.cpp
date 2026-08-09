// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Cues/GC_OSClashStatic.h"

#include "Characters/OSCharacter.h"
#include "Data/OSHitDamageContext.h"
#include "Utilities/ChooserHelper.h"


bool UGC_OSClashStatic::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	AOSCharacter* OSChar = Cast<AOSCharacter>(MyTarget);
	if (!OSChar || !ChooserTable) return false;

	UAnimInstance* animInst = OSChar->GetMesh() ? OSChar->GetMesh()->GetAnimInstance() : nullptr;
	if (!animInst) return false;

	const FOSGameplayEffectContext* ctx = nullptr;
	if (!TryGetOSGameplayEffectContext(Parameters.EffectContext, ctx))
	{
		return false;
	}
	const FOSClashInfo& info = ctx->ClashInfo;

	UAnimMontage* montage = OSChooser::Evaluate<UAnimMontage>(ChooserTable, info);
	
	if (!montage) return false;

	animInst->Montage_Play(montage, 1.f);
	return true;
}
