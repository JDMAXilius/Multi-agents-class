// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSNiagaraParamSet.h"
#include "OSNiagaraParamSet_Generic.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FOSNiagaraFloatParam
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Value = 0.f;
};

USTRUCT(BlueprintType)
struct FOSNiagaraIntParam
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Value = 0;
};

USTRUCT(BlueprintType)
struct FOSNiagaraVectorParam
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Value = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FOSNiagaraColorParam
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor Value = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct FOSNiagaraObjectParam
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UObject> Value = nullptr;
};


UCLASS()
class ONSIGHT_API UOSNiagaraParamSet_Generic : public UOSNiagaraParamSet
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, FOSNiagaraFloatParam> FloatParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, FOSNiagaraIntParam> IntParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, FOSNiagaraVectorParam> VectorParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, FOSNiagaraColorParam> ColorParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, FOSNiagaraObjectParam> ObjectParams;

	virtual void ApplyTo_Implementation(UNiagaraComponent* Comp) const override;
	virtual void MergeFrom_Implementation(const UOSNiagaraParamSet* OtherBase) override;
};
