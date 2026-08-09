#pragma once
// Block, dodge, and hit-reaction structs/enums (FOSBlockInfo, FOSHitReacts, FOSHitReactInfo, etc.). Used by block/hit-react abilities and cues.

#include "Data/OSHitDamageContext.h"
#include "OSDefenseAndReactions.generated.h"

// --- Enums: Hit direction ---
UENUM(BlueprintType)
enum class EOSDirection : uint8
{
	NONE,
	FRONT,
	BACK,
	LEFT,
	RIGHT
};

// --- Enums: Hit react type ---
UENUM(BlueprintType)
enum class EOSHitReactType : uint8
{
	None,
	Light,
	Heavy,
	Knockdown,
	Launch,
	Death,
	GuardBreak
};

// --- Enums: Limb ---
UENUM(BlueprintType)
enum class EOSLimb : uint8
{
	HEAD,
	FIST_R,
	FIST_L,
	FOOT_R,
	FOOT_L
};

// --- FOSBlockInfo ---
USTRUCT(BlueprintType)
struct FOSBlockInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsBlocking = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsBroken = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EOSAttackType AttackType = EOSAttackType::Light;
	/** @deprecated Use AttackType. Kept for Chooser table compatibility (CT_OSBlocks). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(DisplayName="Attack Severity (deprecated)"))
	EOSAttackType AttackSeverity = EOSAttackType::Light;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EOSDirection HitDirection = EOSDirection::FRONT;
};

// --- FOSDodgeInfoStruct ---
USTRUCT(BlueprintType)
struct FOSDodgeInfoStruct
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector2D MoveDirection = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly)
	bool bHasMoved = false;
	UPROPERTY(BlueprintReadOnly)
	bool bHasTarget = false;
};

// --- FOSHitReacts ---
USTRUCT(BlueprintType)
struct FOSHitReacts
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EOSDirection HitDirection = EOSDirection::FRONT;
	UPROPERTY(BlueprintReadOnly)
	EOSDirection ReactDirection = EOSDirection::NONE;
	UPROPERTY(BlueprintReadOnly)
	EOSLimb Limb = EOSLimb::HEAD;
	UPROPERTY(BlueprintReadOnly)
	EOSHitReactType ReactType = EOSHitReactType::Light;
	UPROPERTY(BlueprintReadOnly)
	bool IsInAir = false;
};

// --- FOSHitReactInfo ---
USTRUCT(BlueprintType)
struct FOSHitReactInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EOSHitReactType ReactType = EOSHitReactType::Light;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EOSDirection Direction = EOSDirection::FRONT;
};
