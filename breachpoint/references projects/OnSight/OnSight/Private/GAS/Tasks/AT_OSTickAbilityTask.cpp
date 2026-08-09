#include "GAS/Tasks/AT_OSTickAbilityTask.h"

UAT_OSTickAbilityTask::UAT_OSTickAbilityTask()
{
	bTickingTask = true;
}

UAT_OSTickAbilityTask* UAT_OSTickAbilityTask::CreateTickTask(UGameplayAbility* OwningAbility, FName TaskInstanceName)
{
	return NewAbilityTask<UAT_OSTickAbilityTask>(OwningAbility, TaskInstanceName);
}

void UAT_OSTickAbilityTask::Activate()
{
	Super::Activate();
	bTaskActive = true;
}

void UAT_OSTickAbilityTask::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
	if (!bTaskActive || !Ability)
	{
		return;
	}
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnTick.Broadcast(DeltaTime);
	}
}
