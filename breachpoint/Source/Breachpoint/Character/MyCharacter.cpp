#include "Character/MyCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/UserWidget.h"
#include "Weapons/BPWeaponBase.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMyCharacter, Log, All);

namespace
{
	/** The SCS names the Blueprint gave its components. Resolved, never created. */
	const FName N_FPSCamera(TEXT("FPSCamera"));
	const FName N_FollowCamera(TEXT("FollowCamera"));
	const FName N_FPSCam(TEXT("FPSCam"));
	const FName N_CameraBoom(TEXT("CameraBoom"));
	const FName N_Arrow(TEXT("Arrow_MeleeTraceStart"));
	const FName N_FPSCamComp(TEXT("BPC_FPSCamComp"));
	const FName N_FireTimer(TEXT("BPC_FPST_FireTimer"));
	const FName N_LineTracer(TEXT("BPC_FPST_LineTracer"));
	const FName N_FireEffect(TEXT("BPC_FPST_Lyra_FireEffectComp"));
	const FName N_ProcManager(TEXT("BPC_FPST_Procedural_Manager"));
	const FName N_AimAndLean(TEXT("BPC_FPST_Procedural_AimAndLean"));
	const FName N_PoseOffsets(TEXT("BPC_FPST_Procedural_PoseOffsets"));
	const FName N_Recoil(TEXT("BPC_FPST_Procedural_Recoil"));
	const FName N_SwayAndLag(TEXT("BPC_FPST_Procedural_SwayAndLag"));

	/**
	 * Weapon-side VARIABLES, read by reflection. None of these is a getter — BP_FPST_BaseWeapon
	 * exposes real UFUNCTIONs only for GetCurrentFireMode, NextFireMode, GetFireAnimMontage and
	 * GetReloadBoltAnimMontage. Assuming a Get* exists for the rest is the single most repeated
	 * mistake in this port; check the asset before adding a name here.
	 */
	const FName N_AttachSocketName(TEXT("AttachSocketName"));
	const FName N_LinkAnimLayerClass(TEXT("LinkAnimLayerClass"));
	const FName N_MuzzleSocketName(TEXT("MuzzleSocketName"));
	const FName N_MeleeAnimMontage(TEXT("MeleeAnimMontage"));
	const FName N_FireAnimMontage(TEXT("FireAnimMontage"));
	const FName N_AimFireAnimMontage(TEXT("AimFireAnimMontage"));
	const FName N_ReloadAnimMontage(TEXT("ReloadAnimMontage"));
	const FName N_AimReloadAnimMontage(TEXT("AimReloadAnimMontage"));

	/** The notify the graph gated the melee trace on, on AM_MM_Knife_Swing01. */
	const FName N_MeleeNotify(TEXT("AN_FPST_Melee"));

	/** E_FPST_WeaponType / crosshair / pose enumerators, by the ordinals the graph used. */
	constexpr uint8 Pose_HipStand = 1;
	constexpr uint8 Pose_AimStand = 2;
	constexpr uint8 Pose_HipCrouch = 6;
	constexpr uint8 Pose_AimCrouch = 7;
	constexpr uint8 Scope_Default = 0;
	constexpr uint8 Crosshair_Default = 0;
	constexpr uint8 Crosshair_None = 4;
	/** E_FPST_FireMode, read from the asset: three named enumerators in this order. */
	constexpr uint8 FireMode_Single = 0;
	constexpr uint8 FireMode_Auto = 1;
	constexpr uint8 FireMode_Burst = 2;

	/** Weapon-side variables and the one weapon-side getter, all resolved by name. */
	const FName N_FireDelay(TEXT("FireDelay"));
	const FName N_SpreadAngle(TEXT("SpreadAngle"));

	/**
	 * Calls a BlueprintCallable function on a Blueprint class by name.
	 *
	 * The parameter block is allocated and initialised from the UFunction's OWN property
	 * layout, and every argument is looked up by name and type-checked before it is written.
	 * That is the whole point: a hand-written mirror struct passed to ProcessEvent is silent
	 * memory corruption when the Blueprint's signature changes, not a compile error. Here a
	 * renamed or retyped parameter simply fails the Set and is reported.
	 *
	 * ponytail: reflection, because the nine BPC_FPST_* components are Blueprint classes with
	 * no C++ base. Each call site disappears the day its component is ported to C++.
	 */
	class FBPCall
	{
	public:
		FBPCall(UObject* InTarget, const TCHAR* InFuncName)
			: Target(InTarget)
			, Fn(InTarget ? InTarget->FindFunction(FName(InFuncName)) : nullptr)
		{
			if (Fn && Fn->ParmsSize > 0)
			{
				Buffer = FMemory::Malloc(Fn->ParmsSize);
				FMemory::Memzero(Buffer, Fn->ParmsSize);
				for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
				{
					It->InitializeValue_InContainer(Buffer);
				}
			}
			else if (Fn)
			{
				Buffer = nullptr;
			}
		}

		~FBPCall()
		{
			if (Fn && Buffer)
			{
				for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
				{
					It->DestroyValue_InContainer(Buffer);
				}
				FMemory::Free(Buffer);
			}
		}

		FBPCall(const FBPCall&) = delete;
		FBPCall& operator=(const FBPCall&) = delete;

		bool IsBound() const { return Fn != nullptr; }

		bool SetBool(const TCHAR* ParamName, bool Value) const
		{
			if (FBoolProperty* P = FindFProperty<FBoolProperty>(Fn, FName(ParamName)))
			{
				P->SetPropertyValue_InContainer(Buffer, Value);
				return true;
			}
			return false;
		}

		bool SetObject(const TCHAR* ParamName, UObject* Value) const
		{
			if (FObjectPropertyBase* P = FindFProperty<FObjectPropertyBase>(Fn, FName(ParamName)))
			{
				P->SetObjectPropertyValue_InContainer(Buffer, Value);
				return true;
			}
			return false;
		}

		bool SetName(const TCHAR* ParamName, FName Value) const
		{
			if (FNameProperty* P = FindFProperty<FNameProperty>(Fn, FName(ParamName)))
			{
				P->SetPropertyValue_InContainer(Buffer, Value);
				return true;
			}
			return false;
		}

		bool SetByte(const TCHAR* ParamName, uint8 Value) const
		{
			if (FByteProperty* P = FindFProperty<FByteProperty>(Fn, FName(ParamName)))
			{
				P->SetPropertyValue_InContainer(Buffer, Value);
				return true;
			}
			if (FEnumProperty* P = FindFProperty<FEnumProperty>(Fn, FName(ParamName)))
			{
				FNumericProperty* Underlying = P->GetUnderlyingProperty();
				Underlying->SetIntPropertyValue(P->ContainerPtrToValuePtr<void>(Buffer),
					static_cast<int64>(Value));
				return true;
			}
			return false;
		}

		/**
		 * A float parameter. A Blueprint "float" pin is a DOUBLE in UE5, but a function
		 * authored against a C++ float is still FFloatProperty, so both are handled --
		 * writing 4 bytes into an 8-byte slot is exactly the silent corruption this class
		 * exists to prevent.
		 */
		bool SetFloat(const TCHAR* ParamName, float Value) const
		{
			if (FDoubleProperty* P = FindFProperty<FDoubleProperty>(Fn, FName(ParamName)))
			{
				P->SetPropertyValue_InContainer(Buffer, static_cast<double>(Value));
				return true;
			}
			if (FFloatProperty* P = FindFProperty<FFloatProperty>(Fn, FName(ParamName)))
			{
				P->SetPropertyValue_InContainer(Buffer, Value);
				return true;
			}
			return false;
		}

		/** A struct parameter, checked against the UFunction's OWN struct type first. */
		bool SetStruct(const TCHAR* ParamName, UScriptStruct* Type, const void* Value) const
		{
			if (FStructProperty* P = FindFProperty<FStructProperty>(Fn, FName(ParamName)))
			{
				if (P->Struct == Type)
				{
					P->CopySingleValue(P->ContainerPtrToValuePtr<void>(Buffer), Value);
					return true;
				}
			}
			return false;
		}

		bool SetVector(const TCHAR* ParamName, const FVector& Value) const
		{
			return SetStruct(ParamName, TBaseStructure<FVector>::Get(), &Value);
		}

		bool Invoke()
		{
			if (!Fn || !Target)
			{
				return false;
			}
			Target->ProcessEvent(Fn, Buffer);
			return true;
		}

		/** The function's ReturnValue, when it is a UClass reference. */
		UClass* GetReturnClass() const
		{
			if (FClassProperty* P = FindFProperty<FClassProperty>(Fn, TEXT("ReturnValue")))
			{
				return Cast<UClass>(P->GetObjectPropertyValue_InContainer(Buffer));
			}
			return nullptr;
		}

		FName GetReturnName() const
		{
			if (FNameProperty* P = FindFProperty<FNameProperty>(Fn, TEXT("ReturnValue")))
			{
				return P->GetPropertyValue_InContainer(Buffer);
			}
			return NAME_None;
		}

		/**
		 * The function's ReturnValue as an enum ordinal. A Blueprint user-defined enum
		 * reaches C++ as either an FByteProperty or an FEnumProperty depending on how the
		 * pin was authored, so both are handled rather than guessed at.
		 */
		bool GetReturnByte(uint8& OutValue) const
		{
			if (const FByteProperty* P = FindFProperty<FByteProperty>(Fn, TEXT("ReturnValue")))
			{
				OutValue = P->GetPropertyValue_InContainer(Buffer);
				return true;
			}
			if (const FEnumProperty* P = FindFProperty<FEnumProperty>(Fn, TEXT("ReturnValue")))
			{
				const FNumericProperty* Underlying = P->GetUnderlyingProperty();
				const void* Addr = P->ContainerPtrToValuePtr<void>(Buffer);
				OutValue = static_cast<uint8>(Underlying->GetSignedIntPropertyValue(Addr));
				return true;
			}
			return false;
		}

	private:
		UObject* Target = nullptr;
		UFunction* Fn = nullptr;
		void* Buffer = nullptr;
	};
}

AMyCharacter::AMyCharacter()
{
	// Law 4. EventTick's per-frame trace became AimTraceTimer; nothing here needs a tick.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// No CreateDefaultSubobject calls, by design — see the class comment. The Blueprint
	// owns the component tree, and assigning Mesh from here is what T-poses the character.
}

UActorComponent* AMyCharacter::FindNamedComponent(FName ComponentName) const
{
	for (UActorComponent* Component : GetComponents())
	{
		if (Component && Component->GetFName() == ComponentName)
		{
			return Component;
		}
	}
	return nullptr;
}

void AMyCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Name first, then class.
	//
	// These cached pointers are all Cached*-prefixed for one non-obvious reason: a Blueprint
	// CANNOT name an SCS component after a property its parent C++ class already declares.
	// While this class owned a property literally called FPSCamera, adding an "FPSCamera"
	// component to a child silently produced one called "Camera" instead — no warning, and
	// name-based lookup then missed it. Prefixing frees the names the Blueprint wants.
	// The class fallback below stays as a safety net for Blueprints authored before that.
	CachedFPSCamera = Cast<UCameraComponent>(FindNamedComponent(N_FPSCamera));
	CachedFollowCamera = Cast<UCameraComponent>(FindNamedComponent(N_FollowCamera));
	CachedFPSCam = Cast<UCameraComponent>(FindNamedComponent(N_FPSCam));
	CachedCameraBoom = Cast<USpringArmComponent>(FindNamedComponent(N_CameraBoom));
	CachedArrowMeleeTraceStart = Cast<UArrowComponent>(FindNamedComponent(N_Arrow));

	if (!CachedFPSCamera && !CachedFPSCam)
	{
		// Whichever camera exists becomes the view camera. Three cameras of one class cannot
		// be told apart by type, so a hand-named one always wins the lookup above.
		CachedFPSCamera = FindComponentByClass<UCameraComponent>();
	}
	if (!CachedCameraBoom)
	{
		CachedCameraBoom = FindComponentByClass<USpringArmComponent>();
	}
	if (!CachedArrowMeleeTraceStart)
	{
		CachedArrowMeleeTraceStart = FindComponentByClass<UArrowComponent>();
	}

	FPSCamComp = FindNamedComponent(N_FPSCamComp);
	FireTimerComp = FindNamedComponent(N_FireTimer);
	LineTracerComp = FindNamedComponent(N_LineTracer);
	FireEffectComp = FindNamedComponent(N_FireEffect);
	ProceduralManagerComp = FindNamedComponent(N_ProcManager);
	AimAndLeanComp = FindNamedComponent(N_AimAndLean);
	PoseOffsetsComp = FindNamedComponent(N_PoseOffsets);
	RecoilComp = FindNamedComponent(N_Recoil);
	SwayAndLagComp = FindNamedComponent(N_SwayAndLag);

	// Named once, at load, rather than per call site. A missing component means the
	// Blueprint's construction script changed, which is a real finding, not a warning
	// to live with.
	const TArray<TPair<FName, const UObject*>> Required = {
		{N_FPSCamComp, FPSCamComp}, {N_FireTimer, FireTimerComp},
		{N_LineTracer, LineTracerComp}, {N_FireEffect, FireEffectComp},
		{N_ProcManager, ProceduralManagerComp}, {N_AimAndLean, AimAndLeanComp},
		{N_PoseOffsets, PoseOffsetsComp}, {N_Recoil, RecoilComp},
		{N_SwayAndLag, SwayAndLagComp},
	};
	for (const TPair<FName, const UObject*>& Pair : Required)
	{
		UE_CLOG(Pair.Value == nullptr, LogMyCharacter, Warning,
			TEXT("MyCharacter: Blueprint component '%s' not found — its hook will no-op."),
			*Pair.Key.ToString());
	}
}

// =====================================================================================
// BeginPlay. Order is the graph's Sequence, then_0 .. then_3, verbatim.
// =====================================================================================

void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// then_0 — CastToPlayerController -> GetEnhancedInputLocalPlayerSubsystem -> AddMappingContext(prio 0)
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (UInputMappingContext* Context = DefaultMappingContext.LoadSynchronous())
			{
				// DEVIATION FROM 1:1, forced by an integration the template never had.
				//
				// The graph added this at priority 0. In the template's world nothing else
				// added a context, so 0 was fine. Here ABPPlayerController::BeginPlay adds
				// its OWN two contexts (the BR DefaultMappingContext and MouseLookMappingContext)
				// at priority 0 as well, and they map the same physical keys to DIFFERENT
				// actions. At equal priority those shadow IMC_FPST_Controls, the IA_FPST_*
				// actions bound in SetupPlayerInputComponent never trigger, and the pawn is
				// input-dead with nothing logged.
				//
				// Higher number wins, so the possessed character's own context outranks the
				// controller's generic one. Config-overridable rather than a literal.
				Subsystem->AddMappingContext(Context, MappingContextPriority);
				UE_LOG(LogMyCharacter, Log,
					TEXT("MyCharacter: added '%s' at priority %d."),
					*Context->GetName(), MappingContextPriority);
			}
			else
			{
				UE_LOG(LogMyCharacter, Warning,
					TEXT("MyCharacter: DefaultMappingContext unset — pin it under [/Script/Breachpoint.MyCharacter]."));
			}
		}
	}

	// then_1 — CreateWeapons -> HideAllWeapons -> unhide current -> LinkAnimClassLayers
	//          -> ChangeInfo -> FireTimer.StartComp(Mesh)
	CreateWeapons();
	SelectStartupWeapon();
	HideAllWeapons();
	if (ABPWeaponBase* Current = GetCurrentWeapon())
	{
		Current->SetActorHiddenInGame(false);
	}
	LinkWeaponAnimLayers(/*bUnarmed=*/false);
	ChangeProceduralInfo(NAME_None);
	StartFireTimerComp();

	// then_2 — CreateWidget -> SetHudWidget -> AddToViewport(0) -> ShowCrosshair
	// The graph created the widget directly. That bypasses BRUIManagerSubsystem, which owns
	// the layer stack, so it is NOT reproduced here; the HUD lands when the UI ticket wires
	// this character into the stack. Crosshair state is still driven, via the hook.
	ShowCrosshair(Crosshair_Default, /*bAiming=*/false);

	// then_3 — BindEventToEventDispatcher(Fire). The dispatcher lives on BPC_FPST_FireTimer;
	// FireEvent() below is the handler it binds to once that component is ported.

	GetWorldTimerManager().SetTimer(AimTraceTimer, this, &AMyCharacter::AimTraceTick,
		AimTraceInterval, /*bLoop=*/true);

	// A POSITIVE assertion, not a warning. Everything else this class logs fires only on
	// failure, which makes a silent log ambiguous: it reads the same whether the port worked
	// or the pawn never spawned at all. This line is the difference, and a standalone run
	// that does not contain it did not run this character.
	const ABPWeaponBase* Startup = GetCurrentWeapon();
	const USkeletalMeshComponent* MeshComp = GetMesh();
	const UAnimInstance* Anim = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	UE_LOG(LogMyCharacter, Log,
		TEXT("MyCharacter READY: pawn=%s mesh=%s anim=%s animIface=%s weapons=%d/%d "
			 "startType=%u current=%s socket=%s canCrouch=%s"),
		*GetName(),
		MeshComp ? *GetNameSafe(MeshComp->GetSkeletalMeshAsset()) : TEXT("NONE"),
		*GetNameSafe(Anim ? Anim->GetClass() : nullptr),
		(Anim && Anim->FindFunction(TEXT("SetSprinting"))) ? TEXT("YES") : TEXT("NO"),
		AvailableWeapons.Num(), StartupWeaponClasses.Num(),
		GetCurrentWeaponType(),
		*GetNameSafe(Startup),
		*GetWeaponAttachSocket(Startup).ToString(),
		(GetCharacterMovement() && GetCharacterMovement()->CanEverCrouch()) ? TEXT("YES") : TEXT("NO"));
}

void AMyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AimTraceTimer);
	GetWorldTimerManager().ClearTimer(GrenadeThrowTimer);
	Super::EndPlay(EndPlayReason);
}

// =====================================================================================
// EventTick, as a timer.
// =====================================================================================

void AMyCharacter::AimTraceTick()
{
	// LineTracer.Trace(cam location, cam forward, 120) -> HUD dot red on hit, default off.
	// SwayAndLag.Update(bIsAiming) and Recoil.ApplyTickValueToController(PC) followed it;
	// both are component hooks and land with their components.
	const UCameraComponent* Cam = CachedFPSCamera ? CachedFPSCamera.Get() : CachedFPSCam.Get();
	if (!Cam)
	{
		return;
	}

	FHitResult Hit;
	const bool bHit = WeaponTrace(Cam->GetComponentLocation(), Cam->GetForwardVector(),
		AimTraceDistance, Hit);
	// HudWidget->ChangeTargetDotColor(bHit ? red : default) — with the UI ticket.
	(void)bHit;
}

// =====================================================================================
// The eleven functions.
// =====================================================================================

void AMyCharacter::CreateWeapons()
{
	AllWeapons.Reset();
	AvailableWeapons.Reset();

	// ForEach E_FPST_WeaponType added an empty slot per enumerator; CreateWeapon then wrote
	// into slot (int)Type, so the array must be sized before the first write.
	AllWeapons.SetNum(FMath::Max(StartupWeaponClasses.Num() + 1, 5));

	for (int32 Index = 0; Index < StartupWeaponClasses.Num(); ++Index)
	{
		// Graph order: Pistol, Rifle, Shotgun, Knife — enumerators 1..4.
		if (UClass* WeaponClass = StartupWeaponClasses[Index].TryLoadClass<ABPWeaponBase>())
		{
			CreateWeapon(static_cast<uint8>(Index + 1), WeaponClass);
		}
		else
		{
			UE_LOG(LogMyCharacter, Error, TEXT("MyCharacter: StartupWeaponClasses[%d] '%s' failed to load."),
				Index, *StartupWeaponClasses[Index].ToString());
		}
	}
}

void AMyCharacter::SelectStartupWeapon()
{
	// The graph's BeginPlay carries a node group commented "Rifle - Start Weapon" whose enum
	// pin literal is E_FPST_WeaponType::NewEnumerator2 — Rifle. CreateWeapons alone leaves
	// CurrentWeaponIndex at 0, which is the FIRST SPAWNED weapon (the Pistol, because the
	// config lists Pistol first), not the start weapon. Without this the character spawns
	// holding the pistol.
	if (AvailableWeapons.Num() == 0)
	{
		return;
	}

	const int32 Found = AvailableWeapons.IndexOfByKey(StartupWeaponType);
	if (Found != INDEX_NONE)
	{
		CurrentWeaponIndex = Found;
		return;
	}

	CurrentWeaponIndex = 0;
	UE_LOG(LogMyCharacter, Warning,
		TEXT("MyCharacter: StartupWeaponType %u is not among the %d spawned weapons — falling "
			 "back to slot 0. Check StartupWeaponClasses order against E_FPST_WeaponType."),
		StartupWeaponType, AvailableWeapons.Num());
}

FName AMyCharacter::GetWeaponAttachSocket(const ABPWeaponBase* InWeapon) const
{
	if (!InWeapon)
	{
		return NAME_None;
	}

	// Same doctrine as FBPCall: find the member by NAME off the reflection data rather than
	// casting to a Blueprint type. A renamed or deleted property yields NAME_None and the
	// warning at the call site, never a bad read.
	if (const FNameProperty* Prop =
			FindFProperty<FNameProperty>(InWeapon->GetClass(), N_AttachSocketName))
	{
		return Prop->GetPropertyValue_InContainer(InWeapon);
	}
	return NAME_None;
}

void AMyCharacter::CreateWeapon(uint8 InWeaponType, TSubclassOf<ABPWeaponBase> InWeaponClass)
{
	if (!InWeaponClass)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	Params.Instigator = this;

	ABPWeaponBase* Weapon = GetWorld()->SpawnActor<ABPWeaponBase>(InWeaponClass, FTransform::Identity, Params);
	if (!Weapon)
	{
		return;
	}

	// AttachActorToComponent(Mesh, AttachSocketName, SnapToTarget, SnapToTarget).
	//
	// AttachSocketName is a VARIABLE on BP_FPST_BaseWeapon (display name "Attach Socket
	// Name"), not a getter — there is no GetAttachSocketName function on that asset, and the
	// graph read the property directly. Its base default is the Manny skeleton's `hand_r`
	// bone; the four children inherit it. Read by reflection, because a hard reference to a
	// Blueprint class is banned (law 3).
	//
	// Passing NAME_None here is what left the weapon at the mesh ROOT — under the character's
	// feet rather than in its hand. Nothing about that failure is loud, so it is logged.
	const FName SocketName = GetWeaponAttachSocket(Weapon);
	Weapon->AttachToComponent(GetMesh(),
		FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget,
			EAttachmentRule::KeepWorld, /*bWeldSimulatedBodies=*/false),
		SocketName);

	if (SocketName.IsNone())
	{
		UE_LOG(LogMyCharacter, Warning,
			TEXT("MyCharacter: '%s' has no AttachSocketName — attached at the mesh root, which "
				 "puts it at the character's feet, not in its hand."),
			*GetNameSafe(Weapon));
	}
	else if (GetMesh() && !GetMesh()->DoesSocketExist(SocketName))
	{
		UE_LOG(LogMyCharacter, Warning,
			TEXT("MyCharacter: AttachSocketName '%s' on '%s' is neither a socket nor a bone on "
				 "mesh '%s' — the weapon falls back to the mesh root."),
			*SocketName.ToString(), *GetNameSafe(Weapon), *GetNameSafe(GetMesh()->GetSkeletalMeshAsset()));
	}

	const int32 Slot = static_cast<int32>(InWeaponType);
	if (AllWeapons.IsValidIndex(Slot))
	{
		AllWeapons[Slot] = Weapon;
	}
	AvailableWeapons.Add(InWeaponType);
}

void AMyCharacter::HideAllWeapons()
{
	for (ABPWeaponBase* Weapon : AllWeapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->SetActorHiddenInGame(true);
		}
	}
}

ABPWeaponBase* AMyCharacter::GetCurrentWeapon() const
{
	if (!AvailableWeapons.IsValidIndex(CurrentWeaponIndex))
	{
		return nullptr;
	}
	const int32 Slot = static_cast<int32>(AvailableWeapons[CurrentWeaponIndex]);
	return AllWeapons.IsValidIndex(Slot) ? AllWeapons[Slot].Get() : nullptr;
}

uint8 AMyCharacter::GetCurrentWeaponType() const
{
	return AvailableWeapons.IsValidIndex(CurrentWeaponIndex) ? AvailableWeapons[CurrentWeaponIndex] : 0;
}

void AMyCharacter::NextWeapon()
{
	if (AvailableWeapons.Num() == 0)
	{
		return;
	}
	CurrentWeaponIndex = (CurrentWeaponIndex + 1) % AvailableWeapons.Num();
}

void AMyCharacter::PrevWeapon()
{
	if (AvailableWeapons.Num() == 0)
	{
		return;
	}
	// The graph decremented, took the modulo, then added Length back if it went negative.
	CurrentWeaponIndex = (CurrentWeaponIndex - 1) % AvailableWeapons.Num();
	if (CurrentWeaponIndex < 0)
	{
		CurrentWeaponIndex += AvailableWeapons.Num();
	}
}

uint8 AMyCharacter::GetCrosshairType() const
{
	// Valid weapon -> the weapon's own crosshair; invalid -> enumerator 4.
	return IsValid(GetCurrentWeapon()) ? Crosshair_Default : Crosshair_None;
}

uint8 AMyCharacter::GetCurrentWeaponScopeType() const
{
	// Valid weapon -> BPFPSTBaseScope::GetScopeType; invalid -> enumerator 0.
	return Scope_Default;
}

bool AMyCharacter::CanJumpInternal_Implementation() const
{
	return Super::CanJumpInternal_Implementation();
}

// =====================================================================================
// Input.
// =====================================================================================

void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogMyCharacter, Error, TEXT("MyCharacter: InputComponent is not EnhancedInput."));
		return;
	}

	auto Bind = [Input, this](const TSoftObjectPtr<UInputAction>& Action, ETriggerEvent Event,
			void (AMyCharacter::*Func)())
	{
		if (UInputAction* Loaded = Action.LoadSynchronous())
		{
			Input->BindAction(Loaded, Event, this, Func);
		}
	};

	if (UInputAction* Move = MoveAction.LoadSynchronous())
	{
		Input->BindAction(Move, ETriggerEvent::Triggered, this, &AMyCharacter::OnMove);
	}
	if (UInputAction* Look = LookAction.LoadSynchronous())
	{
		Input->BindAction(Look, ETriggerEvent::Triggered, this, &AMyCharacter::OnLook);
	}

	Bind(JumpAction, ETriggerEvent::Started, &AMyCharacter::OnJumpStarted);
	Bind(JumpAction, ETriggerEvent::Completed, &AMyCharacter::OnJumpCompleted);
	Bind(CrouchAction, ETriggerEvent::Triggered, &AMyCharacter::OnCrouchTriggered);
	Bind(SprintAction, ETriggerEvent::Started, &AMyCharacter::OnSprintStarted);
	Bind(SprintAction, ETriggerEvent::Completed, &AMyCharacter::OnSprintCompleted);
	Bind(AimAction, ETriggerEvent::Started, &AMyCharacter::OnAimStarted);
	Bind(AimAction, ETriggerEvent::Completed, &AMyCharacter::OnAimCompleted);
	Bind(FireAction, ETriggerEvent::Started, &AMyCharacter::OnFireStarted);
	Bind(FireAction, ETriggerEvent::Completed, &AMyCharacter::OnFireCompleted);
	Bind(ReloadAction, ETriggerEvent::Triggered, &AMyCharacter::OnReload);

	// The graph bound these to raw keys rather than to input actions. Kept as raw keys so
	// the transfer is one for one; they become IA_* assets when the input ticket lands.
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AMyCharacter::OnLeanLeftPressed);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Released, this, &AMyCharacter::OnLeanLeftReleased);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AMyCharacter::OnLeanRightPressed);
	PlayerInputComponent->BindKey(EKeys::E, IE_Released, this, &AMyCharacter::OnLeanRightReleased);
	PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &AMyCharacter::OnGrenadeKey);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AMyCharacter::OnMeleeKey);
	PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &AMyCharacter::OnNextFireModeKey);
	PlayerInputComponent->BindKey(EKeys::X, IE_Pressed, this, &AMyCharacter::OnSwapNext);
	PlayerInputComponent->BindKey(EKeys::Z, IE_Pressed, this, &AMyCharacter::OnSwapPrev);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AMyCharacter::OnSwapNext);
	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AMyCharacter::OnSwapPrev);
}

void AMyCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	RightAxisValue = Axis.X;
	ForwardAxisValue = Axis.Y;

	AddMovementInput(GetActorRightVector(), RightAxisValue);
	AddMovementInput(GetActorForwardVector(), ForwardAxisValue);
}

void AMyCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	TurnAxisValue = Axis.X;
	LookupAxisValue = Axis.Y;

	AddControllerYawInput(TurnAxisValue * LookSensitivity);
	AddControllerPitchInput(LookupAxisValue * LookSensitivity);
}

void AMyCharacter::OnJumpStarted()
{
	// Started: if crouched, UnCrouch first, then Jump either way.
	if (bIsCrouched)
	{
		UnCrouch();
	}
	Jump();
}

void AMyCharacter::OnJumpCompleted()
{
	StopJumping();
}

void AMyCharacter::OnCrouchTriggered()
{
	// Verified condition: (!bIsCrouched AND !IsFalling) ? Crouch : UnCrouch.
	// The IsFalling term matters — pressing crouch mid-air UNcrouches, it does not crouch.
	// A plain bIsCrouched toggle gets that case wrong.
	const bool bCanCrouchNow = !bIsCrouched
		&& !(GetCharacterMovement() && GetCharacterMovement()->IsFalling());
	if (bCanCrouchNow)
	{
		Crouch();
		ChangePose(bIsAiming ? Pose_AimCrouch : Pose_HipCrouch, Scope_Default, CrouchPoseChangeSpeed);
	}
	else
	{
		UnCrouch();
		ChangePose(bIsAiming ? Pose_AimStand : Pose_HipStand, Scope_Default, CrouchPoseChangeSpeed);
	}
}

void AMyCharacter::OnSprintStarted()
{
	// Verified condition: the sprint branch is gated on ForwardAxisValue > 0 and has no else.
	// Sprinting sideways or backwards is not a thing; without this you can.
	if (ForwardAxisValue <= 0.f)
	{
		return;
	}
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintWalkSpeed;
	}
	SetSprinting(true);
}

void AMyCharacter::OnSprintCompleted()
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed;
	}
	SetSprinting(false);
}

void AMyCharacter::OnAimStarted()
{
	bIsAiming = true;
	SetADSUpper(true);
	SetADS(true);
	ShowCrosshair(GetCrosshairType(), /*bAiming=*/true);
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = AimWalkSpeed;
	}
	ChangePose(bIsCrouched ? Pose_AimCrouch : Pose_AimStand, GetCurrentWeaponScopeType(), AimPoseChangeSpeed);
	ChangeCameraTargetFOV(AimFOV, FOVChangeSpeed);
}

void AMyCharacter::OnAimCompleted()
{
	bIsAiming = false;
	SetADSUpper(false);
	SetADS(false);
	ShowCrosshair(GetCrosshairType(), /*bAiming=*/false);
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed;
	}
	ChangePose(bIsCrouched ? Pose_HipCrouch : Pose_HipStand, GetCurrentWeaponScopeType(), AimPoseChangeSpeed);
	ChangeCameraTargetFOV(HipFOV, FOVChangeSpeed);
}

void AMyCharacter::OnLeanLeftPressed() { SetLeaning(-1.f); }
void AMyCharacter::OnLeanLeftReleased() { SetLeaning(0.f); }
void AMyCharacter::OnLeanRightPressed() { SetLeaning(1.f); }
void AMyCharacter::OnLeanRightReleased() { SetLeaning(0.f); }

void AMyCharacter::OnGrenadeKey()
{
	// Sequence then_0: set location + direction, Delay 0.2, SpawnActor.
	// Sequence then_1: the throw montage, immediately and in parallel with the delay.
	GrenadeSpawnLocation = GetActorLocation() + GetActorForwardVector() * 50.f + GetActorUpVector() * 50.f;
	GrenadeSpawnDirection = GetControlRotation();

	// then_1 first in wall-clock terms: the montage does not wait on the delay.
	// PlayMontage(Mesh, throw montage) — with the animation ticket.

	FTimerDelegate Throw = FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UClass* Class = GrenadeClass.TryLoadClass<AActor>();
		if (!Class)
		{
			UE_LOG(LogMyCharacter, Warning, TEXT("MyCharacter: GrenadeClass unset or unloadable."));
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Instigator = this;
		Params.Owner = this;
		GetWorld()->SpawnActor<AActor>(Class,
			FTransform(GrenadeSpawnDirection, GrenadeSpawnLocation), Params);
	});
	GetWorldTimerManager().SetTimer(GrenadeThrowTimer, Throw, GrenadeThrowDelay, /*bLoop=*/false);
}

// =====================================================================================
// Weapon graph.
// =====================================================================================

void AMyCharacter::OnFireStarted()
{
	if (!IsValid(GetCurrentWeapon()))
	{
		return;
	}
	// Armed -> FireTimer.Start(mode 0). Unarmed/knife -> straight to melee.
	// Verified shape: AND of two enum inequalities -> StartFire; else an Equal(Enum) -> melee.
	// Operands unresolved, so the armed test uses the named ordinal.
	if (GetCurrentWeaponType() != UnarmedWeaponType)
	{
		// The graph read both off the weapon: GetCurrentFireMode picks the arm, FireDelay
		// sets the cadence. Passing a fixed 0 here is what made every weapon fire once.
		StartFire(GetWeaponFireMode(), GetWeaponFireDelay());
	}
	else
	{
		MeleeAttack();
	}
}

void AMyCharacter::OnFireCompleted()
{
	StopFire();
	ReturnCameraRecoil();
}

void AMyCharacter::OnReload()
{
	if (!IsValid(GetCurrentWeapon()))
	{
		return;
	}
	// PlayMontage(Mesh, ReloadAnimMontage) -> PlayAnim(WeaponReloadAnimSeq, false)
	PlayWeaponMontage(bIsAiming ? N_AimReloadAnimMontage : N_ReloadAnimMontage);
	PlayWeaponAnim(/*bLooping=*/false);
}

void AMyCharacter::OnNextFireModeKey()
{
	if (IsValid(GetCurrentWeapon()))
	{
		NextFireMode();
	}
}

void AMyCharacter::OnMeleeKey()
{
	MeleeAttack();
}

void AMyCharacter::OnSwapNext() { SwapWeapon(/*bNext=*/true); }
void AMyCharacter::OnSwapPrev() { SwapWeapon(/*bNext=*/false); }

void AMyCharacter::SwapWeapon(bool bNext)
{
	// X, Z, MouseWheelUp and MouseWheelDown each carried this chain twice — eight copies
	// of one routine. The outer branch was "currently armed?", which only decides whether
	// the holster montage plays first.
	const bool bWasArmed = IsValid(GetCurrentWeapon());
	if (bWasArmed)
	{
		// PlayMontage(Mesh, HolsterAnimMontage) — with the animation ticket.
	}

	bNext ? NextWeapon() : PrevWeapon();

	// Verified: the graph tested Equal(Enum) on the weapon TYPE, not the actor's validity.
	// Those differ — a valid actor can occupy the unarmed slot, and an IsValid test then
	// takes the armed arm and tries to unholster nothing.
	const bool bNowUnarmed = (GetCurrentWeaponType() == UnarmedWeaponType);
	if (bNowUnarmed)
	{
		LinkWeaponAnimLayers(/*bUnarmed=*/true);
		ChangeProceduralInfo(FName(TEXT("ABP_UnarmedAnimLayers")));
		HideAllWeapons();
		SetUnarmed(true);
		ShowCrosshair(Crosshair_Default, /*bAiming=*/false);
	}
	else
	{
		LinkWeaponAnimLayers(/*bUnarmed=*/false);
		ChangeProceduralInfo(NAME_None);
		HideAllWeapons();
		if (ABPWeaponBase* Current = GetCurrentWeapon())
		{
			Current->SetActorHiddenInGame(false);
		}
		ShowCrosshair(Crosshair_Default, /*bAiming=*/false);
		SetUnarmed(false);
		// PlayMontage(Mesh, UnholsterAnimMontage) — with the animation ticket.
	}
}

// -------------------------------------------------------------------------------------
// BPI_FPST_AnimInterface — how the character talks to the AnimBP.
//
// The template does NOT push state through a component and it does NOT have the AnimBP
// cast back to the character and pull. It is a Blueprint INTERFACE implemented by
// ABP_Mannequin_Base, and the character sends messages to Mesh->GetAnimInstance():
//
//   SetSprinting(InSprinting) · SetADS(InADS) · SetADS_Upper(InADS_Upper)
//   SetUnarmed(InUnarmed) · SetFPSMode · SetFPSWalkMode · SetControllerPitch
//   SetAimAndLeanInfo · SetPoseTransform · SetMontagePoseOffset · SetFPSPelvisWeight …
//
// ABP_Mannequin_Base stores each into a variable; ABP_ItemAnimLayersBase and its
// per-weapon children read those to choose the pose — `Sprinting` is what selects the
// fPS_Sprint pose slot. An interface function implemented by a Blueprint IS a real
// UFUNCTION on the generated class, so FindFunction resolves it and no hard reference to
// any Blueprint type is needed (law 3).
//
// Only the bool-taking messages are wired here. The struct-taking ones
// (SetAimAndLeanInfo, SetPoseTransform, SetProcApplyTransform) carry S_Procedural_*
// user-defined structs whose layouts would have to be mirrored to be passed, and a wrong
// mirror is silent corruption — they land with their procedural components.
// -------------------------------------------------------------------------------------

bool AMyCharacter::SendAnimInterfaceBool(const TCHAR* FunctionName, const TCHAR* ParamName,
	bool bValue)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* Anim = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return false;
	}

	FBPCall Call(Anim, FunctionName);
	if (!Call.IsBound())
	{
		if (!bAnimInterfaceWarned)
		{
			bAnimInterfaceWarned = true;
			UE_LOG(LogMyCharacter, Warning,
				TEXT("MyCharacter: AnimInstance '%s' has no '%s' — it does not implement "
					 "BPI_FPST_AnimInterface, so sprint/ADS/unarmed state never reaches the "
					 "AnimBP and those animations cannot play."),
				*GetNameSafe(Anim->GetClass()), FunctionName);
		}
		return false;
	}

	if (!Call.SetBool(ParamName, bValue))
	{
		UE_LOG(LogMyCharacter, Warning,
			TEXT("MyCharacter: '%s' has no bool parameter named '%s' — the interface pin was "
				 "renamed. Nothing was sent."),
			FunctionName, ParamName);
		return false;
	}

	return Call.Invoke();
}

UClass* AMyCharacter::GetWeaponClassVar(const ABPWeaponBase* InWeapon, FName VarName) const
{
	if (!InWeapon)
	{
		return nullptr;
	}
	// Not named 'Owner': AActor already has a member by that name and shadowing it is a
	// C4458, which this project builds as an error.
	UClass* WeaponClass = InWeapon->GetClass();

	// Authored as a hard class ref most of the time, but a soft one is legal on the same pin.
	if (const FClassProperty* Hard = FindFProperty<FClassProperty>(WeaponClass, VarName))
	{
		return Cast<UClass>(Hard->GetObjectPropertyValue_InContainer(InWeapon));
	}
	if (const FSoftClassProperty* Soft = FindFProperty<FSoftClassProperty>(WeaponClass, VarName))
	{
		const FSoftObjectPtr& Ptr = Soft->GetPropertyValue_InContainer(InWeapon);
		return Cast<UClass>(Ptr.LoadSynchronous());
	}
	return nullptr;
}

UAnimMontage* AMyCharacter::GetWeaponMontage(const ABPWeaponBase* InWeapon, FName VarName) const
{
	if (!InWeapon)
	{
		return nullptr;
	}
	if (const FObjectPropertyBase* Prop =
			FindFProperty<FObjectPropertyBase>(InWeapon->GetClass(), VarName))
	{
		return Cast<UAnimMontage>(Prop->GetObjectPropertyValue_InContainer(InWeapon));
	}
	return nullptr;
}

bool AMyCharacter::PlayWeaponMontage(FName VarName)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* Anim = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return false;
	}

	UAnimMontage* Montage = GetWeaponMontage(GetCurrentWeapon(), VarName);
	if (!Montage)
	{
		return false;
	}

	// Bind once. The melee trace is driven by this montage's notify, not by the input event.
	if (!bMontageNotifyBound)
	{
		Anim->OnPlayMontageNotifyBegin.AddDynamic(this, &AMyCharacter::HandleMontageNotifyBegin);
		bMontageNotifyBound = true;
	}

	return Anim->Montage_Play(Montage) > 0.f;
}

void AMyCharacter::HandleMontageNotifyBegin(FName NotifyName,
	const FBranchingPointNotifyPayload& Payload)
{
	// The graph hung the melee trace off PlayMontage's OnNotifyBegin gated on the notify NAME
	// being exactly AN_FPST_Melee. Any other notify on any other montage is not ours.
	if (NotifyName == N_MeleeNotify)
	{
		MeleeTrace();
	}
}

void AMyCharacter::SetSprinting(bool bSprinting)
{
	SendAnimInterfaceBool(TEXT("SetSprinting"), TEXT("InSprinting"), bSprinting);
}

void AMyCharacter::SetADS(bool bADS)
{
	SendAnimInterfaceBool(TEXT("SetADS"), TEXT("InADS"), bADS);
}

void AMyCharacter::SetADSUpper(bool bADSUpper)
{
	// The interface spells this one with an underscore; the C++ hook does not.
	SendAnimInterfaceBool(TEXT("SetADS_Upper"), TEXT("InADS_Upper"), bADSUpper);
}

void AMyCharacter::SetUnarmed(bool bUnarmed)
{
	SendAnimInterfaceBool(TEXT("SetUnarmed"), TEXT("InUnarmed"), bUnarmed);
}

// -------------------------------------------------------------------------------------
// BPC_FPST_Procedural_PoseOffsets.ChangePose and BPC_FPSCamComp.ChangeCameraTargetFOV.
// These two are what make ADS read as ADS: the weapon pose interpolates to the aim offset
// and the camera FOV narrows. Both were empty hooks, so aiming changed the walk speed and
// the anim-interface bools and nothing visibly moved.
// -------------------------------------------------------------------------------------

void AMyCharacter::ChangePose(uint8 PoseType, uint8 ScopeType, float ChangeSpeed)
{
	if (!PoseOffsetsComp)
	{
		return;
	}
	FBPCall Call(PoseOffsetsComp, TEXT("ChangePose"));
	if (!Call.IsBound())
	{
		return;
	}
	Call.SetByte(TEXT("InPoseType"), PoseType);
	Call.SetByte(TEXT("InScopeType"), ScopeType);
	Call.SetFloat(TEXT("InChangeSpeed"), ChangeSpeed);
	Call.Invoke();
}

void AMyCharacter::ChangeCameraTargetFOV(float TargetFOV, float Speed)
{
	if (!FPSCamComp)
	{
		return;
	}
	FBPCall Call(FPSCamComp, TEXT("ChangeCameraTargetFOV"));
	if (!Call.IsBound())
	{
		return;
	}
	Call.SetFloat(TEXT("InTargetFov"), TargetFOV);
	Call.SetFloat(TEXT("InSpeed"), Speed);
	Call.Invoke();
}

/** The AnimBP's current FPSMode, via BPI_FPST_AnimInterface. Non-zero = first person. */
uint8 AMyCharacter::GetAnimFPSMode() const
{
	const USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* Anim = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return 0;
	}
	FBPCall Call(Anim, TEXT("GetFPSMode"));
	uint8 Mode = 0;
	if (Call.IsBound() && Call.Invoke() && Call.GetReturnByte(Mode))
	{
		return Mode;
	}
	return 0;
}

/**
 * The muzzle, for the tracer. `MuzzleSocketName` is a VARIABLE on BP_FPST_BaseWeapon and the
 * socket lives on the WEAPON's own SkeletalMesh component, not on the character. Falls back
 * to the weapon's actor transform, then the camera, so a tracer always has a start.
 */
FTransform AMyCharacter::GetMuzzleTransform() const
{
	if (const ABPWeaponBase* Weapon = GetCurrentWeapon())
	{
		FName MuzzleName = NAME_None;
		if (const FNameProperty* Prop =
				FindFProperty<FNameProperty>(Weapon->GetClass(), N_MuzzleSocketName))
		{
			MuzzleName = Prop->GetPropertyValue_InContainer(Weapon);
		}
		if (const USkeletalMeshComponent* WeaponMesh =
				Weapon->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (!MuzzleName.IsNone() && WeaponMesh->DoesSocketExist(MuzzleName))
			{
				return WeaponMesh->GetSocketTransform(MuzzleName);
			}
			return WeaponMesh->GetComponentTransform();
		}
		return Weapon->GetActorTransform();
	}

	if (const UCameraComponent* Cam = CachedFPSCamera ? CachedFPSCamera.Get() : CachedFPSCam.Get())
	{
		return Cam->GetComponentTransform();
	}
	return GetActorTransform();
}

// -------------------------------------------------------------------------------------
// BPC_FPST_Lyra_FireEffectComp — the visible bullet.
//
// The template's fire is HITSCAN: there is no projectile actor anywhere in it. What you
// see as a bullet is the tracer Niagara system this component spawns from the muzzle to
// the hit point, plus the surface-typed impact FX/decal/sound at the far end. Both are
// real UFUNCTIONs on the component (FireTracerEffect, ImpactEffect), so they are called
// by reflection with their own pin names rather than reimplemented here — the Niagara
// assets and the ImpactEffectInfoMap stay on the Blueprint where they belong (law 3).
// -------------------------------------------------------------------------------------

void AMyCharacter::FireTracerEffect(const FVector& HitLocation)
{
	if (!FireEffectComp)
	{
		return;
	}
	FBPCall Call(FireEffectComp, TEXT("FireTracerEffect"));
	if (!Call.IsBound())
	{
		return;
	}
	// BOTH ends. The tracer is drawn muzzle -> hit point: passing only InHitLocation left
	// InMuzzleTransform at IDENTITY, which starts every tracer at the WORLD ORIGIN.
	const FTransform Muzzle = GetMuzzleTransform();
	Call.SetStruct(TEXT("InMuzzleTransform"), TBaseStructure<FTransform>::Get(), &Muzzle);
	Call.SetVector(TEXT("InHitLocation"), HitLocation);
	Call.SetVector(TEXT("InFireLocation"), Muzzle.GetLocation());
	Call.SetVector(TEXT("InFireDirection"), Muzzle.GetUnitAxis(EAxis::X));
	Call.Invoke();
}

void AMyCharacter::ImpactEffect(const FHitResult& Hit)
{
	if (!FireEffectComp)
	{
		return;
	}
	FBPCall Call(FireEffectComp, TEXT("ImpactEffect"));
	if (!Call.IsBound())
	{
		return;
	}
	Call.SetStruct(TEXT("InHitResult"), TBaseStructure<FHitResult>::Get(), &Hit);
	Call.Invoke();
}

// -------------------------------------------------------------------------------------
// BPC_FPST_LineTracer, ported.
// -------------------------------------------------------------------------------------

bool AMyCharacter::WeaponTrace(const FVector& Start, const FVector& Dir, float Distance,
	FHitResult& OutHit)
{
	// TempTraceEnd = FireLocation + FireDirection * Distance, then LineTraceSingle on
	// TraceTypeQuery1 — the component's TraceChannel literal, which is Visibility.
	const FVector End = Start + (Dir * Distance);

	const TArray<AActor*> ActorsToIgnore;
	return UKismetSystemLibrary::LineTraceSingle(
		this, Start, End, UEngineTypes::ConvertToTraceType(ECC_Visibility),
		/*bTraceComplex=*/false, ActorsToIgnore, EDrawDebugTrace::None, OutHit,
		/*bIgnoreSelf=*/true, FLinearColor::Red, FLinearColor::Green, /*DrawTime=*/0.f);
}

bool AMyCharacter::WeaponTraceWithSpread(const FVector& Start, const FVector& Dir, float Distance,
	float Spread, FHitResult& OutHit)
{
	// RandomUnitVectorInConeInDegrees(ConeDir, ConeHalfAngleInDegrees), then the same trace.
	const FVector Scattered = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(Dir, Spread);
	return WeaponTrace(Start, Scattered, Distance, OutHit);
}

// -------------------------------------------------------------------------------------
// BPC_FPST_FireTimer, ported. Start is a SwitchEnum on E_FPST_FireMode.
// -------------------------------------------------------------------------------------

void AMyCharacter::StartFire(uint8 FireType, float Delay)
{
	StopFire();

	// The component's Delay pin came from the weapon's FireDelay; a zero from a caller that
	// did not know the weapon means "ask the weapon", never "fire every frame".
	const float Period = (Delay > 0.f) ? Delay : GetWeaponFireDelay();

	switch (FireType)
	{
	case FireMode_Single:
		// Single: broadcast once, no timer.
		FireEvent();
		break;

	case FireMode_Auto:
		// Auto: one immediate shot, then a looping timer for as long as the trigger is held.
		FireEvent();
		GetWorldTimerManager().SetTimer(FireTimerHandle, this, &AMyCharacter::FireEvent,
			Period, /*bLoop=*/true);
		break;

	case FireMode_Burst:
		// Burst: BurstFireEvent counts shots and stops itself at BurstShotCount.
		BurstFireCount = 0;
		BurstFireTick();
		if (BurstShotCount > 1)
		{
			GetWorldTimerManager().SetTimer(FireTimerHandle, this, &AMyCharacter::BurstFireTick,
				Period, /*bLoop=*/true);
		}
		break;

	default:
		UE_LOG(LogMyCharacter, Warning,
			TEXT("MyCharacter: unknown E_FPST_FireMode %u — firing one shot."), FireType);
		FireEvent();
		break;
	}
}

void AMyCharacter::BurstFireTick()
{
	++BurstFireCount;
	if (BurstFireCount >= BurstShotCount)
	{
		// GreaterEqual_IntInt -> Stop. FireEnd's dispatcher had no subscriber in the graph.
		StopFire();
		return;
	}
	FireEvent();
}

void AMyCharacter::StopFire()
{
	GetWorldTimerManager().ClearTimer(FireTimerHandle);
	BurstFireCount = 0;
}

void AMyCharacter::NextFireMode()
{
	// NextFireMode is the WEAPON's own function — it walks AvailableFireModes and advances
	// CurFireModeIndex. Nothing character-side to port.
	if (ABPWeaponBase* Weapon = GetCurrentWeapon())
	{
		FBPCall Call(Weapon, TEXT("NextFireMode"));
		if (Call.IsBound())
		{
			Call.Invoke();
		}
	}
}

float AMyCharacter::GetWeaponFireDelay() const
{
	if (const ABPWeaponBase* Weapon = GetCurrentWeapon())
	{
		if (const FFloatProperty* P = FindFProperty<FFloatProperty>(Weapon->GetClass(), N_FireDelay))
		{
			const float Value = P->GetPropertyValue_InContainer(Weapon);
			if (Value > 0.f)
			{
				return Value;
			}
		}
		else if (const FDoubleProperty* P64 =
					FindFProperty<FDoubleProperty>(Weapon->GetClass(), N_FireDelay))
		{
			const double Value = P64->GetPropertyValue_InContainer(Weapon);
			if (Value > 0.0)
			{
				return static_cast<float>(Value);
			}
		}
	}
	return FireDelay;
}

float AMyCharacter::GetWeaponSpreadAngle() const
{
	if (const ABPWeaponBase* Weapon = GetCurrentWeapon())
	{
		if (const FFloatProperty* P = FindFProperty<FFloatProperty>(Weapon->GetClass(), N_SpreadAngle))
		{
			return P->GetPropertyValue_InContainer(Weapon);
		}
		if (const FDoubleProperty* P64 =
				FindFProperty<FDoubleProperty>(Weapon->GetClass(), N_SpreadAngle))
		{
			return static_cast<float>(P64->GetPropertyValue_InContainer(Weapon));
		}
	}
	return SpreadAngle;
}

uint8 AMyCharacter::GetWeaponFireMode() const
{
	if (ABPWeaponBase* Weapon = GetCurrentWeapon())
	{
		FBPCall Call(Weapon, TEXT("GetCurrentFireMode"));
		uint8 Mode = FireMode_Single;
		if (Call.IsBound() && Call.Invoke() && Call.GetReturnByte(Mode))
		{
			return Mode;
		}
	}
	return FireMode_Single;
}

void AMyCharacter::FireEvent()
{
	FireCameraRecoil(bIsAiming);
	// PlayMontage(Mesh, FireAnimMontage) -> PlayAnim(WeaponFireAnimSeq, false).
	// The montage is a weapon VARIABLE and the graph picked the Aim* variant while ADS.
	PlayWeaponMontage(bIsAiming ? N_AimFireAnimMontage : N_FireAnimMontage);
	PlayWeaponAnim(/*bLooping=*/false);

	const UCameraComponent* Cam = CachedFPSCamera ? CachedFPSCamera.Get() : CachedFPSCam.Get();
	if (!Cam)
	{
		return;
	}
	const FVector Start = Cam->GetComponentLocation();
	const FVector Dir = Cam->GetForwardVector();

	// The graph's branch: one trace, or a ForLoop 0..5 of spread traces. Both arms then ran
	// the identical tracer -> impact -> damage -> hit-event chain.
	auto ResolveHit = [this](const FHitResult& Hit, bool bHit)
	{
		// On a MISS FHitResult::ImpactPoint is (0,0,0), so firing at the sky sent the tracer
		// to the WORLD ORIGIN. LineTraceSingle fills TraceEnd either way; that is the far end
		// of the shot and what the tracer must reach.
		FireTracerEffect(bHit ? Hit.ImpactPoint : Hit.TraceEnd);
		if (!bHit)
		{
			return;
		}
		ImpactEffect(Hit);
		DealDamage(Hit.GetActor(), Hit, Hit.ImpactNormal);
		HitEffectEvent(Hit);
	};

	// Verified shape: NotEqual(Enum) ? single Trace : ForLoop of spread traces. The operand
	// itself did not resolve, so SpreadWeaponType is the guess, not the branch.
	const bool bSpread = (GetCurrentWeaponType() == SpreadWeaponType);
	if (!bSpread)
	{
		FHitResult Hit;
		ResolveHit(Hit, WeaponTrace(Start, Dir, FireTraceDistance, Hit));
	}
	else
	{
		// SpreadAngle is the WEAPON's variable, not the character's — the shotgun's cone is
		// its own number. This class's SpreadAngle is only the no-weapon fallback.
		const float Cone = GetWeaponSpreadAngle();
		for (int32 Pellet = 0; Pellet < SpreadPelletCount; ++Pellet)
		{
			FHitResult Hit;
			ResolveHit(Hit, WeaponTraceWithSpread(Start, Dir, FireTraceDistance, Cone, Hit));
		}
	}
}

void AMyCharacter::MeleeAttack()
{
	// The graph PLAYS the montage and traces from its AN_FPST_Melee notify — the swing lands
	// damage mid-animation, not on the key press. That seam is now real: PlayWeaponMontage
	// binds OnPlayMontageNotifyBegin, and HandleMontageNotifyBegin calls MeleeTrace.
	if (PlayWeaponMontage(N_MeleeAnimMontage))
	{
		return;
	}

	// No MeleeAnimMontage on this weapon (or no AnimInstance yet). Trace immediately rather
	// than silently doing nothing — melee still functions, it just has no swing to sell it.
	UE_LOG(LogMyCharacter, Verbose,
		TEXT("MyCharacter: no MeleeAnimMontage on the current weapon — tracing without it."));
	MeleeTrace();
}

void AMyCharacter::MeleeTrace()
{
	// The graph branched on FPSMode into two traces with DIFFERENT start points: first
	// person swings from the camera, third person from Arrow_MeleeTraceStart. Collapsing
	// them to the arrow made the 1P swing originate behind the view and miss things the
	// player was looking straight at.
	const UCameraComponent* Cam = CachedFPSCamera ? CachedFPSCamera.Get() : CachedFPSCam.Get();
	const bool bFirstPerson = (GetAnimFPSMode() != 0) && Cam != nullptr;

	FVector Start;
	FVector Dir;
	if (bFirstPerson)
	{
		Start = Cam->GetComponentLocation();
		Dir = Cam->GetForwardVector();
	}
	else if (CachedArrowMeleeTraceStart)
	{
		Start = CachedArrowMeleeTraceStart->GetComponentLocation();
		Dir = CachedArrowMeleeTraceStart->GetForwardVector();
	}
	else
	{
		Start = GetActorLocation();
		Dir = GetActorForwardVector();
	}

	FHitResult Hit;
	if (!WeaponTrace(Start, Dir, MeleeTraceDistance, Hit))
	{
		return;
	}
	ImpactEffect(Hit);
	if (!IsValid(Hit.GetActor()))
	{
		return;
	}
	DealDamage(Hit.GetActor(), Hit, Hit.ImpactNormal);
	HitEffectEvent(Hit);
}

void AMyCharacter::HitEffectEvent(const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	if (!IsValid(HitActor))
	{
		return;
	}

	// Verified: the cast chain is gated on (HitActor != LastHitActor). Without that guard a
	// held trigger replays the elimination sound and animation every trace on one target.
	if (HitActor == LastHitActor)
	{
		return;
	}

	// CastToBP_FPST_Target_Manny, else CastToBP_FPST_Target_Mann_UE4, then branch on the
	// TARGET's GetIsDead: dead -> PlaySound2D + PlayAnim_Elimination + record LastHitActor;
	// alive -> PlayAnim_Hit. GetIsDead lives on the Blueprint target classes, so the test
	// stays false here until those get a C++ interface — that is the only piece missing,
	// the surrounding condition is now the graph's.
	const bool bTargetIsDead = false;
	if (bTargetIsDead)
	{
		// PlaySound2D(vol 2.0, pitch 1.8, bIsUISound=true) — with the audio ticket.
		// PlayAnim_Elimination(crosshair 0)
		LastHitActor = HitActor;
	}
	else
	{
		// PlayAnim_Hit(crosshair 0)
	}
}

// =====================================================================================
// "Setup Weapon" — the three links of BeginPlay's then_1 that touch components.
// =====================================================================================

void AMyCharacter::LinkWeaponAnimLayers(bool bUnarmed)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	// Unarmed used a fixed layer class in the graph; armed asks the weapon for its own.
	// Either way this links a LAYER. It never assigns SkeletalMesh or AnimClass — doing that
	// from C++ is what drops the character into a T-pose.
	UClass* LayerClass = nullptr;
	if (!bUnarmed)
	{
		if (ABPWeaponBase* Weapon = GetCurrentWeapon())
		{
			// `LinkAnimLayerClass` is a VARIABLE on BP_FPST_BaseWeapon — there is no
			// GetLinkAnimLayerClass function, exactly like AttachSocketName. This used to call
			// one, IsBound() was always false, LayerClass stayed null, and the weapon's layer
			// was NEVER linked: every weapon animated with whatever was already on the mesh.
			LayerClass = GetWeaponClassVar(Weapon, N_LinkAnimLayerClass);
		}
		if (!LayerClass)
		{
			UE_LOG(LogMyCharacter, Warning,
				TEXT("MyCharacter: current weapon has no LinkAnimLayerClass — the weapon's anim "
					 "layer was not linked, so it keeps the previous weapon's pose set."));
		}
	}
	else if (!UnarmedAnimLayerClass.IsNull())
	{
		LayerClass = UnarmedAnimLayerClass.TryLoadClass<UAnimInstance>();
	}

	if (LayerClass)
	{
		MeshComp->LinkAnimClassLayers(LayerClass);
	}
}

void AMyCharacter::ChangeProceduralInfo(FName InfoName)
{
	if (!ProceduralManagerComp)
	{
		return;
	}

	// NAME_None means "ask the weapon", which is what the graph's wire did.
	FName Resolved = InfoName;
	if (Resolved.IsNone())
	{
		if (ABPWeaponBase* Weapon = GetCurrentWeapon())
		{
			FBPCall Get(Weapon, TEXT("GetProceduralAnimInfoName"));
			if (Get.IsBound() && Get.Invoke())
			{
				Resolved = Get.GetReturnName();
			}
		}
	}

	FBPCall Call(ProceduralManagerComp, TEXT("ChangeInfo"));
	if (!Call.IsBound())
	{
		UE_LOG(LogMyCharacter, Warning, TEXT("MyCharacter: ChangeInfo not found on the procedural manager."));
		return;
	}
	Call.SetName(TEXT("InInfoName"), Resolved);
	Call.Invoke();
}

void AMyCharacter::StartFireTimerComp()
{
	// The graph's StartComp(InMainMesh = Mesh). Target is the FPS cam component in the
	// current asset; fall back to the fire timer, which carried it in the original.
	UObject* Target = FPSCamComp ? FPSCamComp.Get() : FireTimerComp.Get();
	if (!Target)
	{
		return;
	}

	FBPCall Call(Target, TEXT("StartComp"));
	if (!Call.IsBound())
	{
		UE_LOG(LogMyCharacter, Warning, TEXT("MyCharacter: StartComp not found on %s."), *Target->GetName());
		return;
	}
	Call.SetObject(TEXT("InMainMesh"), GetMesh());
	Call.Invoke();
}

void AMyCharacter::DealDamage(AActor* DamagedActor, const FHitResult& Hit, const FVector& FromDirection)
{
	// The single damage exit. Law 2: attributes move only through a GameplayEffect, and the
	// engine damage API is banned outright, so nothing is applied here yet. This is where
	// GE_Damage + BRDamageExecCalc attach.
	if (!IsValid(DamagedActor))
	{
		return;
	}
	UE_LOG(LogMyCharacter, Verbose, TEXT("MyCharacter: hit %s at %s — damage pipeline not wired."),
		*DamagedActor->GetName(), *Hit.ImpactPoint.ToCompactString());
}
