// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ZoransResistance/AbilitySystem/GameplayTasks/PlayMontageAndWaitForEvent.h"
#include "ZoransCosmeticState.generated.h"

/**
 * 
 */
UCLASS()
class ZORANSRESISTANCE_API UZoransCosmeticState : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Anim Notify Cosmetic State")
	FName CosmeticEventName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Anim Notify Cosmetic State")
	FVector EventLocation = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadOnly, Category = "Anim Notify Cosmetic State")
	FMontageCosmeticData CosmeticData;

protected:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	UPROPERTY()
	UNiagaraComponent* SpawnedNiagaraEffect = nullptr;

	UPROPERTY()
	UAudioComponent* SpawnedSoundEffect = nullptr;
};
