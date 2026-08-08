#pragma once

#include "CoreMinimal.h"

#include "Character/BRCharacter.h"
#include "FPS/BRRecoilTypes.h"

#include "BRFPSCharacter.generated.h"

class UBRAnimInstance;
class UBRAnimLayerInstance;

/**
 * The character's ANIMATION surface: the pawn as the animation layer sees it.
 *
 * WHY THIS EXTENDS `ABRCharacter` INSTEAD OF REPLACING IT, which is the whole point of the file.
 * The bought template's `BP_FPST_Character` carried 137 properties, and the port ledger's verdict
 * on it was **DROP — in full**: its inventory belongs to BP97, its cached axis values to BP92, its
 * walk speeds to a CSV row, and its `onTake*Damage` delegates to a damage API law 2 bans outright.
 * Not one of its 42 authored properties survives contact with our laws.
 *
 * What *is* real is the seam it implies. Somebody has to own the two meshes' AnimInstances, link
 * a weapon's anim layer when equipment changes, and hand recoil to the thing that presents it.
 * `ABRCharacter` (BP96, `Character/`) is the pawn and stays the pawn. This subclass is the
 * anim-facing half, it lives in `FPS/` where anim-builder owns it, and **a second character class
 * was the outcome to avoid, not the goal** — the ticket's own risk note says a half-ported
 * template is worse than either version because it leaves two systems with an undocumented seam.
 * Extending declares the seam instead of hiding it.
 *
 * WHAT IT DELIBERATELY DOES NOT DO:
 *   · no inventory, no ammo, no scoring, no health — §3.5, "the pawn is a body, not a brain";
 *   · no tick. The template ran five procedural components from `Tick`, on dedicated servers
 *     included. Every quantity they computed is computed on the anim worker thread instead;
 *   · no decision. It forwards and it wires. If a line here ever changes what the game DOES
 *     rather than what it looks like, that is a finding against law 4.
 */
UCLASS(config = Game, meta = (DisplayName = "BR FPS Character"))
class BREACHPOINT_API ABRFPSCharacter : public ABRCharacter
{
	GENERATED_BODY()

public:

	ABRFPSCharacter(const FObjectInitializer& ObjectInitializer);

	/**
	 * Link a weapon's anim layer on both meshes.
	 *
	 * **Called by equipment (BP97), never from here.** The character does not know or ask what is
	 * equipped — that is the whole reason `IBRAnimLayer` exists. The layer class arrives as a
	 * SOFT class resolved from the weapon row, so no C++ in this folder names an animation asset;
	 * grep `FPS/` for `ABP_` and every hit is a comment.
	 *
	 * Idempotent: linking the layer already linked is a no-op rather than an unlink/relink, which
	 * would throw away any blend in flight at the exact moment a player swaps under fire.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Animation")
	void LinkWeaponAnimLayer(TSubclassOf<UAnimInstance> LayerClass);

	/** Drop the current layer and fall back to the spine's own pose. Idempotent. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Animation")
	void UnlinkWeaponAnimLayer();

	/**
	 * Hand a recoil kick to both spines.
	 *
	 * `Alpha` is the FIRE ABILITY's roll, passed straight through. It is not generated here and
	 * must not be: recoil randomised independently per machine desynchronises the client's
	 * crosshair from the server's cone by arithmetic, not by bug. See `animation.md` A.6 — and
	 * note this is **weapon-transform** recoil only. Camera recoil is BP98's.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Animation")
	void ApplyWeaponRecoil(const FBRRecoilImpulse& Impulse, float Alpha);

	/** The arms' spine. Null until the mesh has an anim instance of the right class. */
	UFUNCTION(BlueprintPure, Category = "Breachpoint|Animation")
	UBRAnimInstance* GetFirstPersonAnimInstance() const;

	/** The body's spine — the one that speaks for the pawn on the gameplay-event seam. */
	UFUNCTION(BlueprintPure, Category = "Breachpoint|Animation")
	UBRAnimInstance* GetThirdPersonAnimInstance() const;

protected:

	/**
	 * The layer currently linked, so `LinkWeaponAnimLayer` can be idempotent.
	 *
	 * Not replicated. Each machine links from its own copy of the equipment state, and replicating
	 * a *class pointer* to describe something already derivable from the weapon row would be a
	 * second source of truth for the same fact.
	 */
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> LinkedLayerClass;
};
