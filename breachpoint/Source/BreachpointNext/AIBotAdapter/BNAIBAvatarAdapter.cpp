#include "AIBotAdapter/BNAIBAvatarAdapter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "Characters/BNCharacter.h"
#include "Core/AIBBotController.h"
#include "Core/AIBTags.h"
#include "Core/BNGameplayTags.h"
#include "Data/BNDataRows.h"
#include "BreachpointNext.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Match/BNPlayerState.h"
#include "TimerManager.h"
#include "Weapons/BNEquipmentComponent.h"
#include "Weapons/BNWeapon.h"

namespace
{
	/** Re-try cadence for the spawn arm press, and when it starts complaining out loud.
	 *  0.5s is long enough that BNGA_Equip's montage takes, short enough that the bot is
	 *  armed within a swap of the freeze lifting. */
	constexpr float ArmRetrySeconds = 0.5f;
	constexpr int32 ArmComplainAfterPresses = 30;

	/** Can this weapon put rounds downrange, now or after a reload? The swap's whole
	 *  question, and the reason a null Unarmed slot answers "no" without anyone naming it. */
	bool AIBWeaponCanFight(const ABNWeapon* Weapon)   // AIB-prefixed: BN's bot tasks define WeaponCanFight too, and a unity build merges both TUs
	{
		return Weapon && (Weapon->HasAmmo() || Weapon->GetAmmoReserve() > 0);
	}

	/** What this weapon is WORTH at this distance, in expected damage per second.
	 *
	 *  TRANSCRIBED from BN's own bot scoring (BNBotStateTreeTasks.cpp, ScoreWeaponAtRange)
	 *  because that function is a file-local in a gameplay TU this adapter may read but
	 *  never edit or export. Every term is read from the weapon's own shipped row — the
	 *  same numbers the damage pipeline resolves a hit with — so the bot's opinion of a gun
	 *  can never disagree with what the gun does, and retuning the table retunes when bots
	 *  choose it. That is the whole point of scoring instead of hardcoding "shotgun is the
	 *  close-range one": the word shotgun appears nowhere.
	 *
	 *  Zero means "do not bring this to this fight": nothing to fire, or beyond hard Range.
	 */
	float AIBScoreWeaponAtRange(const ABNWeapon* Weapon, float Distance)  // AIB-prefixed for the same unity-build reason
	{
		if (!AIBWeaponCanFight(Weapon))
		{
			return 0.f;
		}
		const FBNWeaponRow* Row = Weapon->GetRow();
		if (!Row)
		{
			return 0.f;
		}
		if (Row->Range > 0.f && Distance > Row->Range)
		{
			return 0.f;
		}

		// The shipped falloff curve, evaluated as the damage pipeline evaluates it. Every
		// shipped row currently has FalloffStart == FalloffEnd == 0, so this is dormant —
		// it stays because the moment a designer fills those columns it starts working.
		float Falloff = 1.f;
		if (Row->FalloffEndDistance > Row->FalloffStartDistance && Distance > Row->FalloffStartDistance)
		{
			const float Alpha = FMath::Clamp(
				(Distance - Row->FalloffStartDistance) / (Row->FalloffEndDistance - Row->FalloffStartDistance),
				0.f, 1.f);
			Falloff = FMath::Lerp(1.f, FMath::Clamp(Row->FalloffMinMultiplier, 0.f, 1.f), Alpha);
		}

		// SPREAD is the real range term in this build, and BN found that by measuring: with
		// the falloff columns empty, scoring on damage alone made every bot pick the Shotgun
		// at every range. The cone's radius at the target is d*tan(theta); once it exceeds a
		// torso's width the fraction landing falls off as the ratio of AREAS, hence squared.
		const float ConeRadius = Distance * FMath::Tan(FMath::DegreesToRadians(FMath::Max(0.f, Row->SpreadAngle)));
		constexpr float TargetRadius = 35.f;   // the nav agent radius — a torso's worth of width
		float HitFraction = 1.f;
		if (ConeRadius > TargetRadius)
		{
			HitFraction = FMath::Square(TargetRadius / ConeRadius);
		}

		const float PerPull = FMath::Max(0.f, Row->Damage) * FMath::Max(1, Row->ShotCount) * Falloff * HitFraction;
		const float Interval = FMath::Max(0.05f, Row->FireDelay);
		float Score = PerPull / Interval;

		// A magazine already in the gun beats a magazine in a pouch.
		if (!Weapon->HasAmmo())
		{
			Score *= 0.5f;
		}
		return Score;
	}
}

void UBNAIBAvatarAdapter::EnsureOn(ABNCharacter* Character, AController* Possessor)
{
	if (!Character || !Possessor || !Possessor->IsA<AAIBBotController>())
	{
		return;
	}
	if (Character->FindComponentByClass<UBNAIBAvatarAdapter>())
	{
		return;
	}
	UBNAIBAvatarAdapter* Adapter = NewObject<UBNAIBAvatarAdapter>(Character, TEXT("AIBAvatarAdapter"));
	Adapter->RegisterComponent();
	Adapter->BeginArming();
}

void UBNAIBAvatarAdapter::BeginArming()
{
	// First fire one retry-interval out: PossessedBy calls EnsureOn BEFORE
	// InitializeCarriedWeapons, so there is nothing to hold yet at this instant.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ArmTimer, this, &UBNAIBAvatarAdapter::ArmIfEmptyHanded,
			ArmRetrySeconds, /*bLoop=*/true, /*FirstDelay=*/ArmRetrySeconds);
	}
}

void UBNAIBAvatarAdapter::ArmIfEmptyHanded()
{
	const AActor* Owner = GetOwner();
	const bool bDone = !Owner || !Owner->HasAuthority() || GetHeldWeapon() != nullptr;
	if (bDone)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ArmTimer);
		}
		return;
	}

	// The same button a human's mouse wheel presses, through the same verb map.
	PressVerb(AIBTags::Verb_WeaponNext);
	ReleaseVerb(AIBTags::Verb_WeaponNext);

	if (++ArmPresses == ArmComplainAfterPresses)
	{
		UE_LOG(LogBN, Warning, TEXT("BNAIB: %s still holds nothing after %d Input.Weapon.Next "
			"presses. Either the match freeze has not lifted, or the carry has no weapon to "
			"equip — the bot cannot fight and will never want to Engage."),
			*GetNameSafe(Owner), ArmPresses);
	}
}

FGameplayTag UBNAIBAvatarAdapter::MapVerb(FGameplayTag VerbTag)
{
	// The map IS the seam: the module speaks AIBot.Verb.*, this game listens on
	// Input.* — only verbs the seam audit (and its BN23/AIB19 additions) proved
	// the input path accepts.
	if (VerbTag == AIBTags::Verb_Fire)       { return BNTags::Input_Weapon_Fire; }
	if (VerbTag == AIBTags::Verb_Jump)       { return BNTags::Input_Jump; }
	if (VerbTag == AIBTags::Verb_Crouch)     { return BNTags::Input_Crouch; }
	if (VerbTag == AIBTags::Verb_Sprint)     { return BNTags::Input_Sprint; }
	if (VerbTag == AIBTags::Verb_Melee)      { return BNTags::Input_Melee; }
	if (VerbTag == AIBTags::Verb_Grenade)    { return BNTags::Input_Grenade; }
	if (VerbTag == AIBTags::Verb_Reload)     { return BNTags::Input_Weapon_Reload; }
	if (VerbTag == AIBTags::Verb_WeaponNext) { return BNTags::Input_Weapon_Next; }
	if (VerbTag == AIBTags::Verb_Aim)        { return BNTags::Input_Weapon_ADS; }
	if (VerbTag == AIBTags::Verb_Grapple)    { return BNTags::Input_Grapple; }
	if (VerbTag == AIBTags::Verb_Dash)       { return BNTags::Input_Dash; }
	return FGameplayTag();
}

UBNAbilitySystemComponent* UBNAIBAvatarAdapter::GetASC() const
{
	// The audited chain: pawn -> PlayerState -> the one ASC that outlives the body.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const ABNPlayerState* PS = Pawn ? Pawn->GetPlayerState<ABNPlayerState>() : nullptr;
	return PS ? PS->GetBNAbilitySystemComponent() : nullptr;
}

ABNWeapon* UBNAIBAvatarAdapter::GetHeldWeapon() const
{
	const ABNCharacter* Character = Cast<ABNCharacter>(GetOwner());
	UBNEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
	return Equipment ? Equipment->GetCurrentWeapon() : nullptr;
}

void UBNAIBAvatarAdapter::PressVerb(FGameplayTag VerbTag)
{
	// The server-only gate lives HERE, per the audit: the ASC does not gate presses.
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const FGameplayTag InputTag = MapVerb(VerbTag);
	if (!InputTag.IsValid())
	{
		// The module's contract: an unmapped verb is a no-op, logged once — never a crash.
		UE_LOG(LogBN, Verbose, TEXT("BNAIB: unmapped verb %s pressed into nothing."), *VerbTag.ToString());
		return;
	}
	if (UBNAbilitySystemComponent* ASC = GetASC())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void UBNAIBAvatarAdapter::ReleaseVerb(FGameplayTag VerbTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const FGameplayTag InputTag = MapVerb(VerbTag);
	if (!InputTag.IsValid())
	{
		return;
	}
	if (UBNAbilitySystemComponent* ASC = GetASC())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

float UBNAIBAvatarAdapter::GetShieldNorm() const
{
	// SAME contract as GetHealthNorm: 1 when unknowable. And critically, 1 when the game
	// has NO SHIELDS — MaxShield <= 0 is a shieldless build, which must read "full" rather
	// than "broken". Reading 0 there would make every bot believe it was one burst from
	// death and flee permanently, which is exactly what shipped for the whole period
	// shields were disabled had this fact existed then.
	const UBNAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return 1.f;
	}
	const float MaxShield = ASC->GetNumericAttribute(UBNAttributeSet::GetMaxShieldAttribute());
	if (MaxShield <= 0.f)
	{
		return 1.f;
	}
	return FMath::Clamp(ASC->GetNumericAttribute(UBNAttributeSet::GetShieldAttribute()) / MaxShield, 0.f, 1.f);
}

float UBNAIBAvatarAdapter::GetHealthNorm() const
{
	// The interface contract: 1 when unknowable — a missing ASC must not read as dying.
	const UBNAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return 1.f;
	}
	const float MaxHealth = ASC->GetNumericAttribute(UBNAttributeSet::GetMaxHealthAttribute());
	if (MaxHealth <= 0.f)
	{
		return 1.f;
	}
	return FMath::Clamp(ASC->GetNumericAttribute(UBNAttributeSet::GetHealthAttribute()) / MaxHealth, 0.f, 1.f);
}

float UBNAIBAvatarAdapter::GetAmmoNorm() const
{
	const ABNWeapon* Weapon = GetHeldWeapon();
	if (!Weapon)
	{
		return 0.f;
	}
	const int32 MagazineSize = Weapon->GetMagazineSize();
	if (MagazineSize <= 0)
	{
		// A WEAPON WITH NO MAGAZINE IS NOT AN EMPTY ONE, and reporting it as empty is what
		// froze bots mid-match in a crouch.
		//
		// MEASURED (28 Aug): 51 reads of `ammo 0/0 reserve 90` in one match. This used to
		// return 0.0, which is exactly the reload trigger, so the bot crouched and pressed
		// reload once a second forever: UBNGA_Reload's own gate is CurrentAmmo < MagazineSize,
		// and 0 < 0 is FALSE, so that press can never succeed. 65 asks, 1 activation, 65
		// refusals, and a Spartan squatting in the open (founder: "they are just staying
		// crouched").
		//
		// 1.0 — "nothing to reload" — is the honest answer: you cannot reload a magazine that
		// does not exist. Same rule as GetShieldNorm's unknowable case: an unknown must never
		// resolve to the value that triggers an action. That answer is unchanged.
		//
		// WHAT WAS WRONG (3 Sep): this branch called itself "row is unresolved" and logged a
		// Warning every time — 2,658 of them in a single 150-second match, which is a log a
		// reader learns to scroll past. Nothing was unresolved. DT_BNWeapons' Knife row
		// carries MagazineSize 0 because a knife HAS no magazine, so the melee weapon is the
		// ORDINARY case here, not a fault. The 28 Aug reading inferred a failed lookup from a
		// zero without asking whether the row was there, and the real missing-row case — the
		// one worth a Warning — was drowned in the noise of the normal one. Ask the row.
		if (!Weapon->GetRow())
		{
			UE_LOG(LogBN, Warning, TEXT("BNAIBAdapter: %s holds %s whose row '%s' is NOT IN THE WEAPON TABLE — reporting ammo FULL so the bot does not crouch on a reload that can never activate, but this weapon has no mesh, no abilities and no numbers either."),
				*GetNameSafe(GetOwner()), *GetNameSafe(Weapon), *Weapon->GetRowName().ToString());
		}
		else
		{
			UE_LOG(LogBN, Verbose, TEXT("BNAIBAdapter: %s holds %s ('%s'), a weapon with no magazine — reporting ammo FULL, which is what a melee weapon truthfully is."),
				*GetNameSafe(GetOwner()), *GetNameSafe(Weapon), *Weapon->GetRowName().ToString());
		}
		return 1.f;
	}
	return FMath::Clamp(static_cast<float>(Weapon->GetCurrentAmmo()) / MagazineSize, 0.f, 1.f);
}

bool UBNAIBAvatarAdapter::HasReserveAmmo() const
{
	const ABNWeapon* Weapon = GetHeldWeapon();
	if (!Weapon)
	{
		return false;
	}
	// THE RAW NUMBERS, once per call at Verbose — the reload loop was diagnosed for an hour
	// from normalised values that could not distinguish "empty magazine" from "no magazine".
	UE_LOG(LogBN, Verbose, TEXT("BNAIBAdapter: %s ammo %d/%d reserve %d"),
		*GetNameSafe(GetOwner()), Weapon->GetCurrentAmmo(), Weapon->GetMagazineSize(), Weapon->GetAmmoReserve());
	return Weapon->GetAmmoReserve() > 0;
}

bool UBNAIBAvatarAdapter::CanWeaponFight() const
{
	// The assembled four-read answer, transcribed from the fire ability's own gates —
	// the recipe the brain must not know, in the one place allowed to know it.
	const ABNWeapon* Weapon = GetHeldWeapon();
	if (!Weapon)
	{
		// The 25 Aug deadlock's exact line: empty hands read as "cannot fight", so the bot
		// could never want to Engage — and the brain's old SeekWeapon want (since retired)
		// sent it hunting a pickup this game does not contain. Named here so the next
		// reader does not have to re-derive the empty hand from the ini.
		UE_LOG(LogBN, Verbose, TEXT("BNAIB: %s holds no weapon (the Unarmed slot) — cannot fight."),
			*GetNameSafe(GetOwner()));
		return false;
	}
	if (!Weapon->HasAmmo() && Weapon->GetAmmoReserve() <= 0)
	{
		return false;
	}
	const UBNAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return false;
	}
	return !ASC->HasMatchingGameplayTag(BNTags::Cooldown_Weapon_Fire)
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Weapon_Reloading)
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Weapon_Melee)
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Dead)
		&& !ASC->HasMatchingGameplayTag(BNTags::State_Match_Frozen);
}

int32 UBNAIBAvatarAdapter::GetGrenadeCount() const
{
	const UBNAbilitySystemComponent* ASC = GetASC();
	return ASC ? FMath::FloorToInt32(ASC->GetNumericAttribute(UBNAttributeSet::GetGrenadesAttribute())) : 0;
}

bool UBNAIBAvatarAdapter::IsGrounded() const
{
	// Movement truth, never a tag: the InAir tag provably misses a walk-off (the audit).
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	return Move ? !Move->IsFalling() : true;
}

bool UBNAIBAvatarAdapter::IsAiming() const
{
	// The header says why this is a tag read and never a mirror of the press.
	const UBNAbilitySystemComponent* ASC = GetASC();
	return ASC && ASC->HasMatchingGameplayTag(BNTags::State_Weapon_ADS);
}

bool UBNAIBAvatarAdapter::IsCrouched() const
{
	// The ENGINE's replicated crouch state, not a tag and not a mirror of our own: the
	// crouch ability, a landing, or an uncrouch forced by a low ceiling all change it
	// without asking us.
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character && Character->bIsCrouched;
}

bool UBNAIBAvatarAdapter::IsAlive() const
{
	const UBNAbilitySystemComponent* ASC = GetASC();
	return ASC && !ASC->HasMatchingGameplayTag(BNTags::State_Dead);
}

float UBNAIBAvatarAdapter::GetMeleeRangeUU() const
{
	// The reach comes from the HELD weapon's row — the same number BNGA_Melee resolves.
	// Restating it on the AI side would be a second source of truth for one distance.
	const ABNWeapon* Weapon = GetHeldWeapon();
	const FBNWeaponRow* Row = Weapon ? Weapon->GetRow() : nullptr;
	return Row ? FMath::Max(0.f, Row->MeleeRange) : 0.f;
}

bool UBNAIBAvatarAdapter::HasUsableWeapon() const
{
	// The POUCH question. Same predicate the scorer's zero-gate uses, applied to every
	// slot instead of the held one — so "usable" means exactly what "scores above zero"
	// means, and the two cannot drift apart into a bot that cycles for a weapon the
	// scorer will then refuse.
	const ABNCharacter* Character = Cast<ABNCharacter>(GetOwner());
	const UBNEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
	if (!Equipment)
	{
		return AIBWeaponCanFight(GetHeldWeapon()); // no carry concept: the hand is the answer
	}
	for (const TObjectPtr<ABNWeapon>& Candidate : Equipment->GetWeapons())
	{
		if (AIBWeaponCanFight(Candidate))
		{
			return true;
		}
	}
	return false;
}

bool UBNAIBAvatarAdapter::IsBestWeaponForRange(float DistanceUU) const
{
	// READ-ONLY, AND THAT IS THE DESIGN. Nothing here writes CurrentIndex or touches
	// UBNEquipmentComponent's cycling: the bot swaps by pressing Input.Weapon.Next, exactly
	// as a human's mouse wheel does, and this answers only "press it again?". That is what
	// lets the carry keep its deliberate null Unarmed slot — a holster state a player can
	// select — with no change to gameplay code shared with humans and BN's own bots. The
	// slot scores 0 (AIBWeaponCanFight is false for null), so it is never the answer, and the
	// caller cycles straight past it.
	const ABNCharacter* Character = Cast<ABNCharacter>(GetOwner());
	const UBNEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
	if (!Equipment)
	{
		return true; // nothing to choose between: never spin the wheel
	}

	const ABNWeapon* Best = nullptr;
	float BestScore = 0.f;
	for (const TObjectPtr<ABNWeapon>& Candidate : Equipment->GetWeapons())
	{
		const float Score = AIBScoreWeaponAtRange(Candidate, DistanceUU);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	const ABNWeapon* Current = GetHeldWeapon();
	// Nothing carried can fight at this range at all: settle for whatever is in hand rather
	// than cycling forever. "You have no good option" is not a reason to keep pressing.
	return Best == nullptr ? AIBWeaponCanFight(Current) : (Current == Best);
}

float UBNAIBAvatarAdapter::GetHealthNormOf(const AActor* Other) const
{
	// FAIRPLAY ruling (26 Aug): the facts builder must NOT call this for live targets.
	// It exists for the adapter's own future uses (the damage-derived estimate).
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Other));
	if (!ASC)
	{
		return 1.f;
	}
	const float MaxHealth = ASC->GetNumericAttribute(UBNAttributeSet::GetMaxHealthAttribute());
	if (MaxHealth <= 0.f)
	{
		return 1.f;
	}
	return FMath::Clamp(ASC->GetNumericAttribute(UBNAttributeSet::GetHealthAttribute()) / MaxHealth, 0.f, 1.f);
}

bool UBNAIBAvatarAdapter::IsAliveTarget(const AActor* Other) const
{
	// The audited no-cast door plus the one tag that means dead.
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Other));
	return ASC && !ASC->HasMatchingGameplayTag(BNTags::State_Dead);
}
