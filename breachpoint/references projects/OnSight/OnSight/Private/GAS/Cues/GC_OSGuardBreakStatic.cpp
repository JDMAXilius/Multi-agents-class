// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Cues/GC_OSGuardBreakStatic.h"
#include "Characters/OSCharacter.h"
#include "Data/OSDefenseAndReactions.h"
#include "Data/OSCombatTypes.h"
#include "Utilities/ChooserHelper.h"


bool UGC_OSGuardBreakStatic::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	auto OSChar = Cast<AOSCharacter>(MyTarget);
	if (!OSChar || !ChooserTable)
		return false;
	
	const FOSGameplayEffectContext* ctx = nullptr;
	if (!TryGetOSGameplayEffectContext(Parameters.EffectContext, ctx))
		return false;

	FOSBlockInfo blockInfo;
	blockInfo.bIsBroken = true;
	blockInfo.AttackType = ctx->HitInfo.AttackInfo.Type;
	blockInfo.AttackSeverity = ctx->HitInfo.AttackInfo.Type;
	
	auto montage = OSChooser::Evaluate<UAnimMontage>(ChooserTable, blockInfo);
	if (!montage)
		return false;
	
	if (auto mesh = OSChar->GetMesh())
		if (auto animInst = mesh->GetAnimInstance())
			animInst->Montage_Play(montage);
	
	return true;
}
