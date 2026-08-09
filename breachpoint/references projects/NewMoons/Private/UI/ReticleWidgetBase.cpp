// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/ReticleWidgetBase.h"
#include "InventorySystem/Weapons/NMWeaponActor.h"

void UReticleWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

void UReticleWidgetBase::SetOwningWeapon(ANMWeaponActor* InWeapon)
{
	OwningWeapon = InWeapon;
}
