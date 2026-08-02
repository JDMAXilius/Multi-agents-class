// Breachpoint. The one widget base. Every screen and panel derives from this.
#pragma once

#include "CommonActivatableWidget.h"
#include "CommonInputModeTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Input/UIActionBindingHandle.h"
#include "UI/BRUITypes.h"

#include "BRActivatableWidget.generated.h"

class UBRVM_Combat;
class UBRVM_Match;
class UBRUIManagerSubsystem;

UENUM(BlueprintType)
enum class EBRWidgetInputMode : uint8
{
	Inherit,

	Game,

	GameAndMenu,

	Menu
};

UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	UBRVM_Combat* GetCombatViewModel() const;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	UBRVM_Match* GetMatchViewModel() const;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	virtual void BindViewModels() {}

	virtual void UnbindViewModels() {}

	UBRUIManagerSubsystem* GetUIManager() const;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Input")
	EBRWidgetInputMode InputMode = EBRWidgetInputMode::Inherit;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Input")
	bool bHideCursorDuringViewportCapture = true;
};
