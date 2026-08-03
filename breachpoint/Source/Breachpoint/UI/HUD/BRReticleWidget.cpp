#include "UI/HUD/BRReticleWidget.h"

#include "Breachpoint.h"
#include "Components/Image.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "INotifyFieldValueChanged.h"
#include "TimerManager.h"
#include "UI/BRUIManagerSubsystem.h"
#include "UI/BRViewModels.h"
#include "UI/Styles/BRUITokens.h"

namespace
{
	/**
	 * Measured this session from Export/UI/HUD/HUD_Reticle_*.svg viewBox attributes.
	 * Square in every case, so one edge is the whole measurement.
	 */
	FBRReticleArt MakeReticle(const TCHAR* SoftPath, const float EdgePx)
	{
		FBRReticleArt Out;
		Out.Art = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(SoftPath));
		Out.SizePx = FVector2D(EdgePx, EdgePx);
		return Out;
	}
}

const FName UBRReticleWidget::StandInReticleWeaponId(TEXT("AR"));

UBRReticleWidget::UBRReticleWidget()
{
	// Content/UI is another packet's owner path. These soft paths create no package
	// dependency and load nothing (BRUITokens.h says the same thing about the fonts) --
	// they are a coordination point with the art pipeline, overridable per-project from
	// Config/DefaultGame.ini without touching this file.
	//
	// Sizes are the export's viewBox, NOT a layout preference. See FBRReticleArt.
	ReticleByWeaponId.Add(TEXT("AR"), MakeReticle(TEXT("/Game/UI/HUD/Reticles/T_Reticle_AR.T_Reticle_AR"), 43.0f));
	ReticleByWeaponId.Add(TEXT("BR"), MakeReticle(TEXT("/Game/UI/HUD/Reticles/T_Reticle_BR.T_Reticle_BR"), 43.0f));
	ReticleByWeaponId.Add(TEXT("Magnum"), MakeReticle(TEXT("/Game/UI/HUD/Reticles/T_Reticle_Magnum.T_Reticle_Magnum"), 36.0f));
	ReticleByWeaponId.Add(TEXT("Shotgun"), MakeReticle(TEXT("/Game/UI/HUD/Reticles/T_Reticle_Shotgun.T_Reticle_Shotgun"), 52.0f));
	ReticleByWeaponId.Add(TEXT("Sniper"), MakeReticle(TEXT("/Game/UI/HUD/Reticles/T_Reticle_Sniper.T_Reticle_Sniper"), 58.0f));

	// NO ROCKET ENTRY. TICKET_BP69: the Rocket ships in DT_Weapons.csv and has no reticle
	// art. It resolves to StandInReticleWeaponId and says so, once, in the log.

	// Four kinds, four assets. The art carries the state, which is why nothing below tints
	// a marker: a shield hit and a flesh hit differ by ASSET, not by a colour this widget
	// invents. The export shipped one 43x43 red state, so three of these four paths point
	// at assets that do not exist yet -- see the report.
	HitMarkerArtByKind.Add(EBRHitMarkerKind::Shield, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/HUD/Reticles/T_HitMarker_Shield.T_HitMarker_Shield"))));
	HitMarkerArtByKind.Add(EBRHitMarkerKind::Flesh, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/HUD/Reticles/T_HitMarker_Flesh.T_HitMarker_Flesh"))));
	HitMarkerArtByKind.Add(EBRHitMarkerKind::Headshot, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/HUD/Reticles/T_HitMarker_Headshot.T_HitMarker_Headshot"))));
	HitMarkerArtByKind.Add(EBRHitMarkerKind::Kill, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/HUD/Reticles/T_HitMarker_Kill.T_HitMarker_Kill"))));
}

void UBRReticleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// The centre of the screen must never eat a click or take focus. It is also not
	// gamepad-navigable BY DESIGN -- there is nothing here to navigate to; every
	// interactive element on the HUD lives in a sibling widget.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (ReticleImage)
	{
		ReticleImage->SetVisibility(ESlateVisibility::HitTestInvisible);

		// FIXED. BP22 (dynamic reticle colour) is blocked behind a fire/target-query path
		// that does not exist; a tint that never changes is honest, a tint wired to
		// nothing is not. BR::Tokens::Shield is the token whose documented meaning is
		// literally "reticle at rest".
		ReticleImage->SetColorAndOpacity(BR::Tokens::Shield());
	}

	if (HitMarkerImage)
	{
		HitMarkerImage->SetVisibility(ESlateVisibility::Collapsed);

		// Untinted: the four assets ARE the four states.
		HitMarkerImage->SetColorAndOpacity(FLinearColor::White);
	}

	// Unknown until a weapon says otherwise. Draws nothing.
	ApplyReticle();

	PreloadArt();
}

void UBRReticleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindViewModel();
}

void UBRReticleWidget::NativeDestruct()
{
	UnbindViewModel();

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitMarkerTimerHandle);
	}

	if (ArtPreloadHandle.IsValid())
	{
		ArtPreloadHandle->CancelHandle();
		ArtPreloadHandle.Reset();
	}

	Super::NativeDestruct();
}

UBRVM_Combat* UBRReticleWidget::GetCombatViewModel() const
{
	UBRUIManagerSubsystem* Manager = UBRUIManagerSubsystem::Get(this);
	return Manager ? Manager->GetCombatViewModel(GetOwningLocalPlayer()) : nullptr;
}

void UBRReticleWidget::BindViewModel()
{
	UBRVM_Combat* Combat = GetCombatViewModel();
	if (!Combat)
	{
		// Join-in-progress / early construct. Nothing to show and nothing to fake.
		return;
	}

	// THE hit-marker wire. Server-confirmed damage -> UBRVM_Combat::ReportHitMarker ->
	// here. This widget never traces, never predicts, never infers a hit from input.
	Combat->OnHitMarker().AddUObject(this, &UBRReticleWidget::HandleHitMarker);

	ActiveWeaponFieldHandle = Combat->AddFieldValueChangedDelegate(
		UBRVM_Combat::FFieldNotificationClassDescriptor::ActiveWeaponName,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
			this, &UBRReticleWidget::HandleCombatFieldChanged));

	// Pull the current value once: the equip that chose this weapon may have happened
	// before this widget existed.
	SetActiveWeaponId(WeaponIdFromDisplayText(Combat->GetActiveWeaponName()));
}

void UBRReticleWidget::UnbindViewModel()
{
	if (UBRVM_Combat* Combat = GetCombatViewModel())
	{
		Combat->OnHitMarker().RemoveAll(this);

		if (ActiveWeaponFieldHandle.IsValid())
		{
			Combat->RemoveFieldValueChangedDelegate(
				UBRVM_Combat::FFieldNotificationClassDescriptor::ActiveWeaponName, ActiveWeaponFieldHandle);
		}
	}

	ActiveWeaponFieldHandle.Reset();
}

void UBRReticleWidget::HandleCombatFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	const UBRVM_Combat* Combat = Cast<UBRVM_Combat>(Source);
	if (Combat && FieldId == UBRVM_Combat::FFieldNotificationClassDescriptor::ActiveWeaponName)
	{
		SetActiveWeaponId(WeaponIdFromDisplayText(Combat->GetActiveWeaponName()));
	}
}

FName UBRReticleWidget::WeaponIdFromDisplayText(const FText& InText)
{
	// CONTRACT GAP, filed in this packet's report: UBRVM_Combat exposes only a LOCALISED
	// display FText for the active weapon, so this is the best identity available. It
	// matches when the text happens to equal the DT_Weapons row name and misses otherwise
	// -- and a miss is not silent, it lands in the stand-in path below. The fix is a row
	// FName on the ViewModel; when it arrives, delete this function and keep everything else.
	return InText.IsEmpty() ? NAME_None : FName(*InText.ToString());
}

void UBRReticleWidget::SetActiveWeaponId(FName InWeaponId)
{
	if (ActiveWeaponId == InWeaponId)
	{
		return;
	}

	ActiveWeaponId = InWeaponId;
	ApplyReticle();
}

void UBRReticleWidget::ApplyReticle()
{
	if (ActiveWeaponId.IsNone())
	{
		// Honest unknown: no weapon has been confirmed, so no reticle. A reticle drawn
		// before the ViewModel has spoken would claim an accuracy the player does not have.
		ApplyArt(ReticleImage, nullptr, FVector2D::ZeroVector);
		if (ReticleImage)
		{
			ReticleImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		BP_OnReticleChanged(NAME_None, FVector2D::ZeroVector, false);
		return;
	}

	const FBRReticleArt* Found = ReticleByWeaponId.Find(ActiveWeaponId);
	bool bIsStandIn = false;

	if (!Found)
	{
		bIsStandIn = true;

		// Loud, once per weapon. Today the only shipped weapon that lands here is the
		// Rocket (TICKET_BP69); an unknown display-name string lands here too.
		if (!WarnedMissingReticleIds.Contains(ActiveWeaponId))
		{
			WarnedMissingReticleIds.Add(ActiveWeaponId);
			UE_LOG(Logbreachpoint, Warning,
				TEXT("UBRReticleWidget: weapon '%s' has no reticle art; standing in with '%s'. ")
				TEXT("This is NOT that weapon's spread. See docs/tickets/TICKET_BP69_HUD_ART_EXCEEDS_SIM.md."),
				*ActiveWeaponId.ToString(), *StandInReticleWeaponId.ToString());
		}

		Found = ReticleByWeaponId.Find(StandInReticleWeaponId);
	}

	if (!Found)
	{
		// The stand-in itself is unconfigured. Draw nothing rather than something wrong.
		ApplyArt(ReticleImage, nullptr, FVector2D::ZeroVector);
		if (ReticleImage)
		{
			ReticleImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		BP_OnReticleChanged(ActiveWeaponId, FVector2D::ZeroVector, bIsStandIn);
		return;
	}

	ApplyArt(ReticleImage, Found->Art, Found->SizePx);
	if (ReticleImage)
	{
		ReticleImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// The size goes to the WBP too: SetDesiredSizeOverride only wins where the layout lets
	// the image size itself. A SizeBox around it must be driven from here, or the five
	// sizes collapse back into one and the spread cue is gone.
	BP_OnReticleChanged(ActiveWeaponId, Found->SizePx, bIsStandIn);
}

void UBRReticleWidget::HandleHitMarker(EBRHitMarkerKind Kind)
{
	ShowHitMarker(Kind);
}

void UBRReticleWidget::ShowHitMarker(EBRHitMarkerKind InKind)
{
	if (InKind == EBRHitMarkerKind::None)
	{
		return;
	}

	// EBRHitMarkerKind is declared in ascending severity (Shield < Flesh < Headshot <
	// Kill), so priority is the enum order. A follow-up body shot must not downgrade a
	// kill confirm that is still on screen -- the player would read the weaker fact and
	// believe the target lived.
	if (ActiveHitMarkerKind != EBRHitMarkerKind::None
		&& static_cast<uint8>(InKind) < static_cast<uint8>(ActiveHitMarkerKind))
	{
		return;
	}

	ActiveHitMarkerKind = InKind;

	const TSoftObjectPtr<UTexture2D>* SoftArt = HitMarkerArtByKind.Find(InKind);
	ApplyArt(HitMarkerImage, SoftArt ? *SoftArt : nullptr, HitMarkerSizePx);

	if (HitMarkerImage)
	{
		HitMarkerImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// Fired unconditionally: the confirm is information, so a WBP animation must be able
	// to carry it even while the four marker assets are still unauthored.
	BP_OnHitMarkerShown(InKind);

	if (const UWorld* World = GetWorld())
	{
		// Law 4: a timer, not a Tick. Re-arming resets the window, which is what a second
		// hit inside the window should do.
		World->GetTimerManager().SetTimer(
			HitMarkerTimerHandle, this, &UBRReticleWidget::HideHitMarker, HitMarkerDurationSeconds, false);
	}
	else
	{
		// No world (design time / teardown): never leave a marker latched on.
		HideHitMarker();
	}
}

void UBRReticleWidget::HideHitMarker()
{
	if (ActiveHitMarkerKind == EBRHitMarkerKind::None)
	{
		return;
	}

	ActiveHitMarkerKind = EBRHitMarkerKind::None;

	if (HitMarkerImage)
	{
		HitMarkerImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	BP_OnHitMarkerHidden();
}

void UBRReticleWidget::ApplyArt(UImage* Image, const TSoftObjectPtr<UTexture2D>& SoftArt, const FVector2D& SizePx)
{
	if (!Image)
	{
		return;
	}

	Image->SetDesiredSizeOverride(SizePx);

	if (SoftArt.IsNull())
	{
		Image->SetBrushResourceObject(nullptr);
		return;
	}

	// Warm after PreloadArt(); the synchronous fallback only ever runs if the preload was
	// cancelled or the asset was added to config after construction. These are 36-58px
	// textures, so the fallback is a fraction of a millisecond, not a weapon-swap hitch.
	UTexture2D* Loaded = SoftArt.Get();
	if (!Loaded)
	{
		Loaded = SoftArt.LoadSynchronous();
	}

	Image->SetBrushResourceObject(Loaded);
}

void UBRReticleWidget::PreloadArt()
{
	// One request for the whole set (at most nine tiny textures) instead of a load per
	// weapon swap. Held by one handle so teardown can cancel it, and so the set stays
	// resident for the life of the HUD -- which is the entire match.
	TArray<FSoftObjectPath> Paths;

	for (const TPair<FName, FBRReticleArt>& Pair : ReticleByWeaponId)
	{
		if (!Pair.Value.Art.IsNull())
		{
			Paths.Add(Pair.Value.Art.ToSoftObjectPath());
		}
	}

	for (const TPair<EBRHitMarkerKind, TSoftObjectPtr<UTexture2D>>& Pair : HitMarkerArtByKind)
	{
		if (!Pair.Value.IsNull())
		{
			Paths.Add(Pair.Value.ToSoftObjectPath());
		}
	}

	if (Paths.IsEmpty())
	{
		return;
	}

	UAssetManager* Manager = UAssetManager::GetIfInitialized();
	if (!Manager)
	{
		return;
	}

	TWeakObjectPtr<UBRReticleWidget> WeakThis(this);
	ArtPreloadHandle = Manager->GetStreamableManager().RequestAsyncLoad(
		Paths,
		FStreamableDelegate::CreateLambda([WeakThis]()
		{
			if (UBRReticleWidget* Self = WeakThis.Get())
			{
				// The reticle may have been chosen before its texture arrived.
				Self->ApplyReticle();
			}
		}),
		FStreamableManager::DefaultAsyncLoadPriority,
		/*bManageActiveHandle=*/false,
		/*bStartStalled=*/false,
		TEXT("BRReticleWidget"));
}
