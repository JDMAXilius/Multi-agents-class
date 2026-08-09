// Fill out your copyright notice in the Description page of Project Settings.


#include "FX/ParamSet/OSNiagaraParamSet_Generic.h"

#include "NiagaraSystem.h"



void UOSNiagaraParamSet_Generic::ApplyTo_Implementation( UNiagaraComponent* Comp ) const
{
	if (!Comp) return;

	for (const auto& It : FloatParams)
	{
		if (It.Value.bOverride) Comp->SetVariableFloat(It.Key, It.Value.Value);
	}
	for (const auto& It : IntParams)
	{
		if (It.Value.bOverride) Comp->SetVariableInt(It.Key, It.Value.Value);
	}
	for (const auto& It : VectorParams)
	{
		if (It.Value.bOverride) Comp->SetVariableVec3(It.Key, It.Value.Value);
	}
	for (const auto& It : ColorParams)
	{
		if (It.Value.bOverride) Comp->SetVariableLinearColor(It.Key, It.Value.Value);
	}
	for (const auto& It : ObjectParams)
	{
		if (!It.Value.bOverride) continue;
		UObject* Obj = It.Value.Value.Get();
		if (!Obj) continue;

		if (auto* Mat = Cast<UMaterialInterface>(Obj))
		{
			Comp->SetVariableMaterial(It.Key, Mat);
		}
		else if (auto* Tex = Cast<UTexture>(Obj))
		{
			Comp->SetVariableTexture(It.Key, Tex);
		}
	}
}

void UOSNiagaraParamSet_Generic::MergeFrom_Implementation( const UOSNiagaraParamSet* OtherBase )
{
	const auto* Other = Cast<UOSNiagaraParamSet_Generic>(OtherBase);
	if (!Other) return;

	if (Other->System) System = Other->System;

	auto MergeMap = [](auto& Dst, const auto& Src)
	{
		for (const auto& It : Src)
		{
			if (!It.Value.bOverride) continue;

			auto* Existing = Dst.Find(It.Key);
			if (Existing)
			{
				*Existing = It.Value;
			}
			else
			{
				Dst.Add(It.Key, It.Value);
			}
		}
	};

	MergeMap(FloatParams, Other->FloatParams);
	MergeMap(IntParams, Other->IntParams);
	MergeMap(VectorParams, Other->VectorParams);
	MergeMap(ColorParams, Other->ColorParams);
	MergeMap(ObjectParams, Other->ObjectParams);
}
