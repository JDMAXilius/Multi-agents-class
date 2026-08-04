#include "UI/Settings/BRSettingsValueObjects.h"

#include "UI/Components/BRComponentTokens.h"

namespace BRSettingsValueInternal
{
	/** Float equality for values a human moved with a slider. Finer than any step we ship. */
	static constexpr float ValueTolerance = KINDA_SMALL_NUMBER;

	/** Guards the normalized conversions against a zero-width display range. */
	static constexpr float MinRangeWidth = KINDA_SMALL_NUMBER;
}

// ---------------------------------------------------------------------------------------------
// UBRSettingsValue_Scalar
// ---------------------------------------------------------------------------------------------

void UBRSettingsValue_Scalar::BindAccessor(TFunction<float()> InGetter, TFunction<void(float)> InSetter)
{
	Getter = MoveTemp(InGetter);
	Setter = MoveTemp(InSetter);

	// At bind time the shipped value and the session baseline are the same reading. They diverge
	// the moment the player touches anything, which is exactly what makes the two fields earn
	// their keep -- see the base class doc.
	DefaultValue = GetValue();
	BaselineValue = DefaultValue;
}

void UBRSettingsValue_Scalar::ConfigureRange(float InDisplayMin, float InDisplayMax, float InStep, EBRScalarDisplayFormat InFormat)
{
	DisplayMin = InDisplayMin;
	DisplayMax = InDisplayMax;
	Step = InStep;
	Format = InFormat;
}

bool UBRSettingsValue_Scalar::IsBound() const
{
	return static_cast<bool>(Getter) && static_cast<bool>(Setter);
}

float UBRSettingsValue_Scalar::GetValue() const
{
	return Getter ? Getter() : 0.0f;
}

void UBRSettingsValue_Scalar::SetValue(float InValue)
{
	if (!IsBound() || !IsEditable())
	{
		return;
	}

	const float Clamped = FMath::Clamp(InValue, DisplayMin, DisplayMax);
	if (FMath::IsNearlyEqual(Clamped, GetValue(), BRSettingsValueInternal::ValueTolerance))
	{
		return;
	}

	Setter(Clamped);
	NotifyChanged();
}

float UBRSettingsValue_Scalar::GetNormalizedValue() const
{
	const float Width = DisplayMax - DisplayMin;
	if (Width <= BRSettingsValueInternal::MinRangeWidth)
	{
		return 0.0f;
	}

	// Clamped for DISPLAY only. A stored value outside the range pins the slider to an end and
	// is left alone in the config -- see the class doc.
	return FMath::Clamp((GetValue() - DisplayMin) / Width, 0.0f, 1.0f);
}

void UBRSettingsValue_Scalar::SetNormalizedValue(float InNormalized)
{
	const float Width = DisplayMax - DisplayMin;
	if (Width <= BRSettingsValueInternal::MinRangeWidth)
	{
		return;
	}

	SetValue(DisplayMin + FMath::Clamp(InNormalized, 0.0f, 1.0f) * Width);
}

void UBRSettingsValue_Scalar::StepBy(int32 StepCount)
{
	if (StepCount == 0 || Step <= 0.0f)
	{
		return;
	}

	SetValue(GetValue() + Step * static_cast<float>(StepCount));
}

FText UBRSettingsValue_Scalar::GetDisplayValue() const
{
	if (!IsBound())
	{
		return BRUI::UnknownValueText();
	}

	const float Value = GetValue();

	switch (Format)
	{
	case EBRScalarDisplayFormat::Percent:
		return FText::AsNumber(FMath::RoundToInt(Value * 100.0f));

	case EBRScalarDisplayFormat::Integer:
		return FText::AsNumber(FMath::RoundToInt(Value));

	case EBRScalarDisplayFormat::Multiplier:
	case EBRScalarDisplayFormat::Raw:
	default:
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 2;
		Options.MaximumFractionalDigits = 2;
		return FText::AsNumber(Value, &Options);
	}
	}
}

bool UBRSettingsValue_Scalar::IsDirty() const
{
	return IsBound() && !FMath::IsNearlyEqual(GetValue(), BaselineValue, BRSettingsValueInternal::ValueTolerance);
}

void UBRSettingsValue_Scalar::ResetToDefault()
{
	SetValue(DefaultValue);
}

void UBRSettingsValue_Scalar::CaptureBaseline()
{
	BaselineValue = GetValue();
}

void UBRSettingsValue_Scalar::RestoreBaseline()
{
	SetValue(BaselineValue);
}

// ---------------------------------------------------------------------------------------------
// UBRSettingsValue_Discrete
// ---------------------------------------------------------------------------------------------

void UBRSettingsValue_Discrete::BindAccessor(TFunction<FName()> InGetter, TFunction<void(FName)> InSetter)
{
	Getter = MoveTemp(InGetter);
	Setter = MoveTemp(InSetter);

	DefaultId = GetSelectedId();
	BaselineId = DefaultId;
}

void UBRSettingsValue_Discrete::SetOptions(TArray<FBRSettingsOption> InOptions)
{
	Options = MoveTemp(InOptions);

	// Deliberately does NOT re-point the selection. If the new list no longer contains the stored
	// id, `GetSelectedIndex` reports INDEX_NONE and the row shows the em dash. Snapping to option
	// 0 here would rewrite the player's setting as a side effect of a list refresh.
	NotifyChanged();
}

bool UBRSettingsValue_Discrete::IsBound() const
{
	return static_cast<bool>(Getter) && static_cast<bool>(Setter);
}

FName UBRSettingsValue_Discrete::GetSelectedId() const
{
	return Getter ? Getter() : NAME_None;
}

void UBRSettingsValue_Discrete::SetSelectedId(FName InOptionId)
{
	if (!IsBound() || !IsEditable())
	{
		return;
	}

	// Refuse ids that are not on the list. A setter that accepts anything is how a settings
	// screen ends up storing a quality level the renderer has never heard of.
	const bool bKnown = Options.ContainsByPredicate(
		[InOptionId](const FBRSettingsOption& Option) { return Option.OptionId == InOptionId; });

	if (!bKnown || InOptionId == GetSelectedId())
	{
		return;
	}

	Setter(InOptionId);
	NotifyChanged();
}

int32 UBRSettingsValue_Discrete::GetSelectedIndex() const
{
	const FName Current = GetSelectedId();
	return Options.IndexOfByPredicate(
		[Current](const FBRSettingsOption& Option) { return Option.OptionId == Current; });
}

void UBRSettingsValue_Discrete::CycleBy(int32 Delta)
{
	if (Delta == 0 || Options.Num() == 0)
	{
		return;
	}

	const int32 Current = GetSelectedIndex();

	// From an unrecognised value the first move lands on 0 rather than on `Delta`, so a player
	// whose config went stale can always get back onto the list with one press in either
	// direction.
	const int32 Next = Current == INDEX_NONE
		? 0
		: ((Current + Delta) % Options.Num() + Options.Num()) % Options.Num();

	SetSelectedId(Options[Next].OptionId);
}

FText UBRSettingsValue_Discrete::GetDisplayValue() const
{
	const int32 Index = GetSelectedIndex();
	return Options.IsValidIndex(Index) ? Options[Index].DisplayText : BRUI::UnknownValueText();
}

bool UBRSettingsValue_Discrete::IsDirty() const
{
	return IsBound() && GetSelectedId() != BaselineId;
}

void UBRSettingsValue_Discrete::ResetToDefault()
{
	SetSelectedId(DefaultId);
}

void UBRSettingsValue_Discrete::CaptureBaseline()
{
	BaselineId = GetSelectedId();
}

void UBRSettingsValue_Discrete::RestoreBaseline()
{
	SetSelectedId(BaselineId);
}
