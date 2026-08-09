#pragma once
// Timing-only notify state for motion warping during attacks.
//
// Warp target INJECTION is handled by GA_OSAttack::InjectWarpTargets (before montage plays).
// This notify handles TIMING: when the engine's warp window opens, it resolves the
// correct WarpTargetName from the animating GA (or falls back to notify properties)
// and creates the root motion modifier via the base class.
//
// For non-GA montages (traversal, etc.): injects fallback targets from FindBestSoftTarget.

#include "CoreMinimal.h"
#include "AnimNotifyState_MotionWarping.h"
#include "OSAnimNotifyState_OSTrackMotionWarpTarget.generated.h"

class UMotionWarpingComponent;
class URootMotionModifier;
class UAnimSequenceBase;

/**
 * Timing-only notify: resolves WarpTargetName from GA and creates root motion modifier.
 *
 * At AddRootMotionModifier (engine callback when warp window opens):
 * - Resolves WarpTargetName from the animating GA_OSAttack (or falls back to notify property)
 * - For non-GA montages: injects fallback targets via FindBestSoftTarget
 * - Configures the modifier template (set-restore pattern) and delegates to Super
 *
 * NOTE: UAnimNotifyState objects are shared CDOs — no mutable per-instance state allowed.
 * Target names are re-resolved from the GA in each method that needs them.
 */
UCLASS(DisplayName = "OS Track Motion Warp Target", meta = (DisplayName = "OS Track Motion Warp Target"))
class ONSIGHT_API UOSAnimNotifyState_OSTrackMotionWarpTarget : public UAnimNotifyState_MotionWarping
{
	GENERATED_BODY()

public:
	UOSAnimNotifyState_OSTrackMotionWarpTarget(const FObjectInitializer& ObjectInitializer);

	// --- Fallback properties (used when no GA_OSAttack is animating) ---

	/** Fallback warp target name. Overridden by GA's WarpTargetName when available. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OS Track Motion Warp|Fallback")
	FName WarpTargetName = "AttackTarget";

	/** Fallback max range for FindBestSoftTarget (non-attack montages). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OS Track Motion Warp|Fallback", meta = (ClampMin = "0", UIMin = "0"))
	float MaxTargetRange = 1200.f;

	/** Fallback: warp distance when no target found (non-attack montages). 0 = no position warp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OS Track Motion Warp|Fallback", meta = (ClampMin = "0", UIMin = "0"))
	float DefaultWarpDistanceWhenNoTarget = 100.f;

	virtual FString GetNotifyName_Implementation() const override;

	virtual URootMotionModifier* AddRootMotionModifier_Implementation(
		UMotionWarpingComponent* MotionWarpingComp,
		const UAnimSequenceBase* Animation,
		float StartTime, float EndTime) const override;

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

#if WITH_EDITOR
	virtual FLinearColor GetEditorColor() override { return FLinearColor(0.9f, 0.5f, 0.1f, 1.0f); }
	virtual void ValidateAssociatedAssets() override;
#endif

private:
	/** Resolve warp target name from the animating GA (if any), falling back to notify property. */
	FName ResolveWarpName(const AActor* Owner) const;

	/** For non-GA montages: try to find a target and inject a static warp. Returns true if a target was injected. */
	bool TryInjectFallbackTarget(UMotionWarpingComponent* MWComp, FName ResolvedName) const;
};
