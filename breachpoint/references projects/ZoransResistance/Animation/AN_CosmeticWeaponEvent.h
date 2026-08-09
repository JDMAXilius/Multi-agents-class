// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_CosmeticWeaponEvent.generated.h"

/**
 * 
 */
UCLASS()
class ZORANSRESISTANCE_API UAN_CosmeticWeaponEvent : public UAnimNotify
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Cosmetic Weapon Event")
	int32 EventIndex = -1;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
