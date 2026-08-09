#include "Core/OSCheatManager.h"
#include "CommonActivatableWidget.h"

#include "Engine/DebugCameraController.h"

#if !UE_BUILD_SHIPPING

#include "Core/OSPlayerController.h"
#include "Core/OSPlayerState.h"
#include "Core/OSGameMode.h"
#include "Characters/OSCharacter.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "GAS/Effects/GE_OSDebugNoHealthDamage.h"
#include "GAS/Effects/GE_OSDebugNoStaminaDrain.h"
#include "GAS/Effects/GE_OSDebugBlockHealthRegen.h"
#include "GAS/Effects/GE_OSDebugBlockStaminaRegen.h"
#include "GAS/Effects/GE_OSDebugStunLock.h"
#include "Actors/OSControlPoint.h"
#include "Characters/OSPunchingBag.h"
#include "Core/GameModes/OSGameMode_Domination.h"
#include "Core/GameStates/OSGameState_Domination.h"
#include "Components/CapsuleComponent.h"
#include "GenericTeamAgentInterface.h"
#include "Data/OSGameplayTags.h"
#include "AbilitySystemInterface.h"
#include "Engine/World.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWeakWidget.h"
#include "Framework/Application/SlateApplication.h"

// ---------------------------------------------------------------------------
// CVars — unified OS.Debug.* namespace with cached bools via change callbacks.
// Extern bools are read by OSHUD draw path and debug status overlay.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Global bools for viewport-level settings (shared across all players in process).
// Visual overlays and punching bag state are viewport-level, not per-player.
// Player cheats (NoHealthDamage, etc.) are per-player — stored on CheatManager instance.
// ---------------------------------------------------------------------------

bool GDebugShowHealthBars     = false;
bool GDebugShowNameplates     = false;
bool GDebugPunchingBag        = false;
bool GDebugMantleViz          = false;
bool GDebugAttackTraceViz     = false;
bool GDebugTargetingViz       = false;
bool GDebugRotationViz        = false;
bool GDebugGrabViz            = false;

static void OnShowHealthBarsChanged(IConsoleVariable* Var)     { GDebugShowHealthBars = Var->GetInt() != 0; }
static void OnShowNameplatesChanged(IConsoleVariable* Var)     { GDebugShowNameplates = Var->GetInt() != 0; }
static void OnPunchingBagChanged(IConsoleVariable* Var)        { GDebugPunchingBag = Var->GetInt() != 0; }
static void OnMantleVizChanged(IConsoleVariable* Var)          { GDebugMantleViz = Var->GetInt() != 0; }
static void OnAttackTraceVizChanged(IConsoleVariable* Var)     { GDebugAttackTraceViz = Var->GetInt() != 0; }
static void OnTargetingVizChanged(IConsoleVariable* Var)       { GDebugTargetingViz = Var->GetInt() != 0; }

static TAutoConsoleVariable<int32> CVarShowHealthBars(
	TEXT("OS.Debug.ShowHealthBars"), 1,
	TEXT("Show health bars above all characters"),
	FConsoleVariableDelegate::CreateStatic(&OnShowHealthBarsChanged),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarShowNameplates(
	TEXT("OS.Debug.ShowNameplates"), 1,
	TEXT("Show player nameplates above all characters"),
	FConsoleVariableDelegate::CreateStatic(&OnShowNameplatesChanged),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarPunchingBag(
	TEXT("OS.Debug.PunchingBag"), 0,
	TEXT("Spawn/despawn debug punching bag"),
	FConsoleVariableDelegate::CreateStatic(&OnPunchingBagChanged),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarMantleViz(
	TEXT("OS.Debug.MantleViz"), 0,
	TEXT("Show real-time mantle trace visualization overlay"),
	FConsoleVariableDelegate::CreateStatic(&OnMantleVizChanged),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAttackTraceViz(
	TEXT("OS.Debug.AttackTraceViz"), 0,
	TEXT("Draw attack trace sweep debug visualization for all characters"),
	FConsoleVariableDelegate::CreateStatic(&OnAttackTraceVizChanged),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarTargetingViz(
	TEXT("OS.Debug.TargetingViz"), 0,
	TEXT("Draw targeting debug visualization (candidates, scores, cone sweep, chosen target)"),
	FConsoleVariableDelegate::CreateStatic(&OnTargetingVizChanged),
	ECVF_Default);

// Master convenience command
static FAutoConsoleCommand CmdDebugLevel(
	TEXT("OS.Debug"),
	TEXT("Set debug overlay level: 0=off, 1=health bars, 2=health bars + nameplates"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		const int32 Level = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		CVarShowHealthBars->Set(Level >= 1 ? 1 : 0, ECVF_SetByConsole);
		CVarShowNameplates->Set(Level >= 2 ? 1 : 0, ECVF_SetByConsole);
	}));

// ---------------------------------------------------------------------------
// Reset all debug CVars to 0. Called on CheatManager creation to ensure each
// PIE session starts clean (CVars are global statics that survive PIE end).
// ---------------------------------------------------------------------------

static void ResetAllDebugCVars()
{
	// Health bars and nameplates default ON — only reset debug-only CVars.
	if (CVarPunchingBag.AsVariable())	CVarPunchingBag->Set(0, ECVF_SetByConsole);
	if (CVarMantleViz.AsVariable())		CVarMantleViz->Set(0, ECVF_SetByConsole);
	if (CVarAttackTraceViz.AsVariable())	CVarAttackTraceViz->Set(0, ECVF_SetByConsole);
	if (CVarTargetingViz.AsVariable())	CVarTargetingViz->Set(0, ECVF_SetByConsole);
	GDebugRotationViz = false;
	GDebugGrabViz = false;
}

// ---------------------------------------------------------------------------
// Helper: toggle a CVar between 0 and 1
// ---------------------------------------------------------------------------

static void ToggleCVar(TAutoConsoleVariable<int32>& CVar)
{
	CVar->Set(CVar->GetInt() != 0 ? 0 : 1, ECVF_SetByConsole);
}

// ---------------------------------------------------------------------------
// Helper: apply or remove a GE class on an ASC based on a bool.
// Null-safe: silently skips if ASC or GEClass is null.
// ---------------------------------------------------------------------------

static void SetDebugEffect(UOSAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> GEClass, bool bApply)
{
	if (!ASC)
	{
		UE_LOG(LogOSDebug, Warning, TEXT("SetDebugEffect: ASC is null — cannot %s effect."), bApply ? TEXT("apply") : TEXT("remove"));
		return;
	}
	if (!GEClass)
	{
		UE_LOG(LogOSDebug, Warning, TEXT("SetDebugEffect: GEClass is null."));
		return;
	}

	const FString EffectName = GEClass->GetName();

	if (bApply)
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = GEClass;
		if (ASC->GetActiveEffects(Query).Num() > 0)
		{
			UE_LOG(LogOSDebug, Log, TEXT("%s already active — skipping."), *EffectName);
			return;
		}

		FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		Ctx.AddSourceObject(ASC->GetOwner());
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GEClass, 1.f, Ctx);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			UE_LOG(LogOSDebug, Log, TEXT("Applied %s"), *EffectName);
		}
	}
	else
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = GEClass;
		const int32 Removed = ASC->RemoveActiveEffects(Query);
		UE_LOG(LogOSDebug, Log, TEXT("Removed %d instance(s) of %s"), Removed, *EffectName);
	}
}

// ---------------------------------------------------------------------------
// GetPlayerASC
// ---------------------------------------------------------------------------

UOSAbilitySystemComponent* UOSCheatManager::GetPlayerASC() const
{
	APlayerController* PC = GetOuterAPlayerController();
	if (!PC) return nullptr;

	if (AOSPlayerState* PS = PC->GetPlayerState<AOSPlayerState>())
	{
		return Cast<UOSAbilitySystemComponent>(PS->GetAbilitySystemComponent());
	}
	return nullptr;
}

UOSAbilitySystemComponent* UOSCheatManager::GetBagASC() const
{
	if (!PunchingBagActor.IsValid()) return nullptr;
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(PunchingBagActor.Get()))
	{
		return Cast<UOSAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
	}
	return nullptr;
}

void UOSCheatManager::BagAction(FGameplayTag AbilityTag)
{
	AOSPlayerController* PC = Cast<AOSPlayerController>(GetOuterAPlayerController());
	if (!PC) return;

	if (!bLoopActions)
	{
		// Single fire via RPC.
		PC->Server_PunchingBagAction(AbilityTag);
		return;
	}

	// Loop mode: toggle. If already looping this action, stop. Otherwise start.
	if (LoopingActionTag == AbilityTag && BagActionLoopHandle.IsValid())
	{
		StopBagActionLoop();
		return;
	}

	StopBagActionLoop();
	LoopingActionTag = AbilityTag;

	if (!PC->GetWorld()) return;

	// Fire immediately, then repeat at interval.
	PC->Server_PunchingBagAction(AbilityTag);

	PC->GetWorld()->GetTimerManager().SetTimer(
		BagActionLoopHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, PC]()
		{
			if (!GDebugPunchingBag)
			{
				StopBagActionLoop();
				return;
			}
			PC->Server_PunchingBagAction(LoopingActionTag);
		}),
		BagActionLoopInterval,
		true);
}

void UOSCheatManager::StopBagActionLoop()
{
	if (BagActionLoopHandle.IsValid())
	{
		APlayerController* PC = GetOuterAPlayerController();
		if (PC && PC->GetWorld())
		{
			PC->GetWorld()->GetTimerManager().ClearTimer(BagActionLoopHandle);
		}
	}
	LoopingActionTag = FGameplayTag();
}

// ===========================================================================
// SLATE DEBUG MENU
// ===========================================================================

/** Label for team attitude; uses raw uint8 to avoid Win32 / macro clashes on ETeamAttitude::Friendly. */
static const TCHAR* OSDebugBagTeamAttitudeString(uint8 AttitudeByte)
{
	// ETeamAttitude: Hostile=0, Neutral=1, Friendly=2 (UE GenericTeamAgentTypes)
	switch (AttitudeByte)
	{
	case 0: return TEXT("Hostile");
	case 1: return TEXT("Neutral");
	default: return TEXT("Friendly");
	}
}

void UOSCheatManager::BuildDebugMenuWidget()
{
	DebugMenuWidget =
		SNew(SBox)
		.WidthOverride(280.f)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 20.f, 0.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.85f))
			.Padding(FMargin(12.f))
			[
				SNew(SVerticalBox)

				// Title
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("DEBUG MENU")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
					.ColorAndOpacity(FLinearColor::Yellow)
				]

				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SScrollBox)

					// --- Player Cheats ---
					+ SScrollBox::Slot().Padding(0, 4)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Player Cheats")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FLinearColor::White)
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return bNoHealthDamage ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { NoHealthDamage(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("No Health Damage"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return bNoStaminaDrain ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { NoStaminaDrain(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("No Stamina Drain"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return bBlockHealthRegen ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { BlockHealthRegen(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Block Health Regen"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return bBlockStaminaRegen ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { BlockStaminaRegen(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Block Stamina Regen"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return bHitReactRetrigger ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { ToggleHitReactRetrigger(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("HitReact Retrigger"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					// --- Visual Overlays ---
					+ SScrollBox::Slot().Padding(0, 8, 0, 4)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Visual Overlays")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FLinearColor::White)
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([]() { return GDebugShowHealthBars ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { ShowHealthBars(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Health Bars"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([]() { return GDebugShowNameplates ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { ShowNameplates(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Nameplates"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([]() { return GDebugMantleViz ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { ShowMantleDebug(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Mantle Debug"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([]() { return GDebugAttackTraceViz ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { ShowAttackTraceDebug(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Attack Trace Debug"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([]() { return GDebugRotationViz ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { ShowRotationDebug(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Rotation / Warp"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([]() { return GDebugTargetingViz ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { ShowTargetingDebug(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Targeting"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([]() { return GDebugGrabViz ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { ShowGrabDebug(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Grab"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { ToggleDebugCameraFromUI(); return FReply::Handled(); })
						[
							SNew(STextBlock)
							.Text_Lambda([]() { return FText::FromString( TEXT("Toggle Debug Camera")); })
						]
					]

					// --- Punching Bag ---
					+ SScrollBox::Slot().Padding(0, 8, 0, 4)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Punching Bag")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FLinearColor::White)
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { TogglePunchingBag(); return FReply::Handled(); })
						[
							SNew(STextBlock)
							.Text_Lambda([]() { return FText::FromString(GDebugPunchingBag ? TEXT("Despawn Bag") : TEXT("Spawn Bag")); })
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return bPunchingBagEnemyTeam ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
						{
							bPunchingBagEnemyTeam = (NewState == ECheckBoxState::Checked);
						})
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Bag: enemy team (off = same team / friendly)")))
							.ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(STextBlock)
						.Visibility_Lambda([]() { return GDebugPunchingBag ? EVisibility::Visible : EVisibility::Collapsed; })
						.ColorAndOpacity(FLinearColor(0.75f, 0.85f, 1.f))
						.Text_Lambda([this]()
						{
							if (!PunchingBagActor.IsValid())
							{
								return FText::GetEmpty();
							}
							const IGenericTeamAgentInterface* TeamIf = Cast<IGenericTeamAgentInterface>(PunchingBagActor.Get());
							if (!TeamIf)
							{
								return FText::FromString(TEXT("Bag team: (no IGenericTeamAgentInterface)"));
							}
							const FGenericTeamId BagId = TeamIf->GetGenericTeamId();
							APlayerController* PC = GetOuterAPlayerController();
							FGenericTeamId MyTeam = FGenericTeamId::NoTeam;
							if (AOSPlayerState* PS = PC ? PC->GetPlayerState<AOSPlayerState>() : nullptr)
							{
								MyTeam = PS->GetGenericTeamId();
							}
							if (BagId == FGenericTeamId::NoTeam)
							{
								return FText::FromString(TEXT("Bag team: NoTeam (neutral / FFA)"));
							}
							const TCHAR* AttStr = OSDebugBagTeamAttitudeString(static_cast<uint8>(
								FGenericTeamId::GetAttitude(MyTeam, BagId)));
							return FText::FromString(FString::Printf(
								TEXT("Bag team: %d | vs you: %s"), BagId.GetId(), AttStr));
						})
					]

					// --- Bag Controller (visible only when bag is spawned) ---
					+ SScrollBox::Slot().Padding(0, 8, 0, 4)
					[
						SNew(SBox)
						.Visibility_Lambda([]() { return GDebugPunchingBag ? EVisibility::Visible : EVisibility::Collapsed; })
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Bag Controller")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							.ColorAndOpacity(FLinearColor::White)
						]
					]
					// Bag Cheats
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.Visibility_Lambda([]() { return GDebugPunchingBag ? EVisibility::Visible : EVisibility::Collapsed; })
						.IsChecked_Lambda([this]() { return bBagNoHealthDamage ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { ToggleBagNoHealthDamage(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("No Health Damage"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.Visibility_Lambda([]() { return GDebugPunchingBag ? EVisibility::Visible : EVisibility::Collapsed; })
						.IsChecked_Lambda([this]() { return bBagNoStaminaDrain ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { ToggleBagNoStaminaDrain(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("No Stamina Drain"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.Visibility_Lambda([]() { return GDebugPunchingBag ? EVisibility::Visible : EVisibility::Collapsed; })
						.IsChecked_Lambda([this]() { return bBagBlockHealthRegen ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { ToggleBagBlockHealthRegen(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Block Health Regen"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.Visibility_Lambda([]() { return GDebugPunchingBag ? EVisibility::Visible : EVisibility::Collapsed; })
						.IsChecked_Lambda([this]() { return bBagBlockStaminaRegen ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { ToggleBagBlockStaminaRegen(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Block Stamina Regen"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.Visibility_Lambda([]() { return GDebugPunchingBag ? EVisibility::Visible : EVisibility::Collapsed; })
						.IsChecked_Lambda([this]() { return bBagHitReactRetrigger ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { ToggleBagHitReactRetrigger(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("HitReact Retrigger"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					// Bag State — IsChecked live-polls the bag's IsBlocking tag so external cancels
					// (guard break, stamina break, dying) flip the checkbox back off automatically.
					// Pressing re-activates when unchecked without the user having to double-click.
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SCheckBox)
						.Visibility_Lambda([]() { return GDebugPunchingBag ? EVisibility::Visible : EVisibility::Collapsed; })
						.IsChecked_Lambda([this]()
						{
							UOSAbilitySystemComponent* ASC = GetBagASC();
							if (!ASC) return ECheckBoxState::Unchecked;
							return ASC->HasMatchingGameplayTag(FOSGameplayTags::Get().IsBlocking)
								? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([this](ECheckBoxState) { TogglePunchingBagBlock(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Block"))).ColorAndOpacity(FLinearColor::White)
						]
					]
					// Bag Actions — loop mode toggle + interval
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SHorizontalBox)
						.Visibility_Lambda([]() { return GDebugPunchingBag ? EVisibility::Visible : EVisibility::Collapsed; })
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
						[
							SNew(SCheckBox)
							.IsChecked_Lambda([this]() { return bLoopActions ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
							.OnCheckStateChanged_Lambda([this](ECheckBoxState)
							{
								bLoopActions = !bLoopActions;
								if (!bLoopActions) StopBagActionLoop();
							})
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Loop"))).ColorAndOpacity(FLinearColor::White)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Interval:"))).ColorAndOpacity(FLinearColor::White)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SSpinBox<float>)
							.MinDesiredWidth(50.f)
							.MinValue(0.5f)
							.MaxValue(10.f)
							.Delta(0.25f)
							.Value_Lambda([this]() { return BagActionLoopInterval; })
							.OnValueChanged_Lambda([this](float NewVal) { BagActionLoopInterval = NewVal; })
						]
					]
					// Bag action buttons
					+ SScrollBox::Slot().Padding(8, 4)
					[
						SNew(SHorizontalBox)
						.Visibility_Lambda([]() { return GDebugPunchingBag ? EVisibility::Visible : EVisibility::Collapsed; })
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
						[
							SNew(SButton)
							.OnClicked_Lambda([this]() { BagLightAttack(); return FReply::Handled(); })
							[
								SNew(STextBlock)
								.Text_Lambda([this]()
								{
									const bool bActive = bLoopActions && LoopingActionTag == FOSGameplayTags::Get().Attack_Light;
									return FText::FromString(bActive ? TEXT("[Light]") : TEXT("Light"));
								})
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
						[
							SNew(SButton)
							.OnClicked_Lambda([this]() { BagHeavyAttack(); return FReply::Handled(); })
							[
								SNew(STextBlock)
								.Text_Lambda([this]()
								{
									const bool bActive = bLoopActions && LoopingActionTag == FOSGameplayTags::Get().Attack_Heavy;
									return FText::FromString(bActive ? TEXT("[Heavy]") : TEXT("Heavy"));
								})
							]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.OnClicked_Lambda([this]() { BagDodge(); return FReply::Handled(); })
							[
								SNew(STextBlock)
								.Text_Lambda([this]()
								{
									const bool bActive = bLoopActions && LoopingActionTag == FOSGameplayTags::Get().Ability_Dodge;
									return FText::FromString(bActive ? TEXT("[Dodge]") : TEXT("Dodge"));
								})
							]
						]
					]

					// --- Character Swap ---
					+ SScrollBox::Slot().Padding(0, 8, 0, 4)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Swap Character")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FLinearColor::White)
					]
					+ SScrollBox::Slot().Padding(8, 2)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
						[
							SNew(SButton)
							.OnClicked_Lambda([this]() { SwapCharacter(1); return FReply::Handled(); })
							[ SNew(STextBlock).Text(FText::FromString(TEXT("Char 1"))) ]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
						[
							SNew(SButton)
							.OnClicked_Lambda([this]() { SwapCharacter(2); return FReply::Handled(); })
							[ SNew(STextBlock).Text(FText::FromString(TEXT("Char 2"))) ]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.OnClicked_Lambda([this]() { SwapCharacter(3); return FReply::Handled(); })
							[ SNew(STextBlock).Text(FText::FromString(TEXT("Char 3"))) ]
						]
					]
				]
			]
		];
}

void UOSCheatManager::ShowDebugMenu()
{
	APlayerController* PC = GetOuterAPlayerController();
	if (!PC) return;

	if (!DebugMenuWidget.IsValid())
	{
		BuildDebugMenuWidget();
	}

	if (UGameViewportClient* ViewportClient = PC->GetWorld()->GetGameViewport())
	{
		DebugMenuWeakWrapper = SNew(SWeakWidget).PossiblyNullContent(DebugMenuWidget);
		ViewportClient->AddViewportWidgetContent(DebugMenuWeakWrapper.ToSharedRef(), 10);
	}

	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (DebugMenuWidget.IsValid())
	{
		Mode.SetWidgetToFocus(DebugMenuWidget);
	}
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;

	bDebugMenuVisible = true;
}

void UOSCheatManager::HideDebugMenu()
{
	APlayerController* PC = GetOuterAPlayerController();
	if (!PC) return;

	if (DebugMenuWeakWrapper.IsValid())
	{
		if (UGameViewportClient* ViewportClient = PC->GetWorld()->GetGameViewport())
		{
			ViewportClient->RemoveViewportWidgetContent(DebugMenuWeakWrapper.ToSharedRef());
		}
		DebugMenuWeakWrapper.Reset();
	}

	FInputModeGameOnly Mode;
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = false;

	bDebugMenuVisible = false;
}

#endif // !UE_BUILD_SHIPPING

// ===========================================================================
// UFUNCTION(Exec) implementations — must exist in all builds (UHT requirement).
// Bodies are empty in shipping builds.
// ===========================================================================

void UOSCheatManager::InitCheatManager()
{
	Super::InitCheatManager();
#if !UE_BUILD_SHIPPING
	// Reset global CVars (survive PIE end as global statics).
	ResetAllDebugCVars();

	// Reset per-player cheat state.
	bNoHealthDamage = false;
	bNoStaminaDrain = false;
	bBlockHealthRegen = false;
	bBlockStaminaRegen = false;
	bHitReactRetrigger = false;

	// Reset per-bag cheat state.
	StopBagActionLoop();
	bLoopActions = false;
	BagActionLoopInterval = 2.0f;
	bBagNoHealthDamage = false;
	bBagNoStaminaDrain = false;
	bBagBlockHealthRegen = false;
	bBagBlockStaminaRegen = false;
	bBagHitReactRetrigger = false;
	PunchingBagActor = nullptr;

	// Invalidate any stale widget from a previous PIE session.
	DebugMenuWidget.Reset();
	DebugMenuWeakWrapper.Reset();
	bDebugMenuVisible = false;

	UE_LOG(LogOSDebug, Log, TEXT("CheatManager initialized — debug state reset."));
#endif
}

void UOSCheatManager::ToggleDebugMenu()
{
#if !UE_BUILD_SHIPPING
	if (bDebugMenuVisible)
		HideDebugMenu();
	else
		ShowDebugMenu();
#endif
}

void UOSCheatManager::NoHealthDamage()
{
#if !UE_BUILD_SHIPPING
	bNoHealthDamage = !bNoHealthDamage;
	if (AOSPlayerController* PC = Cast<AOSPlayerController>(GetOuterAPlayerController()))
	{
		PC->Server_ApplyDebugEffect(UGE_OSDebugNoHealthDamage::StaticClass(), bNoHealthDamage);
	}
#endif
}

void UOSCheatManager::NoStaminaDrain()
{
#if !UE_BUILD_SHIPPING
	bNoStaminaDrain = !bNoStaminaDrain;
	if (AOSPlayerController* PC = Cast<AOSPlayerController>(GetOuterAPlayerController()))
	{
		PC->Server_ApplyDebugEffect(UGE_OSDebugNoStaminaDrain::StaticClass(), bNoStaminaDrain);
	}
#endif
}

void UOSCheatManager::BlockHealthRegen()
{
#if !UE_BUILD_SHIPPING
	bBlockHealthRegen = !bBlockHealthRegen;
	if (AOSPlayerController* PC = Cast<AOSPlayerController>(GetOuterAPlayerController()))
	{
		PC->Server_ApplyDebugEffect(UGE_OSDebugBlockHealthRegen::StaticClass(), bBlockHealthRegen);
	}
#endif
}

void UOSCheatManager::BlockStaminaRegen()
{
#if !UE_BUILD_SHIPPING
	bBlockStaminaRegen = !bBlockStaminaRegen;
	if (AOSPlayerController* PC = Cast<AOSPlayerController>(GetOuterAPlayerController()))
	{
		PC->Server_ApplyDebugEffect(UGE_OSDebugBlockStaminaRegen::StaticClass(), bBlockStaminaRegen);
	}
#endif
}

void UOSCheatManager::ToggleHitReactRetrigger()
{
#if !UE_BUILD_SHIPPING
	bHitReactRetrigger = !bHitReactRetrigger;
	if (AOSPlayerController* PC = Cast<AOSPlayerController>(GetOuterAPlayerController()))
	{
		PC->Server_ApplyDebugEffect(UGE_OSDebugStunLock::StaticClass(), bHitReactRetrigger);
	}
#endif
}


void UOSCheatManager::ShowHealthBars()
{
#if !UE_BUILD_SHIPPING
	ToggleCVar(CVarShowHealthBars);
#endif
}

void UOSCheatManager::ShowNameplates()
{
#if !UE_BUILD_SHIPPING
	ToggleCVar(CVarShowNameplates);
#endif
}

void UOSCheatManager::ShowMantleDebug()
{
#if !UE_BUILD_SHIPPING
	ToggleCVar(CVarMantleViz);
#endif
}

void UOSCheatManager::ShowAttackTraceDebug()
{
#if !UE_BUILD_SHIPPING
	ToggleCVar(CVarAttackTraceViz);
#endif
}

void UOSCheatManager::ShowRotationDebug()
{
#if !UE_BUILD_SHIPPING
	GDebugRotationViz = !GDebugRotationViz;
#endif
}

void UOSCheatManager::ShowTargetingDebug()
{
#if !UE_BUILD_SHIPPING
	ToggleCVar(CVarTargetingViz);
#endif
}

void UOSCheatManager::ShowGrabDebug()
{
#if !UE_BUILD_SHIPPING
	GDebugGrabViz = !GDebugGrabViz;
#endif
}


void UOSCheatManager::ToggleDebugCameraFromUI()
{
	if (!GetWorld()) return;

	ULocalPlayer* LP = GetWorld()->GetGameInstance()
	   ? GetWorld()->GetGameInstance()->GetFirstGamePlayer()
	   : nullptr;
	if (!LP || !LP->PlayerController) return;

	APlayerController* CurrentController = LP->PlayerController;

	if (!Cast<ADebugCameraController>(CurrentController))
		CachedPlayerController = CurrentController;
	
	CurrentController->ConsoleCommand(TEXT("ToggleDebugCamera"), true);
	OnToggleDebugCamera(CurrentController);
}

void UOSCheatManager::TogglePunchingBag()
{
#if !UE_BUILD_SHIPPING
	ToggleCVar(CVarPunchingBag);

	AOSPlayerController* PC = Cast<AOSPlayerController>(GetOuterAPlayerController());
	if (!PC) return;

	if (GDebugPunchingBag)
	{
		if (!PC->GetPawn()) return;

		UClass* BagClass = AOSPunchingBag::StaticClass();

		// Spawn in the player's look direction, snapped to the ground plane.
		const APawn* PlayerPawn = PC->GetPawn();
		const FVector PlayerLoc = PlayerPawn->GetActorLocation();
		const FRotator PlayerRot = PC->GetControlRotation();
		const FVector LookDir2D = FVector(PlayerRot.Vector().X, PlayerRot.Vector().Y, 0.f).GetSafeNormal();
		const FVector TargetLoc = PlayerLoc + LookDir2D * 300.f;

		// Read capsule half-height from the CDO (not hardcoded).
		const float CapsuleHalfHeight = BagClass->GetDefaultObject<ACharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		// Line trace downward to find ground (WorldStatic hits floors reliably).
		FVector SpawnLoc = FVector(TargetLoc.X, TargetLoc.Y, PlayerLoc.Z);
		FHitResult Hit;
		const FVector TraceStart = FVector(TargetLoc.X, TargetLoc.Y, PlayerLoc.Z + 500.f);
		const FVector TraceEnd = FVector(TargetLoc.X, TargetLoc.Y, PlayerLoc.Z - 1000.f);
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(PunchingBagSpawn), false, PlayerPawn);
		if (PC->GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
		{
			// Place feet on ground: offset up by capsule half-height from hit point.
			SpawnLoc = Hit.Location + FVector(0.f, 0.f, CapsuleHalfHeight);
		}

		// Face the player (yaw only — stand upright).
		const FRotator SpawnRot = FRotator(0.f, (PlayerLoc - SpawnLoc).Rotation().Yaw, 0.f);

		// Use server RPC
		PC->Server_TogglePunchingBag(true, SpawnLoc, SpawnRot, bPunchingBagEnemyTeam);
	}
	else
	{
		PC->Server_TogglePunchingBag(false, FVector::ZeroVector, FRotator::ZeroRotator, false);

		// Reset bag cheat bools and stop loop.
		StopBagActionLoop();
		bBagNoHealthDamage = false;
		bBagNoStaminaDrain = false;
		bBagBlockHealthRegen = false;
		bBagBlockStaminaRegen = false;
		bBagHitReactRetrigger = false;
	}
#endif
}

void UOSCheatManager::TogglePunchingBagBlock()
{
#if !UE_BUILD_SHIPPING
	AOSPlayerController* PC = Cast<AOSPlayerController>(GetOuterAPlayerController());
	if (!PC) return;

	// Derive "on/off" from the bag's actual IsBlocking tag instead of a cached UI bool.
	// When guard break or stamina break cancels the ability externally, the tag drops,
	// this check sees bag-not-blocking, and a press starts a fresh block.
	const FOSGameplayTags& OSTags = FOSGameplayTags::Get();
	UOSAbilitySystemComponent* ASC = GetBagASC();
	const bool bIsCurrentlyBlocking = ASC && ASC->HasMatchingGameplayTag(OSTags.IsBlocking);

	if (bIsCurrentlyBlocking)
	{
		PC->Server_PunchingBagCancelAbility(OSTags.Ability_Block);
	}
	else
	{
		PC->Server_PunchingBagAction(OSTags.Ability_Block);
	}
#endif
}

void UOSCheatManager::ToggleBagNoHealthDamage()
{
#if !UE_BUILD_SHIPPING
	bBagNoHealthDamage = !bBagNoHealthDamage;
	SetDebugEffect(GetBagASC(), UGE_OSDebugNoHealthDamage::StaticClass(), bBagNoHealthDamage);
#endif
}

void UOSCheatManager::ToggleBagNoStaminaDrain()
{
#if !UE_BUILD_SHIPPING
	bBagNoStaminaDrain = !bBagNoStaminaDrain;
	SetDebugEffect(GetBagASC(), UGE_OSDebugNoStaminaDrain::StaticClass(), bBagNoStaminaDrain);
#endif
}

void UOSCheatManager::ToggleBagBlockHealthRegen()
{
#if !UE_BUILD_SHIPPING
	bBagBlockHealthRegen = !bBagBlockHealthRegen;
	SetDebugEffect(GetBagASC(), UGE_OSDebugBlockHealthRegen::StaticClass(), bBagBlockHealthRegen);
#endif
}

void UOSCheatManager::ToggleBagBlockStaminaRegen()
{
#if !UE_BUILD_SHIPPING
	bBagBlockStaminaRegen = !bBagBlockStaminaRegen;
	SetDebugEffect(GetBagASC(), UGE_OSDebugBlockStaminaRegen::StaticClass(), bBagBlockStaminaRegen);
#endif
}

void UOSCheatManager::ToggleBagHitReactRetrigger()
{
#if !UE_BUILD_SHIPPING
	bBagHitReactRetrigger = !bBagHitReactRetrigger;
	SetDebugEffect(GetBagASC(), UGE_OSDebugStunLock::StaticClass(), bBagHitReactRetrigger);
#endif
}

void UOSCheatManager::BagLightAttack()
{
#if !UE_BUILD_SHIPPING
	BagAction(FOSGameplayTags::Get().Attack_Light);
#endif
}

void UOSCheatManager::BagHeavyAttack()
{
#if !UE_BUILD_SHIPPING
	BagAction(FOSGameplayTags::Get().Attack_Heavy);
#endif
}

void UOSCheatManager::BagDodge()
{
#if !UE_BUILD_SHIPPING
	BagAction(FOSGameplayTags::Get().Ability_Dodge);
#endif
}

void UOSCheatManager::ShowHostMenu()
{
#if !UE_BUILD_SHIPPING
	if (HostMenuWidget && HostMenuWidget->IsInViewport())
	{
		HostMenuWidget->RemoveFromParent();
		HostMenuWidget = nullptr;
		return;
	}

	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/OnSight/UI/WBP_HostMenuTemp.WBP_HostMenuTemp_C"));
	if (!WidgetClass)
	{
		UE_LOG(LogOSDebug, Warning, TEXT("ShowHostMenu: Failed to load WBP_HostMenuTemp"));
		return;
	}

	APlayerController* PC = GetOuterAPlayerController();
	if (!PC)
		return;

	HostMenuWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (HostMenuWidget)
		HostMenuWidget->AddToViewport(10);
#endif
}

void UOSCheatManager::ShowCommonHostMenu()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogOSDebug, Log, TEXT("ShowCommonHostMenu: invoked"));

	if (HostMenuWidget && HostMenuWidget->IsInViewport())
	{
		UE_LOG(LogOSDebug, Log, TEXT("ShowCommonHostMenu: tearing down existing host menu"));
		if (UCommonActivatableWidget* Active = Cast<UCommonActivatableWidget>(HostMenuWidget))
			Active->DeactivateWidget();
		HostMenuWidget->RemoveFromParent();
		HostMenuWidget = nullptr;
		return;
	}

	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/OnSight/UI/W_HostMenu.W_HostMenu_C"));
	if (!WidgetClass)
	{
		UE_LOG(LogOSDebug, Warning, TEXT("ShowCommonHostMenu: Failed to load W_HostMenu"));
		return;
	}

	APlayerController* PC = GetOuterAPlayerController();
	if (!PC)
	{
		UE_LOG(LogOSDebug, Warning, TEXT("ShowCommonHostMenu: No owning PlayerController"));
		return;
	}

	HostMenuWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (!HostMenuWidget)
	{
		UE_LOG(LogOSDebug, Warning, TEXT("ShowCommonHostMenu: CreateWidget returned null"));
		return;
	}

	HostMenuWidget->AddToViewport(10);
	if (UCommonActivatableWidget* Active = Cast<UCommonActivatableWidget>(HostMenuWidget))
	{
		UE_LOG(LogOSDebug, Log, TEXT("ShowCommonHostMenu: activating CommonActivatableWidget"));
		Active->ActivateWidget();
	}
#endif
}

void UOSCheatManager::SwapCharacter(int32 Index)
{
#if !UE_BUILD_SHIPPING
	if (AOSPlayerController* PC = Cast<AOSPlayerController>(GetOuterAPlayerController()))
	{
		PC->Server_SwapCharacter(Index);
		UE_LOG(LogOSDebug, Log, TEXT("Requested swap to character %d (via Server RPC)"), Index);
	}
#endif
}

void UOSCheatManager::ReapplyDebugState()
{
#if !UE_BUILD_SHIPPING
	AOSPlayerController* PC = Cast<AOSPlayerController>(GetOuterAPlayerController());
	if (!PC) return;

	// Route through Server RPC so both client and server ASC stay in sync.
	PC->Server_ApplyDebugEffect(UGE_OSDebugNoHealthDamage::StaticClass(), bNoHealthDamage);
	PC->Server_ApplyDebugEffect(UGE_OSDebugNoStaminaDrain::StaticClass(), bNoStaminaDrain);
	PC->Server_ApplyDebugEffect(UGE_OSDebugBlockHealthRegen::StaticClass(), bBlockHealthRegen);
	PC->Server_ApplyDebugEffect(UGE_OSDebugBlockStaminaRegen::StaticClass(), bBlockStaminaRegen);
	PC->Server_ApplyDebugEffect(UGE_OSDebugStunLock::StaticClass(), bHitReactRetrigger);
#endif
}

// ------------------------------------------------------------------
// Domination Cheats
// ------------------------------------------------------------------

void UOSCheatManager::DomForceCapture(FString PointName, int32 TeamIndex)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World) return;
	auto* GS = World->GetGameState<AOSGameState_Domination>();
	if (!GS) { UE_LOG(LogOSDomination, Warning, TEXT("[CHEAT] No AOSGameState_Domination")); return; }

	for (AOSControlPoint* Point : GS->ControlPoints)
	{
		if (!IsValid(Point)) continue;
		if (!PointName.IsEmpty() && Point->GetPointName() != PointName) continue;

		Point->ForceCapture(TeamIndex);
	}
#endif
}

void UOSCheatManager::DomSetTeamScore(int32 TeamIndex, int32 Score)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World) return;
	auto* GS = World->GetGameState<AOSGameState_Domination>();
	if (!GS) { UE_LOG(LogOSDomination, Warning, TEXT("[CHEAT] No AOSGameState_Domination")); return; }

	// Reset to 0 then add desired amount
	if (GS->TeamObjectiveScores.IsValidIndex(TeamIndex))
	{
		const int32 Current = GS->GetTeamScore(TeamIndex);
		GS->AddTeamScore(TeamIndex, Score - Current);
		UE_LOG(LogOSDomination, Log, TEXT("[CHEAT] SetTeamScore Team=%d Score=%d"), TeamIndex, Score);
	}
#endif
}

void UOSCheatManager::DomForceWin(int32 TeamIndex)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World) return;
	auto* GM = World->GetAuthGameMode<AOSGameMode_Domination>();
	if (!GM) { UE_LOG(LogOSDomination, Warning, TEXT("[CHEAT] No AOSGameMode_Domination")); return; }

	GM->EndDominationMatch(TeamIndex);
	UE_LOG(LogOSDomination, Log, TEXT("[CHEAT] ForceWin Team=%d"), TeamIndex);
#endif
}

void UOSCheatManager::DomResetPoint(FString PointName)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World) return;
	auto* GS = World->GetGameState<AOSGameState_Domination>();
	if (!GS) { UE_LOG(LogOSDomination, Warning, TEXT("[CHEAT] No AOSGameState_Domination")); return; }

	for (AOSControlPoint* Point : GS->ControlPoints)
	{
		if (!IsValid(Point)) continue;
		if (!PointName.IsEmpty() && Point->GetPointName() != PointName) continue;

		Point->ResetToNeutral();
	}
#endif
}

void UOSCheatManager::DomSetScoreLimit(int32 NewLimit)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World) return;
	auto* GS = World->GetGameState<AOSGameState_Domination>();
	if (!GS) { UE_LOG(LogOSDomination, Warning, TEXT("[CHEAT] No AOSGameState_Domination")); return; }

	GS->DomScoreLimit = NewLimit;
	GS->ForceNetUpdate();

	// Also update GM's ScoreLimit so CheckDomWinCondition uses the new value
	if (auto* GM = World->GetAuthGameMode<AOSGameMode_Domination>())
		GM->SetScoreLimit(NewLimit);

	UE_LOG(LogOSDomination, Log, TEXT("[CHEAT] SetScoreLimit=%d"), NewLimit);
#endif
}

void UOSCheatManager::DomStatus()
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World) return;
	auto* GS = World->GetGameState<AOSGameState_Domination>();
	if (!GS) { UE_LOG(LogOSDomination, Warning, TEXT("[CHEAT] No AOSGameState_Domination")); return; }

	UE_LOG(LogOSDomination, Log, TEXT("=== DOMINATION STATUS ==="));
	UE_LOG(LogOSDomination, Log, TEXT("ScoreLimit=%d | Team0=%d | Team1=%d | MatchOver=%d"),
		GS->DomScoreLimit, GS->GetTeamScore(0), GS->GetTeamScore(1), GS->IsMatchOver());

	for (AOSControlPoint* Point : GS->ControlPoints)
	{
		if (!IsValid(Point)) continue;
		UE_LOG(LogOSDomination, Log, TEXT("  %s: State=%d Owner=%d Cap=%d Progress=%d/%d Contested=%d"),
			*Point->GetPointName(),
			(int32)Point->GetControlPointState(),
			Point->GetOwningTeamIndex(),
			Point->GetCapturingTeamIndex(),
			Point->GetCaptureProgress(),
			Point->GetMaxCap(),
			Point->IsContested());
	}
	UE_LOG(LogOSDomination, Log, TEXT("========================="));
#endif
}
