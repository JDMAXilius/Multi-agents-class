#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"

#include "BREnvQueryContexts.generated.h"

class AActor;
class APawn;
class UWorld;

namespace BRBotArenaTags
{
	BREACHPOINT_API extern const FName Landmark;
	BREACHPOINT_API extern const FName RocketPad;
	BREACHPOINT_API extern const FName Cover;
	BREACHPOINT_API extern const FName Perch;
}

namespace BRBotArena
{
	BREACHPOINT_API void GatherTagged(const UWorld* World, FName Tag, TArray<AActor*>& OutActors);

	BREACHPOINT_API bool GetRocketPadLocation(const UWorld* World, FVector& OutLocation);

	BREACHPOINT_API float GetCoverQualityAt(const APawn* Pawn);
}

UCLASS()
class BREACHPOINT_API UBREnvQueryContext_Threat : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

UCLASS()
class BREACHPOINT_API UBREnvQueryContext_RocketPad : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

UCLASS()
class BREACHPOINT_API UBREnvQueryContext_CoverPoints : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

UCLASS()
class BREACHPOINT_API UBREnvQueryContext_GrapplePerches : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

UCLASS()
class BREACHPOINT_API UBREnvQueryContext_Teammates : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

UCLASS()
class BREACHPOINT_API UBREnvQueryContext_StepAnchor : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
