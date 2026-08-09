// Copyright Zollpa LLC


#include "ZoransResistance/HMS/FHMSActorSave.h"
#include "ZoransResistance/HMS/HMSSavedStateComponent.h"
#include "ZoransResistance/HMS/FHMSComponentSave.h"
#include "ZoransResistance/HMS/FHMSPropertySave.h"

FHMSActorSave::FHMSActorSave(AActor* Actor, TArray<FString> ComponentNames, TArray<FString> CustomPropertyNames)
{
	UHMSSavedStateComponent* savedState = nullptr;
	check(Actor != nullptr);
	check(Actor->FindComponentByClass<UHMSSavedStateComponent>());

	savedState = Actor->FindComponentByClass<UHMSSavedStateComponent>();
	this->ActorTransformation = Actor->GetActorTransform();

	if (savedState->Tag == 0) savedState->Tag = FMath::Rand();
	this->ActorID = savedState->Tag;

	FMemoryWriter ActorWriter = FMemoryWriter(this->ActorData, true);
	FObjectAndNameAsStringProxyArchive ar(ActorWriter, false);
	ar.ArIsSaveGame = true;
	ar.ArNoDelta = true;
	Actor->Serialize(ar);
	ActorWriter.Flush();

	for (FString n : ComponentNames)
	{
		FName nn = *n;
		UActorComponent* comp = Cast<UActorComponent>(Actor->GetDefaultSubobjectByName(nn));
		if (comp)
		{
			FHMSComponentSave save;
			FMemoryWriter CompWriter = FMemoryWriter(save.ComponentData, true);
			FObjectAndNameAsStringProxyArchive car(CompWriter, false);
			car.ArIsSaveGame = true;
			car.ArNoDelta = true;
			comp->Serialize(car);
			this->ComponentData.Add(n, save);
			CompWriter.Flush();
		}

	}

	for (FString n : CustomPropertyNames)
	{
		FProperty* prop = Actor->GetClass()->FindPropertyByName(FName(*n));
		if (!prop) continue;


		FHMSPropertySave save;
		FMemoryWriter StructWriter = FMemoryWriter(save.PropertyData, true);
		FObjectAndNameAsStringProxyArchive car(StructWriter, false);

		if (prop->IsA(FArrayProperty::StaticClass()))
		{
			FArrayProperty* arrProp = CastField<FArrayProperty>(prop);
			arrProp->SerializeItem(FStructuredArchiveFromArchive(car).GetSlot(), arrProp->ContainerPtrToValuePtr<void>(Actor), nullptr);
		}
		else
			if (prop->IsA(FStructProperty::StaticClass()))
			{
				FStructProperty* StructProp = CastField<FStructProperty>(prop);
				StructProp->SerializeItem(FStructuredArchiveFromArchive(car).GetSlot(), StructProp->ContainerPtrToValuePtr<void>(Actor), nullptr);
			}
		this->CustomPropertyData.Add(n, save);
		StructWriter.Flush();
	}
}

void FHMSActorSave::Deserialize(AActor* Actor, TArray<FString> ComponentNames, TArray<FString> CustomPropertyNames)
{
	FMemoryReader ActorReader = FMemoryReader(this->ActorData, true);
	FObjectAndNameAsStringProxyArchive Archive(ActorReader, false);
	Archive.ArIsSaveGame = true;
	Archive.ArNoDelta = true;
	Actor->Serialize(Archive);
	ActorReader.Flush();


	for (FString n : ComponentNames)
	{
		FName nn = *n;
		UActorComponent* comp = Cast<UActorComponent>(Actor->GetDefaultSubobjectByName(nn));
		if (comp)
		{
			FMemoryReader CompReader = FMemoryReader(this->ComponentData.Find(n)->ComponentData, true);
			FObjectAndNameAsStringProxyArchive car(CompReader, false);
			car.ArIsSaveGame = true;
			car.ArNoDelta = true;
			comp->Serialize(car);
			CompReader.Flush();
		}

	}

	for (FString n : CustomPropertyNames)
	{
		FProperty* prop = Actor->GetClass()->FindPropertyByName(FName(*n));
		if (!prop) continue;


		FHMSPropertySave* save = CustomPropertyData.Find(n);
		FMemoryReader StructReader = FMemoryReader(save->PropertyData, true);
		FObjectAndNameAsStringProxyArchive car(StructReader, false);

		//if (prop->IsA(FArrayProperty::StaticClass()))
		//{
		//	FArrayProperty* arrProp = Cast<FArrayProperty>(prop);
		//	arrProp->SerializeItem(FStructuredArchiveFromArchive(car).GetSlot(), arrProp->ContainerPtrToValuePtr<void>(Actor), nullptr);
		//}
		//else
		//	if (prop->IsA(FStructProperty::StaticClass()))
		//	{
		//		FStructProperty* StructProp = Cast<FStructProperty>(prop);
		//		StructProp->SerializeItem(FStructuredArchiveFromArchive(car).GetSlot(), StructProp->ContainerPtrToValuePtr<void>(Actor), nullptr);
		//	}
		StructReader.Flush();
	}
}
