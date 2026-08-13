#include "Match/BNGameMode.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "Match/BNPlayerController.h"
#include "Match/BNPlayerState.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ABNGameMode::ABNGameMode()
{
	DefaultPawnClass = ABNCharacter::StaticClass();
	PlayerControllerClass = ABNPlayerController::StaticClass();
	PlayerStateClass = ABNPlayerState::StaticClass();
}

void ABNGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (UClass* PawnClass = DefaultPawnClassPath.TryLoadClass<APawn>())
	{
		DefaultPawnClass = PawnClass;
	}
}

void ABNGameMode::RequestRespawn(AController* Controller)
{
	UWorld* World = GetWorld();
	if (!Controller || !World)
	{
		return;
	}

	// The timer is the GAME MODE's, never the dying pawn's: the corpse is destroyed before the
	// delay is up and a timer living on it would be destroyed with it. Not stored — a respawn
	// already in flight is never cancelled this wave, so there is no handle anyone reads.
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateUObject(this, &ABNGameMode::RespawnPlayer, TWeakObjectPtr<AController>(Controller)),
		FMath::Max(0.1f, RespawnDelay), /*bLoop=*/false);
}

void ABNGameMode::RespawnPlayer(TWeakObjectPtr<AController> WeakController)
{
	AController* Controller = WeakController.Get();
	if (!Controller)
	{
		return;
	}

	// The corpse goes first, and explicitly: ABNCharacter::EndPlay is what takes the crouch GE off
	// the persistent ASC (debt A2) and UBNEquipmentComponent::EndPlay is what revokes the weapon's
	// ability set. Both must have run before the new body grants its own.
	if (APawn* OldPawn = Controller->GetPawn())
	{
		Controller->UnPossess();
		OldPawn->Destroy();
	}

	// Then the clean slate, on the ASC — which is the PERSISTENT PlayerState's, not the pawn's.
	// Cancel ends the death ability, and that is what removes State.Dead; the sweep then takes any
	// State.* GE the old life left behind. Both happen BEFORE the new pawn exists, so it is born
	// onto an ASC with no tags and full attributes rather than being cleaned up afterwards.
	ABNPlayerState* PS = Controller->GetPlayerState<ABNPlayerState>();
	if (UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr)
	{
		ASC->CancelAbilities();

		FGameplayTagContainer StateTags;
		StateTags.AddTag(BNTags::State);
		ASC->RemoveActiveEffectsWithGrantedTags(StateTags);

		// Through the init GE, never hand-set: that GE is the one place the starting numbers live.
		PS->ApplyInitAttributes();
	}

	// The engine's own start point. There is deliberately no ABNPlayerStart: AGameModeBase::
	// FindPlayerStart already picks an APlayerStart, and a BN subclass would hold nothing until
	// teams and spawn scoring exist — which is a later roadmap's, not this fence's.
	RestartPlayer(Controller);
}
