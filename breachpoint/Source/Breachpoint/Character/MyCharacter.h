#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "MyCharacter.generated.h"

class ABPWeaponBase;
class UAnimMontage;
class UArrowComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UUserWidget;
struct FInputActionValue;

/**
 * The FPS template character's Blueprint graph, transferred to C++.
 *
 * SOURCE OF TRUTH for this port is the exec-order extraction of
 * /Game/Characters/BP_FPST_Character (11 functions, EventTick, and the collapsed
 * BeginPlay / "Input - Control" / "Weapon" graphs). Node order below matches that
 * extraction; where it does not, the C++ is wrong, not the graph.
 *
 * THREE THINGS THIS CLASS DELIBERATELY DOES NOT DO.
 *
 * 1. It creates NO components. The camera stack (FPSCamera, FollowCamera, FPSCam,
 *    CameraBoom, Arrow_MeleeTraceStart) and the nine BPC_FPST_* components live on the
 *    Blueprint's construction script and are resolved here by pointer. Re-creating them
 *    in C++ would duplicate the tree, and touching Mesh at all is what produces a T-pose.
 *    Mesh, AnimClass and the skeleton are never assigned from C++ — anim changes go
 *    through LinkAnimClassLayers, exactly as the graph did.
 *
 * 2. It does not call the engine damage API. The graph used the engine point-damage node
 *    in six places with a hardcoded BaseDamage of 10.0; law 2 bans that API outright and
 *    law 3 bans the literal. All six sites funnel into DealDamage() below, which is the
 *    single seam where the GAS pipeline lands.
 *
 * 3. It does not tick. The graph's EventTick ran a 120-unit line trace every frame to
 *    colour the HUD dot. Law 4 forbids a gameplay tick, so that became a timer.
 *
 * The BPC_* hooks at the bottom are empty on purpose. They are the exec order expressed
 * in C++ with the component calls named but not yet implemented, because those functions
 * live on Blueprint component classes whose signatures cannot be called from C++ without
 * guessing at a ProcessEvent parameter layout. Each hook is the one place its component's
 * port lands. See the ticket log for the ordering.
 */
UCLASS(config = Game)
class BREACHPOINT_API AMyCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	AMyCharacter();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// ---------------------------------------------------------------------------------
	// The eleven Blueprint functions, one for one.
	// ---------------------------------------------------------------------------------

	/** Clears both arrays, sizes AllWeapons to the enum, then spawns the four weapons. */
	UFUNCTION(BlueprintCallable, Category = "Weapons")
	void CreateWeapons();

	/** SpawnActor -> AttachActorToComponent(Mesh, socket, SnapToTarget) -> slot + available. */
	UFUNCTION(BlueprintCallable, Category = "Weapons")
	void CreateWeapon(uint8 InWeaponType, TSubclassOf<ABPWeaponBase> InWeaponClass);

	UFUNCTION(BlueprintCallable, Category = "Weapons")
	void HideAllWeapons();

	UFUNCTION(BlueprintPure, Category = "Weapons")
	ABPWeaponBase* GetCurrentWeapon() const;

	UFUNCTION(BlueprintPure, Category = "Weapons")
	uint8 GetCurrentWeaponType() const;

	UFUNCTION(BlueprintCallable, Category = "Weapons")
	void NextWeapon();

	UFUNCTION(BlueprintCallable, Category = "Weapons")
	void PrevWeapon();

	UFUNCTION(BlueprintPure, Category = "Weapons")
	uint8 GetCrosshairType() const;

	UFUNCTION(BlueprintPure, Category = "Weapons")
	uint8 GetCurrentWeaponScopeType() const;

	virtual bool CanJumpInternal_Implementation() const override;

protected:

	// ---------------------------------------------------------------------------------
	// Input actions. Soft + Config, mirroring how ABPCharacter resolves its own actions,
	// so no hard asset ref reaches C++ (law 3). Pin under
	// [/Script/Breachpoint.MyCharacter] in DefaultGame.ini.
	// ---------------------------------------------------------------------------------

	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> LookAction;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> SprintAction;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> AimAction;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> FireAction;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> ReloadAction;

	/** The four weapon classes CreateWeapons spawns. Soft: the graph hardcoded the paths. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Weapons")
	TArray<FSoftClassPath> StartupWeaponClasses;

	/** Thrown by the G handler after GrenadeThrowDelay. Graph hardcoded BP_FPST_Grenade. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Weapons")
	FSoftClassPath GrenadeClass;

	/** The unarmed anim layer the swap chain links. Graph hardcoded ABP_UnarmedAnimLayers. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Weapons")
	FSoftClassPath UnarmedAnimLayerClass;

	// ---------------------------------------------------------------------------------
	// The twenty Blueprint variables. Names kept, except IsAiming? -> bIsAiming ('?' is
	// not a legal C++ identifier). DELETE the Blueprint's own copies of these or the
	// asset will not compile: a BP variable that shadows a parent member is an error.
	// ---------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Weapons")
	uint8 WeaponType = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Weapons")
	TObjectPtr<ABPWeaponBase> CurrWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "Weapons")
	TArray<TObjectPtr<ABPWeaponBase>> AllWeapons;

	UPROPERTY(BlueprintReadOnly, Category = "Weapons")
	TArray<uint8> AvailableWeapons;

	UPROPERTY(BlueprintReadOnly, Category = "Weapons")
	int32 CurrentWeaponIndex = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
	float DefaultWalkSpeed = 600.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
	float AimWalkSpeed = 250.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
	float SprintWalkSpeed = 900.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
	float LookSensitivity = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	bool bIsAiming = false;

	UPROPERTY(BlueprintReadOnly, Category = "AxisValues")
	float TurnAxisValue = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AxisValues")
	float LookupAxisValue = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AxisValues")
	float RightAxisValue = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AxisValues")
	float ForwardAxisValue = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Widgets")
	TObjectPtr<UUserWidget> HudWidget;

	UPROPERTY(BlueprintReadOnly, Category = "Widgets")
	TObjectPtr<UUserWidget> SettingWidget;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AActor> LastHitActor;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FVector GrenadeSpawnLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FRotator GrenadeSpawnDirection = FRotator::ZeroRotator;

	// ---------------------------------------------------------------------------------
	// Tuning the graph carried as literals. Kept as members so they are visible and
	// overridable rather than buried; the CSV move belongs to the weapons ticket.
	// ---------------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	float AimFOV = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	float HipFOV = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	float FOVChangeSpeed = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	float AimPoseChangeSpeed = 18.f;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	float CrouchPoseChangeSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	float MeleeTraceDistance = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	float FireTraceDistance = 100000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	float SpreadAngle = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	int32 SpreadPelletCount = 6;

	/**
	 * E_FPST_WeaponType ordinals the graph compared against. UNVERIFIED — the enum operands
	 * on those NotEqual(Enum)/Equal(Enum) nodes did not resolve, so the COMPARISON is the
	 * graph's and the VALUE is a guess. Named and overridable rather than a magic number
	 * buried in a branch, so correcting one is a config edit, not a code hunt.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	uint8 UnarmedWeaponType = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	uint8 SpreadWeaponType = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	float GrenadeThrowDelay = 0.2f;

	/** EventTick traced every frame; this is that trace's period instead. */
	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	float AimTraceInterval = 0.033f;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	float AimTraceDistance = 120.f;

	// ---------------------------------------------------------------------------------
	// Components owned by the Blueprint, resolved by name in PostInitializeComponents.
	// Never created here. A null one is reported once and then tolerated.
	// ---------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> CachedFPSCamera;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> CachedFollowCamera;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> CachedFPSCam;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CachedCameraBoom;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> CachedArrowMeleeTraceStart;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActorComponent> FPSCamComp;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActorComponent> FireTimerComp;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActorComponent> LineTracerComp;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActorComponent> FireEffectComp;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActorComponent> ProceduralManagerComp;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActorComponent> AimAndLeanComp;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActorComponent> PoseOffsetsComp;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActorComponent> RecoilComp;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActorComponent> SwayAndLagComp;

	// ---------------------------------------------------------------------------------
	// Input handlers. One per entry point in "Input - Control" and "Weapon".
	// ---------------------------------------------------------------------------------

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpStarted();
	void OnJumpCompleted();
	void OnCrouchTriggered();
	void OnSprintStarted();
	void OnSprintCompleted();
	void OnAimStarted();
	void OnAimCompleted();
	void OnFireStarted();
	void OnFireCompleted();
	void OnReload();
	void OnNextFireModeKey();
	void OnLeanLeftPressed();
	void OnLeanLeftReleased();
	void OnLeanRightPressed();
	void OnLeanRightReleased();
	void OnGrenadeKey();
	void OnMeleeKey();
	void OnSwapNext();
	void OnSwapPrev();

	/** X / Z / wheel-up / wheel-down were eight byte-identical copies of this in the graph. */
	void SwapWeapon(bool bNext);

	/** FireEvent: recoil -> montage -> weapon anim -> single trace or the 6-pellet loop. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void FireEvent();

	/** MeleeAttack and the F key ran this same chain, duplicated four times. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void MeleeAttack();

	/** HitEffectEvent: Target_Manny / Target_Mann_UE4 cast, sound, elimination or hit anim. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void HitEffectEvent(const FHitResult& Hit);

	/**
	 * The ONLY damage exit in this class, replacing six engine point-damage calls.
	 * Deliberately does not apply damage yet — routing it belongs to the GAS pipeline,
	 * not to a character port. Logs so a playtest shows the hits landing.
	 * ponytail: single seam, becomes a GE application when the weapons ticket lands.
	 */
	virtual void DealDamage(AActor* DamagedActor, const FHitResult& Hit, const FVector& FromDirection);

	/** The timer that replaced EventTick's per-frame HUD trace. */
	void AimTraceTick();

	// ---------------------------------------------------------------------------------
	// Calls that belong to the nine BPC_* Blueprint component classes. Empty here.
	// The exec order around them is already correct, so implementing a component is a
	// local change to one hook rather than a re-read of the graph.
	// ---------------------------------------------------------------------------------

	virtual void ChangePose(uint8 PoseType, uint8 ScopeType, float ChangeSpeed) {}
	virtual void ChangeCameraTargetFOV(float TargetFOV, float Speed) {}
	virtual void SetADS(bool bADS) {}
	virtual void SetADSUpper(bool bADSUpper) {}
	virtual void SetSprinting(bool bSprinting) {}
	virtual void SetLeaning(float Leaning) {}
	virtual void SetUnarmed(bool bUnarmed) {}
	virtual void ShowCrosshair(uint8 CrosshairType, bool bAiming) {}
	virtual void StartFire(uint8 FireType, float Delay) {}
	virtual void StopFire() {}
	virtual void NextFireMode() {}
	virtual void FireCameraRecoil(bool bADS) {}
	virtual void ReturnCameraRecoil() {}
	/**
	 * These three are IMPLEMENTED, not stubs — they are the graph's "Setup Weapon" chain.
	 * LinkAnimClassLayers is engine C++ on the mesh; the layer class and the info name are
	 * read off the current weapon by reflection, and StartComp is a Blueprint component
	 * function called the same way. The skeleton is never touched: only the anim LAYER is
	 * linked, exactly as the graph did.
	 */
	virtual void LinkWeaponAnimLayers(bool bUnarmed);
	virtual void ChangeProceduralInfo(FName InfoName);
	virtual void StartFireTimerComp();
	virtual bool WeaponTrace(const FVector& Start, const FVector& Dir, float Distance, FHitResult& OutHit) { return false; }
	virtual bool WeaponTraceWithSpread(const FVector& Start, const FVector& Dir, float Distance, float Spread, FHitResult& OutHit) { return false; }
	virtual void FireTracerEffect(const FVector& HitLocation) {}
	virtual void ImpactEffect(const FHitResult& Hit) {}
	virtual void PlayWeaponAnim(bool bLooping) {}

private:

	/** Resolves one Blueprint-owned component by its SCS name, warning once if absent. */
	UActorComponent* FindNamedComponent(FName ComponentName) const;

	FTimerHandle AimTraceTimer;
	FTimerHandle GrenadeThrowTimer;
};
