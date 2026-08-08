#include "FPS/BRFPSCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "FPS/BRAnimInstance.h"

ABRFPSCharacter::ABRFPSCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// `BP_FPST_Character` shipped with `bCanEverTick: true` AND `bAllowTickOnDedicatedServer:
	// true`, driving five procedural components every frame on a machine that renders nothing.
	// Law 4 forbids gameplay Tick, and every quantity those components produced is computed on the
	// animation worker thread instead.
	//
	// Tick is ALLOWED here but starts DISABLED, and only a camera FOV blend ever enables it. It
	// disables itself the frame the blend settles, so a pawn at rest costs nothing -- which is the
	// property law 4 protects. And never on a dedicated server: an FOV blend on a machine with no
	// viewport is pure waste.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
}

void ABRFPSCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyAnimInstanceClasses();

	TargetFOV = DefaultFOV;
	if (UCameraComponent* Camera = GetFirstPersonCameraComponent())
	{
		Camera->SetFieldOfView(DefaultFOV);
	}
}

void ABRFPSCharacter::ApplyAnimInstanceClasses()
{
	// Resolved from a SOFT class in config, so no C++ in this project names an animation asset.
	// `Variant_Shooter` was deleted for hard-referencing an AnimInstance class per weapon; this is
	// the shape that does the same job without the reference.
	if (USkeletalMeshComponent* Mesh1P = GetFirstPersonMesh())
	{
		if (UClass* Loaded = FirstPersonAnimClass.LoadSynchronous())
		{
			Mesh1P->SetAnimInstanceClass(Loaded);
		}
	}

	if (USkeletalMeshComponent* Mesh3P = GetMesh3P())
	{
		if (UClass* Loaded = ThirdPersonAnimClass.LoadSynchronous())
		{
			Mesh3P->SetAnimInstanceClass(Loaded);
		}
	}
}

void ABRFPSCharacter::SetAimingCamera(bool bAiming)
{
	TargetFOV = bAiming ? AimFOV : DefaultFOV;

	// Waking the tick is the ONLY thing that ever enables it. A pawn that never aims never ticks.
	SetActorTickEnabled(true);
}

void ABRFPSCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UCameraComponent* Camera = GetFirstPersonCameraComponent();
	if (!Camera)
	{
		SetActorTickEnabled(false);
		return;
	}

	const float Current = Camera->FieldOfView;

	// Settled: stop. `FInterpTo` is asymptotic and never exactly arrives, so without an explicit
	// epsilon this tick would run forever at a hundredth of a degree from its target -- which is
	// precisely the always-on per-frame callback law 4 exists to prevent, wearing a disguise.
	if (FMath::IsNearlyEqual(Current, TargetFOV, 0.01f))
	{
		Camera->SetFieldOfView(TargetFOV);
		SetActorTickEnabled(false);
		return;
	}

	Camera->SetFieldOfView(FMath::FInterpTo(Current, TargetFOV, DeltaSeconds, FOVInterpSpeed));
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

bool ABRFPSCharacter::LinkLayerOnMesh(USkeletalMeshComponent* Mesh, TSubclassOf<UAnimInstance> LayerClass,
	TSubclassOf<UAnimInstance>& InOutLinked)
{
	if (!Mesh)
	{
		InOutLinked = nullptr;
		return false;
	}

	// VERIFY, do not trust the record. LinkAnimClassLayers is void and no-ops silently when the
	// mesh has no anim instance; and any InitAnim resets the linked list without telling us. The
	// mesh is the only honest source of truth for what is actually linked.
	if (!Mesh->GetAnimInstance())
	{
		InOutLinked = nullptr;
		return false;
	}

	if (InOutLinked == LayerClass)
	{
		return true;
	}

	if (InOutLinked)
	{
		Mesh->UnlinkAnimClassLayers(InOutLinked);
	}

	if (LayerClass)
	{
		Mesh->LinkAnimClassLayers(LayerClass);
	}

	InOutLinked = LayerClass;
	return true;
}

void ABRFPSCharacter::LinkWeaponAnimLayer(TSubclassOf<UAnimInstance> LayerClass)
{
	DesiredLayerClass = LayerClass;

	// PER MESH, and each is idempotent on its own. Idempotence is not a micro-optimisation:
	// LinkAnimClassLayers on an already-linked class tears the layer instance down and rebuilds
	// it, discarding every blend and inertialised transition in flight -- so equipment
	// re-broadcasting its state would hitch the weapon pose exactly when a player is swapping
	// under fire. But it must be per-mesh, or one mesh failing to link poisons the other's guard.
	const bool bLinked1P = LinkLayerOnMesh(GetFirstPersonMesh(), LayerClass, LinkedLayer1P);
	const bool bLinked3P = LinkLayerOnMesh(GetMesh3P(), LayerClass, LinkedLayer3P);

	// A mesh that was not ready keeps `DesiredLayerClass` pending, so the next broadcast retries
	// only the one that failed. Before this, a partial link was recorded as complete and never
	// recovered -- the arms held a rifle and the body did not, until the player swapped weapons.
	if (!bLinked1P || !bLinked3P)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("BRFPSCharacter: layer link deferred (1P:%d 3P:%d) - mesh not ready."),
			bLinked1P ? 1 : 0, bLinked3P ? 1 : 0);
	}
}

void ABRFPSCharacter::UnlinkWeaponAnimLayer()
{
	DesiredLayerClass = nullptr;
	LinkLayerOnMesh(GetFirstPersonMesh(), nullptr, LinkedLayer1P);
	LinkLayerOnMesh(GetMesh3P(), nullptr, LinkedLayer3P);
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
