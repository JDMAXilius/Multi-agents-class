
// Bullet shell actor that simulates ejected casing

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BulletShellBase.generated.h"

// Forward declaration
class UStaticMeshComponent;

UCLASS()
class SHOOTERCORE_API ABulletShellBase : public AActor
{
	GENERATED_BODY()
	
public:
	ABulletShellBase();
	// Adds impulse to simulate shell ejection
	void ShellImpulse(FVector Impulse);

private:
	// Shell mesh component with physics simulation
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* ShellMesh;
};
