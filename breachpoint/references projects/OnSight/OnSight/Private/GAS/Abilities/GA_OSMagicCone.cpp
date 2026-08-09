// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/GA_OSMagicCone.h"
#include "AbilitySystemComponent.h"
#include "Characters/OSCharacter.h"
#include "Data/OSCombatTypes.h"
#include "Data/OSGameplayTags.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "Utilities/AbilityHelper.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"
#include "GAS/Components/OSAbilitySystemComponent.h"

UGA_OSMagicCone::UGA_OSMagicCone()
	: ConeHalfAngleDegrees(30.0f)
	, ConeRange(600.0f)
	, AttackType(EOSAttackType::Special)
	, bDrawDebugCone(false)
	, DebugDrawDuration(2.0f)
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	ExcludeTargetTags.AddTag(Tags.IsDead);
	ExcludeTargetTags.AddTag(Tags.State_Invincibility);

	// Shared cooldown slot for all magic melee abilities. BPs only need to set CooldownDuration.
	CooldownTags.AddTag(Tags.Cooldown_Magic_Melee);
}

// --- Magic Event ---

void UGA_OSMagicCone::OnMagicEventReceived_Implementation(FGameplayEventData Payload)
{
	if (!HasAuthority(&CurrentActivationInfo))
		return;

	HitActorsThisCast.Reset();
	PerformConeDamage();
}

// --- Cone Damage ---

void UGA_OSMagicCone::PerformConeDamage()
{
	AOSCharacter* Instigator = GetOwningCharacter();
	if (!Instigator)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] PerformConeDamage: no owning character"), *GetName());
		return;
	}

	TArray<AOSCharacter*> Targets = GatherTargetsInCone();

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugCone)
		DrawDebugConeVisualization(Instigator->GetActorLocation(), Instigator->GetActorForwardVector(), Targets);
#endif

	for (AOSCharacter* Victim : Targets)
	{
		if (!Victim || HitActorsThisCast.Contains(Victim))
			continue;

		auto Context = AbilityHelper::BuildEffectContext(Instigator, Victim, AttackType);
		ApplyGameplayEffects(Victim, Context);
		HitActorsThisCast.Add(Victim);
	}
}

// --- Targeting ---

TArray<AOSCharacter*> UGA_OSMagicCone::GatherTargetsInCone() const
{
	TArray<AOSCharacter*> Results;

	const AOSCharacter* Char = GetOwningCharacter();
	if (!Char)
		return Results;

	const FVector Origin = Char->GetActorLocation();
	const FVector Forward = Char->GetActorForwardVector();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Char);

	Char->GetWorld()->OverlapMultiByChannel(
		Overlaps, Origin, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(ConeRange),
		QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AOSCharacter* HitCharacter = Cast<AOSCharacter>(Overlap.GetActor());
		if (!HitCharacter || HitCharacter == Char)
			continue;

		// Friendly fire filter — removes teammates from the target list BEFORE cone math and
		// cue emission. ApplyDirectDamage already blocks GE application to friendlies, but
		// cues/VFX/SFX fire through the target list directly, so missing this filter would
		// cause fire/frost visuals on allies even though no damage was dealt.
		if (UOSCombatBlueprintLibrary::AreActorsFriendly(Char, HitCharacter))
			continue;

		if (!IsInCone(Origin, Forward, HitCharacter))
			continue;

		UAbilitySystemComponent* TargetASC = HitCharacter->GetAbilitySystemComponent();
		if (TargetASC && ExcludeTargetTags.Num() > 0 && TargetASC->HasAnyMatchingGameplayTags(ExcludeTargetTags))
			continue;

		Results.Add(HitCharacter);
	}

	return Results;
}

bool UGA_OSMagicCone::IsInCone(const FVector& Origin, const FVector& Forward, const AActor* Target) const
{
	if (!Target)
		return false;

	const FVector ToTarget = (Target->GetActorLocation() - Origin).GetSafeNormal();
	const float DotProduct = FVector::DotProduct(Forward, ToTarget);
	const float ConeThreshold = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngleDegrees));

	return DotProduct >= ConeThreshold;
}

// --- Debug ---

#if ENABLE_DRAW_DEBUG
void UGA_OSMagicCone::DrawDebugConeVisualization(const FVector& Origin, const FVector& Forward, const TArray<AOSCharacter*>& HitTargets) const
{
	const UWorld* World = GetWorld();
	if (!World)
		return;

	const float HalfAngleRad = FMath::DegreesToRadians(ConeHalfAngleDegrees);
	const FVector Direction = Forward.GetSafeNormal();
	const FVector EndCenter = Origin + Direction * ConeRange;
	const float EndRadius = FMath::Tan(HalfAngleRad) * ConeRange;

	FVector Right, Up;
	Direction.FindBestAxisVectors(Up, Right);

	constexpr int32 NumSegments = 24;
	const float AngleStep = 2.0f * PI / NumSegments;

	FVector PrevPoint = EndCenter + Right * EndRadius;
	for (int32 i = 1; i <= NumSegments; i++)
	{
		const float Angle = AngleStep * i;
		const FVector Point = EndCenter + (Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * EndRadius;

		DrawDebugLine(World, PrevPoint, Point, FColor::Orange, false, DebugDrawDuration, 0, 1.5f);

		if (i % (NumSegments / 8) == 0)
			DrawDebugLine(World, Origin, Point, FColor::Orange, false, DebugDrawDuration, 0, 1.0f);

		PrevPoint = Point;
	}

	DrawDebugLine(World, Origin, EndCenter, FColor::Yellow, false, DebugDrawDuration, 0, 2.0f);
	DrawDebugSphere(World, Origin, ConeRange, 16, FColor(255, 165, 0, 60), false, DebugDrawDuration, 0, 0.5f);
	DrawDebugSphere(World, Origin, 15.0f, 8, FColor::Blue, false, DebugDrawDuration, 0, 2.0f);

	for (const AOSCharacter* Target : HitTargets)
	{
		if (!Target)
			continue;
		const FVector TargetLoc = Target->GetActorLocation();
		DrawDebugSphere(World, TargetLoc, 30.0f, 8, FColor::Green, false, DebugDrawDuration, 0, 2.0f);
		DrawDebugLine(World, Origin, TargetLoc, FColor::Green, false, DebugDrawDuration, 0, 1.0f);
	}
}
#endif
