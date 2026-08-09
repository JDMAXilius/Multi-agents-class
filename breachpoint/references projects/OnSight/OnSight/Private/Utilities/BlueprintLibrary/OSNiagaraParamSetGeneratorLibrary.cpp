// Fill out your copyright notice in the Description page of Project Settings.


#include "Utilities/BlueprintLibrary/OSNiagaraParamSetGeneratorLibrary.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#endif
#include "NiagaraSystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FX/ParamSet/OSNiagaraParamSet_Generic.h"
#include "FX/ParamSet/Wrapper/OSNiagaraParamSetAsset.h"
#include "UObject/SavePackage.h"
static void GetNiagaraUserParams(const UNiagaraSystem* System, TArray<FNiagaraVariable>& OutVars)
{
	OutVars.Reset();
	if (!System) return;

	const FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
	Store.GetUserParameters(OutVars); // exposed user params
}


static bool IsFloat(const FNiagaraTypeDefinition& T) { return T == FNiagaraTypeDefinition::GetFloatDef(); }
static bool IsInt  (const FNiagaraTypeDefinition& T) { return T == FNiagaraTypeDefinition::GetIntDef(); }
static bool IsVec3 (const FNiagaraTypeDefinition& T) { return T == FNiagaraTypeDefinition::GetVec3Def(); }
static bool IsColor(const FNiagaraTypeDefinition& T){ return T == FNiagaraTypeDefinition::GetColorDef(); }

static bool IsUObject(const FNiagaraTypeDefinition& T)
{
	return T.IsUObject();
}

static void PopulateGenericFromSystem(
	UOSNiagaraParamSet_Generic* Params,
	UNiagaraSystem* System,
	bool bAssignDefaults)
{
	if (!Params || !System) return;

	Params->System = System;

	const FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();

	TArray<FNiagaraVariable> UserVars;
	Store.GetUserParameters(UserVars);

	for (const FNiagaraVariable& Var : UserVars)
	{
		const FName Name = Var.GetName();
		const FNiagaraTypeDefinition Type = Var.GetType();

		// float
		if (Type == FNiagaraTypeDefinition::GetFloatDef())
		{
			auto& Entry = Params->FloatParams.FindOrAdd(Name);

			if (bAssignDefaults)
			{
				float V = 0.f;
				Store.GetParameterValue(V, Var);
				Entry.bOverride = true;
				Entry.Value = V;
			}
			continue;
		}

		// int
		if (Type == FNiagaraTypeDefinition::GetIntDef())
		{
			auto& Entry = Params->IntParams.FindOrAdd(Name);

			if (bAssignDefaults)
			{
				int32 V = 0;
				Store.GetParameterValue(V, Var);
				Entry.bOverride = true;
				Entry.Value = V;
			}
			continue;
		}

		// vec3
		if (Type == FNiagaraTypeDefinition::GetVec3Def())
		{
			auto& Entry = Params->VectorParams.FindOrAdd(Name);

			if (bAssignDefaults)
			{
				FVector3f V3f = FVector3f::ZeroVector;
				Store.GetParameterValue(V3f, Var);
				Entry.bOverride = true;
				Entry.Value = FVector(V3f);
			}
			continue;
		}

		// color
		if (Type == FNiagaraTypeDefinition::GetColorDef())
		{
			auto& Entry = Params->ColorParams.FindOrAdd(Name);

			if (bAssignDefaults)
			{
				FLinearColor C = FLinearColor::White;
				Store.GetParameterValue(C, Var);
				Entry.bOverride = true;
				Entry.Value = C;
			}
			continue;
		}

		// materials/textures/etc.
		if (Type.IsUObject())
		{
			auto& Entry = Params->ObjectParams.FindOrAdd(Name);

			if (bAssignDefaults)
			{
				TObjectPtr<UObject> Obj = Store.GetUObject(Var);
				if (Obj)
				{
					Entry.bOverride = true;
					Entry.Value = Obj.Get();
				}
				else
				{
					Entry.bOverride = false;
					Entry.Value = nullptr;
				}
			}
			continue;
		}

		UE_LOG(LogTemp, Warning, TEXT("Unsupported Niagara user param: %s (%s)"), *Name.ToString(), *Type.GetName());
	}
}

static bool SaveGeneratedPackage(UPackage* Package, UObject* Asset)
{
	if (!Package || !Asset) return false;

	const FString LongPackageName = Package->GetName();
	if (!FPackageName::IsValidLongPackageName(LongPackageName))
	{
		UE_LOG(LogTemp, Error, TEXT("Save failed: invalid long package name: %s"), *LongPackageName);
		return false;
	}

	const FString Filename = FPackageName::LongPackageNameToFilename(
		LongPackageName,
		FPackageName::GetAssetPackageExtension());

	if (Filename.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Save failed: empty filename for package: %s"), *LongPackageName);
		return false;
	}

	// does folder exist
	const FString Directory = FPaths::GetPath(Filename);
	IFileManager::Get().MakeDirectory(*Directory, true);

	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_None;
	Args.Error = GError;

	const bool bOk = UPackage::SavePackage(Package, Asset, *Filename, Args);
	if (!bOk)
	{
		UE_LOG(LogTemp, Error, TEXT("SavePackage returned false: %s"), *Filename);
	}
	return bOk;
}

// Function body is WITH_EDITOR-only (uses AssetTools + editor-only asset creation APIs), but the
// function itself must exist in all builds to satisfy the reflection declaration in the header
// (UHT constraint — see header comment). Non-editor builds get a stub that returns nullptr.
UObject* UOSNiagaraParamSetGeneratorLibrary::GenerateGenericParamSetAsset( UNiagaraSystem* System,
                                                                           const FString& PackagePath, const FString& AssetName, const bool bAssignDefaults )
{
#if WITH_EDITOR
	if (!System) return nullptr;

	const FString PackageName = PackagePath / AssetName;

	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid long package name: %s"), *PackageName);
		return nullptr;
	}

	FString UniquePackageName, UniqueAssetName;
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	AssetTools.CreateUniqueAssetName(PackageName, TEXT(""), UniquePackageName, UniqueAssetName);
	UPackage* Package = CreatePackage(*UniquePackageName);

	auto* Asset = NewObject<UOSNiagaraParamSetAsset>(Package, UOSNiagaraParamSetAsset::StaticClass(), *UniqueAssetName, RF_Public | RF_Standalone);
	auto* Params = NewObject<UOSNiagaraParamSet_Generic>(Asset, UOSNiagaraParamSet_Generic::StaticClass(), NAME_None, RF_Transactional);
	Params->System = System;

	Asset->Params = Params;


	PopulateGenericFromSystem(Params, System, bAssignDefaults);

	Package->MarkPackageDirty();
	Asset->MarkPackageDirty();

	FAssetRegistryModule& AssetRegistryModule =
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	AssetRegistryModule.AssetCreated(Asset);

	SaveGeneratedPackage(Package, Asset);

	return Asset;
#else
	// Non-editor stub: editor-only asset creation is not available outside WITH_EDITOR builds.
	// The UFUNCTION declaration exists in the header (UHT requirement) so the linker needs a
	// definition; this stub satisfies it. CallInEditor is a no-op in shipping builds anyway.
	return nullptr;
#endif
}


