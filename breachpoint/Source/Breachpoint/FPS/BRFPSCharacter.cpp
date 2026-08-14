#include "FPS/BRFPSCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "Animations/BRAnimInstance.h"
#include "Animations/BRProceduralAnimComponent.h"

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

	ProceduralAnim = CreateDefaultSubobject<UBRProceduralAnimComponent>(TEXT("ProceduralAnim"));
}

void ABRFPSCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyAnimInstanceClasses();

	// Link the unarmed layer, or the locomotion state machine has no poses to play. Equipment
	// replaces this the moment it equips something; this is the empty-handed case, not a default
	// nobody revisits.
	if (UClass* LayerClass = DefaultWeaponAnimLayer.LoadSynchronous())
	{
		LinkWeaponAnimLayer(LayerClass);
	}

	TargetFOV = DefaultFOV;
	if (UCameraComponent* Camera = GetFirstPersonCameraComponent())
	{
		Camera->SetFieldOfView(DefaultFOV);
	}
}

void ABRFPSCharacter::ApplyAnimInstanceClasses()
{
	// Resolved from a SOFT class in config, so no C++ in this project names an animation asset.
	// The Shooter template was deleted for hard-referencing an AnimInstance class per weapon; this is
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

void ABRFPSCharacter::LinkWeaponAnimLayer(TSubclassOf<UAnimInstance> LayerClass)
{
	// Forwarded, not reimplemented. The component owns the whole "what weapon is up and how does
	// it move" concern; a character that kept its own copy of the linked state would be the second
	// owner of one fact, which is the defect this packet has already hit three times.
	if (UBRProceduralAnimComponent* Procedural = GetProceduralAnim())
	{
		Procedural->LinkWeaponAnimLayer(LayerClass);
	}
}

void ABRFPSCharacter::UnlinkWeaponAnimLayer()
{
	if (UBRProceduralAnimComponent* Procedural = GetProceduralAnim())
	{
		Procedural->UnlinkWeaponAnimLayer();
	}
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
