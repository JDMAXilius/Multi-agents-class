// Breachpoint. Log channels, collision channel aliases, and the GAS stage gate.
#include "Core/BRCore.h"

#include "Character/BRCharacter.h"

DEFINE_LOG_CATEGORY(LogBRCombat);
DEFINE_LOG_CATEGORY(LogBRNet);
DEFINE_LOG_CATEGORY(LogBRAI);
DEFINE_LOG_CATEGORY(LogBROnline);
DEFINE_LOG_CATEGORY(LogBRUI);

DEFINE_LOG_CATEGORY(LogBRInput);

const TCHAR* BRGas::ToString(EBRGasStage Stage)
{
	switch (Stage)
	{
	case EBRGasStage::Off:            return TEXT("Off");
	case EBRGasStage::AttributesOnly: return TEXT("AttributesOnly");
	case EBRGasStage::Granting:       return TEXT("Granting");
	case EBRGasStage::InputRouted:    return TEXT("InputRouted");
	case EBRGasStage::Sprint:         return TEXT("Sprint");
	case EBRGasStage::Weapons:        return TEXT("Weapons");
	case EBRGasStage::FullSandbox:    return TEXT("FullSandbox");
	case EBRGasStage::Cues:           return TEXT("Cues");
	default:                          return TEXT("<unknown>");
	}
}

EBRGasStage BRGas::GetStage()
{
	static const EBRGasStage Resolved = ABRCharacter::GetConfiguredGasStage();
	return Resolved;
}

bool BRGas::IsStageEnabled(EBRGasStage Required)
{
	return GetStage() >= Required;
}
