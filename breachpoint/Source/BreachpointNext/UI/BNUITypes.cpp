#include "UI/BNUITypes.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "UI/Components/BRButton.h"

FBNUITags FBNUITags::Singleton;

namespace
{
	// COMPONENT-SPECS §2 idle row: top 1.0, bottom + both ticks 0.3. Hover moves the bottom.
	constexpr float EdgeBright = 1.0f;
	constexpr float EdgeDim = 0.3f;

	// bHovered is PASSED, not read back: the hover flag on the outer UUserWidget is not
	// guaranteed to be set yet at the moment CommonUI fires OnHovered, and a state machine that
	// samples its own trigger is the kind of thing that works on the desk and flickers in play.
	void ApplyEdges(UBRButton* Button, bool bHovered, BNButtonEdges::EChrome Chrome)
	{
		if (!Button)
		{
			return;
		}

		// Hover and selection are the SAME visual state on a menu row (the style's
		// SelectedBase and NormalHovered are the same white plate), so they drive the same
		// bottom line.
		const bool bActive = bHovered || Button->GetSelected();

		// GetWidgetFromName because these four are BP-tree widgets with no C++ binding: that
		// missing binding IS the gap. A null return is the correct no-op for any WBP that
		// does not carry the Figma line textures.
		// MEASURED off the founder's hover reference, sampled rather than eyeballed: the plate
		// and ALL FOUR border edges come back 108-109 against a 21 page, i.e. every one of them
		// is white at the same ~43% capture dimming, with the label at a true 0. So hover is not
		// "the bottom line brightens" - the WHOLE frame lights with the plate. Idle keeps the
		// COMPONENT-SPECS split: top 1.0, bottom and both ticks 0.3.
		const TCHAR* const Names[] = { TEXT("EdgeTop"), TEXT("EdgeBottom"), TEXT("EdgeLeft"), TEXT("EdgeRight") };
		// Hover is the same full bright box for both profiles. Idle is where they part: a boxed
		// button keeps its top line and dim ticks; a menu-list row shows the bottom rule ALONE.
		const bool bMenuRow = (Chrome == BNButtonEdges::EChrome::MenuRow);
		// Reference nav (2 Sep crop): unselected tabs are a UNIFORM dim box; the selected one
		// and a hovered one light all four edges.
		const float Top = bActive ? EdgeBright : (bMenuRow ? 0.0f : EdgeDim);
		const float Bottom = bActive ? EdgeBright : EdgeDim;
		const float Side = bActive ? EdgeBright : (bMenuRow ? 0.0f : EdgeDim);
		const float Opacities[] = { Top, Bottom, Side, Side };

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
		{
			if (UWidget* Edge = Button->GetWidgetFromName(FName(Names[Index])))
			{
				Edge->SetRenderOpacity(Opacities[Index]);
			}
		}
	}
}

void BNButtonEdges::Bind(UBRButton* Button, EChrome Chrome)
{
	if (!Button)
	{
		return;
	}

	// AddWeakLambda keyed on the button: the screen outlives its rows, but a row torn down and
	// rebuilt (the scoreboard grows its list) must not leave a dangling handler behind.
	Button->OnHovered().AddWeakLambda(Button, [Button, Chrome]() { ApplyEdges(Button, true, Chrome); });
	Button->OnUnhovered().AddWeakLambda(Button, [Button, Chrome]() { ApplyEdges(Button, false, Chrome); });
	// Selection (the nav's current tab) lights the box exactly like hover does.
	Button->OnIsSelectedChanged().AddLambda([Chrome, Weak = TWeakObjectPtr<UBRButton>(Button)](bool /*bSelected*/)
	{
		if (UBRButton* Owner = Weak.Get()) { ApplyEdges(Owner, Owner->IsHovered(), Chrome); }
	});

	// The idle pass. Callers bind AFTER SetIsSelected so a selected tab starts lit.
	ApplyEdges(Button, false, Chrome);
}

float BNTabBar::Layout(TArrayView<UBRButton* const> Tabs, float X, float Y, float Height, float Gap, float PadX)
{
	float Cursor = X;
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	for (UBRButton* Tab : Tabs)
	{
		if (!Tab)
		{
			continue;
		}
		UCanvasPanelSlot* TabSlot = Cast<UCanvasPanelSlot>(Tab->Slot);
		if (!TabSlot)
		{
			continue;
		}
		// The label is the component's own (`Label`, BindWidget); its font is whatever style the
		// button carries, so the measurement follows the style rather than a number typed here.
		float Width = TabSlot->GetSize().X;
		if (const UTextBlock* Label = Cast<UTextBlock>(Tab->GetWidgetFromName(TEXT("Label"))))
		{
			const FVector2D TextSize = Measure->Measure(Label->GetText(), Label->GetFont());
			Width = FMath::CeilToFloat(TextSize.X + 2.0f * PadX);
		}
		TabSlot->SetPosition(FVector2D(Cursor, Y));
		TabSlot->SetSize(FVector2D(Width, Height));
		Cursor += Width + Gap;
	}
	return Cursor - X - Gap;
}
