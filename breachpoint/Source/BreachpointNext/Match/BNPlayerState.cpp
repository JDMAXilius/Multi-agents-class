#include "Match/BNPlayerState.h"
#include "AbilitySystem/BNAbilitySet.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "AbilitySystem/Abilities/BNGA_Death.h"
#include "AbilitySystem/Abilities/BNGA_Equip.h"
#include "AbilitySystem/Abilities/BNGA_ADS.h"
#include "AbilitySystem/Abilities/BNGA_Grapple.h"
#include "AbilitySystem/Abilities/BNGA_Grenade.h"
#include "AbilitySystem/Abilities/BNGA_HitReact.h"
#include "AbilitySystem/Abilities/BNGA_Melee.h"
#include "AbilitySystem/Abilities/BNMovementAbilities.h"
#include "Core/BNGameplayTags.h"
#include "BreachpointNext.h"
#include "Net/UnrealNetwork.h"

ABNPlayerState::ABNPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UBNAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UBNAttributeSet>(TEXT("AttributeSet"));

	InitEffect = UBNGE_InitAttributes::StaticClass();

	SetNetUpdateFrequency(100.f);
}

void ABNPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABNPlayerState, Kills);
	DOREPLIFETIME(ABNPlayerState, Deaths);
	DOREPLIFETIME(ABNPlayerState, ObjectivePoints);
	// COND_None on purpose: team membership is HUD-grade — the scoreboard shows it to
	// everyone — and nothing positional rides this byte (the packet's security audit).
	DOREPLIFETIME(ABNPlayerState, TeamId);
	DOREPLIFETIME_CONDITION(ABNPlayerState, RespawnAtServerTime, COND_OwnerOnly);
}

void ABNPlayerState::SetGenericTeamId(const FGenericTeamId& NewTeamId)
{
	// Authority only, and there is NO Server RPC behind this — the GameMode's assignment seam
	// is the only intended caller, so a client calling it writes a local byte the server never
	// hears (the degenerate cheat case the spec plan pins).
	if (HasAuthority() && TeamId != NewTeamId)
	{
		TeamId = NewTeamId;
		// The authority runs no OnRep, and a listen host's own HUD is a subscriber like any
		// client's — the Kills idiom, third application; the OnRep body is the one broadcaster.
		OnRep_TeamId();
	}
}

void ABNPlayerState::AddKill()
{
	if (HasAuthority())
	{
		++Kills;
		// The authority runs no OnRep, and a listen host's own HUD is a subscriber like any
		// client's — the R4 GameState discipline, applied here.
		OnScoreChanged.Broadcast(this);
	}
}

void ABNPlayerState::AddDeath()
{
	if (HasAuthority())
	{
		++Deaths;
		OnScoreChanged.Broadcast(this);
	}
}

void ABNPlayerState::AddObjectivePoints(int32 Points)
{
	if (HasAuthority() && Points > 0)
	{
		ObjectivePoints += Points;
		OnScoreChanged.Broadcast(this);
	}
}

void ABNPlayerState::ResetScore()
{
	if (HasAuthority())
	{
		Kills = 0;
		Deaths = 0;
		ObjectivePoints = 0;
		// TeamId survives on purpose: a restart resets the NUMBERS, not the sides — clearing
		// it here would send everyone back through assignment mid-lobby for no reason.
		OnScoreChanged.Broadcast(this);
	}
}

void ABNPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	// The score and the side, and ONLY those. Not the respawn stamp (a stamp from the previous
	// map's clock is worse than none), not the ASC or its attributes — those are rebuilt by
	// GrantDefaults and the init GE on the new PlayerState, which is the one path allowed to set
	// them. TeamId rides along because seamless travel builds a NEW PlayerState: uncarried, a
	// map rotation would silently reset every team to NoTeam — a scoreboard bug in disguise.
	if (ABNPlayerState* Copy = Cast<ABNPlayerState>(PlayerState))
	{
		Copy->Kills = Kills;
		Copy->Deaths = Deaths;
		// ALL THREE score ints, not two: GetScore() = Kills + ObjectivePoints, and the
		// REFUTER (BN15 N2) caught this function carrying the side but dropping the hill
		// seconds — a travel with the hill on would zero every objective point while
		// kills survived, which reads as a scoreboard bug and is a lifecycle one.
		Copy->ObjectivePoints = ObjectivePoints;
		Copy->TeamId = TeamId;
	}
}

void ABNPlayerState::SetRespawnAtServerTime(double InServerTime)
{
	if (HasAuthority() && RespawnAtServerTime != InServerTime)
	{
		RespawnAtServerTime = InServerTime;
		OnRespawnStampChanged.Broadcast(this);
	}
}

// The OnReps are "the scoreboard binds to them later", said 17 Aug — later arrived: they now
// broadcast, and the Verbose lines stay as the no-HUD way to see the number land.
void ABNPlayerState::OnRep_Kills()
{
	UE_LOG(LogBN, Verbose, TEXT("BNPlayerState: %s kills -> %d"), *GetPlayerName(), Kills);
	OnScoreChanged.Broadcast(this);
}

void ABNPlayerState::OnRep_ObjectivePoints()
{
	UE_LOG(LogBN, Verbose, TEXT("BNPlayerState: %s objective -> %d"), *GetPlayerName(), ObjectivePoints);
	OnScoreChanged.Broadcast(this);
}

void ABNPlayerState::OnRep_Deaths()
{
	UE_LOG(LogBN, Verbose, TEXT("BNPlayerState: %s deaths -> %d"), *GetPlayerName(), Deaths);
	OnScoreChanged.Broadcast(this);
}

void ABNPlayerState::OnRep_RespawnAtServerTime()
{
	OnRespawnStampChanged.Broadcast(this);
}

// Cosmetic only (law 3): deleting this body changes no gameplay outcome — combat asks
// GetGenericTeamId through BNTeams, never this notify. The broadcast is for the HUD, which
// derives friendly/enemy RELATIVE to the local player; the Verbose line is the no-HUD way
// to watch an id land.
void ABNPlayerState::OnRep_TeamId()
{
	UE_LOG(LogBN, Verbose, TEXT("BNPlayerState: %s team -> %d"), *GetPlayerName(), TeamId.GetId());
	OnTeamChanged.Broadcast(this);
}

UAbilitySystemComponent* ABNPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABNPlayerState::ApplyInitAttributes()
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative() || !InitEffect)
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(InitEffect, 1.f, AbilitySystemComponent->MakeEffectContext());
	if (SpecHandle.IsValid())
	{
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void ABNPlayerState::BroadcastDeath(ABNPlayerState* Killer, FName SourceName)
{
	// Authority only: the GameMode is the subscriber and only exists there, and a client
	// broadcasting a death would be a second source of truth for the one thing that is entirely
	// the server's.
	if (AbilitySystemComponent && AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		OnPlayerDeath.Broadcast(this, Killer, SourceName);
	}
}

void ABNPlayerState::GrantDefaults()
{
	if (bDefaultsGranted || !AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}
	bDefaultsGranted = true;

	ApplyInitAttributes();

	// Granted OUTSIDE the set/fallback split and with no input tag: death is not a key and not a
	// weapon's verb. UBNHealthComponent's verdict activates it by class, on the authority.
	AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UBNGA_Death::StaticClass(), 1));

	// Body verbs, granted OUTSIDE the set/fallback split on purpose: melee and grenade must exist
	// whether or not DefaultAbilitySet is configured, must survive every weapon swap, and must not
	// wait on an editor edit to two DA_BNAbilitySet assets to become reachable. Melee still reads
	// its montage, damage and reach from the CURRENT weapon's row.
	// If either later joins DefaultAbilitySet, delete it here — two grants means two specs.
	FGameplayAbilitySpec MeleeSpec(UBNGA_Melee::StaticClass(), 1);
	MeleeSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Melee);
	AbilitySystemComponent->GiveAbility(MeleeSpec);

	FGameplayAbilitySpec GrenadeSpec(UBNGA_Grenade::StaticClass(), 1);
	GrenadeSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Grenade);
	AbilitySystemComponent->GiveAbility(GrenadeSpec);

	FGameplayAbilitySpec ADSSpec(UBNGA_ADS::StaticClass(), 1);
	ADSSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Weapon_ADS);
	AbilitySystemComponent->GiveAbility(ADSSpec);

	// BN23 — the Grappleshot, a body verb like the three above: granted here so no
	// ability-set asset edit gates it, survives every weapon swap.
	FGameplayAbilitySpec GrappleSpec(UBNGA_Grapple::StaticClass(), 1);
	GrappleSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Grapple);
	AbilitySystemComponent->GiveAbility(GrappleSpec);

	// No input tag: nobody presses "flinch". UBNHealthComponent activates it by class when health
	// drops and stays above zero — death's shape, one delegate over.
	AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UBNGA_HitReact::StaticClass(), 1));

	if (DefaultAbilitySet)
	{
		FBNAbilitySetHandles Handles;
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, Handles);
	}
	else
	{
		FGameplayAbilitySpec JumpSpec(UBNGA_Jump::StaticClass(), 1);
		JumpSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Jump);
		AbilitySystemComponent->GiveAbility(JumpSpec);

		FGameplayAbilitySpec CrouchSpec(UBNGA_Crouch::StaticClass(), 1);
		CrouchSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Crouch);
		AbilitySystemComponent->GiveAbility(CrouchSpec);

		// Swap is the INVENTORY's verb, not any weapon's. Granted by the weapon's set it
		// would clear its own spec by running — the next press inside a round trip carries a
		// dead handle and is dropped — and a row with a null AbilitySet would leave no swap
		// spec at all, locking the player to that weapon for the life.
		FGameplayAbilitySpec SwapNextSpec(UBNGA_SwapNext::StaticClass(), 1);
		SwapNextSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Weapon_Next);
		AbilitySystemComponent->GiveAbility(SwapNextSpec);

		FGameplayAbilitySpec SwapPreviousSpec(UBNGA_SwapPrevious::StaticClass(), 1);
		SwapPreviousSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Weapon_Previous);
		AbilitySystemComponent->GiveAbility(SwapPreviousSpec);

		// Character verbs, not weapon verbs: sprint and lean belong to the body and must survive
		// every weapon swap, so they are granted here and never by a weapon's ability set.
		FGameplayAbilitySpec SprintSpec(UBNGA_Sprint::StaticClass(), 1);
		SprintSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Sprint);
		AbilitySystemComponent->GiveAbility(SprintSpec);

		FGameplayAbilitySpec LeanLeftSpec(UBNGA_LeanLeft::StaticClass(), 1);
		LeanLeftSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Lean_Left);
		AbilitySystemComponent->GiveAbility(LeanLeftSpec);

		FGameplayAbilitySpec LeanRightSpec(UBNGA_LeanRight::StaticClass(), 1);
		LeanRightSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Lean_Right);
		AbilitySystemComponent->GiveAbility(LeanRightSpec);
	}
}
