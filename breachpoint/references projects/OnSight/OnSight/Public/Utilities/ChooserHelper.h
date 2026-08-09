#pragma once
// Chooser evaluation: Evaluate<T>(ChooserTable, ParamStruct). Used by hit reaction, dodge, guard break to pick montage or variant.
#include "StructUtils/InstancedStruct.h"
#include "Chooser/Public/ChooserFunctionLibrary.h"
#include "Animation/AnimMontage.h"
#include "OSLogCategories.h"

namespace OSChooser
{
	template<typename TResult, typename TParamStruct>
	static TResult* Evaluate(
		UChooserTable* ChooserTable,
		const TParamStruct& ParamStruct,
		bool bResultIsClass = false)
	{
		static_assert(TIsDerivedFrom<TResult, UObject>::IsDerived,
			"TResult must derive from UObject.");

		if (!ChooserTable) return nullptr;

		TParamStruct Temp = ParamStruct; // Chooser needs mutable memory

		FChooserEvaluationContext Context;
		Context.AddStructParam(Temp);

		const FInstancedStruct Instanced = UChooserFunctionLibrary::MakeEvaluateChooser(ChooserTable);

		UObject* Obj = UChooserFunctionLibrary::EvaluateObjectChooserBase(
			Context,
			Instanced,
			TResult::StaticClass(),
			bResultIsClass);
		UE_LOG(LogOSChooser, Verbose, TEXT("[OSChooser] Table: %s | Result: %s | Path: %s"),
			*ChooserTable->GetName(),
			Obj ? *Obj->GetName() : TEXT("nullptr"),
			*ChooserTable->GetPathName());

		if (const UScriptStruct* StructType = TParamStruct::StaticStruct())
		{
			FString StructString;
			StructType->ExportText(StructString, &Temp, nullptr, nullptr, PPF_None, nullptr);
			UE_LOG(LogOSChooser, Verbose, TEXT("Params: %s"), *StructString);
		}
		

		
		return Cast<TResult>(Obj);
	}
}


