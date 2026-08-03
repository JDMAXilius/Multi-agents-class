#pragma once

#include "CommonUserWidget.h"
#include "FieldNotificationId.h"
#include "UI/BRUITypes.h"

#include "BRReticleWidget.generated.h"

class UBRVM_Combat;
class UImage;
class UTexture2D;
struct FStreamableHandle;

/**
 * One reticle's art plus the size it was authored at.
 *
 * THE SIZE IS THE INFORMATION. Every reticle in the Figma export is square, and its edge
 * length is the weapon's spread told to the player without a number: Magnum 36, AR/BR 43,
 * Shotgun 52, Sniper 58. Normalising them to one box would silently delete the only
 * accuracy cue the centre of the screen carries -- so the size travels WITH the art, in
 * data, and there is exactly one widget class for all five.
 */
USTRUCT(BlueprintType)
struct FBRReticleArt
{
	GENERATED_BODY()

	/** SOFT (law 3). Nothing here loads until a weapon actually asks for it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Reticle")
	TSoftObjectPtr<UTexture2D> Art;

	/** Measured from the export's viewBox, base 1280x720. Square in every shipped reticle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breachpoint|Reticle")
	FVector2D SizePx = FVector2D(43.0, 43.0);
};

/**
 * The centre HUD surface: reticle plus hit markers.
 *
 * Two rules govern this class.
 *
 * 1. HIT MARKERS ARE GAMEPLAY INFORMATION (ue5-ui-architecture Sec 4). Shield-hit and
 *    flesh-hit are DIFFERENT facts, and both arrive from the server-confirmed damage path
 *    via UBRVM_Combat::OnHitMarker -- never from a local trace and never from a guess.
 *    This widget owns no opinion about whether a shot landed; it only draws what the
 *    ViewModel was told. See the report's contract gap: the ViewModel end of that wire is
 *    live, but nothing calls ReportHitMarker yet.
 *
 * 2. THE RETICLE SIZE IS THE SPREAD. See FBRReticleArt.
 *
 * FIXED COLOUR, deliberately. BP22 (reticle turns Enemy-red over a target) needs a fire /
 * target-query path that does not exist yet, and a colour channel wired to nothing would
 * be a lie with a tint on it. The rest-state tint is the one token that names this use:
 * BR::Tokens::Shield ("reticle at rest").
 *
 * Zero Tick (law 4): the marker decays on a timer, the reticle changes on a push.
 */
UCLASS(Abstract, Config = Game, meta = (DisableNativeTick))
class BREACHPOINT_API UBRReticleWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UBRReticleWidget();

	/**
	 * The weapon identity this widget wants: a DT_Weapons row name (AR / Magnum / Rocket).
	 * NAME_None means "no weapon known yet" and draws NOTHING -- that is the honest
	 * join-in-progress state, not a bug.
	 *
	 * This is the entry point whoever owns weapon-equip should call. Today it is also
	 * driven, weakly, off the ViewModel's display text -- see WeaponIdFromDisplayText.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Reticle")
	void SetActiveWeaponId(FName InWeaponId);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Reticle")
	FName GetActiveWeaponId() const { return ActiveWeaponId; }

	/** Public for tests and for the ViewModel delegate. Higher-priority kinds win. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Reticle")
	void ShowHitMarker(EBRHitMarkerKind InKind);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Reticle")
	EBRHitMarkerKind GetActiveHitMarkerKind() const { return ActiveHitMarkerKind; }

	/**
	 * The weapon whose reticle stands in when a weapon has no art of its own.
	 *
	 * This exists for exactly one weapon: the ROCKET. It is the only row that actually
	 * ships in DT_Weapons.csv, and it is the only weapon with no `SET Reticle /` art
	 * (TICKET_BP69). Inventing a Rocket reticle here would be design-by-programmer, and
	 * falling through silently would tell the player the Rocket has the AR's spread. So it
	 * falls through LOUDLY: named constant, one warning per weapon, and a BP hook flag so
	 * the WBP can mark it on screen if the founder wants it marked.
	 */
	static const FName StandInReticleWeaponId;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Layout and animation only. Optional: a WBP may drive the visuals purely from the hooks. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|Reticle")
	TObjectPtr<UImage> ReticleImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|Reticle")
	TObjectPtr<UImage> HitMarkerImage;

	/**
	 * Weapon row name -> art + authored size. Config = Game, so tuning is an ini edit and
	 * not a recompile; the constructor defaults are the measured export and a coordination
	 * point for the art packet, not a promise the .uassets exist yet (same posture as
	 * BR::Tokens::FontRajdhani).
	 *
	 * The Rocket is ABSENT ON PURPOSE. Do not add it here to silence the warning.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Reticle")
	TMap<FName, FBRReticleArt> ReticleByWeaponId;

	/** Four distinct kinds, four distinct assets. Shield-hit must not look like flesh-hit. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Reticle")
	TMap<EBRHitMarkerKind, TSoftObjectPtr<UTexture2D>> HitMarkerArtByKind;

	/** Measured: the hit-marker export is 43x43, matching the AR/BR reticle box. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Reticle")
	FVector2D HitMarkerSizePx = FVector2D(43.0, 43.0);

	/**
	 * How long a marker stays up. NOT a gameplay number -- it changes nothing the server
	 * computes -- so it is not in DT_Weapons.csv. It is still a tuned value, so it is not
	 * buried as a literal at the call site either: it is config-overridable here, and the
	 * default is flagged in the report as unsourced until someone playtests it.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Reticle", meta = (ClampMin = "0.01"))
	float HitMarkerDurationSeconds = 0.15f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|Reticle", meta = (DisplayName = "On Reticle Changed"))
	void BP_OnReticleChanged(FName WeaponId, FVector2D SizePx, bool bIsStandIn);

	/** Fired even when no art resolves, so a WBP animation can still carry the confirm. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|Reticle", meta = (DisplayName = "On Hit Marker Shown"))
	void BP_OnHitMarkerShown(EBRHitMarkerKind Kind);

	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|Reticle", meta = (DisplayName = "On Hit Marker Hidden"))
	void BP_OnHitMarkerHidden();

private:
	void BindViewModel();
	void UnbindViewModel();

	UBRVM_Combat* GetCombatViewModel() const;

	void HandleHitMarker(EBRHitMarkerKind Kind);
	void HandleCombatFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);

	void ApplyReticle();
	void HideHitMarker();
	void PreloadArt();

	/** FText display name -> row name. The weak link; see the contract gap in the report. */
	static FName WeaponIdFromDisplayText(const FText& InText);

	static void ApplyArt(UImage* Image, const TSoftObjectPtr<UTexture2D>& SoftArt, const FVector2D& SizePx);

	UPROPERTY(Transient)
	FName ActiveWeaponId;

	UPROPERTY(Transient)
	EBRHitMarkerKind ActiveHitMarkerKind = EBRHitMarkerKind::None;

	/** One warning per unknown weapon, not one per equip. */
	TSet<FName> WarnedMissingReticleIds;

	FTimerHandle HitMarkerTimerHandle;

	FDelegateHandle ActiveWeaponFieldHandle;

	TSharedPtr<FStreamableHandle> ArtPreloadHandle;
};
