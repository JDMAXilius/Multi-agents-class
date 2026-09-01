#pragma once

#include "GameFramework/Actor.h"
#include "BNFrontEndDisplay.generated.h"

class USkeletalMeshComponent;

/**
 * The front-end stage's hero prop — a body mesh with a weapon that actually rides a hand
 * socket.
 *
 * WHY THIS CLASS EXISTS, since a plain SkeletalMeshActor plus a component would be less
 * code: `USceneComponent::AttachSocketName` carries no EditAnywhere specifier, so it is
 * invisible to the details panel and to `ObjectTools.set_properties`. There is no property
 * write that sockets a mesh — it takes a `SetupAttachment`/`AttachToComponent` call, and
 * that takes C++. The BN42/BN43 editor pass proved the gap the expensive way: the rifle was
 * placed by a fixed relative transform and did not track the hand through the idle.
 *
 * It is a PROP, not a pawn: no ASC, no movement, no collision, and no Tick (law 4). Every
 * asset — body mesh, idle animation, weapon mesh — is assigned on the PLACED ACTOR in the
 * level, never here: law 3 forbids hard asset refs and ConstructorHelpers in C++.
 */
UCLASS()
class BREACHPOINTNEXT_API ABNFrontEndDisplay : public AActor
{
	GENERATED_BODY()

public:
	ABNFrontEndDisplay();

	/** The character. Set SkeletalMeshAsset + AnimationMode/AnimationData on the instance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BN|FrontEnd")
	TObjectPtr<USkeletalMeshComponent> Body;

	/** The weapon. Rides `WeaponSocket` on Body, so it follows the hand through the idle. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BN|FrontEnd")
	TObjectPtr<USkeletalMeshComponent> Weapon;

	/** Socket or bone on Body the weapon rides. `weapon_r` is the UE mannequin's. */
	UPROPERTY(EditAnywhere, Category = "BN|FrontEnd")
	FName WeaponSocket = TEXT("weapon_r");

protected:
	/** Re-seats the weapon when the socket is retyped in the details panel. */
	virtual void OnConstruction(const FTransform& Transform) override;
};
