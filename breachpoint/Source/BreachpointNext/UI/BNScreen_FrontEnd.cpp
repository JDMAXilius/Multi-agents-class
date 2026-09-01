#include "UI/BNScreen_FrontEnd.h"
#include "BreachpointNext.h"
#include "Components/Button.h"
#include "Components/Image.h"
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

	if (PlayButton)
	{
		PlayButton->OnClicked.AddUniqueDynamic(this, &UBNScreen_FrontEnd::HandlePlayClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddUniqueDynamic(this, &UBNScreen_FrontEnd::HandleQuitClicked);
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(LOCTEXT("FrontEndHint", "Set up a match against bots."));
		DescriptionText->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim));
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
