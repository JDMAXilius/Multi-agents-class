// OSmantleVolume.cpp
// Implementation of the MantleVolume actor

#include "Actors/OSmantlevolume.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "GameplayTagContainer.h"

AOSmantlevolume::AOSmantlevolume()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create the box component
    MantleVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("MantleVolume"));
    RootComponent = MantleVolume;

    // Set default box size (can be adjusted per instance)
    MantleVolume->SetBoxExtent(FVector(200.0f, 200.0f, 100.0f));

    // Make it non-collidable (it's just for detection)
    MantleVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MantleVolume->SetCollisionResponseToAllChannels(ECR_Ignore);

    // Visual settings for editor
    MantleVolume->ShapeColor = VolumeColor;
    MantleVolume->bHiddenInGame = !bShowDebugVisualization;

#if WITH_EDITORONLY_DATA
    bIsSpatiallyLoaded = false;
#endif
}

void AOSmantlevolume::BeginPlay()
{
    Super::BeginPlay();

    UpdateVisualization();
}

bool AOSmantlevolume::IsLocationInVolume(const FVector& Location) const
{
    if (!bIsActive || !MantleVolume)
    {
        return false;
    }

    // Check if the point is inside the box bounds
    const FVector LocalPoint = MantleVolume->GetComponentTransform().InverseTransformPosition(Location);
    const FVector BoxExtent = MantleVolume->GetScaledBoxExtent();

    return FMath::Abs(LocalPoint.X) <= BoxExtent.X &&
        FMath::Abs(LocalPoint.Y) <= BoxExtent.Y &&
        FMath::Abs(LocalPoint.Z) <= BoxExtent.Z;
}

void AOSmantlevolume::SetVolumeActive(bool bNewActive)
{
    if (bIsActive != bNewActive)
    {
        bIsActive = bNewActive;
        UpdateVisualization();
    }
}

bool AOSmantlevolume::CanCharacterMantleInVolume(const FGameplayTagContainer& CharacterTags) const
{
    if (!bIsActive)
    {
        return false;
    }

    // Check if character has required tags
    if (!RequiredTags.IsEmpty())
    {
        if (!CharacterTags.HasAll(RequiredTags))
        {
            return false;
        }
    }

    // Check if character has any blocked tags
    if (!BlockedTags.IsEmpty())
    {
        if (CharacterTags.HasAny(BlockedTags))
        {
            return false;
        }
    }

    return true;
}

void AOSmantlevolume::UpdateVisualization()
{
    if (!MantleVolume)
    {
        return;
    }

    // Update color based on active state
    FColor DisplayColor = bIsActive ? VolumeColor : FColor(128, 128, 128); // Gray when inactive
    MantleVolume->ShapeColor = DisplayColor;

    // Update visibility
    MantleVolume->bHiddenInGame = !bShowDebugVisualization;

    // Draw debug box if enabled
    if (bShowDebugVisualization && GetWorld())
    {
        const FVector Center = MantleVolume->GetComponentLocation();
        const FVector Extent = MantleVolume->GetScaledBoxExtent();
        const FQuat Rotation = MantleVolume->GetComponentQuat();

        DrawDebugBox(
            GetWorld(),
            Center,
            Extent,
            Rotation,
            DisplayColor,
            true, // Persistent
            -1.0f,
            0,
            2.0f
        );
    }
}

#if WITH_EDITOR
void AOSmantlevolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();

    // Update visualization when relevant properties change
    if (PropertyName == GET_MEMBER_NAME_CHECKED(AOSmantlevolume, bIsActive) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AOSmantlevolume, VolumeColor) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(AOSmantlevolume, bShowDebugVisualization))
    {
        FlushPersistentDebugLines(GetWorld());
        UpdateVisualization();
    }
}
#endif