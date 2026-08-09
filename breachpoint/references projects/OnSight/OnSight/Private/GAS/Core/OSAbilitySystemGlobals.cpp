#include "GAS/Core/OSAbilitySystemGlobals.h"
#include "Data/OSHitDamageContext.h"

FGameplayEffectContext* UOSAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	// #103: Validate that our overridden context allocator returns the expected
	// custom context type. Downstream code relies on GetScriptStruct() checks
	// (see TryGetOSGameplayEffectContext).
	FOSGameplayEffectContext* Ctx = new FOSGameplayEffectContext();
	ensureAlwaysMsgf(
		Ctx && Ctx->GetScriptStruct() == FOSGameplayEffectContext::StaticStruct(),
		TEXT("[OSAbilitySystemGlobals] AllocGameplayEffectContext returned unexpected type. Returned=%s Expected=%s"),
		Ctx && Ctx->GetScriptStruct() ? *Ctx->GetScriptStruct()->GetName() : TEXT("<null>"),
		*FOSGameplayEffectContext::StaticStruct()->GetName());
	return Ctx;



}
