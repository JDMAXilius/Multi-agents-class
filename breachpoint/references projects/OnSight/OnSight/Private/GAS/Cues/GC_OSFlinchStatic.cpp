// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Cues/GC_OSFlinchStatic.h"

#include "Characters/OSCharacter.h"
#include "Data/OSDefenseAndReactions.h"
#include "Utilities/ChooserHelper.h"

bool UGC_OSFlinchStatic::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	auto OSChar = Cast<AOSCharacter>(MyTarget);
	if (!OSChar || !ChooserTable)
		return false;
	
	FOSHitReacts hitReact;
	
	auto montage = OSChooser::Evaluate<UAnimMontage>(ChooserTable, hitReact);
	if (!montage)
		return false;
	
	if (auto mesh = OSChar->GetMesh())
		if (auto animInst = mesh->GetAnimInstance())
			animInst->Montage_Play(montage);
	return true;
}
