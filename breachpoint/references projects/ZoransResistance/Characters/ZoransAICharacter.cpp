// Copyright Zollpa LLC


#include "ZoransAICharacter.h"


// Sets default values
AZoransAICharacter::AZoransAICharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("SpringArmComponent")))
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AZoransAICharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AZoransAICharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AZoransAICharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

