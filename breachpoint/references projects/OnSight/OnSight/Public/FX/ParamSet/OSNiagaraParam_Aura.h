// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "OSNiagaraParamSet.h"
#include "OSNiagaraParam_Aura.generated.h"



/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSNiagaraParam_Aura : public UOSNiagaraParamSet
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UMaterialInstance> Aura_SkeletalMesh_MaterialOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UMaterialInstance> Aura_Eyes_MaterialInstance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(NiagaraVar="Lifetime"))
	float Lifetime = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(NiagaraVar="SpawnRate"))
	float SpawnRate = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(NiagaraVar="SpawnRate2"))
	float SpawnRate2 = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(NiagaraVar="SpawnCount"))
	int32 SpawnCount = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(NiagaraVar="ManualScale"))
	float ManualScale = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(NiagaraVar="Emissive"))
	float Emissive = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(NiagaraVar="Opacity"))
	float Opacity = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(NiagaraVar="MaterialInterface_1"))
	TObjectPtr<UMaterialInterface> MaterialInterface_1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(NiagaraVar="MaterialInterface_2"))
	TObjectPtr<UMaterialInterface> MaterialInterface_2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(NiagaraVar="MaterialInterface_3"))
	TObjectPtr<UMaterialInterface> MaterialInterface_3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(NiagaraVar="MaterialInterface_4"))
	TObjectPtr<UMaterialInstance> MaterialInterface_4;
	
};
