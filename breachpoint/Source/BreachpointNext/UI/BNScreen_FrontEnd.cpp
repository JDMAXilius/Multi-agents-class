#include "UI/BNScreen_FrontEnd.h"
#include "BreachpointNext.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "UI/Components/BRButton.h"
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
	// The design's four-slot rail, with the two that do nothing shipped disabled and named —
	// what the reference game does with locked entries, and what this class's comment promises.
	if (CustomsButton)
	{
		CustomsButton->SetLabelText(LOCTEXT("MenuCustoms", "CUSTOM GAMES"));
		CustomsButton->SetIsEnabled(false);
	}
	if (AcademyButton)
	{
		AcademyButton->SetLabelText(LOCTEXT("MenuAcademy", "ACADEMY"));
		AcademyButton->SetIsEnabled(false);
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(LOCTEXT("FrontEndHint", "Set up a match against bots."));
		DescriptionText->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim));
	}
	// THE NAV BAR. PLAY is where we already are, so it is selected, not clickable-to-nowhere.
	// The other three are named and disabled: the band's 666 width is design, and hiding the
	// tabs would quietly change the header's balance.
	if (NavPlayTab)
	{
		NavPlayTab->SetLabelText(LOCTEXT("NavPlay", "PLAY"));
		NavPlayTab->SetIsSelected(true);
	}
	if (NavCreateTab)
	{
		NavCreateTab->SetLabelText(LOCTEXT("NavCreate", "CREATE"));
		NavCreateTab->SetIsEnabled(false);
	}
	if (NavCommunityTab)
	{
		NavCommunityTab->SetLabelText(LOCTEXT("NavCommunity", "COMMUNITY"));
		NavCommunityTab->SetIsEnabled(false);
	}
	if (NavShopTab)
	{
		NavShopTab->SetLabelText(LOCTEXT("NavShop", "SHOP"));
		NavShopTab->SetIsEnabled(false);
	}
	// The Idle→Hover edge transition the shared component drops on the floor — see
	// BNButtonEdges. LAST in the button block, and deliberately: it reads GetSelected(), so
	// binding before NavPlayTab->SetIsSelected(true) would start the selected tab dim.
	for (UBRButton* Button : { PlayButton.Get(), CustomsButton.Get(), AcademyButton.Get(), QuitButton.Get(),
		NavPlayTab.Get(), NavCreateTab.Get(), NavCommunityTab.Get(), NavShopTab.Get() })
	{
		BNButtonEdges::Bind(Button);
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
