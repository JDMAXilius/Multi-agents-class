
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/Enumeration/PlayerEnums.h"
#include "AnimDataReceiverInterface.generated.h"

UINTERFACE(MinimalAPI)
class UAnimDataReceiverInterface : public UInterface
{
	GENERATED_BODY()
};


class SHOOTERCORE_API IAnimDataReceiverInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "AnimDataReceiver")
	void ReceiveCurrentGait(EGait Gait);

	UFUNCTION(BlueprintNativeEvent, Category = "AnimDataReceiver")
	void ReceiveCurrentLocomotionState(ELocomotionState LocomotionState);

	UFUNCTION(BlueprintNativeEvent, Category = "AnimDataReceiver")
	void ReceiveGroundDistance(float Distance);

	UFUNCTION(BlueprintNativeEvent, Category = "AnimDataReceiver")
	void ReceiveCurrentOverlayStates(EOverlayStates OverlayState);

};
