#include "UI/Components/BRFeatureCard.h"

#include "Animation/WidgetAnimation.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "UI/Components/BRHairlineBorder.h"

#define LOCTEXT_NAMESPACE "BRFeatureCard"

FText UBRFeatureCard::GetUnknownValueText()
{
	// An em dash. Written as an escape so this file stays pure ASCII, matching UBRProfileBar.
	return LOCTEXT("UnknownValue", "\u2014");
}

void UBRFeatureCard::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RootSizeBox)
	{
		// HEIGHT ONLY. The width is the rail's 349 column; see the q13 note on the class.
		RootSizeBox->SetHeightOverride(CardHeight);
		RootSizeBox->ClearWidthOverride();
	}

	if (ImageBox)
	{
		ImageBox->SetHeightOverride(ImageHeight);
	}

	if (Ground)
	{
		Ground->SetColorAndOpacity(BRUI::ResolveColorToken(GroundToken));
	}

	if (Border)
	{
		Border->SetEdgeDimmed(EBRBorderEdge::Bottom, true);
	}

	// Start honest rather than trusting whatever the WBP author left in the preview brush.
	ClearFeature();
}

void UBRFeatureCard::NativeConstruct()
{
	Super::NativeConstruct();

	// CPP-AUDIT D9: destruct cancels the stream but the object survives a pop, keeping its
	// RequestedImage with no texture behind it. Without this re-request, popping the screen
	// mid-stream and re-pushing it left the 196.7 band empty until the carousel happened to
	// advance — forever, on a static single-entry carousel. SetFeatureImage's resident-check
	// makes this free when the texture survived in memory.
	if (!RequestedImage.IsNull() && !ImageHandle.IsValid())
	{
		SetFeatureImage(RequestedImage);
	}
}

void UBRFeatureCard::NativeDestruct()
{
	// The handle outliving the widget is the leak that turns a menu open/close loop into a
	// streaming stall. Cancel, do not merely release.
	if (ImageHandle.IsValid())
	{
		ImageHandle->CancelHandle();
		ImageHandle.Reset();
	}

	Super::NativeDestruct();
}

void UBRFeatureCard::SetFeatureImage(TSoftObjectPtr<UTexture2D> InImage)
{
	RequestedImage = InImage;

	if (ImageHandle.IsValid())
	{
		ImageHandle->CancelHandle();
		ImageHandle.Reset();
	}

	const FSoftObjectPath RequestedPath = RequestedImage.ToSoftObjectPath();

	if (RequestedPath.IsNull())
	{
		// No art for this entry. Hidden, not Collapsed: the 196.7 band keeps its space.
		if (FeatureImage)
		{
			FeatureImage->SetBrushResourceObject(nullptr);
			FeatureImage->SetVisibility(ESlateVisibility::Hidden);
		}

		return;
	}

	if (RequestedImage.Get() != nullptr)
	{
		// Already resident (the carousel came back to an entry). No request, no frame of nothing.
		HandleImageLoaded(RequestedPath);
		return;
	}

	// Hide the previous article's art immediately -- showing it while the new one streams is a
	// stale frame, and a stale frame states something false about what this card opens.
	if (FeatureImage)
	{
		FeatureImage->SetVisibility(ESlateVisibility::Hidden);
	}

	ImageHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		RequestedPath,
		FStreamableDelegate::CreateWeakLambda(this, [this, RequestedPath]()
		{
			HandleImageLoaded(RequestedPath);
		}));
}

void UBRFeatureCard::HandleImageLoaded(FSoftObjectPath LoadedPath)
{
	// The carousel can swap faster than the streamer resolves. Anything that is not the entry
	// currently requested is a straggler and is dropped, not drawn.
	if (LoadedPath != RequestedImage.ToSoftObjectPath())
	{
		return;
	}

	UTexture2D* Texture = Cast<UTexture2D>(LoadedPath.ResolveObject());

	if (!FeatureImage)
	{
		return;
	}

	if (!Texture)
	{
		FeatureImage->SetBrushResourceObject(nullptr);
		FeatureImage->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	// bMatchSize = false: the band is 349 x 196.7 whatever the source texture measures.
	FeatureImage->SetBrushFromTexture(Texture, /*bMatchSize*/ false);
	FeatureImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBRFeatureCard::SetCaptionText(const FText& InCaption)
{
	if (Caption)
	{
		Caption->SetText(InCaption.IsEmpty() ? GetUnknownValueText() : InCaption);
	}
}

void UBRFeatureCard::ClearFeature()
{
	SetCaptionText(FText::GetEmpty());
	SetFeatureImage(TSoftObjectPtr<UTexture2D>());
}

UPanelWidget* UBRFeatureCard::GetDotsContainer() const
{
	return DotsContainer;
}

void UBRFeatureCard::ApplyHoverState(bool bHovered)
{
	// COMPONENT-SPECS Sec 2 hover: Bottom Line opacity 0.3 -> 1. The card carries the same cue as
	// the rows beneath it, so focus reads continuously down the rail.
	if (Border)
	{
		Border->SetEdgeDimmed(EBRBorderEdge::Bottom, !bHovered);
	}

	if (HoverAnim)
	{
		if (bHovered)
		{
			PlayAnimationForward(HoverAnim);
		}
		else
		{
			PlayAnimationReverse(HoverAnim);
		}
	}
}

void UBRFeatureCard::NativeOnHovered()
{
	Super::NativeOnHovered();

	ApplyHoverState(true);
}

void UBRFeatureCard::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	ApplyHoverState(false);
}

#undef LOCTEXT_NAMESPACE
