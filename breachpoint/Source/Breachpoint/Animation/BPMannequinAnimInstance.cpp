#include "Animation/BPMannequinAnimInstance.h"

// The interface setters are deliberately plain writes. ABP_Mannequin_Base's own
// implementations are exactly that - the graph read shows
//   SetADS(InADS): K2Node_SetVariableOnPersistentFrame(InADS); ExecuteUbergraph(0)
// - a variable write plus a graph kick. The kick has no equivalent here because the
// ubergraph is not ported; NativeUpdateAnimation is where that work will land.

void UBPMannequinAnimInstance::SetADS(bool InADS)
{
	// ADSStateChanged and WasADSLastUpdate are the ABP's own edge-detection pair; keeping
	// them in step here means a future ubergraph port does not have to re-derive the edge.
	ADSStateChanged = (bADS != InADS);
	WasADSLastUpdate = bADS;
	bADS = InADS;
}

void UBPMannequinAnimInstance::SetADS_Upper(bool InADS_Upper)
{
	IsADS_Upper = InADS_Upper;
}

void UBPMannequinAnimInstance::SetSprinting(bool InSprinting)
{
	BSprinting = InSprinting;
}

void UBPMannequinAnimInstance::SetUnarmed(bool InUnarmed)
{
	BUnarmed = InUnarmed;
}

void UBPMannequinAnimInstance::SetFPSMode(uint8 InFPSMode)
{
	// bFPSMode on the asset is a BOOL, not the enum the interface pin implies.
	BFPSMode = (InFPSMode != 0);
}

void UBPMannequinAnimInstance::SetFPSWalkMode(uint8 InFPSWalkMode)
{
	BFPSWalkMode = (InFPSWalkMode != 0);
}
