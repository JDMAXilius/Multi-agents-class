// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotifyGameplayEvent.generated.h"

/**
 * 
 */
UCLASS()
class ZORANSRESISTANCE_API UAnimNotifyGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Montage")
	FGameplayTag EventTag;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	
};
