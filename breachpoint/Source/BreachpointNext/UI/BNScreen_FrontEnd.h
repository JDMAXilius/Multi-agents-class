#pragma once

#include "UI/BNActivatableWidget.h"
#include "BNScreen_FrontEnd.generated.h"

class UButton;
class UTextBlock;

/**
 * THE MAIN MENU — Menu layer, Menu input, the first thing a booted game shows.
 *
 * Figma truth: `01-MENU-MEASURED.md` §2 — FE_Play `21:32824`. One Menu Combo at (69,138)
 * 349×510 on the left third, the centre column is the 3D subject and the right is status.
 * The layout lives in the WBP; this class owns behaviour and strings, per the house split
 * (a WBP never types a string, C++ never places a pixel).
 *
 * TWO live rows, not four. The design's rail has four slots; this project today has
 * exactly two actions a button can honestly perform — PLAY and QUIT — and a button that
 * opens nothing is worse than a button that is absent (the pause screen's own ruling).
 * The WBP keeps the four-slot geometry; the two dead slots ship disabled with their
 * design names, which is what the reference game itself does with locked entries.
 *
 * Session shape (founder, 1 Sep): no online sessions — one computer, one player, bots.
 * PLAY therefore goes straight to the play-setup screen; there is no matchmaking search
 * and nothing here pretends there is.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNScreen_FrontEnd : public UBNActivatableWidget
{
	GENERATED_BODY()

public:
	UBNScreen_FrontEnd();

protected:
	virtual void NativeOnInitialized() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UFUNCTION()
	void HandlePlayClicked();

	UFUNCTION()
	void HandleQuitClicked();

	/** PLAY — pushes the play-setup screen on the same Menu stack, so back pops home. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UButton> PlayButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UButton> QuitButton;

	/** The rail's hint strip (Description Frame, 349×37). Set from C++ on focus-worthy
	 *  moments; a WBP never types the copy. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UTextBlock> DescriptionText;
};
