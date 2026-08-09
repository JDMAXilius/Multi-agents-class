#include "ZoransResistance/HMS/HMSSavedStateComponent.h"
#include "UObject/NoExportTypes.h"
#include "Engine.h"
#include "Engine/NetConnection.h"
#include "ImageUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "FindSessionsCallbackProxy.h"
#include "GameFramework/PlayerController.h"
#include "Serialization/StructuredArchive.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZoransResistance/HMS/HMSFunctions.h"
#include "ZoransResistance/HMS/FHMSActorSave.h"
#include "Kismet/KismetStringLibrary.h"

// Sets default values for this component's properties
UHMSSavedStateComponent::UHMSSavedStateComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	Enabled = true;
	bTickInEditor = true;
	SetIsReplicatedByDefault(true);
	
	SerializeSelf = true;
	Tag = 0;

	// ...
}


// Called when the game starts
void UHMSSavedStateComponent::BeginPlay()
{
	Super::BeginPlay();
	// ...
	if(SerializeSelf)
	{
		FString name = GetName();
		int32 index = UKismetStringLibrary::FindSubstring(name,".");
		ComponentsToSerialize.Add(UKismetStringLibrary::GetSubstring(name,index+1,name.Len()-index));
	}
}

// Called every frame
void UHMSSavedStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner()) return;
	if (!GetOwner()->HasAuthority()) return;

	if (Tag == 0)
		Tag = FMath::Rand() * FMath::Rand();
	// ...
}

void UHMSSavedStateComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHMSSavedStateComponent, Tag);
}

void UHMSSavedStateComponent::LoadActorState(FHMSActorSave State)
{
	UHMSFunctions::DeserializeActor(GetOwner(),State,ComponentsToSerialize,CustomStructuresToSerialize);
	if(GetOwner()->GetClass()->ImplementsInterface(UHMSSavedState::StaticClass()))
		IHMSSavedState::Execute_LoadActorState(GetOwner());
	UActorComponent* comp = nullptr;
	for(FString& str : ComponentsToSerialize)
	{
		comp = UHMSFunctions::GetComponentByName(GetOwner(),str);
		if(comp)
			if(comp->GetClass()->ImplementsInterface(UHMSSavedState::StaticClass()))
				IHMSSavedState::Execute_LoadActorState(comp);
	}
}

const FHMSActorState UHMSSavedStateComponent::SaveState() const
{
	if (!GetOwner()) return FHMSActorState();

	for (const FString& comp : ComponentsToSerialize)
	{
		UActorComponent* c = UHMSFunctions::GetComponentByName(GetOwner(), comp);

		if (!c) continue;

		if (c->Implements<UHMSSavedState>())
			IHMSSavedState::Execute_PreSaveActorState(c);
	}

	const FHMSActorSave save = UHMSFunctions::SerializeActor(GetOwner(), ComponentsToSerialize, CustomStructuresToSerialize);
	FHMSActorState state;
	state.ActorSave = save;
	state.ActorClass = UHMSFunctions::ClassToString(GetOwner()->GetClass());

	const APawn* pawn = Cast<APawn>(GetOwner());
	const APlayerController* pc = nullptr;
	if (pawn)
		pc = Cast<APlayerController>(pawn->GetController());

	if (pc)
	{
		FString ID;
		bool valid = false;
		UHMSFunctions::GetPlayerUniqueNetID(pc, ID, valid);
		if (valid)
			state.ControllerID = ID;
	}

	return state;
}

void UHMSSavedStateComponent::ResetOwner()
{
	if (GetOwner())
		GetOwner()->Reset();
}

void UHMSSavedStateComponent::OwnerBeginPlayCall()
{
	if (GetOwner())
		GetOwner()->DispatchBeginPlay(false);
}
