// Breachpoint. The HUD C++ base.
#include "UI/BRHUDLayout.h"

#include "Components/PanelWidget.h"
#include "Core/BRCore.h"
#include "Engine/World.h"
#include "INotifyFieldValueChanged.h"
#include "MVVMSubsystem.h"
#include "UI/BRUISettings.h"
#include "UI/BRViewModels.h"
#include "View/MVVMView.h"

void UBRKillfeedEntryWidget::SetEntry(const FBRKillfeedViewEntry& InEntry)
{
	Entry = InEntry;
	BP_OnEntrySet();
}

UBRHUDLayout::UBRHUDLayout(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, KillfeedPool(*this)
{
	InputMode = EBRWidgetInputMode::Game;
}

void UBRHUDLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	KillfeedPool.SetWorld(GetWorld());
	KillfeedPool.SetDefaultPlayerController(GetOwningPlayer());

	const UBRUISettings& Settings = UBRUISettings::Get();
	if (!Settings.KillfeedEntryClass.IsNull())
	{
		ResolvedKillfeedEntryClass = Settings.KillfeedEntryClass.LoadSynchronous();
	}

	if (!ResolvedKillfeedEntryClass)
	{
		UE_LOG(LogBRUI, Warning,
			TEXT("BRUISettings.KillfeedEntryClass is unset or failed to load; the killfeed will ")
			TEXT("render no rows."));
	}
}

void UBRHUDLayout::ReleaseSlateResources(bool bReleaseChildren)
{
	KillfeedPool.ReleaseAllSlateResources();

	Super::ReleaseSlateResources(bReleaseChildren);
}

void UBRHUDLayout::BindViewModels()
{
	Super::BindViewModels();

	PushViewModelsIntoMVVMView();

	if (UBRVM_Combat* Combat = GetCombatViewModel())
	{
		Combat->OnHitMarker().AddUObject(this, &UBRHUDLayout::HandleHitMarker);

		VitalsStateFieldHandle = Combat->AddFieldValueChangedDelegate(
			UBRVM_Combat::FFieldNotificationClassDescriptor::VitalsState,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
				this, &UBRHUDLayout::HandleViewModelFieldChanged));

		BP_OnVitalsStateChanged(Combat->GetVitalsState());
	}
	else
	{
		BP_OnVitalsStateChanged(EBRUIDataState::Unknown);
	}

	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		Match->OnKillfeedChanged().AddUObject(this, &UBRHUDLayout::HandleKillfeedChanged);

		MatchStateFieldHandle = Match->AddFieldValueChangedDelegate(
			UBRVM_Match::FFieldNotificationClassDescriptor::MatchState,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
				this, &UBRHUDLayout::HandleViewModelFieldChanged));

		BP_OnMatchStateChanged(Match->GetMatchState());
		RebuildKillfeed();
	}
	else
	{
		BP_OnMatchStateChanged(EBRUIDataState::Unknown);
	}
}

void UBRHUDLayout::UnbindViewModels()
{
	if (UBRVM_Combat* Combat = GetCombatViewModel())
	{
		Combat->OnHitMarker().RemoveAll(this);
		if (VitalsStateFieldHandle.IsValid())
		{
			Combat->RemoveFieldValueChangedDelegate(
				UBRVM_Combat::FFieldNotificationClassDescriptor::VitalsState, VitalsStateFieldHandle);
		}
	}
	VitalsStateFieldHandle.Reset();

	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		Match->OnKillfeedChanged().RemoveAll(this);
		if (MatchStateFieldHandle.IsValid())
		{
			Match->RemoveFieldValueChangedDelegate(
				UBRVM_Match::FFieldNotificationClassDescriptor::MatchState, MatchStateFieldHandle);
		}
	}
	MatchStateFieldHandle.Reset();

	Super::UnbindViewModels();
}

void UBRHUDLayout::PushViewModelsIntoMVVMView()
{
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!View)
	{
		return;
	}

	const UBRUISettings& Settings = UBRUISettings::Get();

	if (UBRVM_Combat* Combat = GetCombatViewModel())
	{
		View->SetViewModel(Settings.CombatViewModelContextName, Combat);
	}
	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		View->SetViewModel(Settings.MatchViewModelContextName, Match);
	}
}

void UBRHUDLayout::HandleHitMarker(EBRHitMarkerKind Kind)
{
	switch (Kind)
	{
	case EBRHitMarkerKind::Shield:
		BP_OnShieldHit();
		break;
	case EBRHitMarkerKind::Flesh:
		BP_OnFleshHit();
		break;
	case EBRHitMarkerKind::Headshot:
		BP_OnHeadshotHit();
		break;
	case EBRHitMarkerKind::Kill:
		BP_OnKillConfirmed();
		break;
	case EBRHitMarkerKind::None:
	default:
		break;
	}
}

void UBRHUDLayout::HandleViewModelFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	if (const UBRVM_Combat* Combat = Cast<UBRVM_Combat>(Source))
	{
		if (FieldId == UBRVM_Combat::FFieldNotificationClassDescriptor::VitalsState)
		{
			BP_OnVitalsStateChanged(Combat->GetVitalsState());
		}
		return;
	}

	if (const UBRVM_Match* Match = Cast<UBRVM_Match>(Source))
	{
		if (FieldId == UBRVM_Match::FFieldNotificationClassDescriptor::MatchState)
		{
			BP_OnMatchStateChanged(Match->GetMatchState());
		}
	}
}

void UBRHUDLayout::HandleKillfeedChanged()
{
	RebuildKillfeed();
}

void UBRHUDLayout::RebuildKillfeed()
{
	if (!KillfeedContainer || !ResolvedKillfeedEntryClass)
	{
		return;
	}

	const UBRVM_Match* Match = GetMatchViewModel();
	if (!Match)
	{
		return;
	}

	KillfeedContainer->ClearChildren();
	KillfeedPool.ReleaseAll(false);

	const TArray<FBRKillfeedViewEntry>& Entries = Match->GetKillfeedEntries();
	int32 NumRows = 0;

	for (const FBRKillfeedViewEntry& Entry : Entries)
	{
		UBRKillfeedEntryWidget* Row = KillfeedPool.GetOrCreateInstance<UBRKillfeedEntryWidget>(ResolvedKillfeedEntryClass);
		if (!Row)
		{
			UE_LOG(LogBRUI, Warning, TEXT("Killfeed pool could not produce a row widget; %d of %d rendered."),
				NumRows, Entries.Num());
			break;
		}

		Row->SetEntry(Entry);
		KillfeedContainer->AddChild(Row);
		++NumRows;
	}

	BP_OnKillfeedRebuilt(NumRows);
}
