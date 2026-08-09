// Copyright Zollpa LLC

// !!! This code is auto-generated. Do not modify. !!!

#pragma once

#include "PlayFabJsonObject.h"
#include "CoreMinimal.h"
#include "ZoransAzurePlayFabData.generated.h"

USTRUCT(BlueprintType)
struct FGenericAzureResponse
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Error;
};

USTRUCT(BlueprintType)
struct FTagData
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool Developer = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool Cool = false;
};

USTRUCT(BlueprintType)
struct FBattlePassData
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int64 PassEndUnix = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int64 Experience = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Id = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int64 PassPurchaseUnix = 0;
};

UENUM(BlueprintType)
enum class EChallengeType : uint8
{
	Unknown UMETA(DisplayName = "Unknown"),
	Kills UMETA(DisplayName = "Kills"),
	Headshots UMETA(DisplayName = "Headshots"),
	Mitigation UMETA(DisplayName = "Mitigation"),
	Objective UMETA(DisplayName = "Objective"),
	WinGame UMETA(DisplayName = "WinGame"),
	PlayGame UMETA(DisplayName = "PlayGame"),
	CompleteChallenges UMETA(DisplayName = "CompleteChallenges"),
};

UENUM(BlueprintType)
enum class EChallengeAggregateType : uint8
{
	Unknown UMETA(DisplayName = "Unknown"),
	Consecutive UMETA(DisplayName = "Consecutive"),
	Max UMETA(DisplayName = "Max"),
	Sum UMETA(DisplayName = "Sum"),
};

UENUM(BlueprintType)
enum class EChallengeConditionType : uint8
{
	Unknown UMETA(DisplayName = "Unknown"),
	Map UMETA(DisplayName = "Map"),
	Weapon UMETA(DisplayName = "Weapon"),
	Ability UMETA(DisplayName = "Ability"),
	Gamemode UMETA(DisplayName = "Gamemode"),
	LessThanDeaths UMETA(DisplayName = "LessThanDeaths"),
};

USTRUCT(BlueprintType)
struct FChallengeCondition
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EChallengeConditionType Type = EChallengeConditionType::Unknown;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Value;
};

USTRUCT(BlueprintType)
struct FChallengeData
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EChallengeType Type = EChallengeType::Unknown;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Required = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EChallengeAggregateType AggregateFunc = EChallengeAggregateType::Unknown;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool Mission = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Experience = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FChallengeCondition> Conditions = {};
};

UENUM(BlueprintType)
enum class EChallengeStatus : uint8
{
	InProgress UMETA(DisplayName = "InProgress"),
	Completed UMETA(DisplayName = "Completed"),
	Collected UMETA(DisplayName = "Collected"),
};

USTRUCT(BlueprintType)
struct FChallengeInstanceData
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FChallengeData Challenge;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Progress = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString InstanceId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EChallengeStatus Status = EChallengeStatus::InProgress;
};

USTRUCT(BlueprintType)
struct FChallengeSetData
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int64 DailyEndTimeUnix = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ChallengesCompleted = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FChallengeInstanceData> DailyChallenges = {};
};

USTRUCT(BlueprintType)
struct FMiscData
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString SquadId;
};

USTRUCT(BlueprintType)
struct FSteamBeginPurchaseRequest
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString SessionTicket;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Language = "en";

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ItemSku;
};

USTRUCT(BlueprintType)
struct FSteamEndPurchaseRequest
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString OrderId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool Success = false;
};

USTRUCT(BlueprintType)
struct FGetChallengesResponse
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FChallengeSetData ChallengeSet;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Error;
};

USTRUCT(BlueprintType)
struct FClaimChallengeRewardRequest
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ChallengeInstanceId;
};

USTRUCT(BlueprintType)
struct FPurchaseItemRequest
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ItemId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Cost = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool PurchaseWithPremium = false;
};

USTRUCT(BlueprintType)
struct FCreateSquadRequest
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString SquadName;
};

USTRUCT(BlueprintType)
struct FCreateSquadResponse
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString SquadId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString SquadName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Error;
};

USTRUCT(BlueprintType)
struct FGetSquadResponse
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString SquadName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString SquadId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString LeaderId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FString> Members = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Error;
};

USTRUCT(BlueprintType)
struct FInviteToSquadRequest
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString InviteePlayFabId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString SquadId;
};

USTRUCT(BlueprintType)
struct FAcceptSquadInviteRequest
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString SquadId;
};

USTRUCT(BlueprintType)
struct FGetSquadInvitesResponse
{
    GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FString> Squads = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Error;
};
UCLASS()
class ZORANSRESISTANCE_API UZoransAzurePlayFabDataJsonBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    
    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static FGenericAzureResponse JsonToGenericAzureResponse(UPlayFabJsonObject* JsonObject)
    {
        FGenericAzureResponse DataStruct;
        DataStruct.Error = JsonObject->GetStringField("Error");
        return DataStruct;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static FTagData JsonToTagData(UPlayFabJsonObject* JsonObject)
    {
        FTagData DataStruct;
        DataStruct.Developer = JsonObject->GetBoolField("Developer");
        DataStruct.Cool = JsonObject->GetBoolField("Cool");
        return DataStruct;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static FBattlePassData JsonToBattlePassData(UPlayFabJsonObject* JsonObject)
    {
        FBattlePassData DataStruct;
        DataStruct.PassEndUnix = JsonObject->GetNumberField("PassEndUnix");
        DataStruct.Experience = JsonObject->GetNumberField("Experience");
        DataStruct.Id = JsonObject->GetNumberField("Id");
        DataStruct.PassPurchaseUnix = JsonObject->GetNumberField("PassPurchaseUnix");
        return DataStruct;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static FChallengeCondition JsonToChallengeCondition(UPlayFabJsonObject* JsonObject)
    {
        FChallengeCondition DataStruct;
        DataStruct.Type = static_cast<EChallengeConditionType>(StaticEnum<EChallengeConditionType>()->GetIndexByNameString(JsonObject->GetStringField("Type")));
        DataStruct.Value = JsonObject->GetStringField("Value");
        return DataStruct;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static FChallengeData JsonToChallengeData(UPlayFabJsonObject* JsonObject)
    {
        FChallengeData DataStruct;
        DataStruct.Type = static_cast<EChallengeType>(StaticEnum<EChallengeType>()->GetIndexByNameString(JsonObject->GetStringField("Type")));
        DataStruct.Required = JsonObject->GetNumberField("Required");
        DataStruct.AggregateFunc = static_cast<EChallengeAggregateType>(StaticEnum<EChallengeAggregateType>()->GetIndexByNameString(JsonObject->GetStringField("AggregateFunc")));
        DataStruct.Mission = JsonObject->GetBoolField("Mission");
        DataStruct.Experience = JsonObject->GetNumberField("Experience");
        for (UPlayFabJsonObject* JsonObjectInstance : JsonObject->GetObjectArrayField("Conditions"))
              DataStruct.Conditions.Add(JsonToChallengeCondition(JsonObjectInstance));
        return DataStruct;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static FChallengeInstanceData JsonToChallengeInstanceData(UPlayFabJsonObject* JsonObject)
    {
        FChallengeInstanceData DataStruct;
        DataStruct.Challenge = JsonToChallengeData(JsonObject->GetObjectField("Challenge"));
        DataStruct.Progress = JsonObject->GetNumberField("Progress");
        DataStruct.InstanceId = JsonObject->GetStringField("InstanceId");
        DataStruct.Status = static_cast<EChallengeStatus>(StaticEnum<EChallengeStatus>()->GetIndexByNameString(JsonObject->GetStringField("Status")));
        return DataStruct;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static FChallengeSetData JsonToChallengeSetData(UPlayFabJsonObject* JsonObject)
    {
        FChallengeSetData DataStruct;
        DataStruct.DailyEndTimeUnix = JsonObject->GetNumberField("DailyEndTimeUnix");
        DataStruct.ChallengesCompleted = JsonObject->GetNumberField("ChallengesCompleted");
        for (UPlayFabJsonObject* JsonObjectInstance : JsonObject->GetObjectArrayField("DailyChallenges"))
              DataStruct.DailyChallenges.Add(JsonToChallengeInstanceData(JsonObjectInstance));
        return DataStruct;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static UPlayFabJsonObject* MiscDataToJson(UObject* WorldContextObject, const FMiscData& DataStruct)
    {
        UPlayFabJsonObject* JsonObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
        JsonObject->SetStringField("SquadId", DataStruct.SquadId);
        return JsonObject;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static FMiscData JsonToMiscData(UPlayFabJsonObject* JsonObject)
    {
        FMiscData DataStruct;
        DataStruct.SquadId = JsonObject->GetStringField("SquadId");
        return DataStruct;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static UPlayFabJsonObject* SteamBeginPurchaseRequestToJson(UObject* WorldContextObject, const FSteamBeginPurchaseRequest& DataStruct)
    {
        UPlayFabJsonObject* JsonObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
        JsonObject->SetStringField("SessionTicket", DataStruct.SessionTicket);
        JsonObject->SetStringField("Language", DataStruct.Language);
        JsonObject->SetStringField("ItemSku", DataStruct.ItemSku);
        return JsonObject;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static UPlayFabJsonObject* SteamEndPurchaseRequestToJson(UObject* WorldContextObject, const FSteamEndPurchaseRequest& DataStruct)
    {
        UPlayFabJsonObject* JsonObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
        JsonObject->SetStringField("OrderId", DataStruct.OrderId);
        JsonObject->SetBoolField("Success", DataStruct.Success);
        return JsonObject;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static FGetChallengesResponse JsonToGetChallengesResponse(UPlayFabJsonObject* JsonObject)
    {
        FGetChallengesResponse DataStruct;
        JsonObject = JsonObject->GetObjectField("data")->GetObjectField("FunctionResult");
        DataStruct.ChallengeSet = JsonToChallengeSetData(JsonObject->GetObjectField("ChallengeSet"));
        DataStruct.Error = JsonObject->GetStringField("Error");
        return DataStruct;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static UPlayFabJsonObject* ClaimChallengeRewardRequestToJson(UObject* WorldContextObject, const FClaimChallengeRewardRequest& DataStruct)
    {
        UPlayFabJsonObject* JsonObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
        JsonObject->SetStringField("ChallengeInstanceId", DataStruct.ChallengeInstanceId);
        return JsonObject;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static UPlayFabJsonObject* PurchaseItemRequestToJson(UObject* WorldContextObject, const FPurchaseItemRequest& DataStruct)
    {
        UPlayFabJsonObject* JsonObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
        JsonObject->SetStringField("ItemId", DataStruct.ItemId);
        JsonObject->SetNumberField("Cost", DataStruct.Cost);
        JsonObject->SetBoolField("PurchaseWithPremium", DataStruct.PurchaseWithPremium);
        return JsonObject;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static UPlayFabJsonObject* CreateSquadRequestToJson(UObject* WorldContextObject, const FCreateSquadRequest& DataStruct)
    {
        UPlayFabJsonObject* JsonObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
        JsonObject->SetStringField("SquadName", DataStruct.SquadName);
        return JsonObject;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static FCreateSquadResponse JsonToCreateSquadResponse(UPlayFabJsonObject* JsonObject)
    {
        FCreateSquadResponse DataStruct;
        JsonObject = JsonObject->GetObjectField("data")->GetObjectField("FunctionResult");
        DataStruct.SquadId = JsonObject->GetStringField("SquadId");
        DataStruct.SquadName = JsonObject->GetStringField("SquadName");
        DataStruct.Error = JsonObject->GetStringField("Error");
        return DataStruct;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static FGetSquadResponse JsonToGetSquadResponse(UPlayFabJsonObject* JsonObject)
    {
        FGetSquadResponse DataStruct;
        JsonObject = JsonObject->GetObjectField("data")->GetObjectField("FunctionResult");
        DataStruct.SquadName = JsonObject->GetStringField("SquadName");
        DataStruct.SquadId = JsonObject->GetStringField("SquadId");
        DataStruct.LeaderId = JsonObject->GetStringField("LeaderId");
        DataStruct.Members = JsonObject->GetStringArrayField("Members");
        DataStruct.Error = JsonObject->GetStringField("Error");
        return DataStruct;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static UPlayFabJsonObject* InviteToSquadRequestToJson(UObject* WorldContextObject, const FInviteToSquadRequest& DataStruct)
    {
        UPlayFabJsonObject* JsonObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
        JsonObject->SetStringField("InviteePlayFabId", DataStruct.InviteePlayFabId);
        JsonObject->SetStringField("SquadId", DataStruct.SquadId);
        return JsonObject;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static UPlayFabJsonObject* AcceptSquadInviteRequestToJson(UObject* WorldContextObject, const FAcceptSquadInviteRequest& DataStruct)
    {
        UPlayFabJsonObject* JsonObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
        JsonObject->SetStringField("SquadId", DataStruct.SquadId);
        return JsonObject;
    }

    UFUNCTION(BlueprintCallable, Category = "Zorans Azure PlayFab Data Json Blueprint Function Library", meta = (WorldContext = "WorldContextObject"))
    static FGetSquadInvitesResponse JsonToGetSquadInvitesResponse(UPlayFabJsonObject* JsonObject)
    {
        FGetSquadInvitesResponse DataStruct;
        JsonObject = JsonObject->GetObjectField("data")->GetObjectField("FunctionResult");
        DataStruct.Squads = JsonObject->GetStringArrayField("Squads");
        DataStruct.Error = JsonObject->GetStringField("Error");
        return DataStruct;
    }

};