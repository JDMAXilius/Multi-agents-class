// Copyright Zollpa LLC


#include "ZoransLocalPlayerSubsystemBase.h"

void UZoransLocalPlayerSubsystemBase::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

bool UZoransLocalPlayerSubsystemBase::ShouldCreateSubsystem(UObject* Outer) const
{
	if (this->GetClass()->IsInBlueprint()) { return true; } 

	return false;
}
