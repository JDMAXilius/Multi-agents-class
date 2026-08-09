// Copyright Zollpa LLC.


#include "ReticleWidgetBase.h"

#include "ZoransResistance/Characters/ZoransCharacterBase.h"


void UReticleWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ReticleTick(InDeltaTime);
}

void UReticleWidgetBase::SetReticleSpread(const float InSpread)
{
	WeaponSpread = InSpread;
}

void UReticleWidgetBase::SetReticleIndex(const int32 InIndex)
{
	const int32 PreviousReticleIndex = ReticleIndex;
	
	ReticleIndex = FMath::Clamp<int32>(InIndex, 0, ReticleMaxIndex);

	if (ReticleIndex != PreviousReticleIndex)
	{
		ReticleIndexChanged(ReticleIndex);
	}
}

void UReticleWidgetBase::ModifyReticleIndex(const bool bIncrease)
{
	if (ReticleMaxIndex == 0)
	{
		return;
	}
	
	if (bIncrease)
	{
		if (ReticleIndex + 1 > ReticleMaxIndex)
		{
			if (bReticleLoopOnClamp)
			{
				SetReticleIndex(0);
			}
		}
		else
		{
			SetReticleIndex(ReticleIndex + 1);
		}
	}
	else
	{
		if (ReticleIndex - 1 < 0)
		{
			if (bReticleLoopOnClamp)
			{
				SetReticleIndex(ReticleMaxIndex);
			}
		}
		else
		{
			SetReticleIndex(ReticleIndex - 1);
		}
	}
}

void UReticleWidgetBase::SetOwningWeapon(AZoransWeaponActor* InWeapon)
{
	OwningWeapon = InWeapon;
}

void UReticleWidgetBase::SetTargetCharacter(AZoransCharacterBase* InTargetCharacter)
{
	if (InTargetCharacter != GetTargetCharacter())
	{
		TargetCharacter = InTargetCharacter;

		TargetCharacterUpdated(GetTargetCharacter(), IsValid(GetTargetCharacter()));
	}
}
