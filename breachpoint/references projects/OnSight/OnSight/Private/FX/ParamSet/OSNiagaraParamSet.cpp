// Fill out your copyright notice in the Description page of Project Settings.


#include "FX/ParamSet/OSNiagaraParamSet.h"

#include "Utilities/Misc.h"


void UOSNiagaraParamSet::MergeFrom_Implementation(const UOSNiagaraParamSet* Other)
{
	if (!Other) return;
	
	if (Other->GetClass() != GetClass()) return;

	for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop) continue;

		if (Prop->HasAnyPropertyFlags(CPF_Transient)) continue;

		const void* SrcPtr = Prop->ContainerPtrToValuePtr<void>(Other);
		void* DstPtr = Prop->ContainerPtrToValuePtr<void>(this);
		
		if (const FFloatProperty* P = CastField<FFloatProperty>(Prop))
		{
			const float V = P->GetPropertyValue(SrcPtr);
			if (V >= 0.f) P->SetPropertyValue(DstPtr, V);
			continue;
		}

		if (const FIntProperty* P = CastField<FIntProperty>(Prop))
		{
			const int32 V = P->GetPropertyValue(SrcPtr);
			if (V >= 0) P->SetPropertyValue(DstPtr, V);
			continue;
		}

		if (const FObjectProperty* P = CastField<FObjectProperty>(Prop))
		{
			UObject* V = P->GetObjectPropertyValue(SrcPtr);
			if (V) P->SetObjectPropertyValue(DstPtr, V);
			continue;
		}

		if (const FStructProperty* P = CastField<FStructProperty>(Prop))
		{
			P->CopyCompleteValue(DstPtr, SrcPtr);
			continue;
		}

		Prop->CopyCompleteValue(DstPtr, SrcPtr);
	}
	
	
}

void UOSNiagaraParamSet::ApplyTo_Implementation( UNiagaraComponent* Comp ) const
{
	if (!Comp) return;

	for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop) continue;
		
		if (Prop->HasAnyPropertyFlags(CPF_Transient)) continue;
		if (Prop->GetFName() == GET_MEMBER_NAME_CHECKED(UOSNiagaraParamSet, System)) continue;

		ApplyPropertyToNiagara(Comp, this, Prop);
	}
}


