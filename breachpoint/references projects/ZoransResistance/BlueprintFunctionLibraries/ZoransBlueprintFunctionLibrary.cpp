// Copyright Zollpa LLC


// ReSharper disable CppUseStructuredBinding
#include "ZoransBlueprintFunctionLibrary.h"
#include "JsonObjectConverter.h"
#include "PlayFabJsonObject.h"
#include "Components/Widget.h"
#include "ZoransResistance/Game/Data/StaticGameData.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "ZoransResistance/Economy/DA_ZoranAttachment.h"
#include "Internationalization/Regex.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "ZoransResistance/Widgets/SortableWidgetInterface.h"

#if !WITH_SERVER_CODE

#include "Windows/WindowsPlatformApplicationMisc.h"

#endif

FString UZoransBlueprintFunctionLibrary::ExtractNumericValues(const FString& InputString)
{
	FString Result = "";
	const FRegexPattern Pattern(TEXT("(\\d+)"));
	FRegexMatcher Matcher(Pattern, InputString);

	while (Matcher.FindNext())
	{
		Result += Matcher.GetCaptureGroup(0);
	}

	return Result;
}


EZoranDirection4 UZoransBlueprintFunctionLibrary::GetActorDirection4(const AActor* Actor, const FVector& Direction)
{
	const FVector2D Direction2D = FVector2D(Direction.X, Direction.Y).GetSafeNormal();
	
	if (!IsValid(Actor) || Direction2D.IsNearlyZero(0.01f))
	{
		return EZoranDirection4::Forward;
	}
	
	const FVector2D ActorForward = FVector2D(Actor->GetActorForwardVector().X, Actor->GetActorForwardVector().Y).GetSafeNormal();
	const float ForwardDot = FVector2D::DotProduct(ActorForward, Direction2D);

	if (FMath::Abs(ForwardDot) >= 0.5f)
	{
		if (ForwardDot >= 0.5f)
		{
			return EZoranDirection4::Forward;
		}
		return EZoranDirection4::Backward;
	}

	const FVector2D ActorRight = FVector2D(Actor->GetActorRightVector().X, Actor->GetActorRightVector().Y).GetSafeNormal();
	const float RightDot = FVector2D::DotProduct(ActorRight, Direction2D);
	
	if (FMath::Abs(RightDot) >= 0.5f)
	{
		if (RightDot >= 0.5f)
		{
			return EZoranDirection4::Right;
		}
		return EZoranDirection4::Left;
	}
	
	return EZoranDirection4::Forward;
}

EZoranDirection6 UZoransBlueprintFunctionLibrary::GetActorDirection6(const AActor* Actor, const FVector& Direction)
{
	if (!IsValid(Actor) || Direction.IsNearlyZero(0.01f))
	{
		return EZoranDirection6::Forward;
	}

	const FVector ActorUp = Actor->GetActorUpVector();
	const float UpDot = FVector::DotProduct(ActorUp, Direction);

	if (FMath::Abs(UpDot) >= 0.5f)
	{
		if (UpDot >= 0.5f)
		{
			return EZoranDirection6::Up;
		}
		return EZoranDirection6::Down;
	}
	
	switch (GetActorDirection4(Actor, Direction))
	{
	case EZoranDirection4::Forward:
		return EZoranDirection6::Forward;
	case EZoranDirection4::Left:
		return EZoranDirection6::Left;
	case EZoranDirection4::Right:
		return EZoranDirection6::Right;
	case EZoranDirection4::Backward:
		return EZoranDirection6::Backward;
	}

	return EZoranDirection6::Forward;
}

bool UZoransBlueprintFunctionLibrary::IsWithEditor()
{
	return GIsPlayInEditorWorld;
}

bool UZoransBlueprintFunctionLibrary::IsDevelopmentBuild()
{
	bool bDev = false;
	
#if UE_BUILD_DEVELOPMENT
	bDev = true;
#endif
	
	return bDev;
}

FString UZoransBlueprintFunctionLibrary::GetProjectVersion()
{
	FString AppVersion = "";
	
	GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), AppVersion, GGameIni);

	return AppVersion;
}

void UZoransBlueprintFunctionLibrary::CopyText(FString Text)
{
	
#if !WITH_SERVER_CODE
	
	FWindowsPlatformApplicationMisc::ClipboardCopy(*Text);

#endif
	
}

FString UZoransBlueprintFunctionLibrary::PasteText()
{

#if !WITH_SERVER_CODE	
	
	FString LocalString;
	FWindowsPlatformApplicationMisc::ClipboardPaste(LocalString);
	return LocalString;

#endif

	return "";
}

UEnvQueryInstanceBlueprintWrapper* UZoransBlueprintFunctionLibrary::GenerateEQS(const UObject* WorldContextObject, UObject* QueryContext, const UEnvQuery* QueryTemplate, const TMap<FName, float>& QueryParams, const EEnvQueryRunMode::Type QueryResultType)
{
	UEnvQueryInstanceBlueprintWrapper* QueryInstanceWrapper = nullptr;

	if (IsValid(WorldContextObject) && IsValid(QueryContext) && IsValid(QueryTemplate))
	{
		if (UEnvQueryManager* EQS = UEnvQueryManager::GetCurrent(WorldContextObject))
		{
			QueryInstanceWrapper = NewObject<UEnvQueryInstanceBlueprintWrapper>(EQS, UEnvQueryInstanceBlueprintWrapper::StaticClass());
			check(QueryInstanceWrapper);

			FEnvQueryRequest QueryRequest(QueryTemplate, QueryContext);

			if (!QueryParams.IsEmpty())
			{
				for (const TTuple<FName, float>& QueryParam : QueryParams)
				{
					QueryRequest.SetFloatParam(QueryParam.Key, QueryParam.Value);
				}
			}
			
			QueryInstanceWrapper->SetInstigator(WorldContextObject);
			QueryInstanceWrapper->RunQuery(QueryResultType, QueryRequest);
		}
	}

	return QueryInstanceWrapper;
}

void UZoransBlueprintFunctionLibrary::CalculateEchelonLevelByExperience(const float inExperience, int32& Level,	float& Progress, float& LevelStart, float& LevelEnd)
{
	Level = 0;
	Progress = 0.f;
	LevelStart = 0.f;
	LevelEnd = EchelonLevelExperienceRequirement;

	if (inExperience > 0.f)
	{
		Level = FMath::TruncToInt32(UKismetMathLibrary::SafeDivide(inExperience, EchelonLevelExperienceRequirement));
		const float ConsumedExperience = Level * EchelonLevelExperienceRequirement;
		const float RemainingExperience = inExperience - ConsumedExperience;
		Progress = UKismetMathLibrary::SafeDivide(RemainingExperience, EchelonLevelExperienceRequirement);
		if (Level > 0)
		{
			LevelStart = ConsumedExperience;
			LevelEnd = ConsumedExperience + EchelonLevelExperienceRequirement;
		}
	}
}

void UZoransBlueprintFunctionLibrary::CalculateEliteAccessPassLevelByExperience(const float inExperience, int32& Level,	float& Progress, float& LevelStart, float& LevelEnd)
{
	Level = 0;
	Progress = 0.f;
	LevelStart = 0.f;
	LevelEnd = EliteAccessPassExperienceRequirement;

	if (inExperience > 0.f)
	{
		Level = FMath::TruncToInt32(UKismetMathLibrary::SafeDivide(inExperience, EliteAccessPassExperienceRequirement));
		const float ConsumedExperience = Level * EliteAccessPassExperienceRequirement;
		const float RemainingExperience = inExperience - ConsumedExperience;
		Progress = UKismetMathLibrary::SafeDivide(RemainingExperience, EliteAccessPassExperienceRequirement);
		if (Level > 0)
		{
			LevelStart = ConsumedExperience;
			LevelEnd = ConsumedExperience + EliteAccessPassExperienceRequirement;
		}
	}
}

UWorld* UZoransBlueprintFunctionLibrary::GetWorldFromPlayerInput(const UEnhancedPlayerInput* InPlayerInput)
{
	UWorld* PlayerWorld = nullptr;

	if(InPlayerInput)
	{
		if(const UObject* InputOuter = InPlayerInput->GetOuter())
		{
			PlayerWorld = InputOuter->GetWorld();
		}
	}

	return PlayerWorld;
}

FString UZoransBlueprintFunctionLibrary::LoadoutToJsonString(const FPlayerLoadout& Loadout)
{
	FString Result;
	FJsonObjectConverter::UStructToJsonObjectString(Loadout, Result, 0, CPF_RepSkip, 0, nullptr, false);
	return Result;
}

FPlayerLoadout UZoransBlueprintFunctionLibrary::JsonStringToLoadout(const FString& JsonString)
{
	FPlayerLoadout Loadout;
	FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &Loadout);
	return Loadout;
}

TArray<FName> UZoransBlueprintFunctionLibrary::AttachCosmeticComponent(USkeletalMeshComponent* InParentMeshComponent, UDA_ZoranAttachment_Static* InStaticAttachment, UDA_ZoranAttachment_Skeletal* InSkeletalAttachment, const FName InAttachmentName)
{
	TArray<FName> RemovedComponents;
	if (IsValid(InParentMeshComponent) && InParentMeshComponent->GetOuter())
	{
		if (AActor* MeshOwner = Cast<AActor>(InParentMeshComponent->GetOuter()))
		{
			const FAttachmentTransformRules AttachRules = FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true);

			if (InStaticAttachment != nullptr)
			{
				const EZoranBodySection BodySection = InStaticAttachment->BodySection;
				if (BodySectionTagMap.Contains(BodySection))
				{
					RemovedComponents = RemoveCosmeticAttachment(InParentMeshComponent, BodySection);
					const FName BodySectionTag = BodySectionTagMap[BodySection];
					if (InStaticAttachment->PrimaryMesh.IsValid())
					{
						UActorComponent* NewComponent = MeshOwner->AddComponentByClass(UStaticMeshComponent::StaticClass(), true, MeshOwner->GetActorTransform(), false);
						if (UStaticMeshComponent* NewStaticMeshComponent = Cast<UStaticMeshComponent>(NewComponent))
						{
							NewStaticMeshComponent->AttachToComponent(InParentMeshComponent, AttachRules, InStaticAttachment->PrimaryAttachmentSocket);
							NewStaticMeshComponent->ComponentTags.AddUnique(BodySectionTag);
							NewStaticMeshComponent->RegisterComponent();
							NewStaticMeshComponent->SetStaticMesh(InStaticAttachment->PrimaryMesh.Get());
							if (InStaticAttachment->PrimaryMaterial.IsValid())
							{
								NewStaticMeshComponent->SetMaterial(0, InSkeletalAttachment->PrimaryMaterial.Get());
							}
							if (!InAttachmentName.IsNone())
							{
								const FString AttachmentNameString = FString("Attachment." + InAttachmentName.ToString());
								const FName AttachmentTag = FName(AttachmentNameString);
								NewStaticMeshComponent->ComponentTags.AddUnique(AttachmentTag);
							}

							NewStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
							NewStaticMeshComponent->SetCollisionObjectType(ECC_Destructible);
							NewStaticMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
						}
					}

					if (InStaticAttachment->SecondaryMesh.IsValid())
					{
						UActorComponent* NewComponent = MeshOwner->AddComponentByClass(UStaticMeshComponent::StaticClass(), true, MeshOwner->GetActorTransform(), false);
						if (UStaticMeshComponent* NewStaticMeshComponent = Cast<UStaticMeshComponent>(NewComponent))
						{
							NewStaticMeshComponent->AttachToComponent(InParentMeshComponent, AttachRules, InStaticAttachment->SecondaryAttachmentSocket);
							NewStaticMeshComponent->ComponentTags.AddUnique(BodySectionTag);
							NewStaticMeshComponent->RegisterComponent();
							NewStaticMeshComponent->SetStaticMesh(InStaticAttachment->SecondaryMesh.Get());
							if (InStaticAttachment->SecondaryMaterial.IsValid())
							{
								NewStaticMeshComponent->SetMaterial(0, InSkeletalAttachment->SecondaryMaterial.Get());
							}
							if (!InAttachmentName.IsNone())
							{
								const FString AttachmentNameString = FString("Attachment." + InAttachmentName.ToString());
								const FName AttachmentTag = FName(AttachmentNameString);
								NewStaticMeshComponent->ComponentTags.AddUnique(AttachmentTag);
							}

							NewStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
							NewStaticMeshComponent->SetCollisionObjectType(ECC_Destructible);
							NewStaticMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
						}
					}
					else if (InStaticAttachment->bMirrorPrimaryAsSecondary && InStaticAttachment->PrimaryMesh.IsValid())
					{
						UActorComponent* NewComponent = MeshOwner->AddComponentByClass(UStaticMeshComponent::StaticClass(), true, MeshOwner->GetActorTransform(), false);
						if (UStaticMeshComponent* NewStaticMeshComponent = Cast<UStaticMeshComponent>(NewComponent))
						{
							NewStaticMeshComponent->AttachToComponent(InParentMeshComponent, AttachRules, InStaticAttachment->SecondaryAttachmentSocket);
							NewStaticMeshComponent->ComponentTags.AddUnique(BodySectionTag);
							NewStaticMeshComponent->RegisterComponent();
							NewStaticMeshComponent->SetStaticMesh(InStaticAttachment->PrimaryMesh.Get());
							if (InStaticAttachment->PrimaryMaterial.IsValid())
							{
								NewStaticMeshComponent->SetMaterial(0, InSkeletalAttachment->PrimaryMaterial.Get());
							}
							if (!InAttachmentName.IsNone())
							{
								const FString AttachmentNameString = FString("Attachment." + InAttachmentName.ToString());
								const FName AttachmentTag = FName(AttachmentNameString);
								NewStaticMeshComponent->ComponentTags.AddUnique(AttachmentTag);
							}
							NewStaticMeshComponent->SetRelativeScale3D(FVector(-1.f, 1.f, 1.f));

							NewStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
							NewStaticMeshComponent->SetCollisionObjectType(ECC_Destructible);
							NewStaticMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
						}
					}
				}
			}

			if (InSkeletalAttachment != nullptr)
			{
				const EZoranBodySection BodySection = InSkeletalAttachment->BodySection;
				if (BodySectionTagMap.Contains(BodySection))
				{
					RemovedComponents = RemoveCosmeticAttachment(InParentMeshComponent, BodySection);
					const FName BodySectionTag = BodySectionTagMap[BodySection];
					if (InSkeletalAttachment->PrimaryMesh.IsValid())
					{
						UActorComponent* NewComponent = MeshOwner->AddComponentByClass(USkeletalMeshComponent::StaticClass(), true, MeshOwner->GetActorTransform(), false);
						if (USkeletalMeshComponent* NewSkeletalMeshComponent = Cast<USkeletalMeshComponent>(NewComponent))
						{
							NewSkeletalMeshComponent->AttachToComponent(InParentMeshComponent, AttachRules, InSkeletalAttachment->PrimaryAttachmentSocket);
							NewSkeletalMeshComponent->ComponentTags.AddUnique(BodySectionTag);
							NewSkeletalMeshComponent->RegisterComponent();
							NewSkeletalMeshComponent->SetSkeletalMeshAsset(InSkeletalAttachment->PrimaryMesh.Get());
							if (InSkeletalAttachment->PrimaryMaterial.IsValid())
							{
								NewSkeletalMeshComponent->SetMaterial(0, InSkeletalAttachment->PrimaryMaterial.Get());
							}
							if (!InAttachmentName.IsNone())
							{
								const FString AttachmentNameString = FString("Attachment." + InAttachmentName.ToString());
								const FName AttachmentTag = FName(AttachmentNameString);
								NewSkeletalMeshComponent->ComponentTags.AddUnique(AttachmentTag);
							}

							NewSkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
							NewSkeletalMeshComponent->SetCollisionObjectType(ECC_Destructible);
							NewSkeletalMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
						}
					}

					if (InSkeletalAttachment->SecondaryMesh.IsValid())
					{
						UActorComponent* NewComponent = MeshOwner->AddComponentByClass(USkeletalMeshComponent::StaticClass(), true, MeshOwner->GetActorTransform(), false);
						if (USkeletalMeshComponent* NewSkeletalMeshComponent = Cast<USkeletalMeshComponent>(NewComponent))
						{
							NewSkeletalMeshComponent->AttachToComponent(InParentMeshComponent, AttachRules, InSkeletalAttachment->SecondaryAttachmentSocket);
							NewSkeletalMeshComponent->ComponentTags.AddUnique(BodySectionTag);
							NewSkeletalMeshComponent->RegisterComponent();
							NewSkeletalMeshComponent->SetSkeletalMeshAsset(InSkeletalAttachment->SecondaryMesh.Get());
							if (InSkeletalAttachment->SecondaryMaterial.IsValid())
							{
								NewSkeletalMeshComponent->SetMaterial(0, InSkeletalAttachment->SecondaryMaterial.Get());
							}
							if (!InAttachmentName.IsNone())
							{
								const FString AttachmentNameString = FString("Attachment." + InAttachmentName.ToString());
								const FName AttachmentTag = FName(AttachmentNameString);
								NewSkeletalMeshComponent->ComponentTags.AddUnique(AttachmentTag);
							}

							NewSkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
							NewSkeletalMeshComponent->SetCollisionObjectType(ECC_Destructible);
							NewSkeletalMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
						}
					}
					else if (InSkeletalAttachment->bMirrorPrimaryAsSecondary && InSkeletalAttachment->PrimaryMesh.IsValid())
					{
						UActorComponent* NewComponent = MeshOwner->AddComponentByClass(USkeletalMeshComponent::StaticClass(), true, MeshOwner->GetActorTransform(), false);
						if (USkeletalMeshComponent* NewSkeletalMeshComponent = Cast<USkeletalMeshComponent>(NewComponent))
						{
							NewSkeletalMeshComponent->AttachToComponent(InParentMeshComponent, AttachRules, InSkeletalAttachment->SecondaryAttachmentSocket);
							NewSkeletalMeshComponent->ComponentTags.AddUnique(BodySectionTag);
							NewSkeletalMeshComponent->RegisterComponent();
							NewSkeletalMeshComponent->SetSkeletalMeshAsset(InSkeletalAttachment->PrimaryMesh.Get());
							if (InSkeletalAttachment->PrimaryMaterial.IsValid())
							{
								NewSkeletalMeshComponent->SetMaterial(0, InSkeletalAttachment->PrimaryMaterial.Get());
							}
							if (!InAttachmentName.IsNone())
							{
								const FString AttachmentNameString = FString("Attachment." + InAttachmentName.ToString());
								const FName AttachmentTag = FName(AttachmentNameString);
								NewSkeletalMeshComponent->ComponentTags.AddUnique(AttachmentTag);
							}
							NewSkeletalMeshComponent->SetRelativeScale3D(FVector(-1.f, 1.f, 1.f));

							NewSkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
							NewSkeletalMeshComponent->SetCollisionObjectType(ECC_Destructible);
							NewSkeletalMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
						}
					}
				}
			}
		}
	}

	return RemovedComponents;
}

TArray<FName> UZoransBlueprintFunctionLibrary::RemoveCosmeticAttachment(const USkeletalMeshComponent* InParentMeshComponent, EZoranBodySection BodySection)
{
	TArray<FName> ComponentsRemoved;
	if (IsValid(InParentMeshComponent) && (BodySectionTagMap.Contains(BodySection) || BodySection == EZoranBodySection::None))
	{
		TArray<USceneComponent*> AttachedComponents;
		InParentMeshComponent->GetChildrenComponents(false, AttachedComponents);
		if (!AttachedComponents.IsEmpty())
		{
			TArray<USceneComponent*> ComponentsToRemove;
			if (BodySection == EZoranBodySection::None)
			{
				for (auto &q : AttachedComponents)
				{
					if (!q->ComponentTags.Contains("MenuPawn"))
					{
						ComponentsToRemove.AddUnique(q);
					}
				}
			}
			else
			{
				const FName BodySectionTag = BodySectionTagMap[BodySection];
				for (auto const &x : AttachedComponents)
				{
					if (x->ComponentTags.Contains(BodySectionTag))
					{
						ComponentsToRemove.AddUnique(x);
					}
				}
			}

			if (!ComponentsToRemove.IsEmpty())
			{
				for (auto const &y : ComponentsToRemove)
				{
					for (auto const &Tag : y->ComponentTags)
					{
						const FString TagString = Tag.ToString();
						if (TagString.StartsWith("Attachment."))
						{
							ComponentsRemoved.Add(FName(TagString.RightChop(11)));
						}
					}
					
					y->DestroyComponent();
				}
			}
		}
	}

	return ComponentsRemoved;
}

bool AreFNameArraysIdentical(const TArray<FName>& Arr1, const TArray<FName>& Arr2)
{
	if(Arr1.Num() != Arr2.Num())
		return false;

	for (FName Name : Arr1)
	{
		if(!Arr2.Contains(Name))
		{
			return false;
		}
	}
	return true;
}

bool AreWeaponSetArraysIdentical(const TArray<FWeaponSkinSet>& Arr1, const TArray<FWeaponSkinSet>& Arr2)
{
	if(Arr1.Num() != Arr2.Num())
		return false;

	for (FWeaponSkinSet SkinSet : Arr1)
	{
		if(!Arr2.ContainsByPredicate([SkinSet](const FWeaponSkinSet& OtherSkinSet)
		{
			return SkinSet.WeaponName.IsEqual(OtherSkinSet.WeaponName)
			&& SkinSet.WeaponSkin.IsEqual(OtherSkinSet.WeaponSkin)
			&& SkinSet.WeaponSwatch.IsEqual(OtherSkinSet.WeaponSwatch);
		}))
		{
			return false;
		}
	}
	return true;
}

void LoadoutToSaveJson(UObject* WorldContextObject, const FPlayerLoadout& LoadedLoadout, const FPlayerLoadout& ModifiedLoadout,
	const FString& ObjectName, const bool bForceSave, bool& bSaveRequired, UPlayFabJsonObject* JsonParent)
{
	if(bForceSave || !LoadedLoadout.PrimaryWeapon.IsEqual(ModifiedLoadout.PrimaryWeapon)
		|| !LoadedLoadout.SecondaryWeapon.IsEqual(ModifiedLoadout.SecondaryWeapon)
		|| !LoadedLoadout.Hologram.IsEqual(ModifiedLoadout.Hologram)
		|| !LoadedLoadout.ThrowdownClose.IsEqual(ModifiedLoadout.ThrowdownClose)
		|| !LoadedLoadout.ThrowdownRanged.IsEqual(ModifiedLoadout.ThrowdownRanged)
		|| !LoadedLoadout.ZoranSkin.IsEqual(ModifiedLoadout.ZoranSkin)
		|| !LoadedLoadout.ZoranSwatch.IsEqual(ModifiedLoadout.ZoranSwatch)
		|| !LoadedLoadout.CharacterSelection.IsEqual(ModifiedLoadout.CharacterSelection)
		|| !AreFNameArraysIdentical(LoadedLoadout.AttachedCosmetics, ModifiedLoadout.AttachedCosmetics)
		|| !AreWeaponSetArraysIdentical(LoadedLoadout.WeaponSkinSets, ModifiedLoadout.WeaponSkinSets))
	{
		bSaveRequired = true;
		UPlayFabJsonObject* JsonObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
		JsonObject->SetStringField("PrimaryWeapon", ModifiedLoadout.PrimaryWeapon.ToString());
		JsonObject->SetStringField("SecondaryWeapon", ModifiedLoadout.SecondaryWeapon.ToString());
		JsonObject->SetStringField("Hologram", ModifiedLoadout.Hologram.ToString());
		JsonObject->SetStringField("ThrowdownClose", ModifiedLoadout.ThrowdownClose.ToString());
		JsonObject->SetStringField("ThrowdownRanged", ModifiedLoadout.ThrowdownRanged.ToString());
		JsonObject->SetStringField("ZoranSkin", ModifiedLoadout.ZoranSkin.ToString());
		JsonObject->SetStringField("ZoranSwatch", ModifiedLoadout.ZoranSwatch.ToString());
		JsonObject->SetStringField("CharacterSelection", ModifiedLoadout.CharacterSelection.ToString());

		FString AttachedCosmeticsString = "";
		for (FName AttachedCosmetic : ModifiedLoadout.AttachedCosmetics)
			AttachedCosmeticsString += "," + AttachedCosmetic.ToString();
		AttachedCosmeticsString = "[" + AttachedCosmeticsString.RightChop(1) + "]";
		JsonObject->SetStringField("AttachedCosmetics", AttachedCosmeticsString);

		TArray<UPlayFabJsonObject*> WeaponSkinSetsArray = {};
		for (FWeaponSkinSet WeaponSkinSet : ModifiedLoadout.WeaponSkinSets)
		{
			UPlayFabJsonObject* SkinSetObj = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
			SkinSetObj->SetStringField("WeaponName", WeaponSkinSet.WeaponName.ToString());
			SkinSetObj->SetStringField("WeaponSkin", WeaponSkinSet.WeaponSkin.ToString());
			SkinSetObj->SetStringField("WeaponSwatch", WeaponSkinSet.WeaponSwatch.ToString());
			WeaponSkinSetsArray.Add(SkinSetObj);
		}
		JsonObject->SetObjectArrayField("WeaponSkinSets", WeaponSkinSetsArray);
		
		JsonParent->SetObjectField(ObjectName, JsonObject);
	}
}

void UZoransBlueprintFunctionLibrary::GeneratePlayerSaveData(UObject* WorldContextObject,
	const FZoransPlayerData& LoadedData, const FZoransPlayerData& ModifiedData, const bool bForceSave,
	UPlayFabJsonObject*& SaveArgs, bool& bSaveRequired, bool& bUpdatePresence)
{
	UPlayFabJsonObject* SaveJson = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
	bSaveRequired = false;

	//Player Profile Data
	const FPlayerProfileData& LoadedProfile = LoadedData.ProfileData;
	const FPlayerProfileData& ModifiedProfile = ModifiedData.ProfileData;
	if(bForceSave || !LoadedProfile.Banner.IsEqual(ModifiedProfile.Banner)
		|| !LoadedProfile.Frame.IsEqual(ModifiedProfile.Frame)
		|| !LoadedProfile.Image.IsEqual(ModifiedProfile.Image))
	{
		bSaveRequired = true;
		bUpdatePresence = true;

		UPlayFabJsonObject* JsonObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
		JsonObject->SetStringField("Banner", ModifiedProfile.Banner.ToString());
		JsonObject->SetStringField("Frame", ModifiedProfile.Frame.ToString());
		JsonObject->SetStringField("Image", ModifiedProfile.Image.ToString());
		SaveJson->SetObjectField("ProfileData", JsonObject);
	}

	//Emote List
	const FEmoteList& LoadedEmoteList = LoadedData.EmoteList;
	const FEmoteList& ModifiedEmoteList = ModifiedData.EmoteList;
	if(bForceSave || !LoadedEmoteList.Emote1.IsEqual(ModifiedEmoteList.Emote1)
		|| !LoadedEmoteList.Emote2.IsEqual(ModifiedEmoteList.Emote2)
		|| !LoadedEmoteList.Emote3.IsEqual(ModifiedEmoteList.Emote3)
		|| !LoadedEmoteList.Emote4.IsEqual(ModifiedEmoteList.Emote4)
		|| !LoadedEmoteList.Emote5.IsEqual(ModifiedEmoteList.Emote5)
		|| !LoadedEmoteList.Emote6.IsEqual(ModifiedEmoteList.Emote6))
	{
		bSaveRequired = true;

		UPlayFabJsonObject* JsonObject = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
		JsonObject->SetStringField("Emote1", ModifiedEmoteList.Emote1.ToString());
		JsonObject->SetStringField("Emote2", ModifiedEmoteList.Emote2.ToString());
		JsonObject->SetStringField("Emote3", ModifiedEmoteList.Emote3.ToString());
		JsonObject->SetStringField("Emote4", ModifiedEmoteList.Emote4.ToString());
		JsonObject->SetStringField("Emote5", ModifiedEmoteList.Emote5.ToString());
		JsonObject->SetStringField("Emote6", ModifiedEmoteList.Emote6.ToString());
		SaveJson->SetObjectField("EmoteList", JsonObject);
	}
	
	//Loadouts
	LoadoutToSaveJson(WorldContextObject, LoadedData.Loadouts.PlayerLoadout1, ModifiedData.Loadouts.PlayerLoadout1,
		"Loadout1", bForceSave, bSaveRequired, SaveJson);
	LoadoutToSaveJson(WorldContextObject, LoadedData.Loadouts.PlayerLoadout2, ModifiedData.Loadouts.PlayerLoadout2,
		"Loadout2", bForceSave, bSaveRequired, SaveJson);
	LoadoutToSaveJson(WorldContextObject, LoadedData.Loadouts.PlayerLoadout3, ModifiedData.Loadouts.PlayerLoadout3,
		"Loadout3", bForceSave, bSaveRequired, SaveJson);

	SaveArgs = UPlayFabJsonObject::ConstructJsonObject(WorldContextObject);
	SaveArgs->SetStringField("SaveData", SaveJson->EncodeJson());
}

void UZoransBlueprintFunctionLibrary::GetDedicatedServerLogin(FString& Username, FString& Password)
{
#if UE_SERVER
	Username = "nocontact@zollpa.com";
	Password = "testpassword";
#else
	Username = "";
	Password = "";
#endif
	
}

float GetWidgetSortValue(UWidget* Widget)
{
	if(Widget->Implements<USortableWidgetInterface>())
	{
		
		return ISortableWidgetInterface::Execute_GetSortValue(Widget);
	}
	return 0;
}

void UZoransBlueprintFunctionLibrary::SortWidgets(TArray<UWidget*>& Array)
{
	for (int32 Index = 0; Index < Array.Num(); ++Index)
	{
		UWidget* Key = Array[Index];
		int32 LookBackIndex;
		for (LookBackIndex = Index-1; LookBackIndex >= 0 && GetWidgetSortValue(Array[LookBackIndex]) < GetWidgetSortValue(Key); --LookBackIndex)
		{
			Array[LookBackIndex+1] = Array[LookBackIndex];
		}
		Array[LookBackIndex+1] = Key;
	}
}

int32 UZoransBlueprintFunctionLibrary::GetServerIP(UObject* WorldContextObject, bool bUseFInternetAddr)
{
	if (!WorldContextObject)
	{
		return -1;
	}
	if (bUseFInternetAddr)
	{
		uint32 ip = 0;
		TSharedPtr<FInternetAddr> addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		addr.Get()->GetIp(ip);
		return ip;
	}
	return FCString::Atoi(*WorldContextObject->GetWorld()->GetAddressURL());
}