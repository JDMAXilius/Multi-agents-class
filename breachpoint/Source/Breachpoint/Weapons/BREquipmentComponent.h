#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayAbilitySpecHandle.h"
#include "Templates/SubclassOf.h"

#include "BREquipmentComponent.generated.h"

class ABRWeaponPickup;
class APawn;
class FOutBunch;
class UAbilitySystemComponent;
class UActorChannel;
class UBRAbilitySet;
class UBRWeaponInstance;
class UStaticMeshComponent;
struct FBRWeaponRow;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class EBRWeaponSlot : uint8
{
	Primary,
	Secondary,

	Count UMETA(Hidden)
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FBROnEquippedWeaponChanged,
	class UBREquipmentComponent*, UBRWeaponInstance*);

struct FBRWeaponAbilityGrant
{
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	TArray<FActiveGameplayEffectHandle> EffectHandles;

	bool IsEmpty() const { return AbilityHandles.IsEmpty() && EffectHandles.IsEmpty(); }
	void Reset()
	{
		AbilityHandles.Reset();
		EffectHandles.Reset();
	}
};

UCLASS(ClassGroup = (Breachpoint), meta = (BlueprintSpawnableComponent))
class BREACHPOINT_API UBREquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBREquipmentComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void ReadyForReplication() override;
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UBRWeaponInstance* GetWeaponInSlot(EBRWeaponSlot Slot) const;
	UBRWeaponInstance* GetActiveWeapon() const;
	int32 GetActiveSlotIndex() const { return ActiveSlotIndex; }
	bool IsSlotOccupied(EBRWeaponSlot Slot) const;

	FBROnEquippedWeaponChanged OnEquippedWeaponChanged;

	UBRWeaponInstance* GiveWeapon(const FDataTableRowHandle& InWeaponRow, EBRWeaponSlot Slot);

	UBRWeaponInstance* GiveWeaponWithAmmo(const FDataTableRowHandle& InWeaponRow, EBRWeaponSlot Slot,
		int32 InAmmoInMag, int32 InAmmoReserve);

	bool SetActiveSlot(EBRWeaponSlot Slot);

	ABRWeaponPickup* DropWeapon(EBRWeaponSlot Slot);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestSwapSlot(uint8 SlotIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestPickup(ABRWeaponPickup* Pickup);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestDropSlot(uint8 SlotIndex);

protected:
	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_ActiveSlotIndex();

	void HandleActiveWeaponChanged();

	void GrantAbilitySetForSlot(int32 SlotIndex);

	void ClearAbilitySetForSlot(int32 SlotIndex);

	const UBRAbilitySet* ResolveAbilitySetForRow(const FDataTableRowHandle& InWeaponRow) const;

	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;

	APawn* GetOwnerPawn() const;

	void RefreshEquippedMesh();
	void EnsureWeaponMeshComponent();

	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	FName WeaponAttachSocket = TEXT("GripPoint");

	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TSubclassOf<ABRWeaponPickup> DroppedPickupClass;

private:
	bool HasServerAuthority() const;

	static int32 ToSlotIndex(EBRWeaponSlot Slot);
	static bool IsValidSlotIndex(int32 SlotIndex);

	void DestroyWeaponInSlot(int32 SlotIndex);

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<TObjectPtr<UBRWeaponInstance>> Slots;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveSlotIndex)
	int32 ActiveSlotIndex = INDEX_NONE;

	TArray<FBRWeaponAbilityGrant> SlotGrants;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> EquippedMeshComponent;

	uint32 EquipGeneration = 0;

	TSharedPtr<FStreamableHandle> MeshLoadHandle;
};
