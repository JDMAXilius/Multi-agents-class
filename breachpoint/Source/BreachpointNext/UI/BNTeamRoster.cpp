#include "UI/BNTeamRoster.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/BNUITypes.h"
#include "UI/Components/BRScrollBar.h"

#define LOCTEXT_NAMESPACE "BNTeamRoster"

void UBNTeamRoster::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RootSizeBox)
	{
		// BOUNDED, deliberately, and this is the correction to the previous pass: a list that
		// sizes to its content cannot scroll, it just grows past the panel and over the footer -
		// which is exactly what a 16-player lobby did. The column is a fixed 349 x 599 window
		// and `TeamScroll` moves the content inside it. "Dynamic" belongs to the CONTENT here,
		// not the frame.
		RootSizeBox->SetWidthOverride(PanelWidth);
		RootSizeBox->SetHeightOverride(PanelHeightOverride);
	}
	// The WBP is authored at 599; a shorter instance shrinks the fixed-height canvas children by
	// the same amount so each keeps its bottom margin.
	const float Delta = PanelHeightOverride - PanelHeight;
	if (!FMath::IsNearlyZero(Delta))
	{
		TArray<UWidget*> Tall = { GetWidgetFromName(TEXT("Chassis")), GetWidgetFromName(TEXT("TeamScroll")), ScrollBar.Get() };
		for (UWidget* Child : Tall)
		{
			if (UCanvasPanelSlot* ChildSlot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr)
			{
				const FVector2D Size = ChildSlot->GetSize();
				ChildSlot->SetSize(FVector2D(Size.X, FMath::Max(0.0f, Size.Y + Delta)));
			}
		}
	}

	if (ScrollBar)
	{
		// The reference's bar is 13 wide, which is this class's Wide weight exactly.
		ScrollBar->SetScrollBarWeight(EBRScrollBarWeight::Wide);
	}

	// Resolve once. A null class is an authoring error, not a runtime state, and it must say
	// so out loud — the silent version of this is four BR classes drawing nothing.
	ResolvedRowClass = RowWidgetClass.IsNull() ? nullptr : RowWidgetClass.LoadSynchronous();
	if (!ResolvedRowClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UBNTeamRoster '%s' has no RowWidgetClass; no player rows can be shown."),
			*GetName());
	}
}

void UBNTeamRoster::SetHeaderText(const FText& InLabel, int32 InCapacity)
{
	if (Header)
	{
		Header->SetHeader(InLabel, /*MemberCount*/ -1, InCapacity);
	}
}

void UBNTeamRoster::SetHeaderStatus(const FText& InStatus)
{
	// `UBRRosterHeader` exposes Label + Count only; the reference's "Invite Only" is the Count
	// slot's text, reached by name like every other shared-component gap in this packet.
	if (Header)
	{
		if (UCommonTextBlock* Count = Cast<UCommonTextBlock>(Header->GetWidgetFromName(TEXT("Count"))))
		{
			Count->SetText(InStatus);
			// The shared header lays Count out beside Label; right-justified it reads at the
			// header's right end ("IN MENUS ........ Invite Only") instead of running into the label.
			Count->SetJustification(ETextJustify::Right);
			Count->SetVisibility(InStatus.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}
	if (HeaderIcon)
	{
		HeaderIcon->SetVisibility(InStatus.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UBNTeamRoster::SetTeams(const TArray<FBNRosterTeam>& InTeams)
{
	Teams = InTeams;

	if (!TeamBox)
	{
		return;
	}

	TeamBox->ClearChildren();
	RowCounter = 0;

	const int32 LastIndex = Teams.Num() - 1;
	for (int32 Index = 0; Index <= LastIndex; ++Index)
	{
		BuildTeam(Teams[Index], Index == LastIndex);
	}
}

void UBNTeamRoster::BuildTeam(const FBNRosterTeam& InTeam, bool bIsLast)
{
	// --- the team label: a coloured plate with the team name on it ------------------------
	// A team with no name is a FLAT list (the front end's IN MENUS): rows only, no label plate.
	if (!InTeam.Name.IsEmpty())
	{
	UOverlay* Label = NewObject<UOverlay>(this);

	UImage* Plate = NewObject<UImage>(this);
	Plate->SetColorAndOpacity(InTeam.Color);
	Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* PlateSlot = Cast<UOverlaySlot>(Label->AddChild(Plate)))
	{
		PlateSlot->SetHorizontalAlignment(HAlign_Fill);
		PlateSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UCommonTextBlock* Name = NewObject<UCommonTextBlock>(this);
	Name->SetText(InTeam.Name);
	// The tone decision is DELEGATED, not re-derived: `UBRRosterRow::ComputeTextTone` already
	// owns "does text go black or white against this fill", and a second copy of that rule here
	// is how the label and the rows under it end up disagreeing on a new team colour.
	const bool bLightPlate =
		UBRRosterRow::ComputeTextTone(InTeam.Color) == EBRRosterTextTone::OnLight;
	Name->SetColorAndOpacity(FSlateColor(
		bLightPlate ? FLinearColor::Black : BNUIColors::Self));
	Name->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* NameSlot = Cast<UOverlaySlot>(Label->AddChild(Name)))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Left);
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));
	}

	USizeBox* LabelBox = NewObject<USizeBox>(this);
	LabelBox->SetHeightOverride(TeamLabelHeight);
	LabelBox->AddChild(Label);
	if (UVerticalBoxSlot* LabelSlot = TeamBox->AddChildToVerticalBox(LabelBox))
	{
		// Measured: the first member row starts at label + 37 on a 32-tall label, so 5.
		LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TeamLabelGap));
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	// --- the member rows: the SAME WBP_RosterRow the front end uses ----------------------
	}

	if (!ResolvedRowClass)
	{
		return;
	}

	const int32 LastMember = InTeam.Members.Num() - 1;
	for (int32 Index = 0; Index <= LastMember; ++Index)
	{
		UBRRosterRow* Row = CreateWidget<UBRRosterRow>(this, ResolvedRowClass);
		if (!Row)
		{
			break;
		}

		// Per-row art, cycled on a counter that runs across ALL teams so the pattern does not
		// restart at every label and give two adjacent rows the same plate.
		FBRRosterMemberView Member = InTeam.Members[Index];
		if (Emblems.Num() > 0)
		{
			Member.Emblem = Emblems[RowCounter % Emblems.Num()];
		}

		// TeamFillColor is a TONE HINT here, not a tint: the real fill is the nameplate texture
		// applied below, and a photograph has no single colour for ComputeTextTone to read. Feed
		// it white for a light plate and near-black for a dark one and the row's own luminance
		// rule lands on the right answer without a second copy of it living here.
		const FBNRosterPlate* RowPlate = Nameplates.Num() > 0
			? &Nameplates[RowCounter % Nameplates.Num()] : nullptr;
		Member.TeamFillColor = (RowPlate && RowPlate->bLightPlate)
			? FLinearColor::White : FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
		if (!RankInsignia.IsNull())
		{
			Member.RankInsignia = RankInsignia;
		}
		Row->SetMember(Member);

		// The nameplate goes on by hand: `ApplyMember` only TINTS TeamFill, and the view struct
		// has no texture field for it (see the header note). Reached by name because that
		// missing field IS the gap. Tinted white so the art is not multiplied down by the hint.
		if (RowPlate && !RowPlate->Texture.IsNull())
		{
			if (UTexture2D* PlateTex = RowPlate->Texture.LoadSynchronous())
			{
				if (UImage* Fill = Cast<UImage>(Row->GetWidgetFromName(TEXT("TeamFill"))))
				{
					Fill->SetBrushFromTexture(PlateTex, /*bMatchSize*/ false);
					Fill->SetColorAndOpacity(FLinearColor::White);
					Fill->SetVisibility(ESlateVisibility::HitTestInvisible);
				}
			}
		}

		// The nameplate has to go on by hand: `ApplyMember` only TINTS TeamFill, and the view
		// struct has no texture field for it (see the header note). Reached by name because
		// that missing field IS the gap — a null entry simply leaves the tint alone.
		++RowCounter;

		USizeBox* RowBox = NewObject<USizeBox>(this);
		RowBox->SetHeightOverride(RowHeight);
		RowBox->AddChild(Row);

		if (UVerticalBoxSlot* RowSlot = TeamBox->AddChildToVerticalBox(RowBox))
		{
			// Pitch 35 on a 30-tall row is a 5 gap between rows; after the LAST row of a team
			// the next thing is another team, and that gap is also 5 (blocks are 113 apart on
			// a 108-tall block). Same number, so one expression covers both — except after the
			// final team, where nothing follows and the padding would just pad the scroll.
			const bool bLastOfAll = bIsLast && Index == LastMember;
			RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, bLastOfAll ? 0.0f : TeamGap));
			RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}
}

#undef LOCTEXT_NAMESPACE
