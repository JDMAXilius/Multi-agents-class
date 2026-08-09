#include "Characters/OSPunchingBag.h"
#include "Net/UnrealNetwork.h"

#include "Core/OSPlayerState.h"
#include "GameFramework/Controller.h"

#if !UE_BUILD_SHIPPING
#include "Animation/AnimInstance.h"
#include "Core/OSDebugGlobals.h"
#include "Data/OSGameplayTags.h"
#include "Data/OSLoadoutDataAsset.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "OSLogCategories.h"
#endif

AOSPunchingBag::AOSPunchingBag(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	ACharacter::SetReplicateMovement(true);
	// Mesh, AnimBP, collision, and posture are authored on BP_OSPunchingBag.
	// Controller-less CMC adjustment happens in ConfigureForUnpossessed() from BeginPlay.
}

void AOSPunchingBag::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AOSPunchingBag, bUseTdmDebugTeam);
	DOREPLIFETIME(AOSPunchingBag, TdmDebugTeamId);
}

FGenericTeamId AOSPunchingBag::GetGenericTeamId() const
{
	if (bUseTdmDebugTeam)
	{
		return TdmDebugTeamId;
	}
	return AOSCharacter::GetGenericTeamId();
}

void AOSPunchingBag::SetDebugTdmTeamFromSpawner(AController* SpawnerController, bool bEnemyTeam)
{
	if (!HasAuthority())
	{
		return;
	}

	FGenericTeamId SpawnerTeam = FGenericTeamId::NoTeam;
	if (AOSPlayerState* PS = SpawnerController ? SpawnerController->GetPlayerState<AOSPlayerState>() : nullptr)
	{
		SpawnerTeam = PS->GetGenericTeamId();
	}

	if (SpawnerTeam == FGenericTeamId::NoTeam)
	{
		// FFA: no ally team to mirror; enemy mode still picks a concrete team so damage works like vs team 1.
		if (bEnemyTeam)
		{
			bUseTdmDebugTeam = true;
			TdmDebugTeamId = FGenericTeamId(static_cast<uint8>(1));
		}
		else
		{
			bUseTdmDebugTeam = false;
			TdmDebugTeamId = FGenericTeamId::NoTeam;
		}
		return;
	}

	bUseTdmDebugTeam = true;
	if (bEnemyTeam)
	{
		const uint8 Id = SpawnerTeam.GetId();
		TdmDebugTeamId = FGenericTeamId(static_cast<uint8>(Id == 0 ? 1 : 0));
	}
	else
	{
		TdmDebugTeamId = SpawnerTeam;
	}
}

void AOSPunchingBag::BeginPlay()
{
	Super::BeginPlay();

#if !UE_BUILD_SHIPPING
	// Cache the BP-authored mesh transform so Local_ResetStateForRespawn can return to it after
	// any reset path without needing to hardcode project-standard values here.
	if (USkeletalMeshComponent* MyMesh = GetMesh())
	{
		InitialMeshRelativeTransform = MyMesh->GetRelativeTransform();
	}

	if (HasAuthority() && !GetController())
	{
		ConfigureForUnpossessed();
	}

	// Grant abilities and effects via the authored loadout. Super::BeginPlay already ran
	// InitializeGAS (from AOSAICharacter::BeginPlay), so AbilitySystemComponent is owner-set
	// to this actor and ready to receive grants. Players route through PS->GiveStartupLoadout;
	// the bag has no PS, so it calls the same data-asset apply directly on its own ASC.
	if (HasAuthority() && AbilitySystemComponent && DebugLoadout)
	{
		TArray<FGameplayAbilitySpecHandle> UnusedAbilityHandles;
		TArray<FActiveGameplayEffectHandle> UnusedEffectHandles;
		FGameplayTagContainer UnusedLooseTags;
		DebugLoadout->GiveToAbilitySystemComponent(
			AbilitySystemComponent, this, this,
			UnusedAbilityHandles, UnusedEffectHandles, UnusedLooseTags);
	}

	// Event-driven respawn: listen for IsDead tag grant on our ASC. Works regardless of which
	// code path applies the death state (GA_OSDeath, direct GE apply, future AI death flow).
	if (HasAuthority() && AbilitySystemComponent)
	{
		const FGameplayTag& IsDeadTag = FOSGameplayTags::Get().IsDead;
		if (IsDeadTag.IsValid())
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(IsDeadTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AOSPunchingBag::OnIsDeadTagChanged);
		}
	}
#endif
}

#if !UE_BUILD_SHIPPING

void AOSPunchingBag::ConfigureForUnpossessed()
{
	// CMC without a controller: Character::PerformMovement is skipped unless this flag is set,
	// which would silently drop root motion from the death/hit-react montages. BP_OSPunchingBag
	// owns every other CMC default; keep this single override here so the BP never has to know
	// about the unpossessed-pawn edge case.
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bRunPhysicsWithNoController = true;
	}
}

void AOSPunchingBag::OnIsDeadTagChanged(const FGameplayTag ChangedTag, int32 NewCount)
{
	if (!HasAuthority() || NewCount <= 0)
	{
		return;
	}
	if (RespawnTimerHandle.IsValid())
	{
		// Already scheduled — re-entry from duplicate death broadcasts shouldn't reset the clock.
		return;
	}
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AOSPunchingBag::Respawn,
	                                 AutoRespawnDelay, false);

	if (GDebugPunchingBag)
	{
		UE_LOG(LogOSDebug, Verbose,
			TEXT("[BAG] Death tag '%s' granted on %s — respawn scheduled in %.2fs."),
			*ChangedTag.ToString(), *GetName(), AutoRespawnDelay);
	}
}

void AOSPunchingBag::Respawn()
{
	RespawnTimerHandle.Invalidate();

	if (GDebugPunchingBag)
	{
		UE_LOG(LogOSDebug, Verbose, TEXT("[BAG] Respawn firing on %s."), *GetName());
	}

	// Clear death state before multicast so clients don't re-trigger death from a stale IsDead tag.
	if (AbilitySystemComponent)
	{
		const FOSGameplayTags& OSTags = FOSGameplayTags::Get();

		FGameplayTagContainer DeathTags;
		DeathTags.AddTag(OSTags.IsDead);
		AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(DeathTags);

		if (OSTags.IsDead.IsValid())
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(OSTags.IsDead);
		}

		AbilitySystemComponent->CancelAllAbilities();
	}

	Multicast_Respawn();
}

#endif // !UE_BUILD_SHIPPING

void AOSPunchingBag::Multicast_Respawn_Implementation()
{
#if !UE_BUILD_SHIPPING
	// GA_OSDeath plays its death montage with bStopWhenAbilityEnds=false, so cancelling the
	// ability in Respawn doesn't stop the anim. Without this the mesh stays stuck in the death
	// pose even though attributes/tags are reset — the bag is functionally alive but looks dead
	// until the next hit-react montage unsticks the AnimBP.
	if (USkeletalMeshComponent* MyMesh = GetMesh())
	{
		if (UAnimInstance* AnimInst = MyMesh->GetAnimInstance())
		{
			AnimInst->StopAllMontages(0.1f);
		}
	}

	// Reverse ragdoll damage and re-pin mesh posture.
	Local_ResetStateForRespawn();

	// Reset attributes so the bag doesn't immediately re-trigger death. TODO: move to an Instant
	// GE (`GE_OSResetAttributes`) when the real AI character path needs the same thing — raw
	// setters bypass PostGameplayEffectExecute and don't replicate cleanly.
	if (UOSAttributeSet* AS = Cast<UOSAttributeSet>(AttributeSet))
	{
		AS->SetHealth(AS->GetMaxHealth());
		AS->SetStamina(AS->GetMaxStamina());
		AS->SetAura(AS->GetMaxAura());
	}
#endif
}

void AOSPunchingBag::Local_ResetStateForRespawn()
{
	// Base reverses Multicast_GoRagdoll (physics, collision, movement mode).
	Super::Local_ResetStateForRespawn();

#if !UE_BUILD_SHIPPING
	// Re-pin the mesh to its authored BP-default transform captured at BeginPlay.
	if (USkeletalMeshComponent* MyMesh = GetMesh())
	{
		MyMesh->SetRelativeTransform(InitialMeshRelativeTransform);
		MyMesh->RefreshBoneTransforms();
	}
#endif
}
