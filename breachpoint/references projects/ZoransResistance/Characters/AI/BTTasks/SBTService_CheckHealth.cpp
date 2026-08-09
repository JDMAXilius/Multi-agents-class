// Copyright Zollpa LLC


#include "SBTService_CheckHealth.h"
//#include "ZoransHealthComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

void USBTService_CheckHealth::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	APawn* AIPawn = OwnerComp.GetAIOwner()->GetPawn();
	if (ensure(AIPawn))
	{
		//USAttributeComponent* AttributeComp = USAttributeComponent::GetAttributes(AIPawn);
		//if (ensure(AttributeComp))
		{
			//bool bLowHealth = (AttributeComp->GetHealth() / AttributeComp->GetHealthMax()) < LowHealthFraction;

			//UBlackboardComponent* BlackBoardComp = OwnerComp.GetBlackboardComponent();
			//BlackBoardComp->SetValueAsBool(LowHealthKey.SelectedKeyName, bLowHealth);
		}
	}
}

USBTService_CheckHealth::USBTService_CheckHealth()
{
	LowHealthFraction = 0.3f;
}
