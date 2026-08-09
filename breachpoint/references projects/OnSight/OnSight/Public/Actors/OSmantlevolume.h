// OSmantleVolume.h
// DEPRECATED: No longer referenced by the mantle ability (detection uses geometry directly).
// Kept because Blueprint children exist in levels. Remove class + BP assets together.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "OSmantleVolume.generated.h"

class UBoxComponent;

/**
 * DEPRECATED — Volume that previously defined areas where mantling is allowed.
 * The mantle ability no longer uses MantleVolumes (detection works on any geometry).
 * This class is kept only because Blueprint children are placed in levels.
 * Delete this class and its BP assets together when cleaning up levels.
 */
UCLASS(Blueprintable, ClassGroup = (Custom), meta=(DeprecatedNode, DeprecationMessage="MantleVolumes are no longer used by the mantle ability. Remove from levels."))
class ONSIGHT_API AOSmantlevolume : public AActor
{
    GENERATED_BODY()

public:
    AOSmantlevolume();

protected:
    virtual void BeginPlay() override;

public:
    // Root component - the volume itself
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* MantleVolume;

    // Whether this volume is currently active
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle")
    bool bIsActive = true;

    // Optional: Require specific gameplay tags for mantling in this volume
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle")
    FGameplayTagContainer RequiredTags;

    // Optional: Block mantling if character has these tags
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle")
    FGameplayTagContainer BlockedTags;

    // Visual color in editor (for organization)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|Visualization")
    FColor VolumeColor = FColor::Green;

    // Whether to show debug visualization in-game
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|Visualization")
    bool bShowDebugVisualization = false;

    /**
     * Check if a location is inside this volume
     * @param Location - World space location to check
     * @return true if the location is inside and volume is active
     */
    UFUNCTION(BlueprintCallable, Category = "Mantle")
    bool IsLocationInVolume(const FVector& Location) const;

    /**
     * Enable or disable this mantle volume
     * @param bNewActive - Whether the volume should be active
     */
    UFUNCTION(BlueprintCallable, Category = "Mantle")
    void SetVolumeActive(bool bNewActive);

    /**
     * Check if a character with specific tags can mantle in this volume
     * @param CharacterTags - The character's current gameplay tags
     * @return true if mantling is allowed
     */
    UFUNCTION(BlueprintCallable, Category = "Mantle")
    bool CanCharacterMantleInVolume(const FGameplayTagContainer& CharacterTags) const;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    void UpdateVisualization();
};