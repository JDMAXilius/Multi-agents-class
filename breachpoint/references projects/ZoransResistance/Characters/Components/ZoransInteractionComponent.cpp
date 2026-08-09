// Copyright Zollpa LLC.


#include "ZoransInteractionComponent.h"
#include "GameFramework/PlayerState.h"
#include "ZoransResistance/ZoransResistance.h"
#include "ZoransResistance/InventorySystem/Interactables/InteractionInterface.h"
#include "ZoransResistance/Widgets/ZoransWorldUserWidget.h"

static TAutoConsoleVariable<bool> CVarDebugDrawInteraction(TEXT("Zorans.ShowInteractTrace"), false, TEXT("Enable Debug Lines for Interact Component."), ECVF_Cheat);

UZoransInteractionComponent::UZoransInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	PrimaryComponentTick.TickInterval = 0.2f;

	TraceRadius = 50.0f;
	TraceDistance = 600.0f;
}

void UZoransInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	GetOwningPawn()->OnDestroyed.AddUniqueDynamic(this, &UZoransInteractionComponent::OwningPawnDestroyed);

	if (GetOwningPawn())
	{
		if (const APlayerState* OwningPlayerState = GetOwningPawn()->GetPlayerState())
		{
			OwningPlayerController = OwningPlayerState->GetPlayerController();
		}
	}
}

void UZoransInteractionComponent::Interact()
{
	if (FocusedActor == nullptr)
	{
		return;
	}

	if (GetOwningPawn())
	{
		if (GetOwningPawn())
		IInteractionInterface::Execute_Interact(FocusedActor, GetOwningPawn());

		ServerInteract(FocusedActor);
	}
}

APawn* UZoransInteractionComponent::GetOwningPawn()
{
	if (!OwningPawn.Get())
	{
		if (AActor* OwningActor = GetOwner())
		{
			OwningPawn = Cast<APawn>(OwningActor);
		}
	}

	return OwningPawn.Get();
}

APlayerController* UZoransInteractionComponent::GetOwningPlayerController()
{
	if (!OwningPlayerController.Get())
	{
		if (GetOwningPawn())
		{
			if (const APlayerState* OwningPlayerState = GetOwningPawn()->GetPlayerState())
			{
				OwningPlayerController = OwningPlayerState->GetPlayerController();
			}
		}
	}

	return OwningPlayerController.Get();
}

void UZoransInteractionComponent::DisableInteractionComponent()
{
	SetComponentTickEnabled(false);

	if (DefaultWidgetInstance)
	{
		DefaultWidgetInstance->RemoveFromParent();
	}

	FocusedActor = nullptr;
}

void UZoransInteractionComponent::ServerInteract_Implementation(AActor* InFocus)
{
	if (InFocus == nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, "No Focus Actor to interact.");

		return;
	}

	if (GetOwningPawn())
	{
		IInteractionInterface::Execute_Interact(InFocus, GetOwningPawn());
	}
}

void UZoransInteractionComponent::OwningPawnDestroyed(AActor* Actor)
{
	if (DefaultWidgetInstance)
	{
		DefaultWidgetInstance->RemoveFromParent();
	}
}

void UZoransInteractionComponent::FindBestInteractable()
{
	if (GetOwningPlayerController())
	{
		bool bDebugDraw = CVarDebugDrawInteraction.GetValueOnGameThread();

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_INTERACTABLE);
		
		FVector ViewStart;
		FRotator TraceDirection;
		GetOwningPlayerController()->GetPlayerViewPoint(ViewStart, TraceDirection);

		const FVector TraceStart = GetOwningPawn() ? GetOwningPawn()->GetActorLocation() : ViewStart;

		FVector TraceEnd = ViewStart + (TraceDirection.Vector() * TraceDistance);
		
		TArray<FHitResult> Hits;

		FCollisionShape Shape;
		Shape.SetSphere(TraceRadius);

		bool bBlockingHit = GetWorld()->SweepMultiByObjectType(Hits, TraceStart, TraceEnd, FQuat::Identity, ObjectQueryParams, Shape);

		FColor LineColor = bBlockingHit ? FColor::Green : FColor::Red;

		// Clear ref before trying to fill
		FocusedActor = nullptr;

		for (FHitResult Hit : Hits)
		{
			if (bDebugDraw)
			{
				DrawDebugSphere(GetWorld(), Hit.ImpactPoint, TraceRadius, 32, LineColor, false, 0.2f);
			}

			if (AActor* HitActor = Hit.GetActor())
			{
				if (HitActor->Implements<UInteractionInterface>())
				{
					FocusedActor = HitActor;

					break;
				}
			}
		}

		if (FocusedActor)
		{
			if (IInteractionInterface* InteractionInterface = Cast<IInteractionInterface>(FocusedActor))
			{
				FText Text = InteractionInterface->GetInteractText_Implementation();
			}
			
			if (DefaultWidgetInstance == nullptr && ensure(DefaultWidgetClass))
			{
				DefaultWidgetInstance = CreateWidget<UZoransWorldUserWidget>(GetOwningPlayerController(), DefaultWidgetClass);
			}

			if (DefaultWidgetInstance)
			{
				DefaultWidgetInstance->AttachedActor = FocusedActor;
				FVector BoundsOrigin;
				FVector BoundsExtent;
				FocusedActor->GetActorBounds(false, BoundsOrigin, BoundsExtent, false);

				DefaultWidgetInstance->WorldOffset = FVector(0.f, 0.f, BoundsExtent.Z * 0.5f);

				if (!DefaultWidgetInstance->IsInViewport())
				{
					DefaultWidgetInstance->AddToViewport(-1);
				}
			}
		}
		else
		{
			if (DefaultWidgetInstance)
			{
				DefaultWidgetInstance->RemoveFromParent();
			}
		}
	
		if (bDebugDraw)
		{
			DrawDebugLine(GetWorld(), TraceStart, TraceEnd, LineColor, false, 0.2f, 0, 4.f);
		}
	}
}

void UZoransInteractionComponent::DestroyComponent(const bool bPromoteChildren)
{
	// TODO: See if this works!
	if (DefaultWidgetInstance)
	{
		DefaultWidgetInstance->RemoveWidget();
	}
	
	Super::DestroyComponent(bPromoteChildren);
}

void UZoransInteractionComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwningPawn() &&  GetOwningPawn()->IsLocallyControlled())
	{
		FindBestInteractable();
	}
}
