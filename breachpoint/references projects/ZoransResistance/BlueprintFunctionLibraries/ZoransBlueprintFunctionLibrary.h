// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EnhancedPlayerInput.h"
#include "EnvironmentQuery/EnvQueryInstanceBlueprintWrapper.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "ZoransResistance/Game/Data/StaticGameData.h"
#include "ZoransResistance/Player/Data/LocalPlayerData.h"
#include "ZoransBlueprintFunctionLibrary.generated.h"

class UWidget;
class ISortableWidgetInterface;
class USortableWidgetInterface;
class UPlayFabJsonObject;
class UDA_ZoranAttachment_Skeletal;
class UDA_ZoranAttachment_Static;

UCLASS()
class ZORANSRESISTANCE_API UZoransBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintPure,category = "Zorans Blueprint Function Library")
	static FString ExtractNumericValues(const FString& InputString);

	UFUNCTION(BlueprintCallable, Category = "Zorans Blueprint Function Library")
	static EZoranDirection4 GetActorDirection4(const AActor* Actor, const FVector& Direction);

	UFUNCTION(BlueprintCallable, Category = "Zorans Blueprint Function Library")
	static EZoranDirection6 GetActorDirection6(const AActor* Actor, const FVector& Direction);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Zorans Blueprint Function Library")
	static bool IsWithEditor();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Zorans Blueprint Function Library")
	static bool IsDevelopmentBuild();
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	static FString GetProjectVersion();
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Copy Text to Clipboard"), Category = "Zorans Blueprint Function Library")
	static void CopyText(FString Text);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Paste Text from Clipboard"), Category = "Zorans Blueprint Function Library")
	static FString PasteText();

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Zorans Blueprint Function Library")
	static UEnvQueryInstanceBlueprintWrapper* GenerateEQS(const UObject* WorldContextObject, UObject* QueryContext, const UEnvQuery* QueryTemplate, const TMap<FName, float>& QueryParams, const EEnvQueryRunMode::Type QueryResultType = EEnvQueryRunMode::Type::SingleResult);

	UFUNCTION(BlueprintCallable, Category = "Zorans Blueprint Function Library")
	static void CalculateEchelonLevelByExperience(const float inExperience, int32 &Level, float &Progress, float &LevelStart, float &LevelEnd);

	UFUNCTION(BlueprintCallable, Category = "Zorans Blueprint Function Library")
	static void CalculateEliteAccessPassLevelByExperience(const float inExperience, int32 &Level, float &Progress, float &LevelStart, float &LevelEnd);

	UFUNCTION(BlueprintCallable, Category = "Zorans Blueprint Function Library")
	static FString LoadoutToJsonString(const FPlayerLoadout& Loadout);
	UFUNCTION(BlueprintCallable, Category = "Zorans Blueprint Function Library")
	static FPlayerLoadout JsonStringToLoadout(const FString& JsonString);

	UFUNCTION(BlueprintCallable, Category = "Zorans Blueprint Function Library")
	static TArray<FName> AttachCosmeticComponent(USkeletalMeshComponent* InParentMeshComponent, UDA_ZoranAttachment_Static* InStaticAttachment = nullptr, UDA_ZoranAttachment_Skeletal* InSkeletalAttachment = nullptr, const FName InAttachmentName = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Zorans Blueprint Function Library")
	static TArray<FName> RemoveCosmeticAttachment(const USkeletalMeshComponent* InParentMeshComponent, EZoranBodySection BodySection = EZoranBodySection::None);

	UFUNCTION(BlueprintCallable, Category = "Zorans Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
	static void GeneratePlayerSaveData(UObject* WorldContextObject,
		const FZoransPlayerData& LoadedData, const FZoransPlayerData& ModifiedData, bool bForceSave,
		UPlayFabJsonObject*& SaveArgs, bool& bSaveRequired, bool& bUpdatePresence);

	UFUNCTION(BlueprintCallable, Category = "Zorans Blueprint Function Library")
	static void GetDedicatedServerLogin(FString& Username, FString& Password);

	UFUNCTION(BlueprintCallable, Category = "Zorans Blueprint Function Library")
	static void SortWidgets(UPARAM(ref) TArray<UWidget*>& Array );

	UFUNCTION(BlueprintPure,Category = "Zorans Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
	static int32 GetServerIP(UObject* WorldContextObject, bool bUseFInternetAddr);

	static inline float EchelonLevelExperienceRequirement = 4400.f; //OG: 528000
	static inline float EliteAccessPassExperienceRequirement = 44000.f;

public:
	static UWorld* GetWorldFromPlayerInput(const UEnhancedPlayerInput* InPlayerInput);
};