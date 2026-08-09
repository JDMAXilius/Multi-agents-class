#pragma once
// Anim notify: fires GameplayEvent.Grab.ActiveFrame to owner's ASC at the Entry-section active frame.
// UGA_OSGrab's WaitGameplayEvent subscriber receives this and runs the confirm trace that
// promotes Entry → Grab section on hit, or lets Entry play through on miss.
//
// NOT authority-gated: must fire on both server and owning client so the LocalPredicted
// hit-path runs on both sides (client-side warp seed + attacker rotation prediction + local
// section jump). Cross-ASC server-authoritative side effects (claim GE, victim reaction
// trigger, net-boost) are authority-gated inside UGA_OSGrab::OnGrabActiveFrameEvent.
//
// No configurable properties — purely a timing signal. Trace geometry + tag exclusions live
// on the ability itself.

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_OSGrabActiveFrame.generated.h"

UCLASS(meta=(DisplayName="AN Grab Active Frame"))
class ONSIGHT_API UAN_OSGrabActiveFrame : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
