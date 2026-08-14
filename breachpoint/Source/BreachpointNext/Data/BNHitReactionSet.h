#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BNHitReactionSet.generated.h"

class UAnimMontage;

UENUM()
enum class EBNHitDirection : uint8
{
	Front,
	Back,
	Left,
	Right
};

UENUM()
enum class EBNHitSeverity : uint8
{
	Light,
	Medium,
	Heavy
};

/** One cell of the reaction table: everything that can play for this direction at this severity.
 *  A LIST because the template ships four Front-Light variants and one of most others — variety
 *  where hits are common (you usually face your attacker), no invention where they are not. */
USTRUCT()
struct FBNHitReactionRow
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "BN|HitReaction")
	EBNHitDirection Direction = EBNHitDirection::Front;

	UPROPERTY(EditDefaultsOnly, Category = "BN|HitReaction")
	EBNHitSeverity Severity = EBNHitSeverity::Light;

	UPROPERTY(EditDefaultsOnly, Category = "BN|HitReaction")
	TArray<TSoftObjectPtr<UAnimMontage>> Montages;
};

/**
 * A character's whole reaction personality in one asset — THE grouping file the founder asked for.
 * Rows are (direction, severity) cells; the thresholds turn a damage amount into a severity.
 * Swapping how a character reacts is swapping this asset; a new animation is a new row entry;
 * neither touches C++. Set in the editor by the terminal (ASSET-RULES §7), soft everywhere.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNHitReactionSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The cell's montage list — EXACT severity first, then stepping DOWN (Heavy→Medium→Light),
	 *  because the template ships no Back/Left/Right Heavy and a harder hit must never produce NO
	 *  reaction when a softer animation exists. Returns null only when the direction has nothing. */
	const FBNHitReactionRow* FindRow(EBNHitDirection Direction, EBNHitSeverity Severity) const;

	/** Damage → severity, against the thresholds below. */
	EBNHitSeverity SeverityForDamage(float Damage) const;

	UPROPERTY(EditDefaultsOnly, Category = "BN|HitReaction")
	TArray<FBNHitReactionRow> Rows;

	/** Damage at or below this is Light; above MediumMaxDamage is Heavy. Data, not code — a rifle
	 *  body shot (20) lands Light, a headshot (40) or melee (40) Medium, a near grenade Heavy. */
	UPROPERTY(EditDefaultsOnly, Category = "BN|HitReaction")
	float LightMaxDamage = 25.f;

	UPROPERTY(EditDefaultsOnly, Category = "BN|HitReaction")
	float MediumMaxDamage = 50.f;
};
