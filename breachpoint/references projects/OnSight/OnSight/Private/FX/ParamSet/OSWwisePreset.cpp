// Fill out your copyright notice in the Description page of Project Settings.


#include "FX/ParamSet/OSWwisePreset.h"

void UOSWwisePreset::MergeWithVariationPack( int index )
{
	if (!VariationPacks.IsValidIndex(index)) return;
	BasePack.MergeFrom(VariationPacks[index]);
}

const FOSWwiseParamPack* UOSWwisePreset::GetVariationPack(int32 Index) const
{
	return VariationPacks.IsValidIndex(Index) ? &VariationPacks[Index] : nullptr;
}
