#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "BNPlayerCameraManager.generated.h"

/** The view's pitch ceiling. The anim instance clamps the spine to the same window; without this
 *  the camera can look further than the arms can reach and the gun leaves the crosshair. */
UCLASS()
class BREACHPOINTNEXT_API ABNPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	ABNPlayerCameraManager();
};
