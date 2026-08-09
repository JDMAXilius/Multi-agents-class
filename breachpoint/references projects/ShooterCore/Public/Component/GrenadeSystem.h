
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrenadeSystem.generated.h"

class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;
class AGrenadeSplineEndPointBase;
class AGrenadeBase;
class APlayerCharacter;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SHOOTERCORE_API UGrenadeSystem : public UActorComponent
{
	GENERATED_BODY()

public:	
	UGrenadeSystem();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

public:
	void CreatePredictionSpline();
	void DrawPredictionSpline();
	void DestroyPredictionMesh();
	void DestroyPredictionSpline();
	void SetGrenadeMesh(bool bGrenadeInHand);
	FVector GetThrowVelocity();
	FVector GetThrowLocation();
	void Throw();
	void ThrowCancel();

private:
	USplineComponent* PredictionSpline;
	AGrenadeSplineEndPointBase* PredictionEndPoint;
	APlayerCharacter* Player;
	int32 SplineIndex;

public:
	bool bInHand;
	bool bThrow;
	bool bCancel;

	UPROPERTY()
	TArray<USplineMeshComponent*> PredictionSplineMeshArray;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|GrenadeSystem", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AGrenadeBase> GrenadeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|GrenadeSystem", meta = (AllowPrivateAccess = "true"))
	UStaticMesh* PredictionSplineMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|GrenadeSystem", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AGrenadeSplineEndPointBase> PredictionEndPointClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|GrenadeSystem", meta = (AllowPrivateAccess = "true"))
	float ThrowSpeed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|GrenadeSystem", meta = (AllowPrivateAccess = "true"))
	float SplineMeshLength;

};
