#pragma once
// Blueprint-callable: SetTraceCollision(Character, Channel, Response). Used to toggle collision for attack traces or dodge.

#include "OSControlState.generated.h"

UCLASS()
class UOSControlState : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	static void SetTraceCollision(ACharacter* character, ECollisionChannel channel, ECollisionResponse newResponse);
	
	
	
};