// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Cues/GC_ShieldBubble.h"

#include "Components/StaticMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "Characters/OSCharacter.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

AGC_ShieldBubble::AGC_ShieldBubble()
{
	bAutoDestroyOnRemove = true;
	bAllowMultipleOnActiveEvents = false;

	BubbleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BubbleMesh"));
	RootComponent = BubbleMesh;

	BubbleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BubbleMesh->SetGenerateOverlapEvents(false);
	BubbleMesh->SetCastShadow(false);
	BubbleMesh->bReceivesDecals = false;
	BubbleMesh->SetVisibility(false, true);
}

bool AGC_ShieldBubble::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (!MyTarget || !BubbleMesh)
	{
		return false;
	}

	if (BubbleStaticMesh)
	{
		BubbleMesh->SetStaticMesh(BubbleStaticMesh);
	}
	if (BubbleMaterial)
	{
		BubbleMesh->SetMaterial(0, BubbleMaterial);
	}
	// Try to get the target's ASC and AttributeSet to initialize the bubble scale and bind for updates.
	if (AOSCharacter* OSCharacter = Cast<AOSCharacter>(MyTarget))
	{
		CachedASC = OSCharacter->GetAbilitySystemComponent();
		if (const UOSAttributeSet* AS = CachedASC->GetSet<UOSAttributeSet>())
		{
			// Initialize the bubble scale based on current stamina.
			CachedMaxStamina = AS->GetMaxStamina();
			ApplyShieldScale(AS->GetStamina(), CachedMaxStamina);

			// Register for stamina changes to update the bubble scale.
			StaminaDelegateHandle = CachedASC->GetGameplayAttributeValueChangeDelegate(
				UOSAttributeSet::GetStaminaAttribute())
				.AddUObject(this, &AGC_ShieldBubble::OnStaminaChanged);
		}
	}
	// Attach the cue actor to the target.
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	AttachToActor(MyTarget, FAttachmentTransformRules::KeepWorldTransform, AttachSocket);
	if (bInheritRotation)
		BubbleMesh->SetAbsolute(false, false, false);
	else
	{
		BubbleMesh->SetAbsolute(false, true, false);
		SetActorRotation(FRotator::ZeroRotator);
	}
	BubbleMesh->SetVisibility(true, true);

	UE_LOG(LogTemp, Warning, TEXT("ShieldBubble OnActive: %s"), *GetNameSafe(MyTarget));

	return true;
}

bool AGC_ShieldBubble::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	UE_LOG(LogTemp, Warning, TEXT("ShieldBubble OnExecute: %s Normal=%s"), *GetNameSafe(MyTarget), *Parameters.Normal.ToString());

	OnShieldHit(Parameters.Normal);
	return true;
}

bool AGC_ShieldBubble::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// Clean up ASC delegate.
	if (CachedASC && StaminaDelegateHandle.IsValid())
	{
		CachedASC->GetGameplayAttributeValueChangeDelegate(
			UOSAttributeSet::GetStaminaAttribute())
			.Remove(StaminaDelegateHandle);
		StaminaDelegateHandle.Reset();
	}
	// Clear cached ASC reference.
	CachedASC = nullptr;

	if (BubbleMesh)
	{
		BubbleMesh->SetVisibility(false, true);
	}

	UE_LOG(LogTemp, Warning, TEXT("ShieldBubble OnRemove: %s"), *GetNameSafe(MyTarget));
	return true;
}

void AGC_ShieldBubble::OnStaminaChanged(const FOnAttributeChangeData& Data)
{
	ApplyShieldScale(Data.NewValue, CachedMaxStamina);
}

void AGC_ShieldBubble::ApplyShieldScale(float Current, float Max)
{
	if (!bAdjustScale)
		return;

	const float Percent = FMath::Clamp(Current / FMath::Max(Max, 1.0f), 0.0f, 1.0f);
	const float NewScale = FMath::Lerp(MinScale, RelativeScale.X, Percent);
	BubbleMesh->SetRelativeScale3D(FVector(NewScale));
}

void AGC_ShieldBubble::OnShieldHit(const FVector& HitNormal)
{
	UE_LOG(LogTemp, Warning, TEXT("ShieldBubble OnShieldHit: Normal=%s"), *HitNormal.ToString());

	if (!HitVFX)
	{
		UE_LOG(LogTemp, Warning, TEXT("ShieldBubble OnShieldHit: HitVFX is not set on %s"), *GetNameSafe(this));
		return;
	}

	if (!BubbleMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("ShieldBubble OnShieldHit: BubbleMesh is null on %s"), *GetNameSafe(this));
		return;
	}

	const FVector InvertedNormal = HitNormal * -1.f;
	const FRotator SpawnRotation = InvertedNormal.IsNearlyZero() ? FRotator::ZeroRotator : InvertedNormal.Rotation();

	UNiagaraFunctionLibrary::SpawnSystemAttached(
		HitVFX,
		BubbleMesh,
		NAME_None,
		FVector::ZeroVector,
		SpawnRotation,
		EAttachLocation::KeepRelativeOffset,
		true);
}

