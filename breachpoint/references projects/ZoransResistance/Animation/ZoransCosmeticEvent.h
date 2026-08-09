// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ZoransResistance/AbilitySystem/GameplayTasks/PlayMontageAndWaitForEvent.h"
#include "ZoransCosmeticEvent.generated.h"

/**
 * 
 */
UCLASS()
class ZORANSRESISTANCE_API UZoransCosmeticEvent : public UAnimNotify
{
	GENERATED_BODY()
public:
	UZoransCosmeticEvent();
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Anim Notify Cosmetic Event")
	FName CosmeticEventName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Anim Notify Cosmetic Event")
	FVector EventLocation = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadOnly, Category = "Anim Notify Cosmetic Event")
	FMontageCosmeticData CosmeticData;

protected:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
