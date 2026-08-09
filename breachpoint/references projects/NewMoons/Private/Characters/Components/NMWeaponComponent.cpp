#include "Characters/Components/NMWeaponComponent.h"
#include "Characters/NMCharacterBase.h"
#include "Engine/SkeletalMeshSocket.h"
#include "InventorySystem/Weapons/NMWeaponActor.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Projectiles/NMProjectileBouncing.h"
#include "Projectiles/NMProjectileBase.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/GameplayStaticsTypes.h"

// Sets default values for this component's properties
UNMWeaponComponent::UNMWeaponComponent()
	: CurrentWeaponIndex(-1)
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;

	EquippedWeapons.AddDefaulted(4);
}

ANMCharacterBase* UNMWeaponComponent::GetOwningCharacter()
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

void UNMWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UNMWeaponComponent, CurrentWeaponIndex, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNMWeaponComponent, EquippedWeapon, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNMWeaponComponent, EquippedWeapons, COND_None, REPNOTIFY_Always);
}

// Called when the game starts
void UNMWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UNMWeaponComponent::DestroyComponent(bool bPromoteChildren)
{
	//clear all delegates
	UnequipWeapon();
	// Detach the Weapon to the hand socket RightHandSocket
	Super::DestroyComponent(bPromoteChildren);
}

void UNMWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EquippedWeapon)
	{
		EquippedWeapon->Destroy();
	}
	Super::EndPlay(EndPlayReason);

	for (auto &WeaponInfo : EquippedWeapons)
	{
		if (IsValid(WeaponInfo))
		{
			//WeaponInfo->Destroy();
		}
	}
}

void UNMWeaponComponent::OnRep_CurrentWeapon(const ANMWeaponActor* OldWeapon)
{
	CurrentWeaponChanged.Broadcast(EquippedWeapon);
}

void UNMWeaponComponent::OnRep_WeaponInventory() const
{
	for (int32 x = 0; x < EquippedWeapons.Num(); x++)
	{
		if (CurrentWeaponIndex != x)
		{
			//if (EquippedWeapons.IsValidIndex(x) && IsValid(EquippedWeapons[x].SpawnedWeapon))
			{
				//EquippedWeapons[x].SpawnedWeapon->InternalStowWeapon(true);
			}
		}
		else
		{
			//if (EquippedWeapons.IsValidIndex(x) && IsValid(EquippedWeapons[x].SpawnedWeapon))
			{
				//EquippedWeapons[x].SpawnedWeapon->InternalReadyWeapon();
			}
		}
	}
	CurrentWeaponChanged.Broadcast(EquippedWeapon);
}

// Called every frame
void UNMWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UNMWeaponComponent::FireWeapon()
{
	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerFireWeapon();
		return;
	}

	if (bIsReloading || !EquippedWeapon) return;
	
	if (EquippedWeapon->CanFire())
	{
		EquippedWeapon->StartFire();
	}

	if(!EquippedWeapon->CanFire() && EquippedWeapon->CanReload())
	{
		ReloadWeapon();
	}
}

void UNMWeaponComponent::StopFireWeapon()
{
	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerStopFireWeapon();
		return;
	}
	EquippedWeapon->StopFire();
}

void UNMWeaponComponent::ServerStopFireWeapon_Implementation()
{
	StopFireWeapon();
}

void UNMWeaponComponent::ServerFireWeapon_Implementation()
{
	FireWeapon();
}

void UNMWeaponComponent::EquipWeapon(const FName WeaponName, const int32 WeaponIndex, USkeletalMeshComponent* Mesh, const FName& BoneSocket)
{
	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerEquipWeapon(WeaponName, WeaponIndex, Mesh, BoneSocket);
		return;
	}
	/*if (CurrentWeaponIndex == WeaponIndex || !EquippedWeapons.IsValidIndex(CurrentWeaponIndex)) return;

	EquippedWeapons[CurrentWeaponIndex] = EquippedWeapon;
	// Ensure only one weapon is visible at a time
	for (int32 i = 0; i < EquippedWeapons.Num(); ++i)
	{
		if (EquippedWeapons[i] && i != CurrentWeaponIndex)
		{
			EquippedWeapons[i]->SetActorHiddenInGame(true);
		}
	}
	// Hide current weapon
	if (EquippedWeapons.IsValidIndex(CurrentWeaponIndex))
	{
		EquippedWeapons[CurrentWeaponIndex]->SetActorHiddenInGame(true);
	}
	// Show new weapon
	if (EquippedWeapons.IsValidIndex(WeaponIndex))
	{
		EquippedWeapons[WeaponIndex]->SetActorHiddenInGame(false);
		CurrentWeaponIndex = WeaponIndex;

		// Notify clients about weapon change
		if (CurrentWeaponChanged.IsBound())
		{
			CurrentWeaponChanged.Broadcast(EquippedWeapon);
		}
	}*/

	if (CurrentWeaponIndex == WeaponIndex) return; //Need to be replace WeaponsClasses for WeaponDataTable

	if (bIsReloading)
	{
		return;
	}

	UnequipWeapon(); 
	ensure(WeaponDataTable);
	
	if (const auto WeaponData = WeaponDataTable->FindRow<FWeaponData>(WeaponName, "", true))
	{
		FActorSpawnParameters WeaponSpawnParams;
        WeaponSpawnParams.Owner = GetOwningCharacter();
        WeaponSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        //if (ANMWeaponActor* WeaponToEquip = SpawnWeapon(WeaponIndex))
        if (ANMWeaponActor* WeaponToEquip =	GetWorld()->SpawnActor<ANMWeaponActor>(WeaponData->WeaponClass, WeaponSpawnParams))
        {
        	// Get the Hand Socket
        	const USkeletalMeshSocket* HandSocket = Mesh->GetSocketByName(BoneSocket);
        	if (HandSocket)
        	{
        		// Attach the Weapon to the hand socket RightHandSocket
        		HandSocket->AttachActor(WeaponToEquip, Mesh);
        	}

        	CurrentWeaponIndex = WeaponIndex;
        	EquippedWeapon = WeaponToEquip;
        	EquippedWeapon->SetOwner(GetOwner());
        	if (CurrentWeaponChanged.IsBound())
        	{
        		CurrentWeaponChanged.Broadcast(EquippedWeapon);
        	}
        }
	}
}

void UNMWeaponComponent::ServerEquipWeapon_Implementation(const FName WeaponName, const int32 WeaponIndex, USkeletalMeshComponent* Mesh, const FName& BoneSocket)
{
	EquipWeapon(WeaponName, WeaponIndex, Mesh, BoneSocket);
}

void UNMWeaponComponent::UnequipWeapon()
{
	if (EquippedWeapon)
	{
		EquippedWeapon->Destroy();
		EquippedWeapon = nullptr;
		CurrentWeaponIndex = -1;
	}
}

void UNMWeaponComponent::GiveAmmo(const int32 Amount)
{
	BagAmmo += Amount;
}

void UNMWeaponComponent::UseAmmo(const int32 Amount)
{
	const int32 LoadedAmmo = EquippedWeapon->GetLoadedAmmo();

	if (LoadedAmmo <= 0)
	{
		ReloadWeapon();
	}

	if (LoadedAmmo < Amount)
	{
		EquippedWeapon->SetLoadedAmmo(LoadedAmmo - Amount);
	}
}

void UNMWeaponComponent::ReloadWeapon()
{
	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerReloadWeapon();
	}
	else
	{
		if (!bIsReloading && EquippedWeapon && EquippedWeapon->CanReload())
		{
			bIsReloading = true;

			if (EquippedWeapon->GetCharacterReloadMontage())
			{
				MulticastPlayAnimation(EquippedWeapon->GetCharacterReloadMontage(), true);
			}

			// Set timer to finish reload based on animation duration
			float ReloadDuration = EquippedWeapon->GetCharacterReloadMontage()->GetPlayLength() - 0.3f;
			GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, [&]()
			{
				int32 AmmoToReload = FMath::Min(EquippedWeapon->GetMaxClipAmmo() - EquippedWeapon->GetLoadedAmmo(), BagAmmo);
				EquippedWeapon->Reload(AmmoToReload);
				BagAmmo -= AmmoToReload;
				bIsReloading = false;
			}, ReloadDuration, false);
		}
	}
}

void UNMWeaponComponent::MulticastPlayAnimation_Implementation(UAnimMontage* AnimMontage, const bool InEquippedWeapon)
{
	if (InEquippedWeapon)
	{
			EquippedWeapon->GetOwningCharacter()->PlayAnimMontage(AnimMontage);
	}
	else
	{
		GetOwningCharacter()->PlayAnimMontage(AnimMontage);
	}
}

void UNMWeaponComponent::ServerReloadWeapon_Implementation()
{
	ReloadWeapon();
}

ANMWeaponActor* UNMWeaponComponent::SpawnWeapon(const int32 WeaponIndex)
{
	return GetWorld()->SpawnActor<ANMWeaponActor>(WeaponsClasses[WeaponIndex]);
}
void UNMWeaponComponent::ThrowGrenade()
{
	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerThrowGrenade();
		return;
	}
	else
	{
		if (GrenadeClass)
		{
			MulticastPlayAnimation(ThrowGrenadeMontage, false);
			PreviewGrenadePath();
			
			FHitResult HitResult = GetProjectileTraceResult();
			//FHitResult HitResult = GetEquippedWeapon()->GetHitScanTraceResult();
	
			FVector MuzzleLocation = GetOwningCharacter()->GetMesh()->DoesSocketExist(FName("LeftHandSocket")) ? GetOwningCharacter()->GetMesh()->GetSocketLocation(FName("LeftHandSocket")) + GetOwner()->GetActorForwardVector() * 100.0f : GetOwningCharacter()->GetMesh()->GetComponentLocation() + GetOwner()->GetActorForwardVector() * 100.0f;
			// Determine target location
			FVector TargetLocation = HitResult.bBlockingHit ? HitResult.Location : HitResult.TraceEnd;	
			// Calculate spawn location and launch direction
			FVector SpawnLocation = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 100.0f; // Slightly in front
			FVector LaunchDirection = (TargetLocation - MuzzleLocation).GetSafeNormal();
			LaunchDirection.Z += 0.2f; // Add upward arc
			FRotator SpawnRotation =/* UKismetMathLibrary::FindLookAtRotation(SpawnLocation, TargetLocation);*/ LaunchDirection.Rotation();
			FVector LaunchVelocity = LaunchDirection * GrenadeThrowSpeed;// UKismetMathLibrary::GetForwardVector(SpawnRotation) * GrenadeThrowSpeed;
			
			// Calculate the launch direction toward the crosshair
			FVector LaunchDirectionW = (TargetLocation - MuzzleLocation).GetSafeNormal() + GetOwner()->GetActorForwardVector() * GrenadeThrowSpeed;
			// Adjust the spawn rotation to match the launch direction
			FRotator SpawnRotationW = LaunchDirectionW.Rotation();

			// Set spawn parameters
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = GetOwner();
			SpawnParams.Instigator = Cast<APawn>(GetOwner());

			if (ANMProjectileBase* SpawnedGrenade = GetWorld()->SpawnActor<ANMProjectileBase>(GrenadeClass, MuzzleLocation, SpawnRotation, SpawnParams))
			{
				//SpawnedGrenade->PredictProjectilePath(3, 0.05f, 3.0f);
				//SpawnedGrenade->CollisionComponent->SetPhysicsLinearVelocity(LaunchVelocity);
				SpawnedGrenade->SetInitialVelocity(LaunchVelocity);
			}
		}
	}
}

void UNMWeaponComponent::ServerThrowGrenade_Implementation()
{
	ThrowGrenade();
}

void UNMWeaponComponent::PreviewGrenadePath()
{
	if (GrenadeClass)
	{
		FHitResult HitResult = GetProjectileTraceResult();
		FVector TargetLocation = HitResult.bBlockingHit ? HitResult.Location : HitResult.TraceEnd;	
		// Calculate spawn location and launch direction
		FVector MuzzleLocation = GetOwningCharacter()->GetMesh()->DoesSocketExist(FName("LeftHandSocket")) ? GetOwningCharacter()->GetMesh()->GetSocketLocation(FName("LeftHandSocket")) + GetOwner()->GetActorForwardVector() * 100.0f : GetOwningCharacter()->GetMesh()->GetComponentLocation() + GetOwner()->GetActorForwardVector() * 100.0f;
		FVector SpawnLocation = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 100.0f; // Slightly in front
		FVector LaunchDirection = (TargetLocation - MuzzleLocation).GetSafeNormal();
		LaunchDirection.Z += 0.2f; // Add upward arc
		FRotator SpawnRotation = /*UKismetMathLibrary::FindLookAtRotation(SpawnLocation, TargetLocation);*/ LaunchDirection.Rotation();
		FVector LaunchVelocity = LaunchDirection * GrenadeThrowSpeed; //UKismetMathLibrary::GetForwardVector(SpawnRotation) * GrenadeThrowSpeed;
		
		//FVector SpawnLocation = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 100.0f;
		//FVector LaunchDirection = GetOwner()->GetActorForwardVector() + FVector(0.0f, 0.0f, 0.2f); // Slight upward arc

		//DrawDebugLine(GetWorld(), SpawnLocation, TargetLocation, FColor::Green, false, 2.0f, 0, 2.0f);
		//DrawDebugSphere(GetWorld(), TargetLocation, 10.0f, 12, FColor::Red, false, 2.0f);
		
		FPredictProjectilePathParams Params;
		Params.StartLocation = MuzzleLocation;
		Params.LaunchVelocity = LaunchVelocity;
		Params.ProjectileRadius = 5.0f; // Match grenade collision radius
		Params.OverrideGravityZ = GetWorld()->GetGravityZ() * 1.0f; // Replace 1.0f with actual gravity scale
		Params.SimFrequency = 15.0f;
		Params.MaxSimTime = 3.0f; // Matches grenade's expected lifetime
		Params.bTraceWithCollision = true;
		Params.TraceChannel = ECC_Visibility; // Ensure it matches grenade collision channel

		FPredictProjectilePathResult PathResult;
		if (UGameplayStatics::PredictProjectilePath(GetWorld(), Params, PathResult))
		{
			// Draw debug trajectory
			for (const FPredictProjectilePathPointData& Point : PathResult.PathData)
			{
				//DrawDebugSphere(GetWorld(), Point.Location, 5.0f, 12, FColor::Green, false, 2.0f);
			}
		}
		
		// Create a temporary grenade actor to calculate the path
		ANMProjectileBase* TempGrenade = GetWorld()->SpawnActor<ANMProjectileBase>(GrenadeClass, MuzzleLocation, SpawnRotation);
		if (TempGrenade)
		{
			FVector Velocity = LaunchDirection * GrenadeThrowSpeed;
			TempGrenade->ProjectileMovement->Velocity = Velocity; //LaunchDirection * GrenadeThrowSpeed;
			//TempGrenade->CollisionComponent->SetPhysicsLinearVelocity(LaunchVelocity);
			TempGrenade->PredictProjectilePath(3, 0.05f, 3.0f); // Simulate path for up to 3 bounces
			TempGrenade->Destroy(); // Destroy temporary grenade after prediction
		}
	}

}

FHitResult UNMWeaponComponent::GetProjectileTraceResult()
{
	//const FTransform& CameraTransform = OwnCharacter->GetCameraComponent()->GetComponentTransform();
	//const FVector FocusTraceStart = CameraTransform.GetLocation();
	//const FVector FocusTraceEnd = CameraTransform.GetLocation() + CameraTransform.GetRotation().Vector() * Tracedistance;
	// Get the controller's location and rotation
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner()->GetInstigatorController());
	FVector CameraLocation;
	FRotator CameraRotation;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	// Calculate target location (crosshair aim)
	FVector TraceStart = CameraLocation;
	FVector TraceEnd = TraceStart + CameraRotation.Vector() * 10000.0f; // Far distance
	// Perform a line trace to determine the target location
	//FVector TraceStart = GetOwningCharacter()->GetCamera()->GetComponentTransform().GetLocation();
	//FVector TraceEnd = TraceStart + GetOwningCharacter()->GetCamera()->GetForwardVector() * 10000.0f; // Far distance
	
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwningCharacter());
	TArray<AActor*> ActorsToIgnore = { GetOwningCharacter() };
	//GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	UKismetSystemLibrary::LineTraceSingle(this, TraceStart, TraceEnd, TraceTypeQuery1, true, ActorsToIgnore, EDrawDebugTrace::None, HitResult, true);

	return HitResult;
	//if (HitSuccess){ NewHit.RelativeHitLocation = InHit.ImpactPoint;}
	//else{NewHit.RelativeHitLocation = InHit.TraceEnd; }

	//NewHit.LaunchRotation = UKismetMathLibrary::FindLookAtRotation(NewHit.SpawnLocation, NewHit.RelativeHitLocation);
	//NewHit.LaunchDirection = UKismetMathLibrary::GetForwardVector(NewHit.LaunchRotation);
	
	FVector MuzzleLocation = GetOwningCharacter()->GetMesh()->DoesSocketExist(FName("LeftHandSocket")) ? GetOwningCharacter()->GetMesh()->GetSocketLocation(FName("LeftHandSocket")) : GetOwningCharacter()->GetMesh()->GetComponentLocation();
	// Determine target location
	FVector TargetLocation = HitResult.bBlockingHit ? HitResult.Location : TraceEnd;
	FVector BaseDirection = (TargetLocation - MuzzleLocation).GetSafeNormal();
	FVector FinalDirection = BaseDirection.GetSafeNormal();
	// Calculate the final trace end with spread applied
	FVector WeaponTraceEnd = MuzzleLocation + FinalDirection * 10000.0f;
	
	FHitResult Hits;
	// Perform a sphere trace from the muzzle location
	UKismetSystemLibrary::LineTraceSingle(this, MuzzleLocation, WeaponTraceEnd, TraceTypeQuery1, true, ActorsToIgnore, EDrawDebugTrace::None, Hits, true);

	
}
void UNMWeaponComponent::ChangeFireMode()
{
	if (EquippedWeapon)
	{
		FWeaponData WeaponData;
		WeaponData.FireMode = static_cast<EWeaponFireMode>((static_cast<uint8>(EquippedWeapon->GetWeaponDefaults().FireMode) + 1) % 3);
		EquippedWeapon->WeaponDefaults.FireMode = WeaponData.FireMode;
	}
}