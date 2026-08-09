// Fill out your copyright notice in the Description page of Project Settings.


#include "InventorySystem/Weapons/NMWeaponActor.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Characters/NMCharacterBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Camera/CameraComponent.h"
#include "InventorySystem/Weapons/Data/WeaponData.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundCue.h"


// Sets default values
ANMWeaponActor::ANMWeaponActor()
	: MuzzleSocketName(FName{"BarrelSocket"})
	, LoadedAmmo(10)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	NetUpdateFrequency = 15.f;

	WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
	SetRootComponent(WeaponRoot);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetRootComponent());
}

void ANMWeaponActor::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	if (IsValid(WeaponData.DataTable) && WeaponData.DataTable->GetRowNames().Contains(WeaponData.RowName))
	{
		WeaponDefaults = *WeaponData.DataTable->FindRow<FWeaponData>(WeaponData.RowName, "");
		SetLoadedAmmo(WeaponDefaults.MagazineCapacity); // Full clip
		WeaponIcon = WeaponDefaults.WeaponIcon;
		WeaponName = WeaponDefaults.WeaponDisplayName;
		AmmoIcon = WeaponDefaults.AmmoIcon;
		WeaponMesh->SetSkeletalMesh(WeaponDefaults.WeaponMesh);
	}
}

void ANMWeaponActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ANMWeaponActor, OwningCharacter, COND_InitialOnly);
	DOREPLIFETIME_CONDITION_NOTIFY(ANMWeaponActor, LoadedAmmo, COND_OwnerOnly, REPNOTIFY_Always);
}

void ANMWeaponActor::SetOwningCharacter(ANMCharacterBase* InOwningCharacter)
{
	// Check if there's already an owner and unbind the previous event if necessary
	if (OwningCharacter == nullptr)
	{
		OwningCharacter->OnDestroyed.RemoveDynamic(this, &ANMWeaponActor::OnOwnerDestroyed);
	}

	// Update the owning character
	OwningCharacter = InOwningCharacter;
	// Bind a new event if there's a new owner
	OwningCharacter->OnDestroyed.AddDynamic(this, &ANMWeaponActor::OnOwnerDestroyed);
}

void ANMWeaponActor::OnOwnerDestroyed(AActor* DestroyedActor)
{
	Destroy();
}

ANMCharacterBase* ANMWeaponActor::GetOwningCharacter()
{
	if (!OwningCharacter.Get())
	{
		if (GetOwner())
		{
			OwningCharacter = Cast<ANMCharacterBase>(GetOwner());
		}
	}

	return OwningCharacter.Get();
}

FTransform ANMWeaponActor::GetWeaponMuzzleTransform() const
{
	return GetWeaponMesh() ? GetWeaponMesh()->GetSocketTransform(MuzzleSocketName, ERelativeTransformSpace::RTS_World) : GetActorTransform();
}

FVector ANMWeaponActor::GetMuzzleLocation() const
{
	return  GetWeaponMesh() ? GetWeaponMesh()->GetSocketLocation(MuzzleSocketName) : GetActorLocation();
}

FVector ANMWeaponActor::GetlasserLocation() const
{
	return  GetWeaponMesh() ? GetWeaponMesh()->GetSocketLocation(LasserSocketName) : GetActorLocation();
}

FVector ANMWeaponActor::GetEjectBulletLocation() const
{
	return  GetWeaponMesh() ? GetWeaponMesh()->GetSocketLocation(EjectBulletSocketName) : GetActorLocation();
}

void ANMWeaponActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Destroy();
	Super::EndPlay(EndPlayReason);
}

void ANMWeaponActor::ServerFire_Implementation()
{
	Fire();
}

bool ANMWeaponActor::ServerFire_Validate()
{
	return true;
}

void ANMWeaponActor::PlayFireEffects(const FHitResult& Hits)
{
	USkeletalMeshComponent* UseWeaponMesh = GetWeaponMesh();

	// Play fire montage
	if (GetCharacterFireMontage())
		GetOwningCharacter()->PlayAnimMontage(GetCharacterFireMontage());

	if (UAnimInstance* AnimInstance = GetWeaponMesh()->GetAnimInstance(); AnimInstance && WeaponDefaults.WeaponFireMontage)
		AnimInstance->Montage_Play(WeaponDefaults.WeaponFireMontage);
	
	// Play fire sound
	if (WeaponDefaults.FireSound)
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponDefaults.FireSound, GetMuzzleLocation());
	
	// Spawn the muzzle flash
	if (WeaponDefaults.MuzzleEffect)
		UNiagaraFunctionLibrary::SpawnSystemAttached(WeaponDefaults.MuzzleEffect, WeaponMesh, MuzzleSocketName, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);

	// Spawn trail until target location
	if (WeaponDefaults.TracerEffect)
	{
		FTransform SpawnTransform;
		SpawnTransform.SetLocation(GetMuzzleLocation());
		FVector_NetQuantize TraceTo = Hits.bBlockingHit ? Hits.ImpactPoint : Hits.TraceEnd;
		SpawnTransform.SetRotation(Hits.bBlockingHit? (TraceTo - GetMuzzleLocation()).ToOrientationQuat() : GetOwningCharacter()->GetCamera()->GetForwardVector().ToOrientationQuat());
		FVector MuzzleLocation = WeaponMesh->GetSocketLocation(MuzzleSocketName);
		UNiagaraComponent* Tracer = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, WeaponDefaults.TracerEffect, GetMuzzleLocation(), SpawnTransform.GetRotation().Rotator());
		Tracer->SetVectorParameter(FName("EndLocation"), TraceTo);
		Tracer->SetFloatParameter(FName("Length"), (TraceTo - GetMuzzleLocation()).Length());
	}
	if (WeaponDefaults.LasserEffect)
	{
		FTransform SpawnTransform;
		SpawnTransform.SetLocation(GetlasserLocation());
		FVector_NetQuantize TraceTo = Hits.bBlockingHit ? Hits.ImpactPoint : Hits.TraceEnd;
		SpawnTransform.SetRotation(Hits.bBlockingHit? (TraceTo - GetlasserLocation()).ToOrientationQuat() : GetOwningCharacter()->GetCamera()->GetForwardVector().ToOrientationQuat());
		FVector MuzzleLocation = WeaponMesh->GetSocketLocation(LasserSocketName);
		UNiagaraComponent* Tracer = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, WeaponDefaults.LasserEffect, GetlasserLocation(), SpawnTransform.GetRotation().Rotator());
		Tracer->SetVectorParameter(FName("EndLocation"), TraceTo);
		Tracer->SetFloatParameter(FName("Length"), (TraceTo - GetlasserLocation()).Length());
	}
	if (WeaponDefaults.BulletEjectEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(WeaponDefaults.BulletEjectEffect, WeaponMesh, EjectBulletSocketName, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
		/*FTransform SpawnTransform;
		SpawnTransform.SetLocation(GetEjectBulletLocation());
		FVector_NetQuantize TraceTo = Hits.bBlockingHit ? Hits.ImpactPoint : Hits.TraceEnd;
		SpawnTransform.SetRotation(Hits.bBlockingHit? (TraceTo - GetMuzzleLocation()).ToOrientationQuat() : GetOwningCharacter()->GetCamera()->GetForwardVector().ToOrientationQuat());
		FVector MuzzleLocation = WeaponMesh->GetSocketLocation(EjectBulletSocketName);
		UNiagaraComponent* Tracer = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, WeaponDefaults.TracerEffect, GetMuzzleLocation(), SpawnTransform.GetRotation().Rotator());
		Tracer->SetVectorParameter(FName("EndLocation"), TraceTo);
		Tracer->SetFloatParameter(FName("Length"), (TraceTo - GetMuzzleLocation()).Length());*/
	}
	if (WeaponDefaults.FireCamShake)
	{
		if (APawn* MyOwner = Cast<APawn>(GetOwner()))
		{
			APlayerController* PC = Cast<APlayerController>(MyOwner->GetController());
			if (PC)
			{
				PC->ClientStartCameraShake(WeaponDefaults.FireCamShake);
			}
		}
	}
}

void ANMWeaponActor::PlayImpactEffects(const FHitResult& Hits)
{
	FVector MuzzleLocation = WeaponMesh->GetSocketLocation(MuzzleSocketName);
	FVector_NetQuantize TraceTo = Hits.bBlockingHit ? Hits.ImpactPoint : Hits.TraceEnd;
	FVector ShotDirection = TraceTo - MuzzleLocation;
	ShotDirection.Normalize();

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, WeaponDefaults.HitImpactEffect, TraceTo, ShotDirection.Rotation());
}

// Called when the game starts or when spawned
void ANMWeaponActor::BeginPlay()
{	
	Super::BeginPlay();
}

void ANMWeaponActor::Fire()
{
	if (!HasAuthority())
	{
		ServerFire();
		return;
	}
	
	if (!bCanFire) return;

	for (int32 i = 0; i < WeaponDefaults.TracesPerShot; ++i)
	{
		
		AActor* MyOwner = GetOwner();
		if (MyOwner && CanFire())
		{
			FHitResult Hit = GetHitScanTraceResult();
			bool OutSuccess = Hit.bBlockingHit;
			if(OutSuccess)
			{
				AActor* HitActor = Hit.GetActor();
				UGameplayStatics::ApplyDamage(HitActor, WeaponDefaults.BaseDamage, MyOwner->GetInstigatorController(), MyOwner, UDamageType::StaticClass());
			}
	
			// Start fire cooldown timer
			bCanFire = false;
			GetWorld()->GetTimerManager().SetTimer(FireCooldownTimerHandle, [&]()
			{
				bCanFire = true;
			}, WeaponDefaults.TimeBetweenShots, false);
			
			//Multicasting version
			PlayWeaponEffects(Hit);
		}
	}
	ConsumeAmmo();
	LastFireTime = GetWorld()->TimeSeconds;
}

void ANMWeaponActor::StartFire()
{
	switch (WeaponDefaults.FireMode)
	{
	case EWeaponFireMode::Single:
		Fire();
		break;
	case EWeaponFireMode::Burst:
		BurstCounter = BurstCount;
		HandleBurstFire();
		break;
	case EWeaponFireMode::Automatic:
		GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &ANMWeaponActor::HandleAutomaticFire, FireInterval, true);
		break;
	}
}

void ANMWeaponActor::StopFire()
{
	if (WeaponDefaults.FireMode == EWeaponFireMode::Automatic)
	{
		GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
	}
}

void ANMWeaponActor::HandleBurstFire()
{
	if (BurstCounter > 0)
	{
		Fire();
		BurstCounter--;

		if (BurstCounter > 0)
		{
			GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &ANMWeaponActor::HandleBurstFire, FireInterval, false);
		}
	}
}

void ANMWeaponActor::HandleAutomaticFire()
{
	// Check if the weapon can fire (ammo check, cooldown, etc.)
	if (CanFire())
	{
		Fire();
	}
	else
	{
		// If unable to fire, clear the timer to stop the automatic fire
		GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
	}
}

FHitResult ANMWeaponActor::GetHitScanTraceResult()
{
	// Get weapon and character references
	ANMWeaponActor* WeaponActor = this;
	ANMCharacterBase* OwnCharacter = GetOwningCharacter();

	// Get camera transform and calculate trace start/end points
	FVector FocusTraceStart = OwnCharacter->GetCamera()->GetComponentTransform().GetLocation();
	FVector FocusTraceEnd = FocusTraceStart + OwnCharacter->GetCamera()->GetForwardVector() * WeaponDefaults.WeaponRange;

	// Prepare actors to ignore for tracing
	TArray<AActor*> ActorsToIgnoreFirstTrace = { OwnCharacter, WeaponActor };
	// Perform a line trace to focus
	FHitResult FocusHit;
	UKismetSystemLibrary::LineTraceSingle(this, FocusTraceStart, FocusTraceEnd, TraceTypeQuery1, false, ActorsToIgnoreFirstTrace, EDrawDebugTrace::None, FocusHit, true);

	// Determine the muzzle location and weapon trace end point
	FVector MuzzleLocation = WeaponActor->GetWeaponMuzzleTransform().GetLocation();
	FRotator ViewRotation = GetOwningCharacter()->GetControlRotation();
	//FVector SpreadDirection = GetSpreadDirection(ViewRotation);
	//FVector WeaponTraceEnd = MuzzleLocation + (FocusHit.bBlockingHit ? (FocusHit.Location - MuzzleLocation) + SpreadDirection : (FocusTraceEnd - MuzzleLocation)).GetSafeNormal() + SpreadDirection * WeaponDefaults.WeaponRange;

	// Determine the base direction to the target (either hit location or trace end)
	FVector TargetLocation = FocusHit.bBlockingHit ? FocusHit.Location : FocusTraceEnd;
	FVector BaseDirection = (TargetLocation - MuzzleLocation).GetSafeNormal();

	// Apply spread offset to the base direction
	FVector SpreadDirection = GetSpreadDirection(BaseDirection.Rotation());
	FVector FinalDirection = (BaseDirection + SpreadDirection).GetSafeNormal();
	// Calculate the final trace end with spread applied
	FVector WeaponTraceEnd = MuzzleLocation + FinalDirection * WeaponDefaults.WeaponRange;
	
	FHitResult Hits;
	// Prepare actors to ignore for tracing
	TArray<AActor*> ActorsToIgnore = { OwnCharacter, WeaponActor };
	// Perform a sphere trace from the muzzle location
	UKismetSystemLibrary::LineTraceSingle(this, MuzzleLocation, WeaponTraceEnd, TraceTypeQuery1, false, ActorsToIgnore, EDrawDebugTrace::None, Hits, true);

	// Determine the debug color based on whether any objects were hit
	//FColor DebugColor = Hits.bBlockingHit ? FColor::Green : FColor::Red;
	//DrawDebugLine(GetWorld(), MuzzleLocation, WeaponTraceEnd, DebugColor, false, 1.0f, 0, 1.0f);
		
	return Hits;
}

FVector ANMWeaponActor::GetSpreadDirection(const FRotator& ViewRotation) const
{
	// Get the forward direction of the view
	FVector AimDirection = ViewRotation.Vector();

	// Generate random offset within a small circle around the aim direction
	float SpreadRadius = FMath::DegreesToRadians(WeaponDefaults.BulletSpread);
	float RandomAngle = FMath::RandRange(0.0f, 2 * PI);
	float RandomRadius = FMath::RandRange(0.0f, SpreadRadius);

	// Calculate random offset in the local space of the forward vector
	FVector RightVector = FVector::CrossProduct(AimDirection, FVector::UpVector).GetSafeNormal();
	FVector UpVector = FVector::CrossProduct(RightVector, AimDirection).GetSafeNormal();
	FVector Offset = RightVector * FMath::Cos(RandomAngle) * RandomRadius +
					 UpVector * FMath::Sin(RandomAngle) * RandomRadius;

	// Add the offset to the aim direction
	FVector SpreadDirection = (AimDirection + Offset).GetSafeNormal();
	return SpreadDirection;

	/*// Get the right and up vectors to define a local space for the spread
	FVector ForwardVector = ViewRotation.Vector();
	FVector RightVector = FVector::CrossProduct(ForwardVector, FVector::UpVector).GetSafeNormal();
	FVector UpVector = FVector::CrossProduct(RightVector, ForwardVector).GetSafeNormal();

	// Randomly offset within a small cone around the forward vector
	float SpreadAngleRad = FMath::DegreesToRadians(WeaponDefaults.BulletSpread);
	float RandomAngle = FMath::RandRange(0.0f, 2 * PI);
	float SpreadRadius = FMath::RandRange(0.0f, FMath::Sin(SpreadAngleRad));

	// Calculate offset using the right and up vectors
	FVector SpreadOffset = (RightVector * FMath::Cos(RandomAngle) + UpVector * FMath::Sin(RandomAngle)) * SpreadRadius;

	return SpreadOffset;*/

}

void ANMWeaponActor::PlayWeaponEffects(const FHitResult& Hits)
{
	if (HasAuthority())
	{
		MulticastPlayWeaponEffects(Hits);
	}
	else
	{
		InternalPlayWeaponEffects(Hits);
	}
}

void ANMWeaponActor::SetLoadedAmmo(const int32 InAmmo)
{
	LoadedAmmo = InAmmo;
	UE_LOG(LogTemp, Log, TEXT("Set Loaded Ammo Value: %d"), LoadedAmmo);
	OnLoadedAmmoChangeDelegate.Broadcast(LoadedAmmo);
}

bool ANMWeaponActor::CanFire() const
{
	return LoadedAmmo > 0;
}

bool ANMWeaponActor::CanReload() const
{
	return LoadedAmmo < WeaponDefaults.MagazineCapacity;
}

void ANMWeaponActor::Reload(const int32& AmmoAmount)
{
	if (CanReload())
	{
		SetLoadedAmmo(LoadedAmmo + AmmoAmount);
	}
}

void ANMWeaponActor::OnRep_LoadedAmmo(const int32 OldValue)
{
	OnLoadedAmmoChangeDelegate.Broadcast(LoadedAmmo);
	
	if (LoadedAmmo <= 0)
	{
		OnLoadedAmmoEmpty.Broadcast();
	}
}

bool ANMWeaponActor::ConsumeAmmo(const int32 AmmoAmount)
{
	if (LoadedAmmo < AmmoAmount)
	{
		UE_LOG(LogTemp, Warning, TEXT("TryUseLoadedAmmo :: Insufficient Ammo"));
		return false;
	}

	SetLoadedAmmo(LoadedAmmo - AmmoAmount);
	return true;
}

void ANMWeaponActor::MulticastPlayWeaponEffects_Implementation(const FHitResult& Hits)
{
	if (Owner|| Owner->GetLocalRole() != ROLE_AutonomousProxy)
	{
		InternalPlayWeaponEffects(Hits);
	}
}

void ANMWeaponActor::InternalPlayWeaponEffects(const FHitResult& Hits)
{
	if (Hits.bBlockingHit) PlayImpactEffects(Hits);
	PlayFireEffects(Hits);
}
