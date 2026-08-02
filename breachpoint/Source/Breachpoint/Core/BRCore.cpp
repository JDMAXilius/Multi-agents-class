#include "Core/BRCore.h"

#include "Character/BRCharacter.h"

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
