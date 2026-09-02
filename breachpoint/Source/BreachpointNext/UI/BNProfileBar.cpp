#include "UI/BNProfileBar.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "UI/BNUITypes.h"

#define LOCTEXT_NAMESPACE "BNProfileBar"

void UBNProfileBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RootSizeBox)
	{
		// C++ owns the shell so the 1280 x 50 cannot drift per screen.
		RootSizeBox->SetHeightOverride(BarHeight);
	}

	if (Ground)
	{
		// Measured #000000 @ 0.5. The reference ALSO puts a BACKGROUND_BLUR behind it; that is a
		// UBackgroundBlur, not a tint, and it is deliberately not faked with a darker fill —
		// a blur that is really just more black reads wrong the moment anything moves behind it.
		Ground->SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
	}

	if (FriendCount)
	{
		// Zero collapses rather than printing "0" — an honest empty beats a confident nothing,
		// and there is no presence system behind this yet.
		FriendCount->SetText(FText::AsNumber(DefaultFriendCount));
		FriendCount->SetVisibility(DefaultFriendCount > 0
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// Boot from config so the footer is never a string typed into the .uasset.
	SetIdentity(FText::FromString(DefaultGamertag), DefaultAvatar);
}

void UBNProfileBar::SetIdentity(const FText& InGamertag, const TSoftObjectPtr<UTexture2D>& InAvatar)
{
	if (Gamertag)
	{
		Gamertag->SetText(InGamertag);
		// Honest empty state: no name renders as nothing, never as a stale previous name.
		Gamertag->SetVisibility(InGamertag.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	PendingAvatar = InAvatar;

	if (!Avatar)
	{
		return;
	}

	if (InAvatar.IsNull())
	{
		// Collapse rather than draw an untextured UImage — an Image with no brush is a WHITE
		// BOX, which is the single most common way a placeholder ships looking like a bug.
		Avatar->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (UTexture2D* Loaded = InAvatar.LoadSynchronous())
	{
		// Sync is correct here and only here: this is a single small UI texture resolved once at
		// screen construction, not a streaming path. An async load would need a stale-request
		// guard for a load that finishes in the same frame.
		Avatar->SetBrushFromTexture(Loaded, /*bMatchSize*/ false);
		Avatar->SetDesiredSizeOverride(FVector2D(AvatarSize, AvatarSize));
		Avatar->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Avatar->SetVisibility(ESlateVisibility::Collapsed);
	}
}

#undef LOCTEXT_NAMESPACE
