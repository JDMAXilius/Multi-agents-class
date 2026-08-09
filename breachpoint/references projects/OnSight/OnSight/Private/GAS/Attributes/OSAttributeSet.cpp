// Fill out your copyright notice in the Description page of Project Settings.

// PostGameplayEffectExecute: Health->death broadcast + Event_Death; GuardBreak->Event_GuardBreak; Recoil->Event_Recoil (to attacker); HitReact type->Event_HitReact. Damage/GuardBreak/Recoil are meta (cleared after use).
#include "GAS/Attributes/OSAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Characters/OSCharacter.h"
#include "Data/OSCombatTypes.h"
#include "Data/OSHitDamageContext.h"
#include "Data/OSGameplayTags.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

UOSAttributeSet::UOSAttributeSet()
{
	// MoveSpeedBase is the base units/sec (walk speed baseline).
	InitMoveSpeedBase(200.f);
	// MovementSpeed is used as a multiplier in OnSight (1.0 = normal walk speed).
	// Default it to 1.0 so characters can always move normally even before any init GE runs.
	InitMovementSpeed(1.0f);
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
	InitDamage(0.f);
	InitGuardBreak(0.f);
	InitRecoil(0.f);
}

void UOSAttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UOSAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOSAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOSAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOSAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOSAttributeSet, Aura, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOSAttributeSet, MaxAura, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOSAttributeSet, MoveSpeedBase, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOSAttributeSet, MovementSpeed, COND_None, REPNOTIFY_Always);
}

namespace
{
	// Preserve current/max ratio when Max changes (e.g. MaxHealth 100->120 keeps Health % same). Applies via ASC when possible.
	static void AdjustAttributeForMaxChange(UAttributeSet* AttributeSet,
		FGameplayAttributeData& AffectedAttribute,
		const FGameplayAttributeData& MaxAttribute,
		float NewMaxValue,
		const FGameplayAttribute& AffectedAttributeProperty)
	{
		UAbilitySystemComponent* ASC = AttributeSet ? AttributeSet->GetOwningAbilitySystemComponent() : nullptr;

		const float CurrentMaxValue = MaxAttribute.GetCurrentValue();
		if (!FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue) && CurrentMaxValue > 0.f)
		{
			const float CurrentValue = AffectedAttribute.GetCurrentValue();
			const float NewDelta = (CurrentValue * NewMaxValue / CurrentMaxValue) - CurrentValue;

			// Apply delta via ASC so it replicates/predicts correctly.
			if (ASC)
			{
				ASC->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, NewDelta);
			}
			else
			{
				AffectedAttribute.SetCurrentValue(CurrentValue + NewDelta);
			}
		}
	}
}

// Clamp current to max; when Max changes, scale current to preserve ratio. MovementSpeed clamped 0..3 (multiplier).
void UOSAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	// Precalculations before the attribute changes
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	if (Attribute == GetMaxHealthAttribute())
		AdjustAttributeForMaxChange(this, Health, MaxHealth, NewValue, GetHealthAttribute());
	if (Attribute == GetAuraAttribute())
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxAura());

	if (Attribute == GetStaminaAttribute())
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	if (Attribute == GetMaxStaminaAttribute())
		AdjustAttributeForMaxChange(this, Stamina, MaxStamina, NewValue, GetStaminaAttribute());
	if (Attribute == GetMaxAuraAttribute())
		AdjustAttributeForMaxChange(this, Aura, MaxAura, NewValue, GetAuraAttribute());
	

	if (Attribute == GetMoveSpeedBaseAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 1200.f);
	}

	if (Attribute == GetMovementSpeedAttribute())
	{
		// MovementSpeed is a multiplier (1.0 = normal walk speed). Keep this tight to avoid runaway speeds
		// from mis-authored GEs (e.g., mistakenly setting 450 thinking it's an absolute walk speed).
		NewValue = FMath::Clamp(NewValue, 0.f, 3.f);
	}
}

// After GE execution: GuardBreak meta -> Event_GuardBreak; Damage meta -> subtract Health, BroadcastHealth, then Event_Death or Event_HitReact (gated by GameplayEffect.HitReaction).
void UOSAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& Attr = Data.EvaluatedData.Attribute;

	// GuardBreak meta -> fire gameplay event (authoritative); then reset meta to 0.
	if (Attr == GetGuardBreakAttribute())
	{
		const float LocalGuardBreak = GetGuardBreak();
		SetGuardBreak(0.f);

		if (LocalGuardBreak > 0.f)
		{
			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				const FOSGameplayTags& Tags = FOSGameplayTags::Get();
				if (Tags.Event_GuardBreak.IsValid())
				{
					// Needed for Clients Shield to Break
					// TODO: Look to do the same way as the Hit react
					FGameplayTagContainer BlockTag;
					BlockTag.AddTag(Tags.Ability_Block);
					ASC->CancelAbilities(&BlockTag);

					// Remove any prior guard break GE so the cue lifecycle resets (OnRemove → OnActive).
					// Without this, re-application while the 1.5s GE is still active skips OnActive
					// because the cue system sees the tag as already active (ref-counted).
					static const FGameplayTag GuardBrokenTag = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Guard.IsBroken"), false);
					if (GuardBrokenTag.IsValid())
					{
						FGameplayTagContainer GuardBrokenTags;
						GuardBrokenTags.AddTag(GuardBrokenTag);
						ASC->RemoveActiveEffectsWithGrantedTags(GuardBrokenTags);
					}

					FGameplayEventData EventData;
					EventData.EventTag = Tags.Event_GuardBreak;
					EventData.Target = ASC->GetAvatarActor();
					EventData.Instigator = Data.EffectSpec.GetContext().GetInstigator();
					ASC->HandleGameplayEvent(Tags.Event_GuardBreak, &EventData);
				}
			}
		}
		return;
	}

	// Recoil meta -> fire recoil event at ATTACKER (not self), and execute the ShieldBubble cue
	// on the DEFENDER to trigger the on-hit VFX burst. Reset meta to 0.
	if (Attr == GetRecoilAttribute())
	{
		const float LocalRecoil = GetRecoil();
		SetRecoil(0.f);

		if (LocalRecoil > 0.f)
		{
			const FOSGameplayTags& Tags = FOSGameplayTags::Get();
			const FGameplayEffectContextHandle& Ctx = Data.EffectSpec.GetContext();

			// Dispatch recoil event to attacker (instigator of the damage GE)
			if (UAbilitySystemComponent* InstigatorASC = Ctx.GetInstigatorAbilitySystemComponent())
			{
				FGameplayEventData EventData;
				EventData.EventTag = Tags.Event_Recoil;
				EventData.Instigator = Data.Target.AbilityActorInfo->AvatarActor.Get();  // blocker (defender)
				EventData.Target = InstigatorASC->GetAvatarActor();    // attacker
				EventData.ContextHandle = Ctx;
				InstigatorASC->HandleGameplayEvent(Tags.Event_Recoil, &EventData);
			}

			// Fire-and-forget the ShieldBubble cue on the defender. The active GC_ShieldBubble
			// actor receives this via OnExecute_Implementation and spawns its HitVFX using the
			// hit normal we pack into the cue parameters. Replicates automatically.
			if (UAbilitySystemComponent* SelfASC = GetOwningAbilitySystemComponent())
			{
				FGameplayCueParameters CueParams;
				CueParams.EffectContext = Ctx;

				if (const FHitResult* HitResult = Ctx.GetHitResult())
				{
					CueParams.Location = HitResult->ImpactPoint;
					CueParams.Normal = HitResult->ImpactNormal;
				}

				SelfASC->ExecuteGameplayCue(Tags.Cue_ShieldBubble, CueParams);
			}
		}

		return;
	}

	// Meta Damage -> Health (ExecCalc or GE writes to Damage; we consume and apply to Health)
	if (Attr == GetDamageAttribute())
	{
		float LocalDamage = GetDamage();
		SetDamage(0.f);
		if (LocalDamage > 0.f)
		{
			// No Health Damage: zero health damage but preserve hit reactions (LocalDamage > 0 still true for event flow below).
			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				if (ASC->HasMatchingGameplayTag(FOSGameplayTags::Get().Debug_NoHealthDamage))
				{
					LocalDamage = 0.f;
				}
			}

			const float NewHealth = FMath::Clamp(GetHealth() - LocalDamage, 0.f, GetMaxHealth());
			SetHealth(NewHealth);

			FOSDeathEventInfo DeathEvent;
			const FGameplayEffectContextHandle& CtxHandle = Data.EffectSpec.GetContext();
			const FOSGameplayEffectContext* EffectOSCtx = nullptr;
			TryGetOSGameplayEffectContext(CtxHandle, EffectOSCtx);
			if (EffectOSCtx)
			{
				FUniqueNetIdRepl VictimId = EffectOSCtx->HitInfo.AttackInfo.VictimPlayerStateUniqueId;
				FUniqueNetIdRepl InstigatorId = EffectOSCtx->HitInfo.AttackInfo.InstigatorPlayerStateUniqueId;
				if (!VictimId.IsValid() && Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->OwnerActor.IsValid())
				{
					if (APawn* VictimPawn = Cast<APawn>(Data.Target.AbilityActorInfo->OwnerActor.Get()))
					{
						if (APlayerState* PS = VictimPawn->GetPlayerState())
						{
							VictimId = PS->GetUniqueId();
							DeathEvent.VictimPS = PS;
						}
							
					}
				}
				
				
				DeathEvent = FOSDeathEventInfo(
					EffectOSCtx->HitInfo,
					InstigatorId,
					VictimId);
				
				DeathEvent.InstigatorPS = DeathEvent.GetInstigatorPlayerState(GetWorld());
				DeathEvent.VictimPS = DeathEvent.GetVictimPlayerState(GetWorld());
				
			}
			BroadcastHealth(DeathEvent);

			// Fire authoritative reaction events (simple + tight):
			// - If dead -> GameplayEvent.Death
			// - Else -> GameplayEvent.HitReact (EventMagnitude encodes react type)
			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				// MP GAS best-practice: deliver events to the ASC itself (PlayerState-owned ASC for players),
				// not by "sending to the pawn" and hoping the library resolves the correct ASC.
				AActor* OwnerActor = nullptr;
				AActor* AvatarActor = nullptr;
				if (Data.Target.AbilityActorInfo.IsValid())
				{
					OwnerActor = Data.Target.AbilityActorInfo->OwnerActor.Get();
					AvatarActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
				}

				AActor* EventTargetActor = IsValid(AvatarActor) ? AvatarActor : OwnerActor;
				if (IsValid(EventTargetActor))
				{
					const FOSGameplayTags& Tags = FOSGameplayTags::Get();

					// Allow source to override react bucket via SetByCaller.
					const float OverrideType = Data.EffectSpec.GetSetByCallerMagnitude(Tags.Data_HitReact_Type, false, -1.f);

					int32 ReactTypeInt = 0;
					if (OverrideType >= 0.f)
					{
						ReactTypeInt = FMath::Clamp(FMath::RoundToInt(OverrideType), 0, 6);
					}
					else if (EffectOSCtx)
					{
						const EOSAttackType T = EffectOSCtx->HitInfo.AttackInfo.Type;
						ReactTypeInt = (T == EOSAttackType::Fatal) ? 4 : ((T == EOSAttackType::Heavy || T == EOSAttackType::Special) ? 1 : 0);
					}

						// No Health Damage suppresses both health-death and fatal-death.
					const bool bNoHealthDamage = ASC->HasMatchingGameplayTag(Tags.Debug_NoHealthDamage);
					const bool bIsDeadNow = !bNoHealthDamage && ((NewHealth <= 0.f) || (ReactTypeInt == 4));

					// If guard broke during this execution, force GuardBreak react type.
					// Use captured tags (immune to ordering — GuardBreak handler may cancel block before we run).
					// Damage meta > 0 while blocking (without grab bypass) means overflow from ExecCalc → guard break.
					{
						const FGameplayTagContainer* CapturedTargetTags = Data.EffectSpec.CapturedTargetTags.GetAggregatedTags();
						const FGameplayTagContainer* CapturedSourceTags = Data.EffectSpec.CapturedSourceTags.GetAggregatedTags();
						const bool bWasBlocking = CapturedTargetTags && CapturedTargetTags->HasTag(Tags.IsBlocking);
						const bool bBypassedBlock = CapturedSourceTags && CapturedSourceTags->HasTag(Tags.IsGrabbing);
						if (bWasBlocking && !bBypassedBlock)
						{
							ReactTypeInt = 6; // GuardBreak
						}
					}

					// Gate HitReact: only damage effects carrying GameplayEffect.HitReaction will trigger HitReact.
					if (!bIsDeadNow)
					{
						// NOTE: Non-fatal lookup to avoid editor hot-reload crashes if tags haven't refreshed yet.
						static const FGameplayTag HitReactEffectTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayEffect.HitReaction"), false);
						if (HitReactEffectTag.IsValid())
						{
							FGameplayTagContainer SpecAssetTags;
							Data.EffectSpec.GetAllAssetTags(SpecAssetTags);
							if (!SpecAssetTags.HasTag(HitReactEffectTag))
							{
								return;
							}
						}
					}

					const FGameplayTag EventTag = bIsDeadNow ? Tags.Event_Death : Tags.Event_HitReact;

					FGameplayEventData EventData;
					EventData.EventTag = EventTag;
					// Prefer avatar as the "target" of the event payload (what reacts / plays montage).
					EventData.Target = EventTargetActor;
					EventData.Instigator = CtxHandle.GetInstigator();
					EventData.ContextHandle = CtxHandle;

					// Encode react type as magnitude for HitReact. (Death ignores it.)
					EventData.EventMagnitude = static_cast<float>(ReactTypeInt);

					ASC->HandleGameplayEvent(EventTag, &EventData);
				}
			}
		}
		return;
	}

	// Clamp and broadcast after direct Health/Stamina/MovementSpeed mods
	if (Attr == GetHealthAttribute() || Attr == GetMaxHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
    
		FOSDeathEventInfo DeathEvent;
		const FGameplayEffectContextHandle& CtxHandle = Data.EffectSpec.GetContext();
		const FOSGameplayEffectContext* HealthOSCtx = nullptr;
		if (TryGetOSGameplayEffectContext(CtxHandle, HealthOSCtx))
		{
			DeathEvent = FOSDeathEventInfo(HealthOSCtx->HitInfo, 
				HealthOSCtx->HitInfo.AttackInfo.InstigatorPlayerStateUniqueId,
				HealthOSCtx->HitInfo.AttackInfo.VictimPlayerStateUniqueId);
			DeathEvent.InstigatorPS = DeathEvent.GetInstigatorPlayerState(GetWorld());
			DeathEvent.VictimPS = DeathEvent.GetVictimPlayerState(GetWorld());
		}
    
		BroadcastHealth(DeathEvent);
	}
	if (Attr == GetStaminaAttribute() || Attr == GetMaxStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.f, GetMaxStamina()));
		BroadcastStamina();

		if (Data.Target.AbilityActorInfo.IsValid())
		{
			UAbilitySystemComponent* TargetASC = Data.Target.AbilityActorInfo->AbilitySystemComponent.Get();
			AActor* AvatarActor = Data.Target.AbilityActorInfo->AvatarActor.Get();

			if (TargetASC && IsValid(AvatarActor))
			{
				FGameplayTagContainer OwnedTags;
				TargetASC->GetOwnedGameplayTags(OwnedTags);
				const FOSGameplayTags& Tags = FOSGameplayTags::Get();

				if (OwnedTags.HasTag(Tags.IsBlocking))
				{
					FGameplayEventData EventData;
					EventData.EventTag = Tags.Event_HitReact;
					EventData.Target = AvatarActor;
					EventData.Instigator = Data.EffectSpec.GetContext().GetInstigator();
					EventData.ContextHandle = Data.EffectSpec.GetContext();
					TargetASC->HandleGameplayEvent(Tags.Event_HitReact, &EventData);
				}
			}
		}
	}
	if (Attr == GetAuraAttribute() || Attr == GetMaxAuraAttribute())
	{
		SetAura(FMath::Clamp(GetAura(), 0.f, GetMaxAura()));
		BroadcastAura();
	}
	if (Attr == GetMoveSpeedBaseAttribute())
	{
		SetMoveSpeedBase(FMath::Clamp(GetMoveSpeedBase(), 0.f, 1200.f));
	}
	if (Attr == GetMovementSpeedAttribute())
	{
		// MovementSpeed is a multiplier (1.0 = normal walk speed).
		SetMovementSpeed(FMath::Clamp(GetMovementSpeed(), 0.f, 3.f));
	}
}

// Notify listeners (e.g. HUD, HealthComponent); deathEvent carries hit/instigator/victim for killfeed.
void UOSAttributeSet::BroadcastHealth(const FOSDeathEventInfo& deathEvent)
{
	OnAttributeHealthChanged.Broadcast(GetHealth(), GetMaxHealth(), deathEvent);
}

void UOSAttributeSet::BroadcastStamina()
{
	OnAttributeStaminaChanged.Broadcast(GetStamina(), GetMaxStamina());
}

void UOSAttributeSet::BroadcastAura()
{
	OnAttributeAuraChanged.Broadcast(GetAura(), GetMaxAura());
}

// Replicated Health: update and broadcast (clients get empty death context for UI).
void UOSAttributeSet::OnRep_Health(const FGameplayAttributeData& oldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UOSAttributeSet, Health, oldHealth);
	// On clients, we don't have meaningful death context; broadcast empty for UI listeners.
	FOSHitNetInfo EmptyHitInfo;
	FUniqueNetIdRepl EmptyInstigatorId;
	FUniqueNetIdRepl EmptyVictimId;
	FOSDeathEventInfo EmptyDeathEvent(EmptyHitInfo, EmptyInstigatorId, EmptyVictimId);
	OnAttributeHealthChanged.Broadcast(Health.GetCurrentValue(), MaxHealth.GetCurrentValue(), EmptyDeathEvent);
}

void UOSAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& oldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UOSAttributeSet, MaxHealth, oldHealth);
	FOSHitNetInfo EmptyHitInfo;
	FUniqueNetIdRepl EmptyInstigatorId;
	FUniqueNetIdRepl EmptyVictimId;
	FOSDeathEventInfo EmptyDeathEvent(EmptyHitInfo, EmptyInstigatorId, EmptyVictimId);
	OnAttributeHealthChanged.Broadcast(Health.GetCurrentValue(), MaxHealth.GetCurrentValue(), EmptyDeathEvent);
}

void UOSAttributeSet::OnRep_Stamina(const FGameplayAttributeData& oldStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UOSAttributeSet, Stamina, oldStamina);
	OnAttributeStaminaChanged.Broadcast(Stamina.GetCurrentValue(), MaxStamina.GetCurrentValue());
}

void UOSAttributeSet::OnRep_Aura(const FGameplayAttributeData& oldAura)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UOSAttributeSet, Aura, oldAura);
	OnAttributeAuraChanged.Broadcast(Aura.GetCurrentValue(), MaxAura.GetCurrentValue());
}

void UOSAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& oldStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UOSAttributeSet, MaxStamina, oldStamina);
	OnAttributeStaminaChanged.Broadcast(Stamina.GetCurrentValue(), MaxStamina.GetCurrentValue());
}

void UOSAttributeSet::OnRep_MaxAura(const FGameplayAttributeData& oldAura)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UOSAttributeSet, MaxAura, oldAura);
	OnAttributeAuraChanged.Broadcast(Aura.GetCurrentValue(), MaxAura.GetCurrentValue());
}

void UOSAttributeSet::OnRep_MoveSpeedBase(const FGameplayAttributeData& oldMoveSpeedBase)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UOSAttributeSet, MoveSpeedBase, oldMoveSpeedBase);
}

void UOSAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& oldMovementSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UOSAttributeSet, MovementSpeed, oldMovementSpeed);
}