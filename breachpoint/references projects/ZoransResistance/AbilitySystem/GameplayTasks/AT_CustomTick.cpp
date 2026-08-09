// Copyright Zollpa LLC.


#include "ZoransResistance/AbilitySystem/GameplayTasks/AT_CustomTick.h"

UAT_CustomTick::UAT_CustomTick(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
	bSimulatedTask = true;
}

UAT_CustomTick* UAT_CustomTick::CustomTickTask(UGameplayAbility* OwningAbility, FName TaskInstanceName)
{
	UAT_CustomTick* MyObj = NewAbilityTask<UAT_CustomTick>(OwningAbility);
	return MyObj;
}

void UAT_CustomTick::Activate()
{
	Super::Activate();
}

void UAT_CustomTick::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
	
	CustomTick(DeltaTime);
}


void UAT_CustomTick::CustomTick(float DeltaTime)
{
	OnTick.Broadcast(DeltaTime);
}

void UAT_CustomTick::OnDestroy(bool AbilityIsEnding)
{
	Super::OnDestroy(AbilityIsEnding);
	bTickingTask = false;
}