#pragma once
// Base character: GAS avatar (ASC lives on PlayerState). Handles death, GAS init, and runtime avatar behavior. TDM: IGenericTeamAgentInterface, optional AbilitySet.

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"
#include "Data/OSHitDamageContext.h"
// Dependency: TDM Ability Set (not in this changelist). Uncomment when pushing OSTDMAbilitySet.
// #include "TDM/OSTDMAbilitySet.h"
#include "OSCharacter.generated.h"

class UOSCharacterMovementComponent;
class UOSHealthComponent;
class UOSAbilitySystemComponent;
class UOSAttributeSet;
class UOSGameplayEffect;
class UOSMotionWarpingComponent;
class UWidgetComponent;
class UOSNameplateWidget;
class AOSPlayerState;
class UPrimitiveComponent;
class UGA_OSGrab;
enum class EOSCharacterType : uint8;
	
UCLASS()
class ONSIGHT_API AOSCharacter : public ACharacter, public IAbilitySystemInterface, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AOSCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UOSAttributeSet* GetAttributeSet() const;

	/** IGenericTeamAgentInterface: team from PlayerState (TDM). NoTeam when not in a team mode. */
	virtual FGenericTeamId GetGenericTeamId() const override;
	UFUNCTION(BlueprintCallable)
	UOSCharacterMovementComponent* GetOSMovement() const;
	/** Returns the motion warping component (base type for compatibility with engine/Library APIs). */
	UFUNCTION(BlueprintCallable, Category = "Components")
	UMotionWarpingComponent* GetMotionWarpingComponent() const;

	/** Phase 1 debug-overlay accessors — read-only view into the replicated grab-warp state. */
	bool HasReplicatedGrabAttackerWarp() const { return bHasReplicatedGrabAttackerWarp; }
	bool HasReplicatedGrabVictimWarp()   const { return bHasReplicatedGrabVictimWarp; }
	UFUNCTION(BlueprintCallable)
	UOSHealthComponent* GetHealthComponent() const;

	UFUNCTION(BlueprintCallable)
	void HandleDeath(const FOSDeathEventInfo& DeathEvent);
	
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Death(const FOSDeathEventInfo& DeathEvent);

	/** Server-only: clear death-related effects/tags on the PlayerState-owned ASC before destroying this pawn. */
	//void ClearDeathStateForRespawn();

	/** All-machines: undo the visual/physical damage from Multicast_GoRagdoll so the pawn can be reused.
	 *  No authority check — this is for local presentation state (collision, physics, movement mode).
	 *  Override in subclasses for additional reset (e.g., mesh posture pinning). */
	virtual void Local_ResetStateForRespawn();
	
	/** Multicast RPC to go ragdoll on all clients */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_GoRagdoll();
	
	virtual void OnRep_ReplicatedMovement() override;
	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;

	// GAS init timing (PlayerState-owned ASC best practice)
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void UnPossessed() override;
	/** Authority: applies/removes GE_OSInAirState on the ASC when movement mode changes. */
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;
	
	// A frontend hook that allows the designers decided how a character to apply color onto itself based on its mesh. 
	// Minimal work backend. MaterialSlotOverride is optional and set to -1 by default if unused.
	UFUNCTION(BlueprintImplementableEvent)
	void ApplyColorToMesh(const FLinearColor& Color, const int32 MaterialSlotOverride = -1);

	/** Called whenever the character type changes (PlayerState OnRep) or on first GAS init.
	 *  Override in C++ or Blueprint to apply type-specific visuals, audio switches, etc.
	 *  Blueprint: call Parent to preserve C++ subclass behavior. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character")
	void ApplyCharacterType(EOSCharacterType NewType);

	/** Called once when this character's ASC is fully initialized (server: PossessedBy, client: OnRep_PlayerState).
	 *  Override in C++ (OnGasReady_Implementation) or Blueprint to run GAS-dependent init without timing hacks.
	 *  Blueprint: call Parent to preserve C++ subclass behavior. */
	UFUNCTION(BlueprintNativeEvent, Category = "Ability")
	void OnGasReady(UAbilitySystemComponent* ASC);
	
	void ClearState();

	/** Server-authoritative soft-lock target for melee / aim assist (replicated). */
	UPROPERTY(ReplicatedUsing = OnRep_SoftLockTarget, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AOSCharacter> SoftLockTarget;

	UFUNCTION()
	void OnRep_SoftLockTarget();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetSoftLockTarget(AOSCharacter* NewTarget);

	// ========================================
	// REPLICATED WARP TARGET (proxy root motion matching)
	// ========================================

	/** Server: set the replicated warp target for proxy root motion sync. Called from GA_OSAttack::InjectWarpTargets. */
	void SetReplicatedWarpTarget(const FName& WarpName, const FTransform& WarpTransform);

	/** Server: clear the replicated warp target when attack ends. Called from GA_OSAttack::ClearWarpTarget. */
	void ClearReplicatedWarpTarget();
	

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = "Components")
	TObjectPtr<UOSHealthComponent> HealthComponent;
	
	/** Motion warping component for mantling, parkour, and attack position warp. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UOSMotionWarpingComponent> MotionWarpingComponent;

	// --- Nameplate ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> NameplateComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Nameplate")
	TSubclassOf<UUserWidget> NameplateWidgetClass;




	// --- GAS init (single pipeline) ---
	virtual void InitializeGAS();
	virtual void BindGASDelegates();

	/** If PlayerState GAS is already ready when this pawn appears, fire the Blueprint event now. */
	void TryFireOnGasReady();
	
	/** GAS tag callback: fires on ALL machines when IsGrabbed/IsGrabbing tags replicate.
	 *  Disables pawn collision + orient-to-movement during grab to prevent bystander push
	 *  and CMC rotation fighting the grab snap. */
	void OnGrabTagChanged(FGameplayTag Tag, int32 Count);

	/** Convenience for PlayerState-owned GAS avatars */
	UFUNCTION(BlueprintPure)
	AOSPlayerState* GetOSPlayerState() const;

	/** Apply a "reset attributes" effect to self (server authoritative) */
	void ResetAttributes();

	UPROPERTY()
	TObjectPtr<UOSAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UOSAttributeSet> AttributeSet;
	
	/** Optional reset attributes effect applied by ResetAttributes() */
	UPROPERTY(EditDefaultsOnly, Category="Ability")
	TSubclassOf<UOSGameplayEffect> ResetAttributesEffect;

	/** When set, grants this AbilitySet for TDM path. Cleared on UnPossessed.
	 *  Dependency: OSTDMAbilitySet (not in this changelist). Uncomment when pushing TDM Ability Set. */
	// UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	// TObjectPtr<UOSTDMAbilitySet> TDMAbilitySet;

#if !UE_BUILD_SHIPPING
	// --- Temporary debug: register tag-change listeners for all combat state tags ---
	void RegisterTagDebugListeners(UAbilitySystemComponent* ASC);
#endif

private:
	// --- Replicated warp target for simulated proxy root motion matching ---
	// Set by attack abilities on server; proxy OnRep injects into local MWC so its
	// root motion extraction includes warping, matching the server's trajectory.

	UPROPERTY(Replicated)
	FTransform ReplicatedAttackWarpTarget;

	UPROPERTY(Replicated)
	FName ReplicatedAttackWarpTargetName;

	UPROPERTY(ReplicatedUsing=OnRep_WarpTarget)
	bool bHasReplicatedAttackWarpTarget = false;

	UFUNCTION()
	void OnRep_WarpTarget();

	// --- Replicated grab attacker warp target (Phase 1) ---
	// Server-computed one-shot warp destination for attacker's paired-animation entry.
	// Unlike the attack pipeline above, this replicates unconditionally (not COND_SimulatedOnly)
	// because grab Phase 1 moves InjectAttackerWarps server-only — the owning client also
	// consumes the replicated value via OnRep.

	UPROPERTY(Replicated)
	FTransform ReplicatedGrabAttackerWarp;

	UPROPERTY(Replicated)
	FName ReplicatedGrabAttackerWarpName;

	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedGrabAttackerWarp)
	bool bHasReplicatedGrabAttackerWarp = false;

	UFUNCTION()
	void OnRep_ReplicatedGrabAttackerWarp();

	/** Server-only. Producer for attacker's grab warp target. Sets the replicated
	 *  properties and calls ForceNetUpdate so clients receive it immediately. */
	void SetReplicatedGrabAttackerWarp(const FName& WarpName, const FTransform& WarpTransform);

	/** Server-only. Clears the replicated attacker grab warp target (end of grab). */
	void ClearReplicatedGrabAttackerWarp();

	// --- Replicated grab victim placement warp target (Phase 1) ---
	// Server-computed one-shot victim placement for paired animation. Third-party spectators
	// need this so they see smooth MW positioning instead of teleport. The victim's own client
	// also consumes it (GA_OSGrabReaction does a local teleport at grab start but that's a
	// cosmetic prediction — the MW warp refines placement).

	UPROPERTY(Replicated)
	FTransform ReplicatedGrabVictimWarp;

	UPROPERTY(Replicated)
	FName ReplicatedGrabVictimWarpName;

	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedGrabVictimWarp)
	bool bHasReplicatedGrabVictimWarp = false;

	UFUNCTION()
	void OnRep_ReplicatedGrabVictimWarp();

	/** Server-only. Producer for victim's grab placement warp target. */
	void SetReplicatedGrabVictimWarp(const FName& WarpName, const FTransform& WarpTransform);

	/** Server-only. Clears the replicated victim grab warp target. */
	void ClearReplicatedGrabVictimWarp();

	// --- Nameplate ---

	void InitializeNameplate();

	friend class UGA_OSGrab;
	friend class UOSCharacterMovementComponent;

	/** Server: victim to restore if this attacker pawn is destroyed before GA_OSGrab::EndAbility runs (disconnect). */
	TWeakObjectPtr<AOSCharacter> GrabDisconnectVictimWeak;

	void ServerRegisterGrabVictimForDisconnectCleanup(AOSCharacter* Victim);
	void ServerClearGrabVictimForDisconnectCleanup();

	/** Handle to the active GE_OSInAirState (infinite), applied/removed by OnMovementModeChanged.
	 *  Stored so we can remove the exact effect instance (not a tag-based search). */
	FActiveGameplayEffectHandle InAirStateHandle;

	/** Handle to GE_OSGroundedState (infinite), paired with InAir for Movement.Grounded tag. */
	FActiveGameplayEffectHandle GroundedStateHandle;

	/** Server: apply/remove InAir + Grounded GEs from current CMC mode (also used after GAS init). */
	void SyncLocomotionMovementStateFromMode();

	UFUNCTION()
	void OnCapsuleHitRecoverLocomotion(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	bool fellOutOfWorld = false;

	/** Server: HandlePlayerDeath runs once per life; GA_OSDeath leaves IsDead on the PS ASC before this RPC. */
	bool bDeathNotifiedToGameMode = false;

	bool bOnGasReadyFired = false;

	/** GAS initialized before BeginPlay — deferred until components are ready. */
	bool bGASReadyDeferred = false;
	/** Handles for TDM AbilitySet; revoked in UnPossessed. Dependency: OSTDMAbilitySet. Uncomment when pushing TDM Ability Set. */
	// FOSTDMAbilitySet_GrantedHandles TDMGrantedHandles;
};

