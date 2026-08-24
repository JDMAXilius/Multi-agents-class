#include "Animation/BNLAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"

bool UBNLAnimInstance::ComputeHasAcceleration(bool bAIControlled, const FVector& InputAcceleration,
	const FVector& RequestedVelocity)
{
	// OR, not a branch: a bot that both paths AND adds input (a strafe task) must still read
	// true, and an OR can never be less true than the single read this replaced.
	return !InputAcceleration.IsNearlyZero()
		|| (bAIControlled && !RequestedVelocity.IsNearlyZero());
}

void UBNLAnimInstance::ResolveOwner()
{
	if (!OwningCharacter)
	{
		OwningCharacter = Cast<ABNCharacter>(TryGetPawnOwner());
	}
	if (OwningCharacter)
	{
		OwningMovementComponent = OwningCharacter->GetCharacterMovement();
		MeshComponent = OwningCharacter->GetMesh();
	}
}

/** THE DYNAMIC HALF. A player and a bot reach the same CMC by different roads, and the anim
 *  graph has to know which road to read. Cheap enough to run every frame, and it MUST run every
 *  frame — see the header for why a cached answer is a respawn bug.
 *
 *  Note it deliberately asks the CONTROLLER, not the pawn's class or a spawn-time flag: a pawn
 *  is a body, and who is holding it is a runtime fact.
 */
void UBNLAnimInstance::ResolveOwnerDriver()
{
	const AController* Controller = OwningCharacter ? OwningCharacter->GetController() : nullptr;

	// Unpossessed (a corpse, or a pawn between Possess calls) is neither, and must not fall
	// through to the player branch — an unpossessed pawn has no input and no path request.
	bIsPlayerControlled = Controller && Controller->IsPlayerController();
	bIsAIControlled = Controller && !Controller->IsPlayerController();
	bLocallyControlled = OwningCharacter && OwningCharacter->IsLocallyControlled();
	bFPSMode = bLocallyControlled && bIsPlayerControlled;

	bCachedAIControlled = bIsAIControlled;
}

void UBNLAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	ResolveOwner();
	ResolveOwnerDriver();
	BindAbilitySystem();
}

void UBNLAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!bAbilitySystemBound)
	{
		ResolveOwner();
		BindAbilitySystem();
	}

	if (!OwningMovementComponent)
	{
		return;
	}

	ResolveOwnerDriver();

	CachedVelocity = OwningMovementComponent->Velocity;
	CachedAcceleration = OwningMovementComponent->GetCurrentAcceleration();
	CachedRequestedVelocity = OwningMovementComponent->GetLastUpdateRequestedVelocity();
	CachedActorRotation = OwningCharacter ? OwningCharacter->GetActorRotation() : FRotator::ZeroRotator;
	bCachedFalling = OwningMovementComponent->IsFalling();
	bCachedOnGround = OwningMovementComponent->IsMovingOnGround();
	bCachedCrouching = GameplayTag_IsCrouching || (OwningCharacter && OwningCharacter->bIsCrouched);
	CachedGroundDistance = bCachedOnGround ? OwningMovementComponent->CurrentFloor.FloorDist : -1.f;

	// The ADS lens. Here and not in the thread-safe pass because it touches the camera component,
	// which is game-thread only — NativeUpdateAnimation is the game thread, and RESEARCH-ADS §3
	// names it as the lawful home for this interp precisely so no gameplay Tick has to exist.
	// The gate is the same one UBNAnimInstance uses: owner-only, and a player, never a bot.
	if (OwningCharacter)
	{
		ADSCameraBlend.Update(OwningCharacter->GetFirstPersonCamera(), GameplayTag_IsADS, bFPSMode, DeltaSeconds);
	}
}

void UBNLAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	Velocity = CachedVelocity;
	const FVector Velocity2D(Velocity.X, Velocity.Y, 0.0);
	LocalVelocity2D = CachedActorRotation.UnrotateVector(Velocity2D);
	DisplacementSpeed = Velocity2D.Size();
	HasVelocity = !Velocity2D.IsNearlyZero();

	// HasAcceleration IS NOT ONE QUESTION. Lyra's locomotion state machine leaves Idle on it, so
	// whatever answers it has to mean "this pawn is being told to move" for every kind of owner:
	//
	//   player  — AddMovementInput -> ConsumeInputVector -> CMC::Acceleration. Reads correctly.
	//   bot     — MoveTo* -> PathFollowing -> RequestDirectMove -> CMC::RequestedVelocity.
	//             CalcVelocity applies that through a LOCAL RequestedAcceleration and never
	//             assigns the member Acceleration, so GetCurrentAcceleration() is EXACTLY ZERO
	//             for a pathing bot. That is why bots slid: full velocity, Idle pose, forever.
	//   proxy   — a client's copy of anyone else. The engine already covers this in
	//             UpdateProxyAcceleration(): unreplicated acceleration is synthesised as
	//             Velocity.GetSafeNormal(), so the acceleration read is right there too.
	//
	// Which makes this an AUTHORITY-ONLY bug — it shows on the listen server and in standalone
	// PIE, and does NOT reproduce on a remote client watching the same bot. Do not "verify" the
	// fix from a client window and conclude anything.
	//
	HasAcceleration = ComputeHasAcceleration(bCachedAIControlled, CachedAcceleration, CachedRequestedVelocity);
	IsFalling = bCachedFalling;
	IsOnGround = bCachedOnGround;
	isCrouching = bCachedCrouching;
	GroundDistance = CachedGroundDistance;
}

void UBNLAnimInstance::NativeUninitializeAnimation()
{
	UnbindAbilitySystem();
	Super::NativeUninitializeAnimation();
}

void UBNLAnimInstance::BindAbilitySystem()
{
	if (bAbilitySystemBound)
	{
		return;
	}

	UAbilitySystemComponent* ASC = OwningCharacter
		? OwningCharacter->GetAbilitySystemComponent()
		: UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningActor());
	if (!ASC)
	{
		return;
	}

	AbilitySystem = ASC;

	const TPair<FGameplayTag, bool*> Rows[] = {
		{ BNTags::State_Weapon_ADS,            &GameplayTag_IsADS },
		{ BNTags::State_Weapon_Firing,         &GameplayTag_IsFiring },
		{ BNTags::State_Weapon_Reloading,      &GameplayTag_IsReloading },
		{ BNTags::State_Movement_Sprinting,    &GameplayTag_IsDashing },
		{ BNTags::State_Weapon_Melee,          &GameplayTag_IsMelee },
		{ BNTags::State_Movement_Crouching,    &GameplayTag_IsCrouching },
		{ BNTags::State_Dead,                  &GameplayTag_IsDead },
	};

	TagBools.Reset(UE_ARRAY_COUNT(Rows));
	for (const TPair<FGameplayTag, bool*>& Row : Rows)
	{
		FTagBool Bound;
		Bound.Tag = Row.Key;
		Bound.Value = Row.Value;
		Bound.Handle = ASC->RegisterGameplayTagEvent(Row.Key, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UBNLAnimInstance::OnTagChanged);
		*Row.Value = ASC->HasMatchingGameplayTag(Row.Key);
		TagBools.Add(Bound);
	}

	bAbilitySystemBound = true;
}

void UBNLAnimInstance::UnbindAbilitySystem()
{
	if (AbilitySystem)
	{
		for (FTagBool& Bound : TagBools)
		{
			AbilitySystem->UnregisterGameplayTagEvent(Bound.Handle, Bound.Tag, EGameplayTagEventType::NewOrRemoved);
		}
	}

	TagBools.Reset();
	AbilitySystem = nullptr;
	bAbilitySystemBound = false;
}

void UBNLAnimInstance::OnTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const bool bActive = NewCount > 0;
	for (FTagBool& Bound : TagBools)
	{
		if (Bound.Tag == Tag)
		{
			*Bound.Value = bActive;
			return;
		}
	}
}
