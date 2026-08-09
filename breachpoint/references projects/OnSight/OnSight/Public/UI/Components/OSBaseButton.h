#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "OSBaseButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBaseButtonClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBaseButtonPressed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBaseButtonReleased);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBaseButtonHovered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBaseButtonUnhovered);

// FWD Declarations
class UButton;
class UTextBlock;


/*
  Reusable design-system button. Single behavior path for mouse and controller/keyboard.
  Derive a Blueprint from this, implement the _InternalBP hooks for visuals.
 */
UCLASS()
class ONSIGHT_API UOSBaseButton : public UUserWidget
{
	GENERATED_BODY()

public:
	UOSBaseButton(const FObjectInitializer& ObjectInitializer);

	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- Bound Widgets --- 

	UPROPERTY(meta = (BindWidget))
	UButton* Button;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_ButtonName;

	// --- Exposed Properties --- 

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Base Button")
	FText ButtonName = FText::FromString(TEXT("ButtonName"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Button")
	FMargin NormalPadding = FMargin(0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Button")
	FMargin HoveredPadding = FMargin(5.0f);

	// --- Slot Properties (applied by parent container after adding to panel) ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Button|Slot")
	FMargin SlotPadding = FMargin(0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Button|Slot")
	FSlateChildSize SlotSize = FSlateChildSize(ESlateSizeRule::Automatic);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Button|Slot")
	TEnumAsByte<EHorizontalAlignment> SlotHorizontalAlignment = HAlign_Fill;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Button|Slot")
	TEnumAsByte<EVerticalAlignment> SlotVerticalAlignment = VAlign_Fill;

	// --- Toggle --- 

	UFUNCTION(BlueprintCallable, Category = "Base Button")
	void SetActive(bool bNewActive);

	// --- Dispatchers (for parent widgets to bind to) --- 

	UPROPERTY(BlueprintAssignable, Category = "Base Button")
	FOnBaseButtonClicked OnClicked;

	UPROPERTY(BlueprintAssignable, Category = "Base Button")
	FOnBaseButtonPressed OnPressed;

	UPROPERTY(BlueprintAssignable, Category = "Base Button")
	FOnBaseButtonReleased OnReleased;

	UPROPERTY(BlueprintAssignable, Category = "Base Button")
	FOnBaseButtonHovered OnHovered;

	UPROPERTY(BlueprintAssignable, Category = "Base Button")
	FOnBaseButtonUnhovered OnUnhovered;

protected:
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	// --- Blueprint Hooks (implement in derived BP for visuals) --- 

	UFUNCTION(BlueprintImplementableEvent, Category = "Base Button|Hooks")
	void Clicked();

	UFUNCTION(BlueprintImplementableEvent, Category = "Base Button|Hooks")
	void Pressed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Base Button|Hooks")
	void Released();

	UFUNCTION(BlueprintImplementableEvent, Category = "Base Button|Hooks")
	void Hovered();

	UFUNCTION(BlueprintImplementableEvent, Category = "Base Button|Hooks")
	void Unhovered();

	UFUNCTION(BlueprintImplementableEvent, Category = "Base Button|Hooks")
	void OnActiveStateChanged(bool bActive);

private:
	// --- UButton Callbacks (bound in NativeConstruct) --- 

	UFUNCTION()
	void OnButtonClicked();

	UFUNCTION()
	void OnButtonPressed();

	UFUNCTION()
	void OnButtonReleased();

	UFUNCTION()
	void OnButtonHovered();

	UFUNCTION()
	void OnButtonUnhovered();

	// --- Centralized Internal Logic (single path for mouse + focus) --- 

	void Clicked_Internal();
	void Pressed_Internal();
	void Released_Internal();
	void Hovered_Internal();
	void Unhovered_Internal();

	// --- Style Helpers --- 

	void ApplyStyle();
	void ApplyHoveredStyle();
	void ApplyPressedStyle();

	FButtonStyle CachedStyle;
	FButtonStyle HoveredCachedStyle;
	FButtonStyle PressedCachedStyle;

	bool bIsHoverVisualActive = false;
	bool bIsActive = false;
};
