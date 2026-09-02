#include "UI/BNUITypes.h"
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
	void ApplyEdges(UBRButton* Button, bool bHovered)
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
		const TCHAR* const Names[] = { TEXT("EdgeTop"), TEXT("EdgeBottom"), TEXT("EdgeLeft"), TEXT("EdgeRight") };
		const float Opacities[] = { EdgeBright, bActive ? EdgeBright : EdgeDim, EdgeDim, EdgeDim };

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
		{
			if (UWidget* Edge = Button->GetWidgetFromName(FName(Names[Index])))
			{
				Edge->SetRenderOpacity(Opacities[Index]);
			}
		}
	}
}

void BNButtonEdges::Bind(UBRButton* Button)
{
	if (!Button)
	{
		return;
	}

	// AddWeakLambda keyed on the button: the screen outlives its rows, but a row torn down and
	// rebuilt (the scoreboard grows its list) must not leave a dangling handler behind.
	Button->OnHovered().AddWeakLambda(Button, [Button]() { ApplyEdges(Button, true); });
	Button->OnUnhovered().AddWeakLambda(Button, [Button]() { ApplyEdges(Button, false); });

	// The idle pass. Callers bind AFTER SetIsSelected so a selected tab starts lit.
	ApplyEdges(Button, false);
}
