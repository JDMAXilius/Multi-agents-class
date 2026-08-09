#include "Characters/Player/OSPlayer.h"
#include "Core/OSPlayerState.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "Components/OSHealthComponent.h"
#include "Core/OSPlayerController.h"
#include "Data/OSGameplayTags.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"

AOSPlayer::AOSPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->ProbeChannel = ECC_Camera;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

AOSPlayerController* AOSPlayer::GetOSPlayerController()
{
	if (PlayerController) return PlayerController;
	auto pc = GetController();
	if (!pc) return nullptr;
	PlayerController = Cast<AOSPlayerController>(pc);
	return PlayerController;
}

bool AOSPlayer::IsDead() const
{
	if (!HealthComponent) return false;
	return HealthComponent->IsDead();
}
void AOSPlayer::BeginPlay()
{
	Super::BeginPlay();
	PlayerController = Cast<AOSPlayerController>(Controller);
}

void AOSPlayer::InitializeGAS()
{
	InitAbilitySystemComponent();
	Super::InitializeGAS();
	
}

void AOSPlayer::InitAbilitySystemComponent()
{
	AOSPlayerState* OSPState = Cast<AOSPlayerState>(GetPlayerState());
	if (!OSPState) return;

	UOSAbilitySystemComponent* PSASC = Cast<UOSAbilitySystemComponent>(OSPState->GetAbilitySystemComponent());
	if (!PSASC) return;

	AbilitySystemComponent = PSASC;
	AttributeSet = OSPState->GetAttributeSet();

	// NOTE (#94): Avoid calling InitAbilityActorInfo here.
	// AOSCharacter::InitializeGAS (called during PossessedBy/OnRep_PlayerState) is the
	// canonical place to refresh AbilityActorInfo to the current pawn.
}

// Server: Super::PossessedBy -> AOSCharacter::InitializeGAS (virtual dispatch handles full init).
// Do NOT call InitializeGAS() here — the Super chain already does it, and a second call
// would double-register tag listeners and duplicate runtime bindings.
void AOSPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	PlayerController = Cast<AOSPlayerController>(NewController);
}

// Client: Super::OnRep_PlayerState → AOSCharacter::InitializeGAS (virtual dispatch).
// Same rationale as PossessedBy — don't call InitializeGAS() directly.
// Note: OnRep_PlayerState never fires on listen server host (locally-set replicated props
// don't trigger OnRep), so PossessedBy is the sole init path for the host player.
void AOSPlayer::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
}

void AOSPlayer::RequestSetLoadoutIndex(int32 NewIndex)
{
	if (HasAuthority())
	{
		ServerRequestSetLoadoutIndex(NewIndex);
		return;
	}

	ServerRequestSetLoadoutIndex(NewIndex);
}

void AOSPlayer::ServerRequestSetLoadoutIndex_Implementation(int32 NewIndex)
{
	if (AOSPlayerState* PS = GetPlayerState<AOSPlayerState>())
	{
		PS->SetCurrentLoadoutIndex(NewIndex);
	}
}

// No additional replicated properties in AOSPlayer (camera components are not replicated).
