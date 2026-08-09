#include "UI/Components/OSBaseButton.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

UOSBaseButton::UOSBaseButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UOSBaseButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (Txt_ButtonName)
	{
		Txt_ButtonName->SetText(ButtonName);
	}
}

void UOSBaseButton::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button)
	{
		Button->OnClicked.AddDynamic(this, &UOSBaseButton::OnButtonClicked);
		Button->OnPressed.AddDynamic(this, &UOSBaseButton::OnButtonPressed);
		Button->OnReleased.AddDynamic(this, &UOSBaseButton::OnButtonReleased);
		Button->OnHovered.AddDynamic(this, &UOSBaseButton::OnButtonHovered);
		Button->OnUnhovered.AddDynamic(this, &UOSBaseButton::OnButtonUnhovered);

		CachedStyle = Button->GetStyle();

		HoveredCachedStyle = CachedStyle;
		HoveredCachedStyle.Normal = CachedStyle.Hovered;
		HoveredCachedStyle.NormalForeground = CachedStyle.HoveredForeground;

		PressedCachedStyle = CachedStyle;
		PressedCachedStyle.Normal = CachedStyle.Pressed;
		PressedCachedStyle.NormalForeground = CachedStyle.PressedForeground;
	}
}

void UOSBaseButton::NativeDestruct()
{
	if (Button)
	{
		Button->OnClicked.RemoveAll(this);
		Button->OnPressed.RemoveAll(this);
		Button->OnReleased.RemoveAll(this);
		Button->OnHovered.RemoveAll(this);
		Button->OnUnhovered.RemoveAll(this);
	}
	Super::NativeDestruct();
}

// --- Focus Path (controller / keyboard navigation) --- //

void UOSBaseButton::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	Hovered_Internal();
}

void UOSBaseButton::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);

	if (Button && Button->IsHovered())
	{
		return;
	}

	Unhovered_Internal();
}

// --- UButton Callbacks --- //

void UOSBaseButton::OnButtonClicked()
{
	Clicked_Internal();
}

void UOSBaseButton::OnButtonPressed()
{
	Pressed_Internal();
}

void UOSBaseButton::OnButtonReleased()
{
	Released_Internal();
}

void UOSBaseButton::OnButtonHovered()
{
	Hovered_Internal();
}

void UOSBaseButton::OnButtonUnhovered()
{
	if (Button && Button->HasUserFocus(GetOwningPlayer()))
	{
		return;
	}

	Unhovered_Internal();
}

// --- Centralized Internal Logic --- //

void UOSBaseButton::Clicked_Internal()
{
	Clicked();
	OnClicked.Broadcast();
}

void UOSBaseButton::Pressed_Internal()
{
	ApplyPressedStyle();
	Pressed();
	OnPressed.Broadcast();
}

void UOSBaseButton::Released_Internal()
{
	if (bIsActive || bIsHoverVisualActive)
	{
		ApplyHoveredStyle();
	}
	else
	{
		ApplyStyle();
	}
	Released();
	OnReleased.Broadcast();
}

void UOSBaseButton::Hovered_Internal()
{
	if (bIsHoverVisualActive)
	{
		return;
	}
	bIsHoverVisualActive = true;

	if (bIsActive)
	{
		if (Button)
		{
			FButtonStyle TempStyle = HoveredCachedStyle;
			TempStyle.SetNormalPadding(HoveredPadding);
			TempStyle.SetPressedPadding(HoveredPadding);
			Button->SetStyle(TempStyle);
		}
	}
	else
	{
		ApplyHoveredStyle();
	}

	Hovered();
	OnHovered.Broadcast();
}

void UOSBaseButton::Unhovered_Internal()
{
	if (!bIsHoverVisualActive)
	{
		return;
	}
	bIsHoverVisualActive = false;

	if (bIsActive)
	{
		if (Button)
		{
			FButtonStyle TempStyle = HoveredCachedStyle;
			TempStyle.SetNormalPadding(NormalPadding);
			TempStyle.SetPressedPadding(NormalPadding);
			Button->SetStyle(TempStyle);
		}
	}
	else
	{
		ApplyStyle();
	}

	Unhovered();
	OnUnhovered.Broadcast();
}

// --- Toggle --- //

void UOSBaseButton::SetActive(bool bNewActive)
{
	bIsActive = bNewActive;

	if (bIsActive)
	{
		if (Button)
		{
			FButtonStyle TempStyle = HoveredCachedStyle;
			TempStyle.SetNormalPadding(NormalPadding);
			TempStyle.SetPressedPadding(NormalPadding);
			Button->SetStyle(TempStyle);
		}
	}
	else
	{
		ApplyStyle();
	}

	OnActiveStateChanged(bIsActive);
}

// --- Style Helpers --- //

void UOSBaseButton::ApplyStyle()
{
	if (Button)
	{
		FButtonStyle TempStyle = CachedStyle;
		TempStyle.SetNormalPadding(NormalPadding);
		TempStyle.SetPressedPadding(NormalPadding);
		Button->SetStyle(TempStyle);
	}
}

void UOSBaseButton::ApplyHoveredStyle()
{
	if (Button)
	{
		FButtonStyle TempStyle = HoveredCachedStyle;
		TempStyle.SetNormalPadding(HoveredPadding);
		TempStyle.SetPressedPadding(HoveredPadding);
		Button->SetStyle(TempStyle);
	}
}

void UOSBaseButton::ApplyPressedStyle()
{
	if (Button)
	{
		FButtonStyle TempStyle = PressedCachedStyle;
		TempStyle.SetNormalPadding(NormalPadding);
		TempStyle.SetPressedPadding(NormalPadding);
		Button->SetStyle(TempStyle);
	}
}
