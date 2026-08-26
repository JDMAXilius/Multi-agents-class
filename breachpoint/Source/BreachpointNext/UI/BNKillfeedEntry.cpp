#include "UI/BNKillfeedEntry.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/BNUITypes.h"

void UBNKillfeedEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBNKillfeedEntry::SetEntry(const FBNKillfeedViewEntry& Entry)
{
	// The parts layout needs BOTH widgets and BOTH names. A kill with no killer ("X died") has no
	// parts to lay out, so it takes the composed line even in a WBP that binds all three.
	const bool bParts = KillerText && VictimText && !Entry.KillerText.IsEmpty() && !Entry.VictimText.IsEmpty();

	// TEAMS (BN16): relation → the PART's tint, the one mapping this row is allowed. bInvolvesSelf
	// KEEPS its authority — your line is the white row it is today, WHOLE, relations never dilute
	// it — and tinting exists only in the parts layout (the composed line is one text and takes
	// one row tint). A non-self parts line with any relation known moves the dimming from the ROW
	// to the LEAVES: the row goes neutral, each name takes its relation's hue, a still-unknown
	// (None) name takes today's history ink, and the glyph keeps that same ink so nothing
	// brightens. In FFA both relations are None, this flag is false, and every write below lands
	// the value today's palette already produced — the leaves' White IS their WBP-default (this
	// class's law: the WBP sets no leaf color), written out only because a pooled row must not
	// keep the previous claim's hue.
	const bool bRelationTinted = bParts && !Entry.bInvolvesSelf
		&& (Entry.KillerRelation != EBNUITeamRelation::None || Entry.VictimRelation != EBNUITeamRelation::None);

	auto PartTint = [](EBNUITeamRelation Relation) -> FLinearColor
	{
		switch (Relation)
		{
		case EBNUITeamRelation::Self:  return BNUIColors::Self;
		case EBNUITeamRelation::Ally:  return BNUIColors::Ally;
		case EBNUITeamRelation::Enemy: return BNUIColors::Threat;
		default:                       return BNUIColors::InkDim; // None: honestly unknown, today's ink
		}
	};

	if (KillerText)
	{
		KillerText->SetText(Entry.KillerText);
		KillerText->SetColorAndOpacity(FSlateColor(bRelationTinted ? PartTint(Entry.KillerRelation) : FLinearColor::White));
		KillerText->SetVisibility(bParts ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (VictimText)
	{
		VictimText->SetText(Entry.VictimText);
		VictimText->SetColorAndOpacity(FSlateColor(bRelationTinted ? PartTint(Entry.VictimRelation) : FLinearColor::White));
		VictimText->SetVisibility(bParts ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (LineText)
	{
		LineText->SetText(Entry.Line);
		LineText->SetVisibility(bParts ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (WeaponIcon)
	{
		const bool bHasIcon = !Entry.WeaponIcon.IsNull();
		if (bHasIcon && Entry.WeaponIcon != AppliedIcon)
		{
			WeaponIcon->SetBrushFromSoftTexture(Entry.WeaponIcon, /*bMatchSize=*/false);
			AppliedIcon = Entry.WeaponIcon;
		}
		// The glyph's effective tint must not change with the mode: today it is White-leaf ×
		// InkDim-row; in a relation-tinted line the row is neutral, so the ink moves to the leaf.
		WeaponIcon->SetColorAndOpacity(bRelationTinted ? BNUIColors::InkDim : FLinearColor::White);
		// Collapsed, not Hidden: a feed line with no glyph must close the gap, not leave a hole
		// where the icon would be — the rows are a vertical list of variable-length lines.
		WeaponIcon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// The row, not the leaf — except when the leaves carry the relation tints, where the row goes
	// neutral (multiplying Ally through InkDim is neither hue). Red is still never spent on a
	// line AS a line: the one red above is the enemy's NAME, and the enemy is the one thing
	// Threat was always allowed to name.
	SetColorAndOpacity(bRelationTinted ? FLinearColor::White : (Entry.bInvolvesSelf ? BNUIColors::Self : BNUIColors::InkDim));
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBNKillfeedEntry::ClearEntry()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
