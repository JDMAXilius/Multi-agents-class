// GAS init: PossessedBy/OnRep_PlayerState call InitializeGAS; server applies loadout via PS->GiveStartupLoadout().
#include "Characters/OSCharacter.h"
#include "Engine/Engine.h"
#include "OSLogCategories.h"
#include "Components/OSMotionWarpingComponent.h"
#include "MotionWarpingComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/OSCharacterMovementComponent.h"
#include "Components/OSHealthComponent.h"
#include "Core/OSGameMode.h"
#include "Core/OSPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GAS/Abilities/GA_OSGrab.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GAS/Effects/GE_OSInAirState.h"
#include "GAS/Effects/GE_OSPlayerDefaultAttributes.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "Animations/Instances/OSAnimInstance.h"
#include "Data/OSGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Components/WidgetComponent.h"
#include "UI/Components/OSNameplateWidget.h"
// Dependency: TDM Ability Set (not in this changelist). Uncomment when pushing OSTDMAbilitySet.
// #include "TDM/OSTDMAbilitySet.h"

// Phase 1: role-string helper for grab-warp OnRep diag logs. File-local since only the
// two grab OnReps below need it; other OnReps in this file don't log roles.
static const TCHAR* GrabOnRepRoleStr(ENetRole Role)
{
	return (Role == ROLE_Authority)       ? TEXT("Server") :
	       (Role == ROLE_AutonomousProxy) ? TEXT("AutonomousProxy") :
	                                         TEXT("SimulatedProxy");
}


AOSCharacter::AOSCharacter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<UOSCharacterMovementComponent>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	ACharacter::SetReplicateMovement(true);

	// Smoother multiplayer movement replication
	// (PlayerState NetUpdateFrequency doesn't affect pawn movement replication.)
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(30.0f);

	// ========================================
	// MOVEMENT CONFIGURATION
	// ========================================
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	GetCapsuleComponent()->SetCollisionProfileName("Pawn");
	GetMesh()->SetCollisionProfileName("NoCollision");

	// Action game movement settings for responsive feel
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->MaxAcceleration = 2400.0f; // High acceleration for snappy response
	MoveComp->BrakingFrictionFactor = 1.0f;
	MoveComp->BrakingFriction = 6.0f; // Higher friction for better control
	MoveComp->GroundFriction = 8.0f; // Strong ground friction for precise movement
	MoveComp->BrakingDecelerationWalking = 1600.0f; // Fast deceleration for quick stops
	MoveComp->MinAnalogWalkSpeed = 20.0f;
	// Air movement settings
	//MoveComp->GravityScale = 2.0f; // Higher gravity for faster falling, less floaty feel
	MoveComp->AirControl = 0.15f; // Slightly more air control for better maneuverability
	MoveComp->FallingLateralFriction = 0.5f; // Lower friction for faster falling
	MoveComp->BrakingDecelerationFalling = 400.0f; // Lower air braking for less resistance
	MoveComp->bApplyGravityWhileJumping = true; // Apply gravity immediately for less floaty jumps
	
	// Crouch settings
	MoveComp->NavAgentProps.bCanCrouch = true;
	MoveComp->bCanWalkOffLedgesWhenCrouching = true;
	MoveComp->SetCrouchedHalfHeight(65.0f);
	
	// Character-level movement properties
	MoveComp->MinAnalogWalkSpeed = 20.0f;
	
	// Rotation settings for action gameplay
	MoveComp->bUseControllerDesiredRotation = false; // Character doesn't auto-rotate to controller
	MoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f); // Fast rotation rate for responsive turning
	MoveComp->bAllowPhysicsRotationDuringAnimRootMotion = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	MoveComp->bOrientRotationToMovement = true;
	MoveComp->JumpZVelocity = 700.0f;
	
	HealthComponent = CreateDefaultSubobject<UOSHealthComponent>(TEXT("HealthComponent"));
	MotionWarpingComponent = CreateDefaultSubobject<UOSMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

	NameplateComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("Nameplate"));
	if (NameplateComponent)
	{
		UE_LOG(LogTemp, Display, TEXT("Nameplate created"));
		NameplateComponent->SetupAttachment(RootComponent);
		NameplateComponent->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
		NameplateComponent->SetWidgetSpace(EWidgetSpace::Screen);
		NameplateComponent->SetDrawAtDesiredSize(true);
		NameplateComponent->SetVisibility(false);
		
	}
	if (NameplateComponent)
	{
		NameplateComponent->SetWidgetClass(NameplateWidgetClass);
	}

	// GAS ownership policy (multiplayer best practice):
	// - Players: ASC + AttributeSets live on PlayerState and persist across respawn.
	// - AI/NPCs: ASC + AttributeSets may live on the Character (see AOSAICharacter).
	// Therefore, the base Avatar class does NOT create an ASC/AttributeSet by default.
	AbilitySystemComponent = nullptr;
	AttributeSet = nullptr;

}

// ========================================
// REPLICATION
// ========================================

void AOSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AOSCharacter, HealthComponent);
	DOREPLIFETIME(AOSCharacter, SoftLockTarget);

	// Warp target replication — simulated proxies only. Drives proxy root motion matching.
	DOREPLIFETIME_CONDITION(AOSCharacter, ReplicatedAttackWarpTarget, COND_SimulatedOnly);
	DOREPLIFETIME_CONDITION(AOSCharacter, ReplicatedAttackWarpTargetName, COND_SimulatedOnly);
	DOREPLIFETIME_CONDITION(AOSCharacter, bHasReplicatedAttackWarpTarget, COND_SimulatedOnly);

	// Phase 1: grab warp replication — unconditional, owning client is also a consumer.
	DOREPLIFETIME(AOSCharacter, ReplicatedGrabAttackerWarp);
	DOREPLIFETIME(AOSCharacter, ReplicatedGrabAttackerWarpName);
	DOREPLIFETIME(AOSCharacter, bHasReplicatedGrabAttackerWarp);
	DOREPLIFETIME(AOSCharacter, ReplicatedGrabVictimWarp);
	DOREPLIFETIME(AOSCharacter, ReplicatedGrabVictimWarpName);
	DOREPLIFETIME(AOSCharacter, bHasReplicatedGrabVictimWarp);
}
void AOSCharacter::OnRep_SoftLockTarget()
{
}

void AOSCharacter::SetSoftLockTarget(AOSCharacter* NewTarget)
{
	if (!HasAuthority())
	{
		return;
	}
	SoftLockTarget = NewTarget;
}

// ========================================
// REPLICATED WARP TARGET (proxy root motion matching)
// ========================================

void AOSCharacter::OnRep_WarpTarget()
{
	if (GetLocalRole() != ROLE_SimulatedProxy) return;

	UMotionWarpingComponent* MWC = GetMotionWarpingComponent();
	if (!MWC) return;

	if (bHasReplicatedAttackWarpTarget && !ReplicatedAttackWarpTargetName.IsNone())
	{
		MWC->AddOrUpdateWarpTargetFromTransform(ReplicatedAttackWarpTargetName, ReplicatedAttackWarpTarget);
	}
	else
	{
		// Attack ended — remove warp target
		if (!ReplicatedAttackWarpTargetName.IsNone())
		{
			MWC->RemoveWarpTarget(ReplicatedAttackWarpTargetName);
		}
	}
}

void AOSCharacter::SetReplicatedWarpTarget(const FName& WarpName, const FTransform& WarpTransform)
{
	if (!HasAuthority()) return;
	ReplicatedAttackWarpTargetName = WarpName;
	ReplicatedAttackWarpTarget = WarpTransform;
	bHasReplicatedAttackWarpTarget = true;
	ForceNetUpdate();
}

void AOSCharacter::ClearReplicatedWarpTarget()
{
	if (!HasAuthority()) return;
	bHasReplicatedAttackWarpTarget = false;
	// Keep the name so OnRep can remove the right target
	ForceNetUpdate();
}

// Phase 1: grab attacker warp producer (server-only).
void AOSCharacter::SetReplicatedGrabAttackerWarp(const FName& WarpName, const FTransform& WarpTransform)
{
	if (!HasAuthority()) return;
	ReplicatedGrabAttackerWarpName = WarpName;
	ReplicatedGrabAttackerWarp = WarpTransform;
	bHasReplicatedGrabAttackerWarp = true;
	ForceNetUpdate();
}

void AOSCharacter::ClearReplicatedGrabAttackerWarp()
{
	if (!HasAuthority()) return;
	bHasReplicatedGrabAttackerWarp = false;
	// Keep the name so OnRep can remove the right target by name on clients.
	ForceNetUpdate();
}

// Phase 1: grab victim placement warp producer (server-only).
void AOSCharacter::SetReplicatedGrabVictimWarp(const FName& WarpName, const FTransform& WarpTransform)
{
	if (!HasAuthority()) return;
	ReplicatedGrabVictimWarpName = WarpName;
	ReplicatedGrabVictimWarp = WarpTransform;
	bHasReplicatedGrabVictimWarp = true;
	ForceNetUpdate();
}

void AOSCharacter::ClearReplicatedGrabVictimWarp()
{
	if (!HasAuthority()) return;
	bHasReplicatedGrabVictimWarp = false;
	ForceNetUpdate();
}

// Phase 1: attacker grab warp OnRep — applies replicated warp to local MWC on the
// owning client (autonomous proxy) AND simulated proxies. Server doesn't receive OnRep.
void AOSCharacter::OnRep_ReplicatedGrabAttackerWarp()
{
	UMotionWarpingComponent* MWC = GetMotionWarpingComponent();
	if (!MWC) return;

	const TCHAR* RoleStr = GrabOnRepRoleStr(GetLocalRole());

	if (bHasReplicatedGrabAttackerWarp && !ReplicatedGrabAttackerWarpName.IsNone())
	{
		MWC->AddOrUpdateWarpTargetFromTransform(ReplicatedGrabAttackerWarpName, ReplicatedGrabAttackerWarp);
		UE_LOG(LogOSGASGrab, Verbose,
			TEXT("[GrabDiag] %s OnRep_ReplicatedGrabAttackerWarp applied: name=%s loc=%s"),
			RoleStr, *ReplicatedGrabAttackerWarpName.ToString(),
			*ReplicatedGrabAttackerWarp.GetLocation().ToCompactString());
	}
	else if (!ReplicatedGrabAttackerWarpName.IsNone())
	{
		// Grab ended — remove the warp target by the previously-replicated name.
		MWC->RemoveWarpTarget(ReplicatedGrabAttackerWarpName);
		UE_LOG(LogOSGASGrab, Verbose,
			TEXT("[GrabDiag] %s OnRep_ReplicatedGrabAttackerWarp cleared: name=%s"),
			RoleStr, *ReplicatedGrabAttackerWarpName.ToString());
	}
}

// Phase 1: victim grab warp OnRep — same pattern, applied to victim's MWC.
void AOSCharacter::OnRep_ReplicatedGrabVictimWarp()
{
	UMotionWarpingComponent* MWC = GetMotionWarpingComponent();
	if (!MWC) return;

	const TCHAR* RoleStr = GrabOnRepRoleStr(GetLocalRole());

	if (bHasReplicatedGrabVictimWarp && !ReplicatedGrabVictimWarpName.IsNone())
	{
		MWC->AddOrUpdateWarpTargetFromTransform(ReplicatedGrabVictimWarpName, ReplicatedGrabVictimWarp);
		UE_LOG(LogOSGASGrab, Verbose,
			TEXT("[GrabDiag] %s OnRep_ReplicatedGrabVictimWarp applied: name=%s loc=%s"),
			RoleStr, *ReplicatedGrabVictimWarpName.ToString(),
			*ReplicatedGrabVictimWarp.GetLocation().ToCompactString());
	}
	else if (!ReplicatedGrabVictimWarpName.IsNone())
	{
		MWC->RemoveWarpTarget(ReplicatedGrabVictimWarpName);
		UE_LOG(LogOSGASGrab, Verbose,
			TEXT("[GrabDiag] %s OnRep_ReplicatedGrabVictimWarp cleared: name=%s"),
			RoleStr, *ReplicatedGrabVictimWarpName.ToString());
	}
}

// During grab: bypass CMC smoothing on simulated proxies.
// Normal CMC replication interpolates position updates, causing stutter when the server
// teleports the victim. With IsGrabbed active, snap directly to server position.
void AOSCharacter::OnRep_ReplicatedMovement()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		const FOSGameplayTags& GameplayTags = FOSGameplayTags::Get();
		if (ASC->HasMatchingGameplayTag(GameplayTags.IsGrabbed) || ASC->HasMatchingGameplayTag(GameplayTags.IsGrabbing))
		{
			const FRepMovement& RepMove = GetReplicatedMovement();
			SetActorLocationAndRotation(
				RepMove.Location, RepMove.Rotation,
				false, nullptr, ETeleportType::TeleportPhysics);
			return;
		}

#if !UE_BUILD_SHIPPING
		if (GetLocalRole() == ROLE_SimulatedProxy && ASC->HasMatchingGameplayTag(GameplayTags.IsAttacking))
		{
			const FRepMovement& RepMove = GetReplicatedMovement();
			const FVector Delta = RepMove.Location - GetActorLocation();
			if (!Delta.IsNearlyZero(1.f))
			{
				UE_LOG(LogOSMovement, Warning,
					TEXT("[PROXY_DIAG] OnRep_ReplicatedMovement on %s during IsAttacking — ServerLoc=%s CurLoc=%s Delta=%s (%.1fcm)"),
					*GetName(),
					*RepMove.Location.ToCompactString(),
					*GetActorLocation().ToCompactString(),
					*Delta.ToCompactString(),
					Delta.Size());
			}
		}
#endif
	}
	Super::OnRep_ReplicatedMovement();
}

// Players: ASC on PlayerState (persists across respawn). AI: may use Character-owned ASC.
UAbilitySystemComponent* AOSCharacter::GetAbilitySystemComponent() const
{
	// Multiplayer GAS best practice:
	// - Players: ASC lives on PlayerState and persists across respawn.
	// - AI/NPCs: ASC may live on the Character.
	if (AOSPlayerState* PS = GetOSPlayerState())
	{
		if (UAbilitySystemComponent* PSASC = PS->GetAbilitySystemComponent())
		{
			return PSASC;
		}
	}
	return AbilitySystemComponent;
}

UOSAttributeSet* AOSCharacter::GetAttributeSet() const
{
	if (AOSPlayerState* PS = GetOSPlayerState())
	{
		return PS->GetAttributeSet();
	}
	return AttributeSet;
}

UOSCharacterMovementComponent* AOSCharacter::GetOSMovement() const
{
	return Cast<UOSCharacterMovementComponent>(GetMovementComponent());
}

UMotionWarpingComponent* AOSCharacter::GetMotionWarpingComponent() const
{
	return MotionWarpingComponent;
}

UOSHealthComponent* AOSCharacter::GetHealthComponent() const
{
	return HealthComponent;
}

FGenericTeamId AOSCharacter::GetGenericTeamId() const
{
	if (const AOSPlayerState* PS = GetOSPlayerState())
	{
		return PS->GetGenericTeamId();
	}
	return FGenericTeamId::NoTeam;
}

// Called when health hits 0 (from AttributeSet broadcast). Server runs death logic; client can RPC if needed.
void AOSCharacter::HandleDeath(const FOSDeathEventInfo& DeathEvent)
{
	if (HasAuthority())
	{
		Server_Death_Implementation(DeathEvent);
	}
	else
	{
		Server_Death(DeathEvent);
	}
}

bool AOSCharacter::Server_Death_Validate(const FOSDeathEventInfo& DeathEvent)
{
	// Reject spoofed kill attribution: only the owning controller of this pawn can
	// assert its own death, and only if the pawn isn't already dead (multi-apply guard).
	// The DeathEvent.InstigatorPS field is untrusted until after validation.
	if (!GetController()) return true; // uncontrolled (punching bag etc.) — allow.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(FOSGameplayTags::Get().IsDead))
		{
			return false; // already dead — reject duplicate client RPC.
		}
	}
	return true;
}

// Authority: cancel abilities and notify GameMode. Death GE + IsDead tag are owned by GA_OSDeath.
void AOSCharacter::Server_Death_Implementation(const FOSDeathEventInfo& DeathEvent)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// Same pawn can receive multiple death callbacks same frame — notify GameMode once per life.
	// Do not gate on IsDead here; GA_OSDeath applies IsDead before FinishDeath calls this.
	if (bDeathNotifiedToGameMode)
	{
		return;
	}

	// Preserve Ability_GrabReaction so victims killed mid-grab stay in the Proned loop pose
	// until respawn. CancelAllAbilities() is unfiltered; its engine implementation does not
	// honor WithoutTags (AbilitySystemComponent_Abilities.cpp:1350 — iterates every active
	// spec). Use the filtered overload so the reaction ability survives this sweep.
	FGameplayTagContainer PreserveTags;
	PreserveTags.AddTag(FOSGameplayTags::Get().Ability_GrabReaction);
	ASC->CancelAbilities(nullptr, &PreserveTags, nullptr);

	// have to make a copy here since DeathEvent is const and making it mutable seems like a bad idea
	FOSDeathEventInfo DeathEventCopy = DeathEvent;
	if (!DeathEvent.GetVictimPlayerState(GetWorld()))
		DeathEventCopy.VictimPS = GetPlayerState();

	// Server-only attribution log: who killed who (best-effort).
	{
		const UWorld* World = GetWorld();
		const APlayerState* KillerPS = DeathEventCopy.GetInstigatorPlayerState(World);
		const APlayerState* VictimPS = DeathEventCopy.GetVictimPlayerState(World);

		// NOTE: Avoid FUniqueNetIdRepl::ToString() here (can require OnlineSubsystem linkage).
		const TCHAR* KillerIdState = DeathEventCopy.InstigatorPlayerStateUniqueId.IsValid() ? TEXT("VALID") : TEXT("INVALID");
		const TCHAR* VictimIdState = DeathEventCopy.VictimPlayerStateUniqueId.IsValid() ? TEXT("VALID") : TEXT("INVALID");

		UE_LOG(LogTemp, Log, TEXT("[Death] Killer=%s (Id=%s) Victim=%s (Id=%s) Pawn=%s"),
			KillerPS ? *KillerPS->GetPlayerName() : TEXT("None"),
			KillerIdState,
			VictimPS ? *VictimPS->GetPlayerName() : TEXT("None"),
			VictimIdState,
			*GetNameSafe(this));
	}

	if (AOSGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AOSGameMode>() : nullptr)
	{
		GM->HandlePlayerDeath(Cast<AController>(GetController()), DeathEventCopy);
		bDeathNotifiedToGameMode = true;
	}
}

// Replicated: disable input, stop movement, disable capsule, enable physics blend on mesh for ragdoll.
void AOSCharacter::Multicast_GoRagdoll_Implementation()
{
	USkeletalMeshComponent* mesh = GetMesh();
	if (!mesh) return;
	
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}
	
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	mesh->SetCollisionProfileName(TEXT("Ragdoll"));
	mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	mesh->SetAllBodiesSimulatePhysics(true);
	mesh->SetAllBodiesPhysicsBlendWeight(1.f);
	mesh->bBlendPhysics = true;

	if (NameplateComponent)
		NameplateComponent->SetVisibility(false);
}
// Authority only: remove death GE and death tags so this pawn can be respawned and accept abilities again.
void AOSCharacter::ClearState()
{
	if (!HasAuthority()) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;

	// Nuke all active effects and loose tags — default effects and abilities are re-applied on spawn.
	FGameplayEffectQuery RemoveAllQuery;
	ASC->RemoveActiveEffects(RemoveAllQuery);

	FGameplayTagContainer LooseTags;
	ASC->GetOwnedGameplayTags(LooseTags);
	ASC->RemoveLooseGameplayTags(LooseTags);

	InAirStateHandle.Invalidate();
}

void AOSCharacter::BeginPlay()
{
	Super::BeginPlay();

	/* Fallback: BP instances in this project consistently arrive with the inherited
	   Nameplate subobject serialized as null (likely stale BP component override data).
	   Create the widget component at runtime so the nameplate still spawns. Safe no-op
	   when the serialized subobject is valid. */
	if (!NameplateComponent)
	{
		NameplateComponent = NewObject<UWidgetComponent>(this, TEXT("Nameplate_Runtime"));
		if (NameplateComponent)
		{
			NameplateComponent->SetupAttachment(GetRootComponent());
			NameplateComponent->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
			NameplateComponent->SetWidgetSpace(EWidgetSpace::Screen);
			NameplateComponent->SetDrawAtDesiredSize(true);
			NameplateComponent->RegisterComponent();
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("BeginPlay NameplateComponent: %s"),
	NameplateComponent ? TEXT("VALID") : TEXT("NULL"));

	// If GAS initialized before BeginPlay (PossessedBy/OnRep_PlayerState),
	// fire the deferred OnGasReady now that all components are ready.
	if (bGASReadyDeferred)
	{
		bGASReadyDeferred = false;
		TryFireOnGasReady();
	}
}

void AOSCharacter::ServerRegisterGrabVictimForDisconnectCleanup(AOSCharacter* Victim)
{
	if (!HasAuthority() || !IsValid(Victim))
	{
		return;
	}
	GrabDisconnectVictimWeak = Victim;
}

void AOSCharacter::ServerClearGrabVictimForDisconnectCleanup()
{
	if (!HasAuthority())
	{
		return;
	}
	GrabDisconnectVictimWeak.Reset();
}

void AOSCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (AOSCharacter* Vic = GrabDisconnectVictimWeak.Get())
		{
			UGA_OSGrab::ServerRestoreGrabVictimAfterInterrupted(
				Vic, nullptr, UGA_OSGrab::GetDefaultVictimPlacementWarpName());
			GrabDisconnectVictimWeak.Reset();
		}
	}
	Super::EndPlay(EndPlayReason);
}

// Server: pawn possessed; PlayerState exists. Wire ASC owner/avatar and initialize runtime state.
void AOSCharacter::PossessedBy(AController* NewController)
{
	bDeathNotifiedToGameMode = false;
	Super::PossessedBy(NewController);
	InitializeGAS();
	TryFireOnGasReady();
	
	if (auto ps = GetOSPlayerState())
	{
		ApplyColorToMesh(ps->SavedColor);
	}
}

// Client: PlayerState replicated; cache ASC/AS and initialize runtime bindings.
void AOSCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	// Client-only path: on listen server host, PossessedBy already handled init.
	if (!HasAuthority())
	{
		InitializeGAS();
		TryFireOnGasReady();
		
		if (auto ps = GetOSPlayerState())
		{
			ApplyColorToMesh(ps->SavedColor);
		}
	}
}

void AOSCharacter::TryFireOnGasReady()
{
	if (bOnGasReadyFired)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// Defer until BeginPlay so all components (including BP-added ones like audio) are initialized.
	if (!HasActorBegunPlay())
	{
		bGASReadyDeferred = true;
		return;
	}

	bOnGasReadyFired = true;
	OnGasReady(ASC);

	InitializeNameplate();
}

void AOSCharacter::UnPossessed()
{
	Super::UnPossessed();
}


void AOSCharacter::ResetAttributes()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}
	
	// Respawn best practice (PlayerState-owned ASC persists):
	// - Reset *current* attributes (Health/Stamina/Aura) for the new pawn avatar.
	// - Prefer an explicit ResetAttributesEffect; otherwise use the safe C++ default.
	TSubclassOf<UGameplayEffect> EffectToApply = nullptr;
	if (IsValid(ResetAttributesEffect))
	{
		EffectToApply = ResetAttributesEffect;
	}
	else
	{
		EffectToApply = UGE_OSPlayerDefaultAttributes::StaticClass();
	}
	
	if (!IsValid(EffectToApply)) return;

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	
	Context.AddSourceObject(this);
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectToApply, 1.0f, Context);
	if (Spec.IsValid() && Spec.Data.IsValid())
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

// All-machines: reverse the visual/physical changes from Multicast_GoRagdoll so this pawn can fight again.
// No authority check — presentation state only. Subclasses override for additional reset (mesh posture, etc.).
void AOSCharacter::Local_ResetStateForRespawn()
{
	// 1. Stop ragdoll physics and restore mesh collision to pre-ragdoll default.
	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		CharMesh->SetAllBodiesSimulatePhysics(false);
		CharMesh->SetAllBodiesPhysicsBlendWeight(0.f);
		CharMesh->bBlendPhysics = false;
		CharMesh->SetCollisionProfileName(TEXT("NoCollision"));
		CharMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		CharMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	// 2. Re-enable capsule collision (GoRagdoll set it to NoCollision).
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionProfileName(TEXT("Pawn"));
	}

	// 3. Re-enable movement (GoRagdoll called DisableMovement + StopMovementImmediately).
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	// 4. Restore nameplate (hidden in GoRagdoll).
	UE_LOG(LogTemp, Warning, TEXT("Nameplate Restore"));
	if (NameplateComponent && !IsLocallyControlled())
		NameplateComponent->SetVisibility(true);
	
	InitializeNameplate();
}

// --- Nameplate ---

void AOSCharacter::InitializeNameplate()
{
	if (!NameplateWidgetClass)
		return;
	
	if (!NameplateComponent || !NameplateWidgetClass)
		return;

	NameplateComponent->SetWidgetClass(NameplateWidgetClass);
	NameplateComponent->InitWidget();

	if (UOSNameplateWidget* Nameplate = Cast<UOSNameplateWidget>(NameplateComponent->GetWidget()))
	{
		Nameplate->SetOwningCharacter(this);
		Nameplate->FinalizeNameplate();
	}
	
	if (IsLocallyControlled())
	{
		NameplateComponent->SetVisibility(false);
	}

	NameplateComponent->SetWidgetClass(NameplateWidgetClass);
	NameplateComponent->SetVisibility(true);

	if (!GetOSPlayerState())
		return;

	if (UOSNameplateWidget* Nameplate = Cast<UOSNameplateWidget>(NameplateComponent->GetWidget()))
		Nameplate->SetOwningCharacter(this);
	
}

// Stub: declared in header (CL 1612) but no implementation provided. Prevents linker error.
void AOSCharacter::SyncLocomotionMovementStateFromMode()
{
	// TODO: Implement locomotion movement state sync (declared CL 1612)
}

void AOSCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	if (!HasAuthority())
	{
		Super::FellOutOfWorld(DmgType);
		return;
	}
	if (fellOutOfWorld) return;
	fellOutOfWorld = true;
	
	FOSDeathEventInfo Info;
	Server_Death(Info);
	
}

// Wire HealthComponent to AttributeSet health broadcast and this character to HealthComponent death delegate.
void AOSCharacter::BindGASDelegates()
{
	if (!AbilitySystemComponent) return;

	// Grab state: disable pawn collision + orient-to-movement on all machines when
	// IsGrabbed/IsGrabbing tags replicate. Fires on server, owning client, AND 3rd-party
	// clients (Mixed mode ASC replicates tags to all connections).
	const FOSGameplayTags& GrabTags = FOSGameplayTags::Get();
	AbilitySystemComponent->RegisterGameplayTagEvent(GrabTags.IsGrabbed, EGameplayTagEventType::NewOrRemoved)
		.RemoveAll(this);
	AbilitySystemComponent->RegisterGameplayTagEvent(GrabTags.IsGrabbed, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &AOSCharacter::OnGrabTagChanged);
	AbilitySystemComponent->RegisterGameplayTagEvent(GrabTags.IsGrabbing, EGameplayTagEventType::NewOrRemoved)
		.RemoveAll(this);
	AbilitySystemComponent->RegisterGameplayTagEvent(GrabTags.IsGrabbing, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &AOSCharacter::OnGrabTagChanged);

	// WARN-5 (2026-04-14 grab audit): close the 1-RTT solid-capsule window on simulated
	// proxies during grab start. IsBeingGrabbed replicates to all clients via GE_OSGrabClaim's
	// TargetTagsComponent BEFORE GA_OSGrabReaction's ActivationOwnedTags grant IsGrabbed.
	// Without this listener, third-party views of the victim still have ECR_Block during
	// that window and bystander pawn push creates visible jitter — especially when crowded.
	AbilitySystemComponent->RegisterGameplayTagEvent(GrabTags.IsBeingGrabbed, EGameplayTagEventType::NewOrRemoved)
		.RemoveAll(this);
	AbilitySystemComponent->RegisterGameplayTagEvent(GrabTags.IsBeingGrabbed, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &AOSCharacter::OnGrabTagChanged);

	// Death is routed through GA_OSDeath (triggered by GameplayEvent.Death from AttributeSet):
	// HealthComponent no longer needs to listen for health changes.
}

// All machines: toggle pawn collision + orient-to-movement when grab tags change.
// Registered via RegisterGameplayTagEvent in BindGASDelegates — fires on tag replication.
void AOSCharacter::OnGrabTagChanged(FGameplayTag Tag, int32 Count)
{
	const bool bInGrab = Count > 0;

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, bInGrab ? ECR_Ignore : ECR_Block);
	}

	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->bOrientRotationToMovement = !bInGrab;
	}
}

// Cache ASC/AS from PlayerState (or character for AI), InitAbilityActorInfo, and bind movement/death/runtime delegates.
void AOSCharacter::InitializeGAS()
{
	UAbilitySystemComponent* ASC = nullptr;
	UOSAttributeSet* AS = nullptr;
	AOSPlayerState* PS = GetOSPlayerState();

	if (PS)
	{
		// PlayerState-owned GAS (multiplayer best practice):
		// Make our cached member pointers refer to the PlayerState-owned ASC/AS so ALL helper calls
		// and GetAbilitySystemComponent()/GetAttributeSet() resolve to the authoritative objects.
		UAbilitySystemComponent* PSASC = PS->GetAbilitySystemComponent();
		UOSAbilitySystemComponent* TypedPSASC = Cast<UOSAbilitySystemComponent>(PSASC);
		if (!TypedPSASC)
		{
			UE_LOG(LogTemp, Error, TEXT("InitializeGAS: PlayerState ASC is not UOSAbilitySystemComponent. Pawn=%s PS=%s ASC=%s"),
				*GetNameSafe(this),
				*GetNameSafe(PS),
				*GetNameSafe(PSASC));
			return;
		}

		AbilitySystemComponent = TypedPSASC;
		AttributeSet = PS->GetAttributeSet();
		ASC = AbilitySystemComponent;
		AS = AttributeSet;
		if (ASC)
		{
			// Always refresh Owner/Avatar for PlayerState-owned ASC.
			// FFA/OSGameMode early UnPossess + respawn can leave a stale avatar (destroyed pawn) on
			// server or client; skipping Init when GetAvatarActor()==this prevented loadout re-grant.
			// BindGASDelegates uses RemoveAll before RegisterGameplayTagEvent, so re-entry is safe.
			ASC->InitAbilityActorInfo(PS, this);
			if (HasAuthority())
			{
				// Server: apply current loadout preset on the PlayerState ASC after avatar init.
				PS->GiveStartupLoadout();

				// Extended safety net: clear all combat state tags that can get stuck during a
				// death/respawn cycle when an ability's cleanup path doesn't unwind cleanly
				// (e.g. interrupted montage tasks leaving orphaned ActivationOwnedTags).
				// `RemoveLooseGameplayTags` in ClearState() is NOT replicated, so defensive
				// tag clearing here on the server prevents stuck tags from bleeding into the
				// next life. `SetTagMapCount(tag, 0)` is idempotent — no-op when unset.
				{
					const FOSGameplayTags& StateTags = FOSGameplayTags::Get();
					ASC->SetTagMapCount(StateTags.IsAttacking, 0);
					ASC->SetTagMapCount(StateTags.IsDodging, 0);
					ASC->SetTagMapCount(StateTags.IsMantling, 0);
					ASC->SetTagMapCount(StateTags.IsBlocking, 0);
					ASC->SetTagMapCount(StateTags.IsHitReacting, 0);
					ASC->SetTagMapCount(StateTags.IsRecoiling, 0);
					ASC->SetTagMapCount(StateTags.IsKnockedDown, 0);
					ASC->SetTagMapCount(StateTags.IsStunned, 0);
					ASC->SetTagMapCount(StateTags.IsGrabbed, 0);
					ASC->SetTagMapCount(StateTags.IsBeingGrabbed, 0);
					ASC->SetTagMapCount(StateTags.State_IsStunLock, 0);
					ASC->SetTagMapCount(StateTags.IsSprinting, 0);
				}
			}
		}
	}
	else if (IsValid(AbilitySystemComponent))
	{
		ASC = AbilitySystemComponent;
		AS = AttributeSet;
		// Standalone/AI fallback path: refresh only if avatar differs.
		if (AbilitySystemComponent->GetAvatarActor() != this)
		{
			AbilitySystemComponent->InitAbilityActorInfo(this, this);
		}
		if (HasAuthority())
		{
			ClearState();
			ResetAttributes();
		}
	}

	if (!ASC)
	{
		return;
	}

	// Bind movement speed to GAS attributes now that the ASC is initialized.
	if (UOSCharacterMovementComponent* Move = GetOSMovement())
	{
		Move->BindMovementAttributes(ASC);
		Move->RefreshMovementSpeed();
	}

	// Safety: if something left movement disabled (e.g., ragdoll/death flow), re-enable it when we're not dead.
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (MoveComp->MovementMode == MOVE_None)
		{
			const FGameplayTag& DeadTag = FOSGameplayTags::Get().IsDead;
			const bool bIsDead = DeadTag.IsValid() && ASC->HasMatchingGameplayTag(DeadTag);
			if (!bIsDead)
			{
				MoveComp->SetMovementMode(MOVE_Walking);
			}
		}
	}

	// Movement state: Grounded is the implicit default (absence of GE_OSInAirState).
	// No initial tag setup needed — OnMovementModeChanged (server-only) applies the InAir GE
	// when the character becomes airborne, and GAS replicates it to clients automatically.
	// Previously this block set an initial IsGrounded loose tag on non-SimulatedProxy roles.


#if !UE_BUILD_SHIPPING
	RegisterTagDebugListeners(ASC);
#endif

	BindGASDelegates();
}



#if !UE_BUILD_SHIPPING
// Temporary debug: print on-screen messages when any combat-relevant tag is added or removed.
void AOSCharacter::RegisterTagDebugListeners(UAbilitySystemComponent* ASC)
{
	if (!ASC || !IsLocallyControlled()) return;

	const FOSGameplayTags& T = FOSGameplayTags::Get();

	struct FTagDebugEntry { FGameplayTag Tag; FString Name; FColor Color; };
	static const TArray<FTagDebugEntry> DebugTags = {
		{ T.IsInAir,        TEXT("InAir"),        FColor::Cyan },
		{ T.IsGrounded,     TEXT("Grounded"),     FColor::Green },
		{ T.IsSprinting,    TEXT("Sprinting"),    FColor::Orange },
		{ T.IsAttacking,    TEXT("Attacking"),    FColor::Yellow },
		{ T.IsCharging,     TEXT("Charging"),     FColor::Yellow },
		{ T.IsBlocking,     TEXT("Blocking"),     FColor::Blue },
		{ T.IsDodging,      TEXT("Dodging"),      FColor::Magenta },
		{ T.IsHitReacting,  TEXT("HitReacting"),  FColor::Red },
		{ T.IsDead,         TEXT("Dead"),          FColor::Red },
		{ T.IsStunned,      TEXT("Stunned"),       FColor::Red },
	};
	/*
	for (const FTagDebugEntry& Entry : DebugTags)
	{
		if (!Entry.Tag.IsValid()) continue;

		ASC->RegisterGameplayTagEvent(Entry.Tag, EGameplayTagEventType::NewOrRemoved)
			.AddWeakLambda(this, [Name = Entry.Name, Col = Entry.Color](const FGameplayTag& Tag, int32 Count)
			{
				
				const FString Msg = FString::Printf(TEXT("[State] %s %s"),
					*Name, Count > 0 ? TEXT("ON") : TEXT("OFF"));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, Col, Msg);
				
			});
	}
	*/
}
#endif

// ========================================
// MOVEMENT STATE (GE-based)
// ========================================

// Apply/remove GE_OSInAirState when movement mode changes (jump, land, fall off ledge, etc.).
//
// Server-only (HasAuthority): GE_OSInAirState replicates automatically via GAS (Mixed mode),
// so all clients receive the Gameplay.State.InAir tag through normal GE replication.
// This replaces the old loose-tag + ROLE_SimulatedProxy pattern, which required non-standard
// gating because loose tags don't replicate. Now follows the same pattern as IsSprinting
// (GE_OSSprintState) and IsDead (GE_OSDeathState).
//
// Guard: InAirStateHandle.IsValid() prevents duplicate application on rapid Falling->Flying
// transitions (e.g. mantle triggering MOVE_Flying while already in MOVE_Falling).
void AOSCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	// Server-only: GAS replicates the GE (and its granted tags) to all clients automatically.
	if (!HasAuthority()) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;

	const FOSGameplayTags& GSTags = FOSGameplayTags::Get();

	// Ragdoll triggers movement mode changes on dead characters — ignore those.
	if (ASC->HasMatchingGameplayTag(GSTags.IsDead)) return;

	const EMovementMode NewMode = GetCharacterMovement()->MovementMode;

	if (NewMode == MOVE_Falling || NewMode == MOVE_Flying)
	{
		// Apply InAir GE if not already active (guard prevents stacking on Falling->Flying transition).
		if (!InAirStateHandle.IsValid())
		{
			InAirStateHandle = ASC->ApplyGameplayEffectToSelf(
				UGE_OSInAirState::StaticClass()->GetDefaultObject<UGE_OSInAirState>(),
				1.f, ASC->MakeEffectContext());
	}
	}
	else if (NewMode == MOVE_Walking || NewMode == MOVE_NavWalking)
	{
		// Remove InAir GE on landing — tag is removed, abilities gated by IsInAir unblock.
		if (InAirStateHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(InAirStateHandle);
			InAirStateHandle.Invalidate();
		}
	}
}

void AOSCharacter::OnCapsuleHitRecoverLocomotion(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (ASC && ASC->HasMatchingGameplayTag(FOSGameplayTags::Get().IsDead))
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	// If something left movement disabled (rare edge cases), restore walking on the server.
	if (MoveComp->MovementMode == MOVE_None)
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	// If we're grounded but the InAir GE is still active, clear it to recover locomotion gating.
	if (ASC && InAirStateHandle.IsValid() &&
		(MoveComp->MovementMode == MOVE_Walking || MoveComp->MovementMode == MOVE_NavWalking))
	{
		ASC->RemoveActiveGameplayEffect(InAirStateHandle);
		InAirStateHandle.Invalidate();
	}
}

// Default C++ implementation — subclasses override for C++ init, Blueprints override for BP init.
// Blueprint: call Parent to preserve C++ subclass behavior.
void AOSCharacter::ApplyCharacterType_Implementation(EOSCharacterType NewType)
{
	// Base class: no-op. Subclasses override to apply Wwise switches, visuals, etc.
}

void AOSCharacter::OnGasReady_Implementation(UAbilitySystemComponent* ASC)
{
	// Apply the current character type on all machines.
	// Server has the authoritative value; clients have whatever has replicated so far.
	// If the client's value is still the default, OnRep_SelectedCharacterType will
	// re-apply the correct type when the replicated value arrives.
	if (AOSPlayerState* PS = GetOSPlayerState())
	{
		ApplyCharacterType(PS->GetSelectedCharacterType());
	}
	
	// Wire AnimInstance tag delegates now that ASC is guaranteed valid.
	// NativeInitializeAnimation may fire before ASC is available on clients
	// (mesh initializes before PlayerState replicates), so this is the reliable path.
	if (UOSAnimInstance* AnimInstance = Cast<UOSAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->InitializeWithAbilitySystem(ASC);
	}
}

AOSPlayerState* AOSCharacter::GetOSPlayerState() const
{
	return GetPlayerState<AOSPlayerState>();
}
