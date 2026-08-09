// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ZoransResistance/Game/Data/StaticGameData.h"
#include "ZoransWeaponData.generated.h"

class UZoransGameplayAbility;
class UZoransGameplayEffect;
class UNiagaraSystem;
class AZoransWeaponActor;
class UReticleWidgetBase;

using namespace Zorans_StaticGameData;


UENUM(BlueprintType)
enum class EAmmoType : uint8
{
	None,
	Light,
	Medium,
	Heavy,
	Shotgun,
	Sniper,
	ExplosiveSmall,
	ExplosiveLarge
};

UENUM(BlueprintType)
enum class EDamageType : uint8
{
	None,
	Light,
	Medium,
	Heavy,
	Explosive,
	Impale
};

UENUM(BlueprintType)
enum class AIEngageRange : uint8
{
	None,
	Close,
	Close_Medium,
	Medium,
	Medium_Far,
	Far
};

static TMap<AIEngageRange,float> AIEngageRangeMap =
{
	{AIEngageRange::None, 0.0f},
	{AIEngageRange::Close, 1.0f},
	{AIEngageRange::Close_Medium, 2.0f},
	{AIEngageRange::Medium, 3.0f},
	{AIEngageRange::Medium_Far, 4.0f},
	{AIEngageRange::Far, 5.0f}
};

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	RegularWeapon,
	SpecialWeapon,
	ObjectiveWeapon
};

USTRUCT(BlueprintType)
struct FWeaponInfo
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AZoransWeaponActor> SpawnedWeapon = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FName WeaponName = "Unarmed";

	UPROPERTY(BlueprintReadOnly)
	int32 LoadedAmmo = 0;
};

USTRUCT(BlueprintType)
struct FWeaponStatusEffect
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TSubclassOf<UZoransGameplayEffect> StatusEffect = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	float ApplicationChance = 0.2f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	float EffectDuration = 2.f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TArray<FName> BodySectionFilter;
};

USTRUCT(BlueprintType)
struct FZoransWeaponData : public FTableRowBase 
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	FName WeaponDisplayName = NAME_None;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	EAmmoType AmmoType = EAmmoType::Light;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	int32 AmmoCapacity = 1;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	int32 AmmoPickupAmount = 1;

	// This is how many Hitscans the weapon will perform per shot. Useful for things like shotguns
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	int32 TracesPerShot = 1;

	// If this is a burst weapon, it will attempt to fire this many times per ability activation
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	int32 BurstCount = 0;

	// The base value of damage for this weapon. Modified by falloff curve over distance
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float DamagePerShot = 10.0f;

	// Used for Hitscan abilities on this weapon if applicable
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	EDamageType DamageType = EDamageType::Light;

	// Used as the impulse force if this is the killing hit
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float ImpactForce = 500.f;

	// The RPM (Rounds Per Minute) of the weapon. Used as a type of cooldown
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float RateOfFire = 300.f;

	// The range of a Hitscan
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float WeaponRange = 10000.0f;

	// The range of the initial trace towards crosshair to adjust the aim of the weapon
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float WeaponAssistRange = 10000.0f;

	// The amount of damage of a Hitscan based on the weapon range. Normalized value (0-1)
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	FRuntimeFloatCurve DamageFalloff;

	// The radius of the Hitscan
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float WeaponTraceDiameter = 2.0f;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics", DisplayName = "Aim Down Sights FOV")
	float AimDownSightsFieldOfView = 0.8f;
	
	// This is used to modify the amount of spread while aiming. It is a multiplier, so 0.5f means the spread will be cut in half, making it more accurate
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float ScopedSpreadModifier = 0.75f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float CrouchingSpreadModifier = 0.75f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	FRuntimeFloatCurve RecoilCurve;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	TSubclassOf<UCameraShakeBase> RecoilEffect;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	FVector RecoilDirection = FVector(0.f, 0.f, 1.f);

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float RecoilPerShot = 1.f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float RecoilMaximum = 8.f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float RecoilRecoverySpeed = 8.f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float RecoilAimAdjustment = 0.f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	FRuntimeFloatCurve SpreadCurve;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float SpreadPerShot = 1.f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float SpreadMaximum = 4.f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float SpreadRecoverySpeed = 4.f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float SpreadFromMovement = 1.f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float MeleeDamage = 35.0f;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	TMap<FName, float> BodySectionModifiers;

	// Weak points are any section that take more than 1.f damage. By default, they take increased damage. This modifier is on top of that.
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Statistics")
	float WeakPointBonus = 0.f;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TSubclassOf<UZoransGameplayAbility> PrimaryGameplayAbility = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TSubclassOf<UZoransGameplayAbility> SecondaryGameplayAbility = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TArray<TSubclassOf<UZoransGameplayAbility>> OptionalAddedGameplayAbilities;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TArray<TSubclassOf<UZoransGameplayEffect>> EquippedGameplayEffects;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TArray<FWeaponStatusEffect> WeaponStatusEffects;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability System")
	TArray<FWeaponStatusEffect> OwnerAppliedEffects;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Character Visuals")
	TSubclassOf<AZoransWeaponActor> WeaponClass;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Visuals")
	TObjectPtr<UAnimMontage> WeaponReloadMontage = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Visuals")
	TObjectPtr<UAnimMontage> WeaponMeleeMontage = nullptr;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Pickup")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Pickup")
	FVector PickupOffset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Pickup")
	FLinearColor PickupColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Pickup")
	float PickupCooldown = 10.0f;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Character Visuals")
	TSubclassOf<UAnimInstance> CharacterAnimationClass = nullptr;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Character Visuals")
	FName AttachSocket = CharacterAttachSocketNames::RightHand;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Character Visuals")
	FName OffhandAttachSocket = WeaponSocketNames::OffhandLocation;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Visuals")
	FName MuzzleSocket = WeaponSocketNames::Muzzle;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Visuals")
	FName StowSocket = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "User Interface")
	TObjectPtr<UTexture2D> WeaponIcon = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Sound")
	TObjectPtr<USoundWave> DryFireSound = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "User Interface")
	TSubclassOf<UReticleWidgetBase> ReticleWidgetClass;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "User Interface")
	TObjectPtr<UTexture2D> AmmoIcon = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "User Interface")
	TObjectPtr<UTexture2D> InventoryWeaponIcon = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "User Interface")
	FString WeaponDescription;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AI")
	AIEngageRange EngagementRange = AIEngageRange::None;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Pickup")
	bool bCanBeRandomizedOnPickup = false;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Pickup")
	bool bisSpecial = false;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Pickup")
	EWeaponType WeaponType = EWeaponType::RegularWeapon;

	FZoransWeaponData();
};

inline FZoransWeaponData::FZoransWeaponData()
{
	TArray<FRichCurveKey> Keys;
	const FRichCurveKey OriginKey = FRichCurveKey(0.f, 1.f);
	const FRichCurveKey EndKey = FRichCurveKey(1.f, .5f);
	Keys.Add(OriginKey);
	Keys.Add(EndKey);

	DamageFalloff.EditorCurveData.SetKeys(Keys);
}