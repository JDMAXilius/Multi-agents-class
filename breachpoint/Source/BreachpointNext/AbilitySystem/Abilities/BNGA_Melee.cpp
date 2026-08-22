#include "AbilitySystem/Abilities/BNGA_Melee.h"

#include "AbilitySystem/Effects/BNDamage.h"
#include "AbilitySystem/Tasks/BNAbilityTask_ServerWaitClientTargetData.h"
#include "BreachpointNext.h"
#include "Characters/BNCharacter.h"
#include "Core/BNCollision.h"
#include "Core/BNGameplayTags.h"
#include "Data/BNDataRows.h"
#include "Data/BNGameData.h"
#include "Weapons/BNEquipmentComponent.h"
#include "Weapons/BNWeapon.h"
#include "Engine/GameInstance.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

namespace
{
	// The server's allowance when judging a swing claim — the price of a round trip, not tuning.
	// Tighter than Fire's because a melee reach is ~150uu, not 10000: the same 200uu slack that is
	// noise on a rifle shot would be a free extra metre of reach here.
	constexpr float BNMeleeDistanceTolerance = 60.f;
	constexpr float BNMeleeAngleTolerance = 30.f;

	ABNWeapon* BNMeleeGetWeapon(const FGameplayAbilityActorInfo* ActorInfo)
	{
		const ABNCharacter* Character = ActorInfo ? Cast<ABNCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
		const UBNEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
		return Equipment ? Equipment->GetCurrentWeapon() : nullptr;
	}

	const FBNWeaponRow* BNMeleeGetRow(const FGameplayAbilityActorInfo* ActorInfo)
	{
		if (const ABNWeapon* Weapon = BNMeleeGetWeapon(ActorInfo))
		{
			if (const FBNWeaponRow* Row = Weapon->GetRow())
			{
				return Row;
			}
		}

		// Empty-hand slot: an optional DT row named Unarmed supplies punch numbers/montage.
		const UWorld* World = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const UBNGameData* GameData = GameInstance ? GameInstance->GetSubsystem<UBNGameData>() : nullptr;
		return GameData ? GameData->FindWeaponRow(FName(TEXT("Unarmed"))) : nullptr;
	}

	struct FBNMeleeStats
	{
		float Damage = 40.f;
		float Range = 120.f;
		TSoftObjectPtr<UAnimMontage> Montage;
	};

	FBNMeleeStats BNMeleeResolveStats(const FBNWeaponRow* Row, float UnarmedDamage, float UnarmedRange, const TSoftObjectPtr<UAnimMontage>& UnarmedMontage)
	{
		FBNMeleeStats Stats;
		if (Row)
		{
			Stats.Damage = Row->MeleeDamage;
			Stats.Range = Row->MeleeRange;
			Stats.Montage = Row->MeleeMontage;
			return Stats;
		}
		Stats.Damage = UnarmedDamage;
		Stats.Range = UnarmedRange;
		Stats.Montage = UnarmedMontage;
		return Stats;
	}
}

void UBNGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bSwung = false;
	MeleeHandle = ApplyStateTag(BNTags::State_Weapon_Melee);

	const FBNWeaponRow* Row = BNMeleeGetRow(ActorInfo);
	const FBNMeleeStats Stats = BNMeleeResolveStats(Row, UnarmedMeleeDamage, UnarmedMeleeRange, UnarmedMeleeMontage);
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UWorld* World = GetWorld();
	if (!ASC || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Through the ASC so the swing replicates: simulated proxies must see the arm move, otherwise
	// a melee kill arrives with no animation attached to it.
	float MontageLength = 0.f;
	UAnimMontage* Montage = Stats.Montage.IsNull() ? nullptr : Stats.Montage.LoadSynchronous();
	if (Montage)
	{
		MontageLength = ASC->PlayMontage(this, ActivationInfo, Montage, 1.f);
	}

	// One line per swing, naming the link that is dead — "no melee attack happening" has been
	// reported twice with the log silent, and a silent no-op is indistinguishable from a dead key.
	if (!Montage)
	{
		UE_LOG(LogBN, Warning, TEXT("BNGA_Melee: MeleeMontage is unset or failed to load ('%s') — swinging on the fallback clock with no animation."),
			*Stats.Montage.ToString());
	}
	else if (MontageLength <= 0.f)
	{
		UE_LOG(LogBN, Warning, TEXT("BNGA_Melee: montage '%s' FAILED to play (length %.2f) — the ABP has no slot node for its slot, or another montage holds it."),
			*GetNameSafe(Montage), MontageLength);
	}
	else
	{
		UE_LOG(LogBN, Log, TEXT("BNGA_Melee: swing — montage '%s' (%.2fs)."), *GetNameSafe(Montage), MontageLength);
	}

	if (!ActorInfo->IsLocallyControlled())
	{
		// The authority's instance for a remote swinger: it judges, it never swings. Same reason
		// Fire's does — it must outlive the claim, and the client's EndAbility replicating is what
		// closes it, after the claim has landed on the same ordered channel.
		UBNAbilityTask_ServerWaitClientTargetData* Wait = UBNAbilityTask_ServerWaitClientTargetData::ServerWaitForClientTargetData(this, NAME_None, /*TriggerOnce=*/true);
		Wait->ValidData.AddDynamic(this, &UBNGA_Melee::OnTargetDataReceived);
		Wait->ReadyForActivation();

		// And a way out, which Fire does not need and melee does. Fire's remote instance is cleaned
		// up by the weapon set being revoked on pawn destruction; melee is a PlayerState grant with
		// no such revoke, so losing the client's EndAbility would leave this spec Active forever —
		// and AbilityInputTagPressed only fires on !Spec.IsActive(), which would kill melee for that
		// player for the rest of the match. Seconds, so an honest claim (~1 RTT) is never cut off.
		World->GetTimerManager().SetTimer(SwingTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			UE_LOG(LogBN, Verbose, TEXT("BNGA_Melee: authority timeout — the swinger's EndAbility never arrived."));
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}), FMath::Max(AuthorityTimeout, 1.f), /*bLoop=*/false);
		return;
	}

	// The connect window belongs to the animation. Bind the notify and wait for it — the swing is
	// NOT dealt here, which is the whole point of the notify (MyCharacter.cpp:1267-1276).
	if (UAnimInstance* Anim = (MontageLength > 0.f) ? ActorInfo->GetAnimInstance() : nullptr)
	{
		BoundAnimInstance = Anim;
		Anim->OnPlayMontageNotifyBegin.AddDynamic(this, &UBNGA_Melee::OnMontageNotifyBegin);
	}

	if (!BoundAnimInstance.IsValid())
	{
		// No montage, or no anim instance to hear it: swing immediately rather than swallow the
		// input silently — the reference does exactly this (MyCharacter.cpp:1926-1930).
		SwingTrace();
	}
	else
	{
		// The notify has a deadline. See FallbackConnectTime — if AN_FPST_Melee is a plain
		// UAnimNotify rather than a Play Montage Notify, the delegate above never fires and this
		// is the only thing between "melee connects" and "melee silently does nothing".
		World->GetTimerManager().SetTimer(ConnectTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!bSwung)
			{
				UE_LOG(LogBN, Verbose, TEXT("BNGA_Melee: notify '%s' did not fire — connecting on the fallback clock."), *ConnectNotifyName.ToString());
				SwingTrace();
			}
		}), FMath::Max(FallbackConnectTime, 0.01f), /*bLoop=*/false);
	}

	// The ability's own lifetime. With a montage this is the timeout that keeps a notify-less
	// montage from holding the ability open; without one it is simply the recovery.
	const float Lifetime = (MontageLength > 0.f) ? MontageLength : SwingDuration;
	World->GetTimerManager().SetTimer(SwingTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}), Lifetime, /*bLoop=*/false);
}

void UBNGA_Melee::OnMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	// Any other notify on any other montage is not ours — the reference gates on the name for the
	// same reason: one anim instance hears every montage this mesh plays.
	if (NotifyName == ConnectNotifyName)
	{
		SwingTrace();
	}
}

void UBNGA_Melee::SwingTrace()
{
	if (bSwung)
	{
		return;
	}
	bSwung = true;

	const FGameplayAbilityActorInfo* ActorInfo = CurrentActorInfo;
	ABNWeapon* Weapon = BNMeleeGetWeapon(ActorInfo);
	const FBNWeaponRow* Row = BNMeleeGetRow(ActorInfo);
	const FBNMeleeStats Stats = BNMeleeResolveStats(Row, UnarmedMeleeDamage, UnarmedMeleeRange, UnarmedMeleeMontage);
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UWorld* World = GetWorld();
	// The pawn's controller, not ActorInfo->PlayerController: null for a bot, and an
	// AIController's GetPlayerViewPoint is the pawn's eye view — a PlayerController's is unchanged.
	const APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const AController* ViewController = AvatarPawn ? AvatarPawn->GetController() : nullptr;
	if (!ASC || !World || !ViewController)
	{
		return;
	}

	FScopedPredictionWindow ScopedPrediction(ASC, CurrentActivationInfo.GetActivationPredictionKey());

	// The controller's view point, never the camera component — the camera's world transform sits
	// at the character's feet, which is the trap MyCharacter::GetAimRay documents (.cpp:1817-1862)
	// and which sent every trace into the floor. The reference's third-person branch off
	// Arrow_MeleeTraceStart has no BN equivalent and needs none: BN is first person.
	FVector ViewLocation;
	FRotator ViewRotation;
	ViewController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FCollisionQueryParams QueryParams(FName(TEXT("BNMeleeSwing")), /*bTraceComplex=*/false, ActorInfo->AvatarActor.Get());
	QueryParams.AddIgnoredActor(Weapon);

	FHitResult Hit;
	World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * Stats.Range, BNCollision::MeleeTrace, QueryParams);

	FGameplayAbilityTargetDataHandle TargetData;
	TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(Hit));

	if (ActorInfo->IsNetAuthority())
	{
		// Listen-server host swinging its own pawn: it already holds the truth.
		OnTargetDataReceived(TargetData);
		return;
	}

	ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), TargetData, FGameplayTag(), ASC->ScopedPredictionKey);
}

void UBNGA_Melee::OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& TargetData)
{
	const FGameplayAbilityActorInfo* ActorInfo = CurrentActorInfo;
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	ABNWeapon* Weapon = BNMeleeGetWeapon(ActorInfo);
	const FBNWeaponRow* Row = BNMeleeGetRow(ActorInfo);
	const FBNMeleeStats Stats = BNMeleeResolveStats(Row, UnarmedMeleeDamage, UnarmedMeleeRange, UnarmedMeleeMontage);
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	UWorld* World = GetWorld();
	// Same seam as SwingTrace's: the pawn's controller answers the view for human and bot alike.
	const APawn* AvatarPawn = Cast<APawn>(Avatar);
	const AController* Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr;
	if (!Avatar || !ASC || !World || !Controller)
	{
		return;
	}

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FHitResult* Claim = Data ? Data->GetHitResult() : nullptr;
	if (!Claim || !Claim->bBlockingHit)
	{
		// A claimed miss stays a miss. Nothing to judge and nothing to show.
		return;
	}

	// The server's own view of the swinger, and its own trace along the claimed direction. From
	// here nothing reads Claim except the one thing a client may assert: who it says it hit.
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ServerAim = ViewRotation.Vector();

	const FVector ToClaim = Claim->ImpactPoint - ViewLocation;
	const float Reach = Stats.Range + BNMeleeDistanceTolerance;
	if (ToClaim.SizeSquared() > FMath::Square(Reach))
	{
		UE_LOG(LogBN, Verbose, TEXT("BNGA_Melee: claim rejected — %.0fuu exceeds reach %.0f."), ToClaim.Size(), Reach);
		return;
	}

	const double AimDot = FMath::Clamp(FVector::DotProduct(ToClaim.GetSafeNormal(), ServerAim), -1.0, 1.0);
	if (FMath::RadiansToDegrees(FMath::Acos(AimDot)) > BNMeleeAngleTolerance)
	{
		return;
	}

	FCollisionQueryParams QueryParams(FName(TEXT("BNMeleeConfirm")), /*bTraceComplex=*/false, Avatar);
	QueryParams.AddIgnoredActor(Weapon);
	// The impact cue picks its per-surface row off the physical material, same as Fire's.
	QueryParams.bReturnPhysicalMaterial = true;

	FHitResult ServerHit;
	World->LineTraceSingleByChannel(ServerHit, ViewLocation, ViewLocation + ToClaim.GetSafeNormal() * Reach, BNCollision::MeleeTrace, QueryParams);

	AActor* ClaimedActor = Claim->GetActor();
	AActor* HitActor = ServerHit.GetActor();
	if (!ServerHit.bBlockingHit || !IsValid(ClaimedActor) || HitActor != ClaimedActor || HitActor == Avatar || HitActor == Weapon)
	{
		// Includes the wallhack case: whatever the client claimed, the server's trace hits what it
		// hits FIRST, and if that is not the claimed victim the swing did not connect.
		return;
	}

	// The cues run OUTSIDE the activation's prediction key: a multicast carrying the key is skipped
	// on the machine that generated it, which is the swinger — the same trap Fire's confirm path
	// documents. Nothing predicts the connect locally, so an empty key reaches every machine once.
	FScopedPredictionWindow UnpredictedCues(ASC, FPredictionKey());

	UE_LOG(LogBN, Log, TEXT("BNGA_Melee: validated connect — %s hit %s for %.0f."),
		*GetNameSafe(Avatar), *GetNameSafe(HitActor), Stats.Damage);

	// THE one damage door, the flat melee number from the ROW (or the unarmed Config fallback).
	// No headshot rule: a rifle butt does not care where it lands, and ApplyWeaponDamage would
	// apply the shot's multiplier.
	// The CAUSE is Melee, not the weapon in the hand: a rifle butt is a melee kill in every
	// shooter that draws the distinction, and crediting it to the rifle would put a rifle glyph
	// on a feed line the player knows was a beatdown.
	BNDamage::ApplyDamage(Avatar, HitActor, Stats.Damage, ServerHit, BNDamageSource::Melee);

	FGameplayCueParameters ImpactParams;
	ImpactParams.Location = ServerHit.ImpactPoint;
	ImpactParams.Normal = ServerHit.ImpactNormal;
	ImpactParams.PhysicalMaterial = ServerHit.PhysMaterial.Get();
	ImpactParams.Instigator = Avatar;
	ImpactParams.SourceObject = Weapon;
	K2_ExecuteGameplayCueWithParams(BNTags::GameplayCue_Weapon_Impact, ImpactParams);
}

void UBNGA_Melee::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SwingTimer);
		World->GetTimerManager().ClearTimer(ConnectTimer);
	}

	// The anim instance outlives this ability instance, so a binding left behind would fire the
	// next swing's notify into a dead object. Cached weakly rather than re-resolved through
	// ActorInfo for the reason EndPlay learned in Wave 2: the path back to the pawn is not
	// guaranteed intact by the time teardown runs.
	if (UAnimInstance* Anim = BoundAnimInstance.Get())
	{
		Anim->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UBNGA_Melee::OnMontageNotifyBegin);
	}
	BoundAnimInstance.Reset();
	RemoveStateTag(MeleeHandle);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
