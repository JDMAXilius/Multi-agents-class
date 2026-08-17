#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "BRAnimNotifyState_GameplayEventWindow.generated.h"

/**
 * A montage WINDOW that raises one tag when it opens and a different tag when it closes.
 *
 * THIS IS THE CLASS THAT MAKES THE MELEE WINDOW SAFE. The string-table version of this seam had
 * a defect that no review of the C++ could have caught, because the defect lived in an asset: a
 * window authored as a POINT notify fires its begin and never its end, so
 * `Event.Melee.WindowEnd` is never raised, `BRGA_Melee` opens the trace window and is never told
 * to close it, and every subsequent swing hits. One wrong dropdown selection, no diff, no error.
 *
 * A `UAnimNotifyState` cannot make that mistake. **`NotifyEnd` is guaranteed by the engine** --
 * `FAnimMontageInstance` walks its active branching points in reverse on terminate and ends each
 * one, so an interrupted melee still closes its window and `animation.md` law 5 ("rollback
 * leaves no residue") holds by construction rather than by discipline.
 *
 * Both tags are typed properties, so neither can be misspelled, and both are greppable.
 */
UCLASS(meta = (DisplayName = "BR Gameplay Event Window"))
class BREACHPOINT_API UBRAnimNotifyState_GameplayEventWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:

	UBRAnimNotifyState_GameplayEventWindow();

	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:

	/** Raised as the window opens. `Event.Melee.WindowBegin`. */
	UPROPERTY(EditAnywhere, Category = "Breachpoint", meta = (Categories = "Event"))
	FGameplayTag BeginEventTag;

	/**
	 * Raised as the window closes — including when the montage is INTERRUPTED.
	 *
	 * Separate from the begin tag rather than derived from it. R17's vocabulary names the two
	 * moments independently (`WindowBegin` / `WindowEnd`), and a scheme that mangled one string
	 * into the other would be a naming convention enforced by nothing.
	 */
	UPROPERTY(EditAnywhere, Category = "Breachpoint", meta = (Categories = "Event"))
	FGameplayTag EndEventTag;
};
