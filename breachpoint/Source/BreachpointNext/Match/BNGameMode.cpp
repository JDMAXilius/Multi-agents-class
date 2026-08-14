#include "Match/BNGameMode.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "Match/BNPlayerController.h"
#include "Match/BNPlayerState.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "BreachpointNext.h"
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

void ABNGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);

	// Subscribed here rather than in the PlayerState's own BeginPlay: OnPostLogin is the engine's
	// guarantee that this controller HAS a PlayerState, and the mode is the one object that
	// outlives every pawn and every death, so nothing has to re-subscribe on respawn.
	if (ABNPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<ABNPlayerState>() : nullptr)
	{
		PS->OnPlayerDeath.AddUObject(this, &ABNGameMode::HandlePlayerDeath);
	}
}

void ABNGameMode::HandlePlayerDeath(ABNPlayerState* PlayerState)
{
	// This mode's answer to a death is a timed respawn. Another mode's could be a spectator
	// hand-off, or nothing at all — which is the point of hearing about the death instead of being
	// called by the ability that caused it.
	if (PlayerState)
	{
		RequestRespawn(Cast<AController>(PlayerState->GetOwner()));
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

		// A TRIPWIRE on an invariant that had none. The new pawn's health component watches CHANGES
		// only — reading the value at registration would kill a body on the frame it spawned — so
		// health MUST already be positive by the time that component exists. If the init GE ever
		// fails to run, the player is alive at zero and can never die again, because no change will
		// follow. Silent and permanent, and it presents as "that one player is unkillable".
		const float RestoredHealth = ASC->GetNumericAttribute(UBNAttributeSet::GetHealthAttribute());
		if (RestoredHealth <= 0.f)
		{
			UE_LOG(LogBN, Error, TEXT("BNGameMode: respawning %s with Health %.1f — the init GE did not restore it. "
				"This pawn will be unkillable: its health component only reports CHANGES, and none will come."),
				*GetNameSafe(PS), RestoredHealth);
		}
	}

	// Cleared explicitly, server-side. UBNGA_Death set these on the controller, and the comment
	// there says ClientRestart resets them — which it does, but ClientRestart is a CLIENT RPC, so
	// the server's copy of a remote player's controller runs no such reset. A remote client's
	// movement arrives as ServerMove into the CMC rather than through AddMovementInput, so this is
	// most likely harmless; "most likely" is not verified, the symptom would be a respawned player
	// who cannot move, and the cure is two lines.
	Controller->SetIgnoreMoveInput(false);
	Controller->SetIgnoreLookInput(false);

	// The engine's own start point. There is deliberately no ABNPlayerStart: AGameModeBase::
	// FindPlayerStart already picks an APlayerStart, and a BN subclass would hold nothing until
	// teams and spawn scoring exist — which is a later roadmap's, not this fence's.
	RestartPlayer(Controller);
}
