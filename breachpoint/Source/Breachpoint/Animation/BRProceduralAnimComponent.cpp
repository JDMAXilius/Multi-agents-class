#include "Animation/BRProceduralAnimComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

#include "Animation/BRAnimInstance.h"

UBRProceduralAnimComponent::UBRProceduralAnimComponent()
{
	// THE ENTIRE POINT OF THIS FILE, in two lines. The template's six procedural components each
	// registered a tick and each ran animation maths on the game thread, on dedicated servers
	// included. This one is asked questions and told things; it never wakes up on its own, so
	// there is nothing to register and nothing to run on a server that renders nothing.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Not replicated. Every value here is derived from the equipped weapon, which IS replicated —
	// so replicating the profile too would put the same fact on the wire twice and let the two
	// copies disagree. Each machine resolves its own from the row it already has.
	SetIsReplicatedByDefault(false);
}

void UBRProceduralAnimComponent::BeginPlay()
{
	Super::BeginPlay();

	ClearWeaponProfile();
}

void UBRProceduralAnimComponent::SetWeaponProfile(const FBRSwayAndLagInfo& InSwayAndLag,
	const FBRRecoilInfo& InRecoil, const FBRAimAndLeanInfo& InAimAndLean)
{
	SwayAndLag = InSwayAndLag;
	Recoil = InRecoil;
	AimAndLean = InAimAndLean;
}

void UBRProceduralAnimComponent::ClearWeaponProfile()
{
	// Unarmed is a state, not an absence: empty hands still sway. Falling back to a zeroed struct
	// would freeze the arms solid the instant a player drops a weapon, which reads as a hitch.
	SwayAndLag = DefaultSwayAndLag;
	AimAndLean = DefaultAimAndLean;
	Recoil = FBRRecoilInfo();
}

void UBRProceduralAnimComponent::ForEachSpine(TFunctionRef<void(UBRAnimInstance&)> Fn) const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	// Every skeletal mesh whose anim instance is one of ours. Asked by TYPE rather than by a known
	// mesh name, so a pawn with one mesh, two, or a third for a shoulder-cam all work without this
	// component knowing the character's anatomy -- which is the composition argument made concrete.
	TArray<USkeletalMeshComponent*> Meshes;
	Character->GetComponents<USkeletalMeshComponent>(Meshes);

	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}

		if (UBRAnimInstance* Spine = Cast<UBRAnimInstance>(Mesh->GetAnimInstance()))
		{
			Fn(*Spine);
		}
	}
}

void UBRProceduralAnimComponent::LinkWeaponAnimLayer(TSubclassOf<UAnimInstance> LayerClass)
{
	DesiredLayerClass = LayerClass;

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Character->GetComponents<USkeletalMeshComponent>(Meshes);

	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		// ASK THE MESH. A mesh with no anim instance cannot link, and `LinkAnimClassLayers` will
		// not say so -- it returns void and does nothing. Any `InitAnim` also drops linked layers
		// behind our back, so a remembered "already linked" can be stale in the other direction too.
		if (!Mesh || !Mesh->GetAnimInstance())
		{
			LinkedLayers.Remove(Mesh);
			continue;
		}

		TSubclassOf<UAnimInstance>& Linked = LinkedLayers.FindOrAdd(Mesh);
		if (Linked == LayerClass)
		{
			continue;
		}

		if (Linked)
		{
			Mesh->UnlinkAnimClassLayers(Linked);
		}

		if (LayerClass)
		{
			Mesh->LinkAnimClassLayers(LayerClass);
		}

		Linked = LayerClass;
	}
}

void UBRProceduralAnimComponent::UnlinkWeaponAnimLayer()
{
	LinkWeaponAnimLayer(nullptr);
	DesiredLayerClass = nullptr;
}

void UBRProceduralAnimComponent::FireRecoil(float Alpha, bool bIsADS)
{
	// Hip and ADS are separately authored envelopes, not one scaled by the other. Recoil is among
	// the few things a player judges a weapon by, and "ADS is hip times 0.5" is a shortcut that
	// makes every weapon in the game feel related.
	const FBRRecoilImpulse& Impulse = bIsADS ? Recoil.ADS : Recoil.Default;

	// The SAME alpha to every spine. Rolling per mesh would make the arms and the body kick
	// differently for one shot; rolling here at all would break the client/server agreement the
	// caller's seed exists to hold (animation.md A.6).
	ForEachSpine([&Impulse, Alpha](UBRAnimInstance& Spine)
	{
		Spine.AddRecoilImpulse(Impulse, Alpha);
	});
}
