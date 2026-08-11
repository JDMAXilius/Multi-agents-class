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
class UAnimMontage;
class UUserWidget;
struct FBranchingPointNotifyPayload;
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

	/**
	 * Points CurrentWeaponIndex at StartupWeaponType. The graph's BeginPlay labels this
	 * "Rifle - Start Weapon"; CreateWeapons on its own leaves index 0, which is whichever
	 * weapon the config happens to list first.
	 */
	void SelectStartupWeapon();

	/**
	 * The weapon's own `AttachSocketName` FName variable, read by reflection — it is a
	 * VARIABLE on BP_FPST_BaseWeapon, not a getter. NAME_None if the property is absent,
	 * which attaches at the mesh root and is logged.
	 */
	FName GetWeaponAttachSocket(const ABPWeaponBase* InWeapon) const;

	/**
	 * A weapon-side UClass variable (`LinkAnimLayerClass`), read by reflection. Handles both a
	 * hard and a soft class pin, because either is legal on that variable.
	 */
	UClass* GetWeaponClassVar(const ABPWeaponBase* InWeapon, FName VarName) const;

	/** A weapon-side UAnimMontage* variable (`MeleeAnimMontage`, `FireAnimMontage`, …). */
	UAnimMontage* GetWeaponMontage(const ABPWeaponBase* InWeapon, FName VarName) const;

	/**
	 * Plays one of the current weapon's montages on the mesh, binding the montage-notify
	 * delegate the first time. Returns false when there is no AnimInstance or the weapon has
	 * no montage in that slot — callers fall back rather than silently doing nothing.
	 */
	bool PlayWeaponMontage(FName VarName);

	/** The melee trace itself. Driven by the montage notify, not by the input event. */
	void MeleeTrace();

	/** The AnimBP's FPSMode via BPI_FPST_AnimInterface. Non-zero = first person. */
	uint8 GetAnimFPSMode() const;

	/**
	 * Muzzle transform for the tracer -- the WEAPON's own SkeletalMesh socket named by its
	 * `MuzzleSocketName` VARIABLE, not anything on the character.
	 */
	FTransform GetMuzzleTransform() const;

	UFUNCTION()
	void HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

	/** OnPlayMontageNotifyBegin is bound once, lazily, not every montage play. */
	bool bMontageNotifyBound = false;

	/**
	 * The current weapon's `FireDelay` and `SpreadAngle` — both are VARIABLES on
	 * BP_FPST_BaseWeapon, read by reflection. The fallbacks are this class's own defaults,
	 * used only when there is no weapon or the property is gone.
	 */
	float GetWeaponFireDelay() const;
	float GetWeaponSpreadAngle() const;

	/**
	 * `GetCurrentFireMode` IS a real UFUNCTION on BP_FPST_BaseWeapon (unlike the socket
	 * name), returning an E_FPST_FireMode byte: 0=Single, 1=Auto, 2=Burst.
	 */
	uint8 GetWeaponFireMode() const;

	/** The Burst arm's timer callback — BPC_FPST_FireTimer's BurstFireEvent. */
	void BurstFireTick();

	/**
	 * Sends one bool-taking BPI_FPST_AnimInterface message to the mesh's AnimInstance.
	 * ParamName is the interface's own pin name (`InSprinting`, `InADS`, …), which is what
	 * the reflection lookup matches on — a renamed pin fails loudly instead of writing
	 * into the wrong offset. Returns false if there is no AnimInstance or no such function.
	 */
	bool SendAnimInterfaceBool(const TCHAR* FunctionName, const TCHAR* ParamName, bool bValue);

	/** Warn once per character, not once per frame, when the AnimBP lacks the interface. */
	bool bAnimInterfaceWarned = false;

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

	/**
	 * Priority for the context above. Higher wins. Must stay ABOVE the priority
	 * ABPPlayerController uses for its own contexts (0), or those shadow this one on the
	 * same physical keys and the pawn takes no input at all — silently.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	int32 MappingContextPriority = 1;

	/**
	 * Clear every other mapping context before adding this pawn's own.
	 *
	 * The template character was the only thing in its world that added a context, so it
	 * never had to share. Here ABPPlayerController adds two more on the same keys, and
	 * priority alone did NOT resolve it -- 12 actions bound, 0 failed, the IMC verified to
	 * map all eight, and still no input. This makes the possessed pawn's context exclusive,
	 * which is what the template effectively had. Set false to go back to coexisting.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	bool bExclusiveMappingContext = true;

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

	/**
	 * The E_FPST_WeaponType the character spawns holding. 2 = Rifle, matching the graph's
	 * "Rifle - Start Weapon" group. This is a TYPE, not an index into StartupWeaponClasses,
	 * so reordering that list cannot silently change which weapon you start with.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Weapons")
	uint8 StartupWeaponType = 2;

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

	/** Fallback only. The live value is the WEAPON's own SpreadAngle — see GetWeaponSpreadAngle. */
	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	float SpreadAngle = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	int32 SpreadPelletCount = 6;

	/** Fallback only. The live value is the WEAPON's own FireDelay — see GetWeaponFireDelay. */
	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	float FireDelay = 0.12f;

	/**
	 * Shots per burst. BPC_FPST_FireTimer compares BurstFireCount against a literal on a
	 * GreaterEqual_IntInt node; that literal did not resolve out of the asset, so THIS
	 * NUMBER IS A GUESS and the comparison is the component's. Three is the template's
	 * conventional burst. Overridable so correcting it is a config edit.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	int32 BurstShotCount = 3;

	/**
	 * E_FPST_WeaponType ordinals the graph compared against. VERIFIED 11 Aug 2026 — no longer
	 * the guesses this comment used to admit to. E_FPST_WeaponType.uasset carries five named
	 * enumerators in order (Unarmed, Pistol, Rifle, Shotgun, Knife) and each weapon asset's
	 * own WeaponType default agrees: Pistol=NewEnumerator1, Rifle=NewEnumerator2,
	 * Knife=NewEnumerator4. So Unarmed is 0 and the spread weapon is the Shotgun at 3.
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

	/**
	 * BPC_FPST_Procedural_PoseOffsets.ChangePose and BPC_FPSCamComp.ChangeCameraTargetFOV.
	 * NOT stubs any more -- these are what make ADS read as ADS. While they were empty,
	 * aiming changed the walk speed and the anim-interface bools and nothing visibly moved.
	 */
	virtual void ChangePose(uint8 PoseType, uint8 ScopeType, float ChangeSpeed);
	virtual void ChangeCameraTargetFOV(float TargetFOV, float Speed);
	/**
	 * BPI_FPST_AnimInterface messages. NOT stubs any more, and NOT component calls.
	 *
	 * These four are functions on a Blueprint INTERFACE that `ABP_Mannequin_Base`
	 * implements; the character graph sent them to Mesh->GetAnimInstance(). The AnimBP
	 * stores each into a bool that the linked layers (`ABP_ItemAnimLayersBase` and its
	 * per-weapon children) read — `Sprinting` is what selects the `fPS_Sprint` pose. With
	 * these empty, the AnimBP never learned the character's state and the sprint animation
	 * could not play no matter what the movement component did.
	 */
	virtual void SetADS(bool bADS);
	virtual void SetADSUpper(bool bADSUpper);
	virtual void SetSprinting(bool bSprinting);
	virtual void SetLeaning(float Leaning) {}
	virtual void SetUnarmed(bool bUnarmed);
	virtual void ShowCrosshair(uint8 CrosshairType, bool bAiming) {}
	/**
	 * BPC_FPST_FireTimer.Start / .Stop, ported. NOT stubs any more.
	 *
	 * That component is a SwitchEnum on E_FPST_FireMode driving a looping timer that
	 * broadcasts EventDispatcher_Fire; BeginPlay's then_3 bound FireEvent to it. Neither the
	 * dispatcher nor the bind was reproducible from C++, so the switch and the timer live
	 * here and call FireEvent() directly. Same three arms, same cadence.
	 */
	virtual void StartFire(uint8 FireType, float Delay);
	virtual void StopFire();
	virtual void NextFireMode();
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
	/**
	 * BPC_FPST_LineTracer.Trace / .TraceWithSpread, ported. NOT stubs any more.
	 *
	 * That component is a LineTraceSingle on TraceTypeQuery1 (Visibility) from
	 * FireLocation to FireLocation + FireDirection * Distance, ignoring self; the spread
	 * variant first scatters the direction through RandomUnitVectorInConeInDegrees. Both
	 * returned false unconditionally before this, which is why nothing could ever be hit.
	 */
	virtual bool WeaponTrace(const FVector& Start, const FVector& Dir, float Distance, FHitResult& OutHit);
	virtual bool WeaponTraceWithSpread(const FVector& Start, const FVector& Dir, float Distance, float Spread, FHitResult& OutHit);
	/**
	 * BPC_FPST_Lyra_FireEffectComp, ported. NOT stubs any more — these are the visible
	 * bullet. The template's fire is hitscan with no projectile actor; the tracer Niagara
	 * system and the surface-typed impact FX are what a player reads as a shot.
	 */
	virtual void FireTracerEffect(const FVector& HitLocation);
	virtual void ImpactEffect(const FHitResult& Hit);
	virtual void PlayWeaponAnim(bool bLooping) {}

private:

	/** Resolves one Blueprint-owned component by its SCS name, warning once if absent. */
	UActorComponent* FindNamedComponent(FName ComponentName) const;

	FTimerHandle AimTraceTimer;
	FTimerHandle GrenadeThrowTimer;

	/** BPC_FPST_FireTimer's FireTimerHandle and BurstFireCount, by their own names. */
	FTimerHandle FireTimerHandle;
	int32 BurstFireCount = 0;
};
