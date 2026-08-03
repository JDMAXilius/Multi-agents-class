#pragma once

#include "UI/Components/BRHairlineBorder.h"

#include "BRScrim.generated.h"

/**
 * `UBRScrim` -- the dim plate behind every modal (SCREEN-MANIFEST Sec 5 Tier 0, all 31 screens'
 * modal paths; Sec 7.1 places it at slot [1] of the screen root, collapsed by default).
 *
 * It is `UBRHairlineBorder` with zero edges and a fill, so it reuses the one leaf paint path
 * instead of adding a second primitive -- a scrim IS a token-coloured plate. One draw element,
 * no WBP asset, no texture.
 *
 * Two things it deliberately does NOT do:
 *  - it does not push or pop anything. Back-handling and focus restoration are the CommonUI
 *    activatable stack's job (`ue5-ui-architecture` Sec 1); a scrim that closed its own modal
 *    would be a second, competing navigation spine.
 *  - it does not hide the layer beneath. Layers are a stack, not a visibility toggle.
 *
 * Unlike its base it IS hit-testable when shown: swallowing input aimed at the screen behind a
 * modal is the entire functional half of a scrim.
 */
UCLASS(meta = (DisableNativeTick))
class BREACHPOINT_API UBRScrim : public UBRHairlineBorder
{
	GENERATED_BODY()

public:
	UBRScrim();

	/**
	 * Show or hide the plate. Shown => `Visible` (blocks pointer input aimed at the screen
	 * behind). Hidden => `Collapsed` (takes no layout, costs no paint).
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetScrimActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	bool IsScrimActive() const;

	/**
	 * COMPONENT-SPECS Sec 6: the design frame is 1280 x 720. Recorded for reference only -- the
	 * scrim is authored full-bleed (anchors 0,0 -> 1,1, zero offsets) per SCREEN-MANIFEST Sec 7.3,
	 * so it covers ultrawide correctly without either number being typed anywhere.
	 */
	static constexpr float DesignCanvasWidth = 1280.0f;
	static constexpr float DesignCanvasHeight = 720.0f;
};
