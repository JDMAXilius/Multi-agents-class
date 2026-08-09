#pragma once
// Debug HUD: CVar-gated overlays above all characters. Local viewport only.
// Overlays: health bars, nameplates, rotation, targeting, grab, mantle.

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OSHUD.generated.h"

UCLASS()
class ONSIGHT_API AOSHUD : public AHUD
{
	GENERATED_BODY()

public:
	AOSHUD();
	virtual void DrawHUD() override;

private:
	// Warp target names to search for in rotation debug overlay.
	UPROPERTY(EditDefaultsOnly, Category = "Debug|MotionWarping")
	TArray<FName> DebugWarpTargetNames;

#if !UE_BUILD_SHIPPING
	/** Iterate visible characters and draw enabled overlays above each. */
	void DrawDebugOverheadInfo();

	/** Draw a colored health bar and HP text at ScreenX. Advances DrawY downward. */
	void DrawHealthBar(const class AOSCharacter* Character, float ScreenX, float& DrawY);

	/** Draw the player's name at ScreenX / DrawY. Advances DrawY downward. */
	void DrawNameplate(const class AOSCharacter* Character, float ScreenX, float& DrawY);

	/** Draw compact "[DEBUG] GOD | REGEN_HP | ..." strip in top-left when any cheat is active. */
	void DrawDebugStatusOverlay();

	/** Draw rotation system debug overlay: MW/CMC arrows, target indicators, on-screen text.
	 *  CVar-gated: os.debug.RotationViz. Per-frame drawing for continuous state visibility. */
	void DrawRotationDebugOverlay();

	/** Draw targeting debug overlay: candidate spheres with status colors, scores, radar sweep,
	 *  connection beam to chosen target, and final attack direction arrow.
	 *  CVar-gated: OS.Debug.TargetingViz. Shows WHY a target was or wasn't chosen. */
	void DrawTargetingDebugOverlay();

	/** Draw grab debug overlay: centerline, MW warp targets, socket positions, reach/gap labels.
	 *  CVar-gated: os.debug.GrabViz. Per-frame drawing tracks live warp + bone state. */
	void DrawGrabDebugOverlay();

	/** Draw grab diagnostic overlay: per grabbing/grabbed character, role, local vs replicated
	 *  position delta, NetFreq/NetPrio, grab state tags. CVar-gated: OnSight.Grab.DiagLogging >= 2.
	 *  Complement to [GrabDiag] log trace — live on-screen view during playtests. */
	void DrawGrabDiagnostics();

	/** Draw mantle debug overlay: continuous trace visualization, rejection reasons, capsule clearance.
	 *  CVar-gated: os.debug.MantleViz. Shows real-time mantle feasibility from player's current position. */
	void DrawMantleDebugOverlay();
#endif
};
