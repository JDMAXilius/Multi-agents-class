#include "UI/BNTeamRoster.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
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
		// Width pinned to the measured column, height left to the team blocks. A 4v4 lobby and
		// an 8v8 lobby are different heights and both are correct; 599 is the design maximum,
		// not a floor to hold open with empty space.
		RootSizeBox->SetWidthOverride(PanelWidth);
		RootSizeBox->SetMaxDesiredHeight(PanelHeight);
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

void UBNTeamRoster::SetTeams(const TArray<FBNRosterTeam>& InTeams)
{
	Teams = InTeams;

	if (!TeamBox)
	{
		return;
	}

	TeamBox->ClearChildren();

	const int32 LastIndex = Teams.Num() - 1;
	for (int32 Index = 0; Index <= LastIndex; ++Index)
	{
		BuildTeam(Teams[Index], Index == LastIndex);
	}
}

void UBNTeamRoster::BuildTeam(const FBNRosterTeam& InTeam, bool bIsLast)
{
	// --- the team label: a coloured plate with the team name on it ------------------------
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

		Row->SetMember(InTeam.Members[Index]);

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
