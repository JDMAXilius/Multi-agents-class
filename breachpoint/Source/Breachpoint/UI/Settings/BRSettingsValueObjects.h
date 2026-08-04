#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/BRSettingsDataObject.h"

#include "BRSettingsValueObjects.generated.h"

/**
 * The leaves of the settings tree -- the objects that actually hold a value.
 *
 * TWO SHAPES COVER EVERY SETTING WE HAVE, and a third would need a real argument:
 *   `UBRSettingsValue_Scalar`   -- a continuous number on a slider (sensitivity, FOV, volume).
 *   `UBRSettingsValue_Discrete` -- a fixed set of named choices (on/off, Low..Epic, a resolution).
 *
 * A BOOL IS A DISCRETE WITH TWO OPTIONS. It is not its own class. The moment "toggle" became a
 * separate type it would need its own list entry, its own accessor shape and its own spec, to
 * express something `Discrete` already expresses exactly. `UBRSettingsRegistry::MakeToggle` is
 * the one-line factory that makes that ergonomic, so the saving is real and costs no class.
 */

/** How a scalar reads as text. Appearance only -- the stored value is always the raw float. */
UENUM(BlueprintType)
enum class EBRScalarDisplayFormat : uint8
{
	/** `1.40`. Two decimals. Sensitivity and anything with no natural unit. */
	Raw,

	/** `70%`. The value is expected to sit in 0..1 and is shown times 100, rounded. */
	Percent,

	/** `1.4x`. A multiplier over a baseline. */
	Multiplier,

	/** `103` -- whole numbers only. FOV, and anything where a decimal would be noise. */
	Integer
};

/**
 * Common base for the leaves. Holds the two snapshots that make "dirty" and "reset" meaningful:
 * the DEFAULT (the game's shipped value, captured once at bind time) and the BASELINE (what the
 * value was when the screen opened).
 *
 * They are genuinely different and conflating them is the classic bug: reset-to-default has to
 * go back to the shipped value even if the player has been running a custom one for months,
 * while the unsaved-changes prompt has to compare against what they had when they walked in.
 */
UCLASS(Abstract, BlueprintType)
class BREACHPOINT_API UBRSettingsValue : public UBRSettingsDataObject
{
	GENERATED_BODY()

public:
	/** True when this leaf's accessors were bound. An unbound leaf is inert and never dirty. */
	virtual bool IsBound() const PURE_VIRTUAL(UBRSettingsValue::IsBound, return false;);
};

/**
 * A continuous value edited on a slider.
 *
 * THE RANGE IS A DISPLAY RANGE, NOT A CLAMP ON THE STORED VALUE. `DisplayMin`/`DisplayMax` say
 * where the slider's ends sit. The setter is still free to clamp harder, and a value already
 * outside the range (from a config edit, or a build where the range moved) is shown pinned to
 * the nearest end rather than being silently rewritten. Rewriting the player's config because a
 * slider could not draw it is a data-loss bug wearing a UI costume.
 */
UCLASS(BlueprintType)
class BREACHPOINT_API UBRSettingsValue_Scalar : public UBRSettingsValue
{
	GENERATED_BODY()

public:
	/**
	 * Bind the leaf to its backing store. Captures the current value as BOTH the default and the
	 * first baseline -- at registry-build time those are the same thing by definition.
	 */
	void BindAccessor(TFunction<float()> InGetter, TFunction<void(float)> InSetter);

	/** Slider ends, the smallest movement the player can make, and how the number reads. */
	void ConfigureRange(float InDisplayMin, float InDisplayMax, float InStep, EBRScalarDisplayFormat InFormat);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	float GetValue() const;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	void SetValue(float InValue);

	/** 0..1 across the display range -- what a slider widget binds to. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	float GetNormalizedValue() const;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	void SetNormalizedValue(float InNormalized);

	/** One step up or down, clamped to the display range. The gamepad's left/right on a slider. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	void StepBy(int32 StepCount);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	float GetDisplayMin() const { return DisplayMin; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	float GetDisplayMax() const { return DisplayMax; }

	//~ Begin UBRSettingsDataObject interface
	virtual FText GetDisplayValue() const override;
	virtual bool IsDirty() const override;
	virtual void ResetToDefault() override;
	virtual void CaptureBaseline() override;
	virtual void RestoreBaseline() override;
	//~ End UBRSettingsDataObject interface

	//~ Begin UBRSettingsValue interface
	virtual bool IsBound() const override;
	//~ End UBRSettingsValue interface

private:
	/**
	 * NOT `UPROPERTY` -- `TFunction` is not a reflected type. Lifetime is safe because the
	 * registry that binds these captures a `TWeakObjectPtr` to the backing object and every
	 * lambda null-checks it; a leaf that outlives its store returns the default and no-ops the
	 * set, rather than dereferencing a stale raw pointer.
	 */
	TFunction<float()> Getter;
	TFunction<void(float)> Setter;

	UPROPERTY(Transient)
	float DisplayMin = 0.0f;

	UPROPERTY(Transient)
	float DisplayMax = 1.0f;

	UPROPERTY(Transient)
	float Step = 0.01f;

	UPROPERTY(Transient)
	EBRScalarDisplayFormat Format = EBRScalarDisplayFormat::Raw;

	UPROPERTY(Transient)
	float DefaultValue = 0.0f;

	UPROPERTY(Transient)
	float BaselineValue = 0.0f;
};

/** One choice on a discrete setting. The id is what persists; the text is what the player reads. */
USTRUCT(BlueprintType)
struct FBRSettingsOption
{
	GENERATED_BODY()

	FBRSettingsOption() = default;

	FBRSettingsOption(FName InId, const FText& InDisplayText)
		: OptionId(InId)
		, DisplayText(InDisplayText)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Settings")
	FName OptionId;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|Settings")
	FText DisplayText;
};

/**
 * A fixed set of named choices, cycled left/right.
 *
 * THE ACCESSOR IS ID-BASED, NEVER INDEX-BASED, and this is the one decision in the file worth
 * arguing about. An index accessor is simpler and is wrong: the instant anyone inserts a quality
 * level, reorders the anti-aliasing list, or a driver reports a different set of resolutions,
 * every saved index now points at a different option. The player's settings silently change
 * under them and nothing logs. Ids cost a lookup and cannot do that.
 *
 * An id that is no longer in the option list resolves to `INDEX_NONE`, which reads as the em
 * dash rather than snapping to option 0 -- "we do not recognise your saved value" is true and
 * recoverable; "your value is now Low" is a lie.
 */
UCLASS(BlueprintType)
class BREACHPOINT_API UBRSettingsValue_Discrete : public UBRSettingsValue
{
	GENERATED_BODY()

public:
	void BindAccessor(TFunction<FName()> InGetter, TFunction<void(FName)> InSetter);

	/** Replace the option list. Safe to call after binding -- resolutions are filled in late. */
	void SetOptions(TArray<FBRSettingsOption> InOptions);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	const TArray<FBRSettingsOption>& GetOptions() const { return Options; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	FName GetSelectedId() const;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	void SetSelectedId(FName InOptionId);

	/** `INDEX_NONE` when the stored id is not in the list. See the class doc for why that stands. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	int32 GetSelectedIndex() const;

	/**
	 * Move `Delta` places through the list, wrapping. From `INDEX_NONE` the first move lands on
	 * option 0, which is how a player recovers from an unrecognised saved value without leaving
	 * the screen.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	void CycleBy(int32 Delta);

	//~ Begin UBRSettingsDataObject interface
	virtual FText GetDisplayValue() const override;
	virtual bool IsDirty() const override;
	virtual void ResetToDefault() override;
	virtual void CaptureBaseline() override;
	virtual void RestoreBaseline() override;
	//~ End UBRSettingsDataObject interface

	//~ Begin UBRSettingsValue interface
	virtual bool IsBound() const override;
	//~ End UBRSettingsValue interface

private:
	/** See the note on `UBRSettingsValue_Scalar::Getter` -- same lifetime argument. */
	TFunction<FName()> Getter;
	TFunction<void(FName)> Setter;

	UPROPERTY(Transient)
	TArray<FBRSettingsOption> Options;

	UPROPERTY(Transient)
	FName DefaultId;

	UPROPERTY(Transient)
	FName BaselineId;
};
