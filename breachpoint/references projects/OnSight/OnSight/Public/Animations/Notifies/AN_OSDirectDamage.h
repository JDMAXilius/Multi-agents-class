#pragma once
// Anim notify: fires GameplayEvent.DirectDamage to owner's ASC at the visual impact frame.
// Abilities waiting on WaitGameplayEvent receive this and apply damage to their locked target.
// No configurable properties — purely a timing signal. Damage amount and target live on the ability.

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_OSDirectDamage.generated.h"

UCLASS(meta=(DisplayName="AN Direct Damage"))
class ONSIGHT_API UAN_OSDirectDamage : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
