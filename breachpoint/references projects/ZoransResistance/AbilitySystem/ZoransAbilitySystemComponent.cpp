// Copyright Zollpa LLC.


#include "ZoransAbilitySystemComponent.h"
#include "ZoransGameplayAbility.h"
#include "Attributes/CharacterAttributes.h"
#include "Attributes/HealthAttributes.h"
#include "Attributes/MovementAttributes.h"
#include "Data/ZoransGameplayTags.h"
#include "ZoransResistance/Animation/ZoransAnimInstance.h"

static TAutoConsoleVariable<bool> CVarDebugShowAbilitySystemDebugInfo(TEXT("Zorans.ShowAbilitySystemDebugInfo"), false, TEXT("Enable Zorans Ability System Component Debug Messages."), ECVF_Cheat);

void UZoransAbilitySystemComponent::SetupAbilityEnhancedInputBinds()
{
	const FGameplayTagContainer InputTagContainer{GameplayTag_HasInput.GetTag()};
	
	TArray<FGameplayAbilitySpecHandle> AbilitiesWithInputs;
	
	FindAllAbilitiesWithTags(AbilitiesWithInputs, InputTagContainer);

	for (auto const &AbilitySpecHandle : AbilitiesWithInputs)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(AbilitySpecHandle))
		{
			if (UZoransGameplayAbility* ZoransGameplayAbility = Cast<UZoransGameplayAbility>(AbilitySpec->GetPrimaryInstance()))
			{
				ZoransGameplayAbility->SetupEnhancedInputBindings(AbilityActorInfo.Get(), *AbilitySpec);
			}
		}
	}

	//HealthAttributeSet = GetSet<UHealthAttributes>();
	//CharacterAttributeSet = GetSet<UCharacterAttributes>();
	//MovementAttributeSet = GetSet<UMovementAttributes>();
	//HealthAttributeSet = CreateDefaultSubobject<UHealthAttributes>(TEXT("HealthAttributes"));
	//CharacterAttributeSet = CreateDefaultSubobject<UCharacterAttributes>(TEXT("CharacterAttributes"));
	//MovementAttributeSet = CreateDefaultSubobject<UMovementAttributes>(TEXT("MovementAttributes"));
	//GetOrCreateAttributeSubobject()
}

void UZoransAbilitySystemComponent::CancelAbilitiesOnDeath()
{
	FGameplayTagContainer CancelOnDeathContainer;
	CancelOnDeathContainer.AddTag(GameplayTag_HasInput);
	CancelOnDeathContainer.AddTag(GameplayTag_ActiveAbility);
	const FGameplayTagContainer* CancelOnDeath = &CancelOnDeathContainer;
		
	CancelAbilities(CancelOnDeath);

	FGameplayTagContainer EffectRemovalContainer;
	EffectRemovalContainer.AddTag(GameplayTag_GrantThrowdown);
	RemoveActiveEffectsWithGrantedTags(EffectRemovalContainer);
}

void UZoransAbilitySystemComponent::CheckAttributeSets()
{
	HealthAttributeSet = GetOrCreateAttributeSubobject(UHealthAttributes::StaticClass());
	CharacterAttributeSet = GetOrCreateAttributeSubobject(UCharacterAttributes::StaticClass());
	MovementAttributeSet = GetOrCreateAttributeSubobject(UMovementAttributes::StaticClass());
}

void UZoransAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);

	if (OnAbilityGrantedDelegate.IsBound())
	{
		OnAbilityGrantedDelegate.Broadcast(AbilitySpec);
	}
}

void UZoransAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	if (IsValid(InOwnerActor))
	{
		Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	}

	if (const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get())
	{
		if (UZoransAnimInstance* ZoransAnimInst = Cast<UZoransAnimInstance>(ActorInfo->GetAnimInstance()))
		{
			ZoransAnimInst->InitializeWithAbilitySystem(this);
		}
	}
}

void UZoransAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ReplicationMode == EGameplayEffectReplicationMode::Mixed)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetAvatarActor();
		SpawnParams.Instigator = Cast<APawn>(GetAvatarActor());
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		TargetingActor = GetWorld()->SpawnActor<AZoransTargetActor>(AZoransTargetActor::StaticClass(), SpawnParams);
	}
}

int32 UZoransAbilitySystemComponent::GenerateLocalProjectileID()
{
	if (LocalProjectileIndex >= ProjectileMaxIndex)
	{
		LocalProjectileIndex = 0;
	}

	LocalProjectileIndex++;
	return LocalProjectileIndex;
}

void UZoransAbilitySystemComponent::InternalProcessServerHitResult(const int32 InProjectileIndex, const FHitResult& InHitResult)
{
	ProcessProjectileHitResult(InProjectileIndex, InHitResult);
}

void UZoransAbilitySystemComponent::ProcessProjectileHitResult_Implementation(const int32 InProjectileIndex, const FHitResult& InHitResult)
{
	if (RegisteredProjectiles.Contains(InProjectileIndex))
	{
		RegisteredProjectiles[InProjectileIndex]->Internal_ClientHitReceived(InHitResult);
	}
}

void UZoransAbilitySystemComponent::RegisterLocalProjectile(const int32 InProjectileIndex, AZoransProjectileBase* InLocalProjectile)
{
	if (IsValid(InLocalProjectile))
	{
		RegisteredProjectiles.Add(InProjectileIndex, InLocalProjectile);
		InLocalProjectile->ProjectileExpiredDelegate.BindUObject(this, &UZoransAbilitySystemComponent::UnregisterLocalProjectile);
	}
}

void UZoransAbilitySystemComponent::UnregisterLocalProjectile(const int32 InProjectileIndex)
{
	if (RegisteredProjectiles.Contains(InProjectileIndex))
	{
		RegisteredProjectiles.Remove(InProjectileIndex);
	}
}

void UZoransAbilitySystemComponent::InternalProcessServerProjectileBounce(const int32 InProjectileIndex, const TArray<FBouncingProjectilePathSegment> &InBouncePath)
{
	ProcessServerProjectileBounce(InProjectileIndex, InBouncePath);
}

void UZoransAbilitySystemComponent::ProcessServerProjectileBounce_Implementation(const int32 InProjectileIndex,	const TArray<FBouncingProjectilePathSegment> &InBouncePath)
{
	if (RegisteredProjectiles.Contains(InProjectileIndex))
	{
		if (AZoransProjectileBase* Projectile = RegisteredProjectiles[InProjectileIndex])
		{
			if (AZoransProjectileBouncing* BouncingProjectile = Cast<AZoransProjectileBouncing>(Projectile))
			{
				BouncingProjectile->ReceiveServerAdjustedBounce(InBouncePath);
			}
		}
	}
}