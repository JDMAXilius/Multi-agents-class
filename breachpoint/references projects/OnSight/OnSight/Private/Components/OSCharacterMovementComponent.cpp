#include "Components/OSCharacterMovementComponent.h"
#include "Characters/OSCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "Data/OSGameplayTags.h"
#include "Engine/World.h"
#include "OSLogCategories.h"


// ========================================
// CONSTRUCTOR
// ========================================

UOSCharacterMovementComponent::UOSCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// IMPORTANT: CharacterMovement requires ticking to simulate movement.
	// We keep tick enabled, but avoid per-frame speed recompute work (speed is event-driven).
	PrimaryComponentTick.bCanEverTick = true;

	// Fallback base walk speed if attributes aren't ready yet.
	BaseWalkSpeed = 500.0f;

	// --- Combat networking tuning ---
	// Motion warped attacks produce large, intentional displacements (200-500+ cm lunges).
	// With LocalPredicted abilities, client and server compute warp destinations from slightly
	// different positions (latency), so CMC corrections during combat are larger than locomotion.

	// Gentle depenetration: push apart over multiple frames instead of violent single-frame snap.
	// Engine default is 100 cm — way too aggressive for melee combat where capsules routinely overlap.
	MaxDepenetrationWithPawn = 25.0f;

	// Smoothing window must cover warp lunge distances so corrections interpolate, not teleport.
	NetworkMaxSmoothUpdateDistance = 500.0f;
	NetworkNoSmoothUpdateDistance = 750.0f;

	// Smoother simulated proxy updates — visible to host watching other players and all clients.
	// 220ms hides per-tick fixup corrections during combat lunges at the cost of ~70ms extra visual lag.
	NetworkSimulatedSmoothLocationTime = 0.220f;
	NetworkSimulatedSmoothRotationTime = 0.150f; // Was 0.050f — too fast, caused visible rotation snap on attack lunge toward soft target

	// Exponential smoothing continuously approaches the target instead of linearly interpolating
	// between discrete network samples. Handles variable-rate corrections (combat lunges, warps)
	// better than Linear mode which can produce visible discontinuities at sample boundaries.
	NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;

	PostAttackServerCorrectionGraceUntilTime = -1.f;
}

void UOSCharacterMovementComponent::OnServerPostAttackMovementResume()
{
	if (!CharacterOwner || !CharacterOwner->HasAuthority())
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		PostAttackServerCorrectionGraceUntilTime =
			World->GetTimeSeconds() + FMath::Max(0.f, PostAttackCorrectionGraceSeconds);
	}
}

// ========================================
// BEGIN PLAY
// ========================================

void UOSCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// Try to bind to attribute set if available
	if (AOSCharacter* Character = Cast<AOSCharacter>(GetOwner()))
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			BindMovementAttributes(ASC);
		}
	}

	// Set initial max walk speed
	RefreshMovementSpeed();
}

// ========================================
// COMPONENT CLEANUP
// ========================================

// Unbind ASC attribute delegates before destruction. CMC lives on Character, ASC lives on
// PlayerState — ASC outlives Character across respawn. Without this cleanup, ASC fires
// attribute change delegates into a destroyed CMC (use-after-free).
//
// MUST run before Super::UninitializeComponent() because Super clears internal state and
// component registration, after which calling GetGameplayAttributeValueChangeDelegate is unsafe.
void UOSCharacterMovementComponent::UninitializeComponent()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		if (UOSAttributeSet* AttrSet = AttributeSet.Get())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(AttrSet->GetMoveSpeedBaseAttribute()).RemoveAll(this);
			ASC->GetGameplayAttributeValueChangeDelegate(AttrSet->GetMovementSpeedAttribute()).RemoveAll(this);
		}
	}

	Super::UninitializeComponent();
}

// ========================================
// PROXY ROOT MOTION SUPPRESSION
// ========================================

// Simulated proxies during attacks: zero local RM translation so the engine's fixup
// (SimulatedRootMotionPositionFixup + SmoothCorrection) is the sole position driver.
// The SkewWarp modifier doesn't exist on proxies (notify has TriggerOnFollower=false),
// so local RM is unwarped and would push the proxy in the wrong direction/distance.
// Returning ZeroVector keeps the proxy still between fixup ticks; the fixup provides
// the server's actual warped position and SmoothCorrection interpolates smoothly.
//
// IMPORTANT: Returns ZeroVector, NOT CurrentVelocity. The original bug returned
// CurrentVelocity (walking speed), which drove the proxy at walking velocity between
// fixup corrections, causing oscillation. Zero = proxy waits for fixup. Fixup drives.
FVector UOSCharacterMovementComponent::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		const AOSCharacter* OSChar = Cast<AOSCharacter>(CharacterOwner);
		if (OSChar && OSChar->bHasReplicatedAttackWarpTarget)
		{
#if !UE_BUILD_SHIPPING
			if (!RootMotionVelocity.IsNearlyZero(1.f))
			{
				UE_LOG(LogOSMovement, Log,
					TEXT("[CMC] Proxy=%s ConstrainVelocity → ZeroVector (suppressed RMVel=%s)"),
					*GetNameSafe(CharacterOwner), *RootMotionVelocity.ToCompactString());
			}
#endif
			return FVector::ZeroVector;
		}
	}

	return Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity, CurrentVelocity);
}

// ========================================
// ATTRIBUTE BINDING
// ========================================

// Caches AttributeSet and subscribes to MoveSpeedBase + MovementSpeed changes; call after GAS init (e.g. from OSCharacter).
void UOSCharacterMovementComponent::BindMovementAttributes(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!InAbilitySystemComponent)
	{
		return;
	}

	// Cache the ASC so UninitializeComponent can unbind delegates on teardown even if the
	// owner has started tearing down. ASC lives on PlayerState and outlives Character across respawn.
	CachedASC = InAbilitySystemComponent;

	if (const UAttributeSet* AttrSetBase = InAbilitySystemComponent->GetAttributeSet(UOSAttributeSet::StaticClass()))
	{
		UOSAttributeSet* AttrSet = const_cast<UOSAttributeSet*>(Cast<UOSAttributeSet>(AttrSetBase));
		if (AttrSet)
		{
			AttributeSet = AttrSet;
		
			// Bind to movement attribute changes
			InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttrSet->GetMoveSpeedBaseAttribute()).RemoveAll(this);
			InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttrSet->GetMoveSpeedBaseAttribute()).AddUObject(this, &UOSCharacterMovementComponent::Internal_OnMovementSpeedChanged);

			InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttrSet->GetMovementSpeedAttribute()).RemoveAll(this);
			InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttrSet->GetMovementSpeedAttribute()).AddUObject(this, &UOSCharacterMovementComponent::Internal_OnMovementSpeedChanged);
			
			// Initialize speed from current attribute value
			RefreshMovementSpeed();
		}
	}
}

// Recomputes MaxWalkSpeed from attributes (and blocking tags) and assigns to MaxWalkSpeed.
void UOSCharacterMovementComponent::RefreshMovementSpeed()
{
	MaxWalkSpeed = CalculateMovementSpeed();
}

UOSAttributeSet* UOSCharacterMovementComponent::GetAttributeSet() const
{
	if (AttributeSet.IsValid())
	{
		return AttributeSet.Get();
	}
	
	// Try to get from owner if not cached
	if (UAbilitySystemComponent* ASC = 
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		if (const UAttributeSet* AttrSetBase = ASC->GetAttributeSet(UOSAttributeSet::StaticClass()))
		{
			return const_cast<UOSAttributeSet*>(Cast<UOSAttributeSet>(AttrSetBase));
		}
	}
	
	return nullptr;
}





  // ========================================
// MAX SPEED CALCULATION
// ========================================

// MoveSpeedBase * MovementSpeed from GAS; if MovementBlockingTags present, use BaseWalkSpeed. Fallback so pawn never has 0 speed.
float UOSCharacterMovementComponent::CalculateMovementSpeed() const
{
	// Always keep a sane, non-zero fallback so a missing/late attribute init can't "freeze" the pawn.
	float OutSpeed = FMath::Max(1.0f, BaseWalkSpeed);
	
	// Check if character has movement blocking tags (stun, death, etc.)
	if (CharacterOwner)
	{
		if (UAbilitySystemComponent* ASC = 
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CharacterOwner))
		{
			// If any blocking tags are present, return base speed
			if (MovementBlockingTags.Num() > 0 && ASC->HasAnyMatchingGameplayTags(MovementBlockingTags))
			{
				return OutSpeed;
			}
			
			// Get speed from GAS: Base (units/sec) * Multiplier
			if (UOSAttributeSet* AttrSet = GetAttributeSet())
			{
				const float BaseAttr = AttrSet->GetMoveSpeedBase();
				const float EffectiveBase = (BaseAttr > KINDA_SMALL_NUMBER) ? BaseAttr : OutSpeed;
				float Mult = AttrSet->GetMovementSpeed();
				// Safety: ensure we can still move if multiplier wasn't initialized yet.
				if (Mult <= KINDA_SMALL_NUMBER)
				{
					Mult = 1.0f;
				}
				OutSpeed = EffectiveBase * Mult;
			}
		}
	}
	
	return OutSpeed;
}

// Called by engine; returns same value as CalculateMovementSpeed() so GAS attributes drive max speed.
float UOSCharacterMovementComponent::GetMaxSpeed() const
{
	return CalculateMovementSpeed();
}

// Called when MoveSpeedBase or MovementSpeed changes; refreshes MaxWalkSpeed and network smoothing distance.
void UOSCharacterMovementComponent::Internal_OnMovementSpeedChanged(const FOnAttributeChangeData& Data)
{
	RefreshMovementSpeed();
	
	// Dynamic network smoothing based on movement speed.
	// IMPORTANT: Use GAS MoveSpeedBase as the baseline when available.
	// Using the fallback BaseWalkSpeed here can produce inconsistent smoothing when attributes differ from fallback.
	float BaselineSpeed = BaseWalkSpeed;
	if (UOSAttributeSet* AttrSet = GetAttributeSet())
	{
		const float BaseAttr = AttrSet->GetMoveSpeedBase();
		if (BaseAttr > KINDA_SMALL_NUMBER)
		{
			BaselineSpeed = BaseAttr;
		}
	}
	BaselineSpeed = FMath::Max(1.0f, BaselineSpeed);

	const float SpeedModifier = MaxWalkSpeed / BaselineSpeed;
	const float SpeedBasedSmooth = FMath::Clamp(256.0f * SpeedModifier, 256.0f, 590.0f);

	// Use the higher of speed-based or constructor floor — combat warps need the larger window.
	NetworkMaxSmoothUpdateDistance = FMath::Max(SpeedBasedSmooth, 500.0f);
	NetworkNoSmoothUpdateDistance = NetworkMaxSmoothUpdateDistance * 1.5f;
}

// Server-side: during attacks, trust the client's predicted position instead of correcting it.
// Motion warps drive the character to a destination computed independently on client and server.
// Small position differences (latency) cause the server to send corrections that fight the warp,
// producing rubberbanding. Skipping the error check here prevents the correction from being sent.
// Uses IsAttacking (ActivationOwnedTags on combo attacks) — no new tags or replication needed.
//
// NOTE: This replaces the previous client-side approach (SmoothClientPosition + ClientAdjustPosition
// overrides). Those suppressed corrections AFTER the server sent them — wasting bandwidth on
// corrections that were immediately discarded and leaving the client with stale ServerLocation state.
// ServerCheckClientError is the engine's designed extension point: it prevents corrections from
// being generated in the first place, so no wasted RPCs and no stale state on the client.
bool UOSCharacterMovementComponent::ServerCheckClientError(
	float ClientTimeStamp, float DeltaTime, const FVector& Accel,
	const FVector& ClientWorldLocation, const FVector& RelativeClientLocation,
	UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	if (CharacterOwner)
	{
		if (const UAbilitySystemComponent* ASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CharacterOwner))
		{
			if (ASC->HasMatchingGameplayTag(FOSGameplayTags::Get().IsAttacking))
			{
				return false;
			}
		}
	}

	// #19: Right after IsAttacking clears, a single ServerCheckClientError can fire a large
	// ClientAdjustPosition (error stacked while corrections were suppressed). Brief grace lets
	// normal ServerMove packets narrow the gap first.
	if (PostAttackServerCorrectionGraceUntilTime > 0.f)
	{
		if (const UWorld* World = GetWorld())
		{
			const float Now = World->GetTimeSeconds();
			if (Now < PostAttackServerCorrectionGraceUntilTime)
			{
				return false;
			}
		}
		PostAttackServerCorrectionGraceUntilTime = -1.f;
	}

	return Super::ServerCheckClientError(ClientTimeStamp, DeltaTime, Accel,
		ClientWorldLocation, RelativeClientLocation,
		ClientMovementBase, ClientBaseBoneName, ClientMovementMode);
}

