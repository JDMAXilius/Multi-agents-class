#pragma once
#include "CoreMinimal.h"


static bool IsWithinAngle(const AActor* Self, const AActor* Other, float Degrees)
{
	if (!Self || !Other) return false;

	const FVector ToOther = (Other->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal();
	const FVector Forward = Self->GetActorForwardVector().GetSafeNormal();

	const float HalfRad = FMath::DegreesToRadians(FMath::Clamp(Degrees, 0.f, 360.f) * 0.5f);
	const float MinDot = FMath::Cos(HalfRad);
	return FVector::DotProduct(Forward, ToOther) >= MinDot;
}

template<typename T>
FORCEINLINE void OverrideIfValid(T& Dest, const T& Src)
{
	if (Src)
	{
		Dest = Src;
	}
}

FORCEINLINE void OverrideIfSet(float& Dest, float Src, float Sentinel = -1.f)
{
	if (Src != Sentinel)
	{
		Dest = Src;
	}
}

FORCEINLINE void OverrideIfSet(int32& Dest, int32 Src, int32 Sentinel = -1)
{
	if (Src != Sentinel)
	{
		Dest = Src;
	}
}
