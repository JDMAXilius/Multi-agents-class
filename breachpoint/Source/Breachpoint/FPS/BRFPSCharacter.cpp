#include "FPS/BRFPSCharacter.h"

#include "Components/SkeletalMeshComponent.h"

#include "FPS/BRAnimInstance.h"

ABRFPSCharacter::ABRFPSCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// NO TICK. `BP_FPST_Character` shipped with `bCanEverTick: true` AND
	// `bAllowTickOnDedicatedServer: true`, driving five procedural components every frame on a
	// machine that renders nothing. CLAUDE.md law 4 forbids gameplay Tick, and every quantity
	// those components produced is computed on the animation worker thread instead.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

UBRAnimInstance* ABRFPSCharacter::GetFirstPersonAnimInstance() const
{
	const USkeletalMeshComponent* Mesh1P = GetFirstPersonMesh();
	return Mesh1P ? Cast<UBRAnimInstance>(Mesh1P->GetAnimInstance()) : nullptr;
}

UBRAnimInstance* ABRFPSCharacter::GetThirdPersonAnimInstance() const
{
	const USkeletalMeshComponent* Mesh3P = GetMesh3P();
	return Mesh3P ? Cast<UBRAnimInstance>(Mesh3P->GetAnimInstance()) : nullptr;
}

void ABRFPSCharacter::LinkWeaponAnimLayer(TSubclassOf<UAnimInstance> LayerClass)
{
	// Idempotent, and not as a micro-optimisation. `LinkAnimClassLayers` on an already-linked
	// class tears the layer instance down and builds a new one, which discards every blend,
	// additive and inertialised transition in flight. Equipment re-broadcasting its state -- on
	// respawn, on a relevancy change, on any OnRep -- would otherwise hitch the weapon pose at
	// exactly the moment a player is swapping under fire. This is the same failure Amendment A
	// §A.3 rejected AnimInstance-swapping for; the mechanism is different, the cost is identical.
	if (LayerClass == LinkedLayerClass)
	{
		return;
	}

	UnlinkWeaponAnimLayer();

	if (!LayerClass)
	{
		return;
	}

	// Both meshes. The arms and the body pose the same weapon and would otherwise disagree about
	// which one they are holding -- visible to everyone except the player causing it, which is
	// the class of bug law 7's three views exists to catch.
	if (USkeletalMeshComponent* Mesh1P = GetFirstPersonMesh())
	{
		Mesh1P->LinkAnimClassLayers(LayerClass);
	}

	if (USkeletalMeshComponent* Mesh3P = GetMesh3P())
	{
		Mesh3P->LinkAnimClassLayers(LayerClass);
	}

	LinkedLayerClass = LayerClass;
}

void ABRFPSCharacter::UnlinkWeaponAnimLayer()
{
	if (!LinkedLayerClass)
	{
		return;
	}

	if (USkeletalMeshComponent* Mesh1P = GetFirstPersonMesh())
	{
		Mesh1P->UnlinkAnimClassLayers(LinkedLayerClass);
	}

	if (USkeletalMeshComponent* Mesh3P = GetMesh3P())
	{
		Mesh3P->UnlinkAnimClassLayers(LinkedLayerClass);
	}

	LinkedLayerClass = nullptr;
}

void ABRFPSCharacter::ApplyWeaponRecoil(const FBRRecoilImpulse& Impulse, float Alpha)
{
	// Forwarded to both spines with the SAME alpha. Rolling once per mesh would make the arms and
	// the body kick differently for one shot -- and rolling here at all would break the
	// client/server agreement the caller's seed exists to hold (animation.md A.6).
	if (UBRAnimInstance* Anim1P = GetFirstPersonAnimInstance())
	{
		Anim1P->AddRecoilImpulse(Impulse, Alpha);
	}

	if (UBRAnimInstance* Anim3P = GetThirdPersonAnimInstance())
	{
		Anim3P->AddRecoilImpulse(Impulse, Alpha);
	}
}
