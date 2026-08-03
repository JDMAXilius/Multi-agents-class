#pragma once

#include "UI/BRActivatableWidget.h"

#include "BRModal_Warning.generated.h"

class UBRScrim;
class UCommonButtonBase;
class UCommonTextBlock;

/**
 * Resolution of a confirm request. Fires EXACTLY ONCE per pushed modal:
 * `true` on confirm, `false` on cancel, back, or any other pop of the stack.
 *
 * Single-cast on purpose -- a confirm has ONE owner, the caller that raised it. Bind it with
 * `CreateUObject` / `CreateWeakLambda` so the binding dies with the caller; the modal outliving a
 * dead caller and firing into freed memory is exactly the crash `ue5-ui-architecture` Sec 2 names.
 */
DECLARE_DELEGATE_OneParam(FBRConfirmResolved, bool /* bConfirmed */);

/**
 * The payload `UBRModal_Warning` takes INSTEAD of a ViewModel (SCREEN-MANIFEST Sec 4.8:
 * "takes an `FBRConfirmRequest` payload, not a ViewModel. Confirm/cancel resolve a delegate on the
 * *caller*").
 *
 * There is no ViewModel because there is no replicated state here: a confirm prompt is a question
 * the caller already knows the answer shape of. Routing it through a ViewModel would invent a
 * second owner for a value that has exactly one.
 */
USTRUCT(BlueprintType)
struct FBRConfirmRequest
{
	GENERATED_BODY()

	/** Renders into the title band. Empty collapses the band rather than drawing an empty rule. */
	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FText Title;

	/** The question itself. Empty collapses the body. */
	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FText Body;

	/**
	 * Empty leaves whatever label the WBP authored -- it never blanks the button.
	 *
	 * CONTRACT GAP: `UCommonButtonBase` has no label setter in UE 5.8 and `UBRHighlightButton`
	 * (SCREEN-MANIFEST Sec 5 Tier 1) does not exist yet, so these are applied through the OPTIONAL
	 * `ConfirmLabelText` / `CancelLabelText` blocks below. When the highlight button lands with a
	 * `SetLabelText` (the precedent is `UBRMenuRow::SetLabelText`), `ApplyRequest` should call that
	 * instead and the two optional blocks come out.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FText ConfirmLabel;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FText CancelLabel;

	/**
	 * Deliberately NOT a `UPROPERTY`: a non-dynamic delegate is not reflectable, and it must not
	 * be, because a Blueprint binding here would be gameplay logic living in a widget graph.
	 * C++ callers only.
	 */
	FBRConfirmResolved OnResolved;
};

/**
 * `UBRModal_Warning` -- `OV_Warning` / `Full Page Warning`, node `940:10676` in the reference file
 * `Kn87U5sy2VD0lP8K7h4LcQ` (SCREEN-MANIFEST Sec 4.8). WBP: `WBP_Modal_Warning` (Sec 2: strip
 * `UBR`, prefix `WBP_`). Pushes to `Layer.Modal`.
 *
 * THE ONE RULE THIS CLASS EXISTS TO ENFORCE: the caller is answered exactly once.
 * Confirm answers `true`; cancel answers `false`; and **popping the stack is itself the cancel
 * path** -- `NativeOnDeactivated` resolves `false` if nothing else has. There is no bespoke back
 * handler: `bIsBackHandler` is CommonUI's own flag, so back deactivates this widget, the stack
 * releases it, and CommonUI restores focus to the layer beneath. Hand-rolled back handling is a
 * finding (`ue5-ui-architecture` Sec 6), and so is a caller left waiting on an answer that never
 * comes because the modal was popped by someone else.
 *
 * It reads no ViewModel and touches no authoritative state. A confirm resolves to the CALLER,
 * which is where the intent-to-the-PlayerController boundary lives.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRModal_Warning : public UBRActivatableWidget
{
	GENERATED_BODY()

public:
	UBRModal_Warning();

	/**
	 * COMPONENT-SPECS Sec 6 / SCREEN-MANIFEST Sec 7.3: the warning is a FULL-PAGE overlay, so the
	 * design frame is recorded for reference only -- the WBP anchors 0,0 -> 1,1 with zero offsets
	 * and is therefore correct at any aspect without either number being typed into a slot.
	 */
	static constexpr float DesignCanvasWidth = 1280.0f;
	static constexpr float DesignCanvasHeight = 720.0f;

	/**
	 * Supply the payload. Safe to call before the widget is constructed: the values are stored and
	 * applied again from `NativeOnActivated`, because `BindWidget` pointers are null until the
	 * underlying Slate widget is built and a modal is routinely configured on the same frame it is
	 * pushed.
	 */
	void SetConfirmRequest(const FBRConfirmRequest& InRequest);

	/**
	 * A modal always takes Menu input, whichever layer it was pushed over. This is declared in C++
	 * rather than by the inherited `InputMode` property precisely because that property is
	 * `EditDefaultsOnly` -- a WBP could otherwise leave a modal over the HUD in Game input, with no
	 * cursor and no focus. The engine's input-mode setters are never called from a widget body --
	 * declaring the config here is the only lawful path (`ue5-ui-architecture` Sec 2).
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	/**
	 * Cancel is the safe answer, so cancel takes focus. CommonUI does the rest of the navigation
	 * (Sec 6: focus, nav and back are the stack's job, not this class's).
	 */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_Modal_Warning`. Figma layer -> UMG name:
	//   `Scrim`        -> Scrim          `Page Title` text -> TitleText
	//   `Body`         -> BodyText       `Highlight Button` (accept) -> ConfirmButton
	//   `Highlight Button` (decline)     -> CancelButton
	// The buttons are typed as `UCommonButtonBase`, not as a Breachpoint button class: the WBP may
	// host `WBP_HighlightButton` (or anything else deriving from it) and this contract still holds.
	// -------------------------------------------------------------------------------------------

	/** Blocks pointer input aimed at the screen beneath. Activated from C++, collapsed by default. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UBRScrim> Scrim;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> BodyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonButtonBase> ConfirmButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonButtonBase> CancelButton;

	/** See `FBRConfirmRequest::ConfirmLabel` -- present only until the button carries its own label setter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> ConfirmLabelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> CancelLabelText;

private:
	void ApplyRequest();

	void HandleConfirmClicked();
	void HandleCancelClicked();

	/** The single funnel. Answers the caller once and only once, then forgets the binding. */
	void Resolve(bool bConfirmed);

	FBRConfirmRequest Request;

	bool bResolved = false;
};
