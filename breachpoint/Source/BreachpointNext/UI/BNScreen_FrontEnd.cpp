#include "UI/BNScreen_FrontEnd.h"
#include "BreachpointNext.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "UI/BNProfileBar.h"
#include "UI/Styles/BRUITokens.h"
#include "UI/Components/BRButton.h"
#include "UI/Components/BRRosterPanel.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/BNUIManager.h"
#include "UI/BNUITypes.h"

#define LOCTEXT_NAMESPACE "BreachpointNextUI"

UBNScreen_FrontEnd::UBNScreen_FrontEnd()
{
	// A menu at boot: cursor, clicks, no game beneath to keep receiving input.
	InputMode = EBNWidgetInputMode::Menu;
	bIsBackHandler = false;   // home has no "back" — backing out of the main menu is Quit, a click
	bIsModal = true;
}

void UBNScreen_FrontEnd::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// FOR clicking, like the pause screen — the two proven mouse screens share this shape.
	SetVisibility(ESlateVisibility::Visible);

	// CommonUI's C++ hook is the NATIVE event OnClicked(), not the dynamic multicast — the
	// dynamic one is private with AllowPrivateAccess, reachable from Blueprint only.
	if (PlayButton)
	{
		PlayButton->OnClicked().AddUObject(this, &UBNScreen_FrontEnd::HandlePlayClicked);
		PlayButton->SetLabelText(LOCTEXT("MenuPlay", "PLAY"));
	}
	if (QuitButton)
	{
		QuitButton->OnClicked().AddUObject(this, &UBNScreen_FrontEnd::HandleQuitClicked);
		QuitButton->SetLabelText(LOCTEXT("MenuQuit", "QUIT"));
	}
	// The design's four-slot rail. The two that lead nowhere are DIMMED, not disabled — and the
	// difference is the whole reason hover looked dead. `SetIsEnabled(false)` on a
	// `UCommonButtonBase` stops it receiving hover AT ALL, so five of this screen's eight
	// buttons never fired `OnHovered` and never inverted; only PLAY and QUIT ever could. The
	// reference does not grey these out either — it renders them at the measured
	// `OpacityNavTabInactive` 0.6 so the current entry reads as current. Dimming gets that look
	// AND keeps the hover, which is what was actually wanted.
	if (CustomsButton)
	{
		CustomsButton->SetLabelText(LOCTEXT("MenuCustoms", "CUSTOM GAMES"));
	}
	if (AcademyButton)
	{
		AcademyButton->SetLabelText(LOCTEXT("MenuAcademy", "ACADEMY"));
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(LOCTEXT("FrontEndHint", "Set up a match against bots."));
		DescriptionText->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim));
	}
	if (PromptText)
	{
		// In C++, not typed into the .uasset: the old one was a literal inside the binary, which
		// no reviewer can grep and no localisation pass can reach.
		PromptText->SetText(LOCTEXT("FrontEndPrompts", "ENTER \u2014 SELECT      ESC \u2014 QUIT"));
		PromptText->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim));
	}
	// THE NAV BAR. PLAY is where we already are, so it is selected. The other three are dimmed
	// rather than disabled, for the same reason as the rail above: disabled kills hover.
	if (NavPlayTab)
	{
		NavPlayTab->SetLabelText(LOCTEXT("NavPlay", "PLAY"));
		NavPlayTab->SetIsSelected(true);
	}
	if (NavCreateTab)
	{
		NavCreateTab->SetLabelText(LOCTEXT("NavCreate", "CREATE"));
	}
	if (NavCommunityTab)
	{
		NavCommunityTab->SetLabelText(LOCTEXT("NavCommunity", "COMMUNITY"));
	}
	if (NavShopTab)
	{
		NavShopTab->SetLabelText(LOCTEXT("NavShop", "SHOP"));
	}
	// The Idle→Hover edge transition the shared component drops on the floor — see
	// BNButtonEdges. LAST in the button block, and deliberately: it reads GetSelected(), so
	// binding before NavPlayTab->SetIsSelected(true) would start the selected tab dim.
	// Hover changes the BACKGROUND PLATE ONLY. The plate (`TextFrameFill`) sits at overlay
	// index 1 — under the border, under the four edge lines, under the label — so the corners,
	// the rules and the text all keep drawing on top of it, unchanged. Nothing here touches the
	// button's own opacity: dimming the whole widget would take the lines and the label with it,
	// which is not what the reference does.
	// Two chromes, both measured. The rail rows live inside a Menu List and rest on a bottom
	// rule alone; the nav tabs are standalone boxes and keep their top line and ticks.
	for (UBRButton* Button : { PlayButton.Get(), CustomsButton.Get(), AcademyButton.Get(), QuitButton.Get() })
	{
		BNButtonEdges::Bind(Button, BNButtonEdges::EChrome::MenuRow);
	}
	for (UBRButton* Button : { NavPlayTab.Get(), NavCreateTab.Get(), NavCommunityTab.Get(), NavShopTab.Get() })
	{
		BNButtonEdges::Bind(Button, BNButtonEdges::EChrome::Boxed);
	}
	// THE ROSTER. One measured component where 23 canvas widgets used to be. The panel owns its
	// own geometry - we hand it data and nothing else, which is the whole reason to use it.
	if (RosterPanel)
	{
		TArray<FBRRosterMemberView> Members;
		Members.Reserve(RosterNames.Num());
		for (const FString& Name : RosterNames)
		{
			FBRRosterMemberView& Member = Members.AddDefaulted_GetRef();
			Member.Gamertag = FText::FromString(Name);
			Member.Emblem = RosterEmblem;
			Member.RankInsignia = RosterRankInsignia;
			// Idle, not Unknown. Unknown is the RIGHT default in a match — the class's own note
			// says a row that renders "not speaking" before voice state arrives is telling the
			// player something false — but this menu has no voice system at all and the whole
			// roster is ini placeholder beside it. Idle here means "this is a mock", which is
			// what every other value in this list already means.
			Member.MicState = EBRRosterMicState::Idle;
			// Entry 0 is us: the row that gets the leader crown and the current-player mark.
			Member.bIsLocalPlayer = Member.bIsPartyLeader = (Members.Num() == 1);
			// Transparent, deliberately. TeamFillColor paints the nameplate banner, and in the
			// reference that banner is 343-owned art - the IP line (01-MENU-MEASURED sec 6)
			// says every image in this game is rendered from this game, so the plate stays
			// empty until we author our own rather than borrowing theirs.
			Member.TeamFillColor = FLinearColor::Transparent;
		}
		RosterPanel->SetMembers(Members);
		// Capacity -1 = print no count. We have no party, so "IN MENUS 1/6" would be a lie
		// dressed as a feature.
		RosterPanel->SetHeaderText(LOCTEXT("RosterInMenus", "IN MENUS"), -1);
	}
	// THE PROGRESSION PANEL, fed from ini rather than invented. Unconfigured is the honest
	// default here — this project has no ranks, so the line collapses and the bar reads 0
	// instead of printing a career the player has not got.
	if (RankText)
	{
		RankText->SetText(FText::FromString(CareerRankLine));
		RankText->SetVisibility(CareerRankLine.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		RankText->SetColorAndOpacity(FSlateColor(BNUIColors::Self));
	}
	if (RankProgress)
	{
		RankProgress->SetPercent(FMath::Clamp(CareerRankFraction, 0.0f, 1.0f));
	}
	if (NewsImage)
	{
		// One plate, once, on a screen that is not yet showing — synchronous is honest here
		// and an async request would only pop the card in a frame or two later.
		if (UTexture2D* Plate = NewsImageTexture.LoadSynchronous())
		{
			NewsImage->SetBrushFromTexture(Plate, /*bMatchSize*/ false);
			NewsImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			// Unconfigured is not broken: the card falls back to its tint plus the headline.
			NewsImage->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogBN, Verbose, TEXT("BNScreen_FrontEnd: no NewsImageTexture configured; "
				"the news card shows its panel ground only."));
		}
	}
}

UWidget* UBNScreen_FrontEnd::NativeGetDesiredFocusTarget() const
{
	return PlayButton;
}

void UBNScreen_FrontEnd::HandlePlayClicked()
{
	// Same stack, so CommonUI's own pop is the whole back-navigation implementation:
	// deactivating the setup screen lands focus right back here with no bespoke code.
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UBNUIManager* Manager = UBNUIManager::Get(this);
	if (!LocalPlayer || !Manager)
	{
		UE_LOG(LogBN, Warning, TEXT("BNScreen_FrontEnd: PLAY clicked with no local player or manager — nothing pushed."));
		return;
	}
	if (!Manager->PushWidgetToLayer(LocalPlayer, FBNUITags::Get().Layer_Menu, Manager->GetPlaySetupScreenClass()))
	{
		// The manager already logged WHY (unset ini or unbuilt asset) — the designed miss.
		UE_LOG(LogBN, Warning, TEXT("BNScreen_FrontEnd: play-setup screen did not push."));
	}
}

void UBNScreen_FrontEnd::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, /*bIgnorePlatformRestrictions*/ false);
}

#undef LOCTEXT_NAMESPACE
