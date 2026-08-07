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
class UAnimInstance;
class UBRAbilitySet;
class UBRWeaponInstance;
class USkeletalMeshComponent;
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
	void EnsureWeaponMeshComponents();

	// The owner's own two bodies. 3P is the character mesh; 1P is the only-owner-see mesh
	// beside it. Either may be null on a pawn that has just one.
	void ResolveOwnerMeshes(USkeletalMeshComponent*& OutFirstPerson, USkeletalMeshComponent*& OutThirdPerson) const;

	// Points the owner's meshes at the held weapon's anim BPs, or back at whatever they
	// were wearing before the first equip when nothing is held.
	void RefreshOwnerAnimLayers();

	// The socket SK_Mannequin actually carries, and the one the template's AShooterCharacter
	// attaches both its meshes to. "GripPoint" — the previous default — is in no asset in
	// this project, and an attach to a name the skeleton does not know does not fail: it
	// falls back to the component origin, putting the weapon at the pawn's feet. Attaching
	// to the hand_r BONE instead would be wrong the same way, just less visibly — the socket
	// exists to carry the grip offset and rotation that the bare bone has no idea about.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	FName WeaponAttachSocket = TEXT("HandGrip_R");

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

	// One per body the owner draws itself with. A pawn with separate first- and third-person
	// meshes hides one from the owning player and the other from everyone else, so a single
	// shared weapon mesh is visible to exactly the wrong half of the room whichever parent
	// it picks. Skeletal, because the held weapon animates.
	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> EquippedMeshComponents;

	// What the owner's meshes wore before any weapon touched them. Captured once, on the
	// first equip, so dropping the last weapon returns the pawn to its unarmed pose instead
	// of leaving it gripping air in a rifle stance.
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> DefaultFirstPersonAnimClass;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> DefaultThirdPersonAnimClass;

	bool bCapturedDefaultAnimClasses = false;

	uint32 EquipGeneration = 0;

	TSharedPtr<FStreamableHandle> MeshLoadHandle;
};
