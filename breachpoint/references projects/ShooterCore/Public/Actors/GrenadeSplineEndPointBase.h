
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrenadeSplineEndPointBase.generated.h"

class UStaticMeshComponent;

UCLASS()
class SHOOTERCORE_API AGrenadeSplineEndPointBase : public AActor
{
	GENERATED_BODY()
	
public:	
	AGrenadeSplineEndPointBase();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|StaticMesh", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Mesh;
	

};
