// Breachpoint. The PlayerState: the ASC and the attribute set live here.

#include "Match/BRPlayerState.h"

#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/BRAttributeSet.h"
#include "Core/BRCore.h"

namespace
{
	/**
	 * PlayerState replication frequency, Hz. §5.4: "PlayerState NetUpdateFrequency raised (default
	 * is 1 Hz — far too slow for an FPS scoreboard/ASC host)."
	 *
	 * NOT a gameplay tuning number and deliberately not in a CSV: it changes how often a fighter's
	 * shield bar and tag state reach observers, and nothing about how much damage anything does.
	 * Moving it to data would invite a designer to "tune" the network. It is a netcode setting and
	 * netcode-builder reviews it (this ticket's step 1 says so explicitly).
	 *
	 * At 1 Hz the default would mean an enemy's shields could read a full second stale — long
	 * enough to change a duel's outcome from the observer's side.
	 */
	constexpr float BRPlayerStateNetUpdateHz = 100.f;
}

ABRPlayerState::ABRPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Law 4: no gameplay Tick. Nothing here polls; the ASC is event-driven and the attribute set
	// reacts to effect execution.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// The ASC. A DEFAULT SUBOBJECT, so it exists on every instance from construction — before any
	// possession, replication or ability grant can look for it. A lazily created ASC races the very
	// first InitAbilityActorInfo and loses on a slow client.
	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<UBRAbilitySystemComponent>(this, TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Mixed is also set in UBRAbilitySystemComponent's own constructor; it is repeated here because
	// this is the site §5.4 describes, and because a future non-player ASC of the same class (a
	// destructible objective, say) should be able to choose differently WITHOUT editing the
	// component class. Belt and braces on the one setting whose failure mode is "works perfectly in
	// PIE, floods the wire in a real match".
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// THE attribute set. A subobject of the PlayerState; the ASC finds and registers it.
	AttributeSet = ObjectInitializer.CreateDefaultSubobject<UBRAttributeSet>(this, TEXT("AttributeSet"));

	// §5.4's raise. See the constant's comment for why the number lives here and not in a CSV.
	SetNetUpdateFrequency(BRPlayerStateNetUpdateHz);
}

UAbilitySystemComponent* ABRPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABRPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Owner = this PlayerState, Avatar = nothing yet.
	//
	// This is the FIRST of the three InitAbilityActorInfo calls in the project and the only one
	// that is not about a pawn: it establishes the OWNER, so the ASC is usable (grants, effects,
	// attribute reads) during the window between "player joined" and "player has a body" — which is
	// exactly when GameMode wants to grant a loadout. ABRCharacter's two calls then re-point the
	// AVATAR at each new pawn without disturbing anything granted here.
	//
	// The other two live in ABRCharacter::PossessedBy and ABRCharacter::OnRep_PlayerState. All
	// three are required; any two of them leave a real failure that PIE hides.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, /*AvatarActor=*/nullptr);

		UE_LOG(LogBRCombat, Verbose, TEXT("BRPlayerState '%s': ASC owner set (avatar still null; a pawn will claim it on possess). NetUpdateFrequency %.0f Hz."),
			*GetName(), GetNetUpdateFrequency());
	}
	else
	{
		// Unreachable via the constructor above; reported rather than assumed away, because a
		// missing ASC here means every ability in the match silently fails to grant.
		UE_LOG(LogBRCombat, Error, TEXT("BRPlayerState '%s': no AbilitySystemComponent after construction. Nothing in the combat system will work for this player."),
			*GetName());
	}
}
