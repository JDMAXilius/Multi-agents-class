#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "BRAnimNotify_GameplayEvent.generated.h"

/**
 * A montage notify that carries its own gameplay tag, in C++, as a typed property.
 *
 * WHY THIS REPLACES THE STRING TABLE. The first version of this seam matched `FName` notify
 * names against a `TMap` in `BRAnimInstance.cpp`. It worked, and it put the correctness of the
 * whole law-4 seam inside a `.uasset` where nothing could check it:
 *
 *   · a notify typed `ReloadCommmit` raises nothing, forever, with no error
 *   · a windowed moment authored as a POINT notify never fires its end, which silently restores
 *     a melee trace window that opens and is never closed -- free hits, from one wrong dropdown
 *   · no diff, no grep, no assert. R18's exact reasoning, load-bearing on binary data.
 *
 * With the tag on the notify, **there is no name to get wrong**. The tag is a `FGameplayTag`, so
 * the tag picker only offers real tags, a renamed tag updates by redirector, and `Event.*` is
 * greppable from C++ again.
 *
 * WHAT IT STILL IS NOT. It raises an event and nothing else -- `animation.md` law 4: notifies
 * announce that a moment arrived, and the sim decides on the authority what it means. Both gates
 * (one mesh speaks, and only authority/owner send) live in `UBRAnimInstance::SendSeamGameplayEvent`,
 * so they cannot be bypassed by adding a notify.
 *
 * Use this for a POINT in time -- `Event.Weapon.ReloadCommit`, `Event.Weapon.SwapCommit`. For a
 * span with an open and a close, use `UBRAnimNotifyState_GameplayEventWindow`.
 */
UCLASS(meta = (DisplayName = "BR Gameplay Event"))
class BREACHPOINT_API UBRAnimNotify_GameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:

	UBRAnimNotify_GameplayEvent();

	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:

	/**
	 * The moment this notify announces. R17's vocabulary: `Event.<Verb>.<Moment>`, where the
	 * moment is what just happened in the animation, never what should result from it.
	 */
	UPROPERTY(EditAnywhere, Category = "Breachpoint", meta = (Categories = "Event"))
	FGameplayTag EventTag;
};
