// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSFXEmitter.h"
#include "OSFXEmitter_Wwise.generated.h"

class UOSWwisePresetAsset;
/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSFXEmitter_Wwise : public UOSFXEmitter
{
	GENERATED_BODY()
public:
	// Base selection
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition="DefaultPreset==nullptr", EditConditionHides))
	TObjectPtr<UOSWwisePresetAsset> DefaultPresetAsset;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, meta=(EditCondition="DefaultPresetAsset==nullptr", EditConditionHides))
	TObjectPtr<UOSWwisePreset> DefaultPreset;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOnTarget = false; // false = instigator, true = target
	
	virtual void Emit_Implementation(const FOSFXContext& Ctx) const override;

private:
	static bool IsActorLocallyControlled(const AActor* Actor);
	static class UAkComponent* FindAkComponent(AActor* Actor);
	static void ApplyParams(UAkComponent* AkComp, const FOSWwiseParamPack& Pack);
	const UOSWwisePreset* ResolveDefaultPreset() const;
};
