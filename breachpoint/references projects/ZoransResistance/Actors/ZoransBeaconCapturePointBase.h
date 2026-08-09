// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoransBeaconCapturePointBase.generated.h"

class AZoransCharacterBase;
class USphereComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeaconHolderOverlap, AZoransCharacterBase*, ZoransCharacterBase);

UCLASS(Blueprintable)
class ZORANSRESISTANCE_API AZoransBeaconCapturePointBase : public AActor
{
	GENERATED_BODY()

public:
	AZoransBeaconCapturePointBase();

	UPROPERTY(BlueprintReadWrite)
	bool bActive = false;

	UFUNCTION()
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	bool IsPlayerBeaconHolder(const AZoransCharacterBase* ZoransCharacterBase) const;
	
protected:
	UPROPERTY(BlueprintAssignable)
	FOnBeaconHolderOverlap OnBeaconHolderOverlap;
	
	UPROPERTY(BlueprintReadOnly, EditInstanceOnly)
	int32 TeamIndex = 0;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	USceneComponent* SceneRoot = nullptr;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	UMeshComponent* Mesh = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	USphereComponent* CollisionShape = nullptr;

private:
	UFUNCTION()
	void OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
