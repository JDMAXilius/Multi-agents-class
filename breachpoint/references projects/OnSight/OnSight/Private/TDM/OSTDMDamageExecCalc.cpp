#include "TDM/OSTDMDamageExecCalc.h"

#include "AbilitySystemComponent.h"
#include "Data/OSGameplayTags.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "GenericTeamAgentInterface.h"

UOSTDMDamageExecCalc::UOSTDMDamageExecCalc()
{
}

void UOSTDMDamageExecCalc::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	const UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	if (!IsValid(SourceASC) || !IsValid(TargetASC))
	{
		return;
	}

	const AActor* SourceActor = SourceASC->GetAvatarActor();
	const AActor* TargetActor = TargetASC->GetAvatarActor();
	if (!IsValid(SourceActor) || !IsValid(TargetActor))
	{
		return;
	}

	// TDM: no friendly fire (same team, excluding self-damage path below).
	if (SourceActor != TargetActor)
	{
		const IGenericTeamAgentInterface* SourceTeam = Cast<IGenericTeamAgentInterface>(SourceActor);
		const IGenericTeamAgentInterface* TargetTeam = Cast<IGenericTeamAgentInterface>(TargetActor);
		if (SourceTeam && TargetTeam)
		{
			if (FGenericTeamId::GetAttitude(SourceTeam->GetGenericTeamId(), TargetTeam->GetGenericTeamId()) == ETeamAttitude::Friendly)
			{
				return;
			}
		}
	}

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	const float IncomingDamage = FMath::Max(0.f, Spec.GetSetByCallerMagnitude(Tags.Data_Damage, false, 0.f));
	if (IncomingDamage <= 0.f)
	{
		return;
	}

	// Same pipeline as UExecCalc_OSDamage overflow path: Damage meta -> PostGameplayEffectExecute
	// (Health delta, FOSDeathEventInfo / Event_Death, hit react, etc.). No block/stamina here.
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UOSAttributeSet::GetDamageAttribute(),
		EGameplayModOp::Additive,
		IncomingDamage));
}
