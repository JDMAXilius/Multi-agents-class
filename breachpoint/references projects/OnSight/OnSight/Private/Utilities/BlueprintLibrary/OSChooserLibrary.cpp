// Fill out your copyright notice in the Description page of Project Settings.


#include "Utilities/BlueprintLibrary/OSChooserLibrary.h"

#include "ChooserFunctionLibrary.h"

UObject* UOSChooserLibrary::EvaluateChooser_Object(UChooserTable* Table, const FInstancedStruct& Params)
{
	if (!Table) return nullptr;

	const UScriptStruct* S = Params.GetScriptStruct();
	const void* Mem = Params.GetMemory();
	if (!S || !Mem) return nullptr;

	FChooserEvaluationContext Context;
	Context.AddStructViewParam(FStructView(S, static_cast<uint8*>(const_cast<void*>(Mem))));

	const FInstancedStruct Instanced = UChooserFunctionLibrary::MakeEvaluateChooser(Table);

	return UChooserFunctionLibrary::EvaluateObjectChooserBase(
		Context,
		Instanced,
		UObject::StaticClass(),
		/* bResultIsClass = */ false);
}

TSubclassOf<UObject> UOSChooserLibrary::EvaluateChooser_Class(UChooserTable* Table, const FInstancedStruct& Params)
{
	if (!Table) return nullptr;

	const UScriptStruct* S = Params.GetScriptStruct();
	const void* Mem = Params.GetMemory();
	if (!S || !Mem) return nullptr;

	FChooserEvaluationContext Context;
	Context.AddStructViewParam(FStructView(S, static_cast<uint8*>(const_cast<void*>(Mem))));

	const FInstancedStruct Instanced = UChooserFunctionLibrary::MakeEvaluateChooser(Table);

	return Cast<UClass>(
		UChooserFunctionLibrary::EvaluateObjectChooserBase(
			Context,
			Instanced,
			UObject::StaticClass(),
			/* bResultIsClass = */ true));
}
