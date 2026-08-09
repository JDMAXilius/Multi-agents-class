// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ZoransResistance/Actors/CosmeticWeapon.h"
#include "ANS_CosmeticWeapon.generated.h"

/**
 * 
 */
UCLASS()
class ZORANSRESISTANCE_API UANS_CosmeticWeapon : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Cosmetic Weapon Event")
	TSubclassOf<ACosmeticWeapon> CosmeticWeaponClass = nullptr;

protected:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
