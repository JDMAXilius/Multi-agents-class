#include "FPS/BPFPSWeapon.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogBPFPSWeapon, Log, All);

namespace
{
	/**
	 * The Blueprint variable names this class reads. Every one was read off the live asset,
	 * not guessed: `mcp-bp/bp_inventory_weapons.json` for the CDO types and defaults,
	 * `mcp-bp/weapon_graphs.json` for which function touches which.
	 *
	 * NOTE THE CASING TRAP: bp_extract.py camelCases its output, so the inventory shows
	 * `attachSocketName`. The REAL UPROPERTY name is PascalCase, and that is what
	 * FindFProperty matches. A lookup with the inventory's spelling silently finds nothing.
	 */
	const FName N_AvailableFireModes(TEXT("AvailableFireModes"));
	const FName N_CurFireModeIndex(TEXT("CurFireModeIndex"));
	const FName N_Scope(TEXT("Scope"));
	const FName N_DefaultScope(TEXT("DefaultScope"));
	const FName N_SkeletalMesh(TEXT("SkeletalMesh"));

	/** The two montage sets, UE5 then UE4, in the order the switch selects them. */
	const FName N_FireAnimMontage(TEXT("FireAnimMontage"));
	const FName N_AimFireAnimMontage(TEXT("AimFireAnimMontage"));
	const FName N_FireAnimMontage_UE4(TEXT("FireAnimMontage_UE4"));
	const FName N_AimFireAnimMontage_UE4(TEXT("AimFireAnimMontage_UE4"));

	const FName N_ReloadBoltAnimMontage(TEXT("ReloadBoltAnimMontage"));
	const FName N_AimReloadBoltAnimMontage(TEXT("AimReloadBoltAnimMontage"));
	const FName N_ReloadBoltAnimMontage_UE4(TEXT("ReloadBoltAnimMontage_UE4"));
	const FName N_AimReloadBoltAnimMontage_UE4(TEXT("AimReloadBoltAnimMontage_UE4"));

	/**
	 * E_FPST_SkeletalType, by the ordinals the switch used. Enumerator 0 is the UE4 mannequin
	 * and 1 is UE5 — read from the graph's own case order, not assumed from the names.
	 */
	constexpr uint8 SkeletalType_UE4 = 0;

	/** `GetScopeType` on the scope actor is a Blueprint function; called by name. */
	const TCHAR* F_GetScopeType = TEXT("GetScopeType");
}

// -----------------------------------------------------------------------------------------
// Reflection accessors.
// -----------------------------------------------------------------------------------------

bool ABPFPSWeapon::GetIntVar(FName VarName, int32& OutValue) const
{
	if (const FIntProperty* Prop = FindFProperty<FIntProperty>(GetClass(), VarName))
	{
		OutValue = Prop->GetPropertyValue_InContainer(this);
		return true;
	}
	return false;
}

bool ABPFPSWeapon::SetIntVar(FName VarName, int32 Value)
{
	if (const FIntProperty* Prop = FindFProperty<FIntProperty>(GetClass(), VarName))
	{
		Prop->SetPropertyValue_InContainer(this, Value);
		return true;
	}
	return false;
}

int32 ABPFPSWeapon::GetArrayLength(FName VarName) const
{
	if (const FArrayProperty* Prop = FindFProperty<FArrayProperty>(GetClass(), VarName))
	{
		FScriptArrayHelper Helper(Prop, Prop->ContainerPtrToValuePtr<void>(this));
		return Helper.Num();
	}
	return 0;
}

bool ABPFPSWeapon::GetByteArrayElement(FName VarName, int32 Index, uint8& OutValue) const
{
	const FArrayProperty* Prop = FindFProperty<FArrayProperty>(GetClass(), VarName);
	if (!Prop)
	{
		return false;
	}

	FScriptArrayHelper Helper(Prop, Prop->ContainerPtrToValuePtr<void>(this));
	if (!Helper.IsValidIndex(Index))
	{
		return false;
	}

	const uint8* Elem = reinterpret_cast<const uint8*>(Helper.GetRawPtr(Index));

	// A Blueprint enum array reaches C++ as either a byte or an enum inner, depending on how
	// the pin was authored. Both are handled rather than one being assumed.
	if (const FByteProperty* AsByte = CastField<FByteProperty>(Prop->Inner))
	{
		OutValue = AsByte->GetPropertyValue(Elem);
		return true;
	}
	if (const FEnumProperty* AsEnum = CastField<FEnumProperty>(Prop->Inner))
	{
		OutValue = static_cast<uint8>(AsEnum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Elem));
		return true;
	}
	return false;
}

UObject* ABPFPSWeapon::GetObjectVarRaw(FName VarName) const
{
	if (const FObjectPropertyBase* Prop = FindFProperty<FObjectPropertyBase>(GetClass(), VarName))
	{
		return Prop->GetObjectPropertyValue_InContainer(this);
	}
	return nullptr;
}

USkeletalMeshComponent* ABPFPSWeapon::GetWeaponMesh() const
{
	// Name first, then class — the same rule AMyCharacter learned the hard way, because a
	// Blueprint may carry more than one component of a class and only the name tells them
	// apart. `SkeletalMesh` is the SCS name in BP_FPST_BaseWeapon.
	for (UActorComponent* Component : GetComponents())
	{
		if (Component && Component->GetFName() == N_SkeletalMesh)
		{
			if (USkeletalMeshComponent* AsMesh = Cast<USkeletalMeshComponent>(Component))
			{
				return AsMesh;
			}
		}
	}
	return FindComponentByClass<USkeletalMeshComponent>();
}

// -----------------------------------------------------------------------------------------
// The eight functions.
// -----------------------------------------------------------------------------------------

uint8 ABPFPSWeapon::GetCurrentFireMode() const
{
	int32 Index = 0;
	GetIntVar(N_CurFireModeIndex, Index);

	uint8 Mode = FireModeSingle;
	if (GetByteArrayElement(N_AvailableFireModes, Index, Mode))
	{
		return Mode;
	}

	// The graph does a bare array Get with no bounds check; out of range there yields a
	// default-constructed enum. Same observable result, but said out loud.
	UE_LOG(LogBPFPSWeapon, Warning,
		TEXT("%s: CurFireModeIndex %d is outside AvailableFireModes (len %d) — returning Single."),
		*GetName(), Index, GetArrayLength(N_AvailableFireModes));
	return FireModeSingle;
}

void ABPFPSWeapon::NextFireMode()
{
	const int32 Length = GetArrayLength(N_AvailableFireModes);
	if (Length <= 0)
	{
		// The graph would divide by zero here. It never does in practice because every weapon
		// ships at least one mode, but a modulo by zero is a crash, not a wrong answer.
		UE_LOG(LogBPFPSWeapon, Warning,
			TEXT("%s: AvailableFireModes is empty — NextFireMode did nothing."), *GetName());
		return;
	}

	int32 Index = 0;
	GetIntVar(N_CurFireModeIndex, Index);
	SetIntVar(N_CurFireModeIndex, (Index + 1) % Length);
}

UAnimMontage* ABPFPSWeapon::GetFireAnimMontage(uint8 InSkeletalType, bool InAiming) const
{
	if (InSkeletalType == SkeletalType_UE4)
	{
		return GetObjectVar<UAnimMontage>(
			InAiming ? N_AimFireAnimMontage_UE4 : N_FireAnimMontage_UE4);
	}
	return GetObjectVar<UAnimMontage>(InAiming ? N_AimFireAnimMontage : N_FireAnimMontage);
}

UAnimMontage* ABPFPSWeapon::GetReloadBoltAnimMontage(uint8 InSkeletalType, bool InAiming) const
{
	if (InSkeletalType == SkeletalType_UE4)
	{
		return GetObjectVar<UAnimMontage>(
			InAiming ? N_AimReloadBoltAnimMontage_UE4 : N_ReloadBoltAnimMontage_UE4);
	}
	return GetObjectVar<UAnimMontage>(
		InAiming ? N_AimReloadBoltAnimMontage : N_ReloadBoltAnimMontage);
}

uint8 ABPFPSWeapon::GetScopeType() const
{
	AActor* ScopeActor = GetObjectVar<AActor>(N_Scope);
	if (!IsValid(ScopeActor))
	{
		return ScopeTypeNone;
	}

	// BP_FPST_BaseScope is a Blueprint class, so this is a call by name — a hard reference to
	// it from C++ is banned (law 3) and would not link anyway.
	UFunction* Fn = ScopeActor->FindFunction(FName(F_GetScopeType));
	if (!Fn)
	{
		UE_LOG(LogBPFPSWeapon, Warning,
			TEXT("%s: scope '%s' has no GetScopeType — returning enumerator 0."),
			*GetName(), *GetNameSafe(ScopeActor));
		return ScopeTypeNone;
	}

	// Parameter block from the UFunction's OWN layout; nothing about it is assumed.
	void* Buffer = FMemory::Malloc(FMath::Max<int32>(Fn->ParmsSize, 1));
	FMemory::Memzero(Buffer, FMath::Max<int32>(Fn->ParmsSize, 1));
	for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(Buffer);
	}

	ScopeActor->ProcessEvent(Fn, Buffer);

	uint8 Result = ScopeTypeNone;
	if (const FByteProperty* Ret = FindFProperty<FByteProperty>(Fn, TEXT("ReturnValue")))
	{
		Result = Ret->GetPropertyValue_InContainer(Buffer);
	}
	else if (const FEnumProperty* RetEnum = FindFProperty<FEnumProperty>(Fn, TEXT("ReturnValue")))
	{
		// Not also named 'Ret': the byte branch above already declares one in this scope, and
		// C4456 (shadowing) is an error in this project.
		Result = static_cast<uint8>(RetEnum->GetUnderlyingProperty()->GetSignedIntPropertyValue(
			RetEnum->ContainerPtrToValuePtr<void>(Buffer)));
	}

	for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Buffer);
	}
	FMemory::Free(Buffer);

	return Result;
}

void ABPFPSWeapon::PlayAnim(UAnimSequenceBase* NewAnimToPlay, bool bLooping)
{
	if (USkeletalMeshComponent* MeshComp = GetWeaponMesh())
	{
		MeshComp->PlayAnimation(NewAnimToPlay, bLooping);
	}
}

void ABPFPSWeapon::ResetAttachments()
{
	// Order matters and is the graph's: DefaultScope first, then Scope. Both IsValid-guarded.
	if (AActor* DefaultScopeActor = GetObjectVar<AActor>(N_DefaultScope))
	{
		if (IsValid(DefaultScopeActor))
		{
			DefaultScopeActor->Destroy();
		}
	}
	if (AActor* ScopeActor = GetObjectVar<AActor>(N_Scope))
	{
		if (IsValid(ScopeActor))
		{
			ScopeActor->Destroy();
		}
	}
}
