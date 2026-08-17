#include "UI/Loading/BRLoadingScreenSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "MoviePlayer.h"
#include "TimerManager.h"
#include "UI/Loading/BRLoadingScreenSettings.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogBRLoadingScreen, Log, All);

const FName UBRLoadingScreenSubsystem::MapTravelReason(TEXT("BR.MapTravel"));
const FName UBRLoadingScreenSubsystem::MinimumDisplayReason(TEXT("BR.MinimumDisplay"));

UBRLoadingScreenSubsystem* UBRLoadingScreenSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UBRLoadingScreenSubsystem>() : nullptr;
}

bool UBRLoadingScreenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// No viewport, no movie player, nothing to draw. Creating it on a dedicated server would
	// give every one of the methods below a null-check that only ever fired in one configuration.
	return !IsRunningDedicatedServer();
}

void UBRLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UBRLoadingScreenSubsystem::HandlePreLoadMap);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UBRLoadingScreenSubsystem::HandlePostLoadMap);
}

void UBRLoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		FTimerManager& Timers = GameInstance->GetTimerManager();
		Timers.ClearTimer(MinimumDisplayTimer);
		Timers.ClearTimer(HoldTimeoutTimer);
	}

	// Tear the widget off the viewport explicitly. A game instance shutting down with a viewport
	// widget still attached leaks it into the next PIE session, where it draws over a game that
	// never asked for it.
	HideViewportWidget();
	OutstandingReasons.Reset();
	LoadingWidget = nullptr;

	Super::Deinitialize();
}

void UBRLoadingScreenSubsystem::HandlePreLoadMap(const FString& MapName)
{
	OutstandingReasons.Add(MapTravelReason);

	UUserWidget* Widget = CreateLoadingWidget();
	if (!Widget)
	{
		return;
	}

	// The movie player is the ONLY thing that renders while the game thread is blocked loading.
	// A UMG widget added to the viewport here would not draw a single frame until the load
	// finished, which is exactly the window we are trying to cover.
	FLoadingScreenAttributes Attributes;
	Attributes.WidgetLoadingScreen = Widget->TakeWidget();
	Attributes.MinimumLoadingScreenDisplayTime = UBRLoadingScreenSettings::Get().MinimumDisplayTimeSeconds;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bWaitForManualStop = false;

	GetMoviePlayer()->SetupLoadingScreen(Attributes);
}

void UBRLoadingScreenSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	const float MinimumDisplay = UBRLoadingScreenSettings::Get().MinimumDisplayTimeSeconds;

	// The movie player has stopped by now. Anything still owed -- the display floor, or a
	// gameplay hold placed before travel -- moves onto the viewport widget from here.
	if (MinimumDisplay > 0.0f)
	{
		OutstandingReasons.Add(MinimumDisplayReason);

		GameInstance->GetTimerManager().SetTimer(
			MinimumDisplayTimer, this, &UBRLoadingScreenSubsystem::HandleMinimumDisplayElapsed,
			MinimumDisplay, /* bLoop */ false);
	}

	OutstandingReasons.Remove(MapTravelReason);
	UpdateVisibility();
}

void UBRLoadingScreenSubsystem::AddLoadingReason(FName Reason)
{
	if (Reason.IsNone())
	{
		return;
	}

	// A TSet, so a caller that adds the same reason in a retry loop still escapes with one
	// removal. Reference counting here would make a leaked add unrecoverable rather than merely
	// wrong -- see the timeout note in the header.
	OutstandingReasons.Add(Reason);
	UpdateVisibility();
}

void UBRLoadingScreenSubsystem::RemoveLoadingReason(FName Reason)
{
	if (OutstandingReasons.Remove(Reason) > 0)
	{
		UpdateVisibility();
	}
}

void UBRLoadingScreenSubsystem::GetOutstandingReasons(TArray<FName>& OutReasons) const
{
	OutReasons = OutstandingReasons.Array();
}

UUserWidget* UBRLoadingScreenSubsystem::CreateLoadingWidget()
{
	if (LoadingWidget)
	{
		return LoadingWidget;
	}

	const TSoftClassPtr<UUserWidget>& SoftClass = UBRLoadingScreenSettings::Get().LoadingScreenWidgetClass;
	if (SoftClass.IsNull())
	{
		// No loading screen configured. Legal, and silent -- warning on every travel in a project
		// that has not authored one yet would be noise, not signal.
		return nullptr;
	}

	// Synchronous, deliberately. This runs at `PreLoadMap`, immediately before a blocking map
	// load: there is no frame to spread an async load across, and the thing we are about to do
	// is far more expensive than this.
	UClass* WidgetClass = SoftClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogBRLoadingScreen, Warning,
			TEXT("Loading screen widget class '%s' failed to load. No loading screen this travel."),
			*SoftClass.ToString());
		return nullptr;
	}

	LoadingWidget = CreateWidget<UUserWidget>(GetGameInstance(), WidgetClass);
	return LoadingWidget;
}

void UBRLoadingScreenSubsystem::ShowViewportWidget()
{
	if (ViewportWidget.IsValid())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!Viewport)
	{
		return;
	}

	UUserWidget* Widget = CreateLoadingWidget();
	if (!Widget)
	{
		return;
	}

	// Cached, because RemoveViewportWidgetContent must be handed the same shared ref that was
	// added. See the member's declaration for what goes wrong otherwise.
	ViewportWidget = Widget->TakeWidget();
	Viewport->AddViewportWidgetContent(ViewportWidget.ToSharedRef(), UBRLoadingScreenSettings::Get().ViewportZOrder);
}

void UBRLoadingScreenSubsystem::HideViewportWidget()
{
	if (!ViewportWidget.IsValid())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(ViewportWidget.ToSharedRef());
	}

	ViewportWidget.Reset();
}

void UBRLoadingScreenSubsystem::UpdateVisibility()
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	FTimerManager& Timers = GameInstance->GetTimerManager();

	if (OutstandingReasons.IsEmpty())
	{
		Timers.ClearTimer(HoldTimeoutTimer);
		HideViewportWidget();
		return;
	}

	ShowViewportWidget();

	// One timeout for the whole outstanding set, armed on the transition into "holding" and not
	// re-armed by every subsequent add. A per-reason timer would let a caller that adds a fresh
	// reason every second hold the screen forever without any single reason ever timing out.
	if (!Timers.IsTimerActive(HoldTimeoutTimer))
	{
		Timers.SetTimer(
			HoldTimeoutTimer, this, &UBRLoadingScreenSubsystem::HandleHoldTimeout,
			UBRLoadingScreenSettings::Get().HoldTimeoutSeconds, /* bLoop */ false);
	}
}

void UBRLoadingScreenSubsystem::HandleMinimumDisplayElapsed()
{
	RemoveLoadingReason(MinimumDisplayReason);
}

void UBRLoadingScreenSubsystem::HandleHoldTimeout()
{
	TArray<FName> Leaked = OutstandingReasons.Array();
	Leaked.Sort(FNameLexicalLess());

	FString Names;
	for (const FName& Reason : Leaked)
	{
		if (!Names.IsEmpty())
		{
			Names += TEXT(", ");
		}

		Names += Reason.ToString();
	}

	// Warning, not Verbose, and it names the reasons. This log line is the entire diagnostic for
	// a class of bug that otherwise presents as "the game hung on a loading screen".
	UE_LOG(LogBRLoadingScreen, Warning,
		TEXT("Loading screen held for %.1fs and was force-released. Outstanding reasons: [%s]. ")
		TEXT("Each of these is an AddLoadingReason whose RemoveLoadingReason never ran."),
		UBRLoadingScreenSettings::Get().HoldTimeoutSeconds, *Names);

	OutstandingReasons.Reset();
	UpdateVisibility();
}
