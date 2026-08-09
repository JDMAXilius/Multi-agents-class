// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "UObject/Object.h"
#include "OSNiagaraParamSet.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

static void ApplyPropertyToNiagara(UNiagaraComponent* Comp, const UObject* Obj, FProperty* Prop)
{
	const FName VarName = Prop->GetFName();
	const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Obj);

	if (const FFloatProperty* P = CastField<FFloatProperty>(Prop))
	{
		const float V = P->GetPropertyValue(ValuePtr);
		if (V >= 0.f) Comp->SetVariableFloat(VarName, V);
		return;
	}

	if (const FIntProperty* P = CastField<FIntProperty>(Prop))
	{
		const int32 V = P->GetPropertyValue(ValuePtr);
		if (V >= 0) Comp->SetVariableInt(VarName, V);
		return;
	}

	if (const FStructProperty* P = CastField<FStructProperty>(Prop))
	{
		if (P->Struct == TBaseStructure<FVector>::Get())
		{
			const FVector& V = *reinterpret_cast<const FVector*>(ValuePtr);
			Comp->SetVariableVec3(VarName, V);
			return;
		}
		if (P->Struct == TBaseStructure<FLinearColor>::Get())
		{
			const FLinearColor& V = *reinterpret_cast<const FLinearColor*>(ValuePtr);
			Comp->SetVariableLinearColor(VarName, V);
			return;
		}
	}

	if (const FObjectProperty* P = CastField<FObjectProperty>(Prop))
	{
		UObject* ObjVal = P->GetObjectPropertyValue(ValuePtr);
		if (!ObjVal) return;

		if (auto* Mat = Cast<UMaterialInterface>(ObjVal))
		{
			Comp->SetVariableMaterial(VarName, Mat);
			return;
		}
		if (auto* Tex = Cast<UTexture>(ObjVal))
		{
			Comp->SetVariableTexture(VarName, Tex);
			return;
		}
	}
}

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class ONSIGHT_API UOSNiagaraParamSet : public UObject
{
	GENERATED_BODY()
public:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UNiagaraSystem> System;
	//override rules defined per-property
	UFUNCTION(BlueprintNativeEvent)
	void MergeFrom(const UOSNiagaraParamSet* Other);
	virtual void MergeFrom_Implementation(const UOSNiagaraParamSet* Other);
	// apply this param set to the Niagara component.
	UFUNCTION(BlueprintNativeEvent)
	void ApplyTo(UNiagaraComponent* Comp) const;
	virtual void ApplyTo_Implementation(UNiagaraComponent* Comp) const;
protected:
	
};


