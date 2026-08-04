#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#include "UI/Settings/BRSettingsDataObject.h"
#include "UI/Settings/BRSettingsRegistry.h"
#include "UI/Settings/BRSettingsValueObjects.h"

/**
 * `Breachpoint.Sim.Settings` -- the settings data model, driven headlessly.
 *
 * IT BINDS TO LOCAL VARIABLES, NOT TO `UGameUserSettings`, and that is the point of the whole
 * accessor design. Every value test below owns a `TSharedRef<float>` or `TSharedRef<FName>` and
 * hands the leaf lambdas over it. Nothing here touches the engine's settings object, so:
 *   1. the suite runs with no viewport, no local player and no display;
 *   2. running it CANNOT corrupt the developer's own `GameUserSettings.ini`, which a test that
 *      wrote through to the real store absolutely would.
 *
 * The registry tests are the one place a real store may be present, so they assert on STRUCTURE
 * only -- tab identity, row composition, edit-state rules -- and never write a value.
 */
BEGIN_DEFINE_SPEC(FBRSettingsSpec, "Breachpoint.Sim.Settings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	/** Strong, because a spec member is not a UPROPERTY and a GC between Its would collect these. */
	TStrongObjectPtr<UBRSettingsValue_Scalar> Scalar;
	TStrongObjectPtr<UBRSettingsValue_Discrete> Discrete;
	TStrongObjectPtr<UBRSettingsCollection> Collection;
	TStrongObjectPtr<UBRSettingsRegistry> Registry;

	/** The stub backing stores the leaves read and write. */
	TSharedPtr<float> ScalarStore;
	TSharedPtr<FName> DiscreteStore;

	static constexpr float Tolerance = 0.001f;

	/** A scalar bound to `ScalarStore`, ranged 0..1 step 0.1. */
	void MakeBoundScalar(float InitialValue);

	/** A discrete bound to `DiscreteStore` over Low/Medium/High. */
	void MakeBoundDiscrete(FName InitialId);

END_DEFINE_SPEC(FBRSettingsSpec)

void FBRSettingsSpec::MakeBoundScalar(float InitialValue)
{
	ScalarStore = MakeShared<float>(InitialValue);

	Scalar.Reset(NewObject<UBRSettingsValue_Scalar>(GetTransientPackage()));
	Scalar->SetIdentity(FName("Test.Scalar"), FText::FromString(TEXT("Scalar")), FText::GetEmpty());
	Scalar->ConfigureRange(0.0f, 1.0f, 0.1f, EBRScalarDisplayFormat::Percent);

	TSharedPtr<float> Store = ScalarStore;
	Scalar->BindAccessor(
		[Store] { return *Store; },
		[Store](float V) { *Store = V; });
}

void FBRSettingsSpec::MakeBoundDiscrete(FName InitialId)
{
	DiscreteStore = MakeShared<FName>(InitialId);

	Discrete.Reset(NewObject<UBRSettingsValue_Discrete>(GetTransientPackage()));
	Discrete->SetIdentity(FName("Test.Discrete"), FText::FromString(TEXT("Discrete")), FText::GetEmpty());

	TArray<FBRSettingsOption> Options;
	Options.Emplace(FName("Low"), FText::FromString(TEXT("Low")));
	Options.Emplace(FName("Medium"), FText::FromString(TEXT("Medium")));
	Options.Emplace(FName("High"), FText::FromString(TEXT("High")));
	Discrete->SetOptions(MoveTemp(Options));

	TSharedPtr<FName> Store = DiscreteStore;
	Discrete->BindAccessor(
		[Store] { return *Store; },
		[Store](FName V) { *Store = V; });
}

void FBRSettingsSpec::Define()
{
	Describe("A scalar setting", [this]
	{
		BeforeEach([this] { MakeBoundScalar(0.5f); });

		It("reads its bound value", [this]
		{
			TestEqual(TEXT("value"), Scalar->GetValue(), 0.5f, Tolerance);
			TestTrue(TEXT("is bound"), Scalar->IsBound());
		});

		It("writes through to the backing store", [this]
		{
			Scalar->SetValue(0.75f);
			TestEqual(TEXT("store"), *ScalarStore, 0.75f, Tolerance);
		});

		It("clamps to the display range instead of storing out-of-range values", [this]
		{
			Scalar->SetValue(9.0f);
			TestEqual(TEXT("clamped high"), Scalar->GetValue(), 1.0f, Tolerance);

			Scalar->SetValue(-9.0f);
			TestEqual(TEXT("clamped low"), Scalar->GetValue(), 0.0f, Tolerance);
		});

		It("round-trips through the normalized form a slider binds to", [this]
		{
			Scalar->SetNormalizedValue(0.25f);
			TestEqual(TEXT("normalized"), Scalar->GetNormalizedValue(), 0.25f, Tolerance);
			TestEqual(TEXT("absolute"), Scalar->GetValue(), 0.25f, Tolerance);
		});

		It("steps by the configured step, clamped at the ends", [this]
		{
			Scalar->StepBy(2);
			TestEqual(TEXT("stepped up"), Scalar->GetValue(), 0.7f, Tolerance);

			Scalar->StepBy(-20);
			TestEqual(TEXT("stepped to floor"), Scalar->GetValue(), 0.0f, Tolerance);
		});

		It("shows a percent format as a whole number", [this]
		{
			Scalar->SetValue(0.7f);
			TestEqual(TEXT("display"), Scalar->GetDisplayValue().ToString(), FString(TEXT("70")));
		});

		It("is not dirty until it moves off its baseline", [this]
		{
			TestFalse(TEXT("clean at bind"), Scalar->IsDirty());

			Scalar->SetValue(0.9f);
			TestTrue(TEXT("dirty after change"), Scalar->IsDirty());

			Scalar->CaptureBaseline();
			TestFalse(TEXT("clean after re-baseline"), Scalar->IsDirty());
		});

		It("separates cancel from reset", [this]
		{
			// Baseline is 0.5 (bind time). Move, re-baseline at 0.8, move again, then check that
			// RestoreBaseline lands on 0.8 while ResetToDefault lands on the shipped 0.5.
			Scalar->SetValue(0.8f);
			Scalar->CaptureBaseline();
			Scalar->SetValue(0.2f);

			Scalar->RestoreBaseline();
			TestEqual(TEXT("cancel goes to baseline"), Scalar->GetValue(), 0.8f, Tolerance);

			Scalar->ResetToDefault();
			TestEqual(TEXT("reset goes to shipped default"), Scalar->GetValue(), 0.5f, Tolerance);
		});

		It("refuses writes while it is not editable", [this]
		{
			Scalar->SetEditState(EBRSettingsEditState::DependencyUnmet, FText::FromString(TEXT("because")));
			Scalar->SetValue(0.9f);

			TestEqual(TEXT("unchanged"), Scalar->GetValue(), 0.5f, Tolerance);
			TestFalse(TEXT("not editable"), Scalar->IsEditable());
		});
	});

	Describe("An unbound scalar", [this]
	{
		BeforeEach([this]
		{
			Scalar.Reset(NewObject<UBRSettingsValue_Scalar>(GetTransientPackage()));
			Scalar->ConfigureRange(0.0f, 1.0f, 0.1f, EBRScalarDisplayFormat::Raw);
		});

		It("reports unbound and shows the unknown dash rather than a fabricated zero", [this]
		{
			TestFalse(TEXT("not bound"), Scalar->IsBound());
			TestEqual(TEXT("display"), Scalar->GetDisplayValue().ToString(), FString(TEXT("—")));
			TestFalse(TEXT("never dirty"), Scalar->IsDirty());
		});

		It("survives a write attempt", [this]
		{
			Scalar->SetValue(0.5f);
			TestEqual(TEXT("still zero"), Scalar->GetValue(), 0.0f, Tolerance);
		});
	});

	Describe("A discrete setting", [this]
	{
		BeforeEach([this] { MakeBoundDiscrete(FName("Medium")); });

		It("resolves the selected index from the stored id", [this]
		{
			TestEqual(TEXT("index"), Discrete->GetSelectedIndex(), 1);
			TestEqual(TEXT("display"), Discrete->GetDisplayValue().ToString(), FString(TEXT("Medium")));
		});

		It("cycles forward and wraps", [this]
		{
			Discrete->CycleBy(1);
			TestEqual(TEXT("to High"), Discrete->GetSelectedId(), FName("High"));

			Discrete->CycleBy(1);
			TestEqual(TEXT("wraps to Low"), Discrete->GetSelectedId(), FName("Low"));
		});

		It("cycles backward and wraps", [this]
		{
			Discrete->CycleBy(-1);
			TestEqual(TEXT("to Low"), Discrete->GetSelectedId(), FName("Low"));

			Discrete->CycleBy(-1);
			TestEqual(TEXT("wraps to High"), Discrete->GetSelectedId(), FName("High"));
		});

		It("refuses an id that is not on the option list", [this]
		{
			Discrete->SetSelectedId(FName("Ultra"));
			TestEqual(TEXT("unchanged"), Discrete->GetSelectedId(), FName("Medium"));
		});

		It("reports an unrecognised stored value as unknown rather than snapping to option zero", [this]
		{
			// The exact scenario the id-based accessor exists to survive: a config written by a
			// build whose option list has since changed.
			*DiscreteStore = FName("Cinematic");

			TestEqual(TEXT("no index"), Discrete->GetSelectedIndex(), INDEX_NONE);
			TestEqual(TEXT("display"), Discrete->GetDisplayValue().ToString(), FString(TEXT("—")));
			TestEqual(TEXT("store untouched"), *DiscreteStore, FName("Cinematic"));
		});

		It("recovers from an unrecognised value onto the first option in either direction", [this]
		{
			*DiscreteStore = FName("Cinematic");
			Discrete->CycleBy(-1);
			TestEqual(TEXT("lands on first"), Discrete->GetSelectedId(), FName("Low"));
		});

		It("does not re-point the selection when the option list is replaced", [this]
		{
			TArray<FBRSettingsOption> Replacement;
			Replacement.Emplace(FName("Low"), FText::FromString(TEXT("Low")));
			Replacement.Emplace(FName("High"), FText::FromString(TEXT("High")));
			Discrete->SetOptions(MoveTemp(Replacement));

			// "Medium" is gone. The stored value must survive as-is, not be rewritten.
			TestEqual(TEXT("store untouched"), *DiscreteStore, FName("Medium"));
			TestEqual(TEXT("no index"), Discrete->GetSelectedIndex(), INDEX_NONE);
		});
	});

	Describe("A collection", [this]
	{
		BeforeEach([this]
		{
			Collection.Reset(NewObject<UBRSettingsCollection>(GetTransientPackage()));
			Collection->SetIdentity(FName("Root"), FText::FromString(TEXT("Root")), FText::GetEmpty());

			MakeBoundScalar(0.5f);
			MakeBoundDiscrete(FName("Low"));

			Collection->AddChild(Scalar.Get());
			Collection->AddChild(Discrete.Get());
		});

		It("flattens its children in order", [this]
		{
			TArray<UBRSettingsDataObject*> Rows;
			Collection->FlattenVisible(Rows);

			TestEqual(TEXT("row count"), Rows.Num(), 2);
			TestEqual(TEXT("first"), Rows[0], static_cast<UBRSettingsDataObject*>(Scalar.Get()));
		});

		It("drops Unsupported rows but keeps DependencyUnmet ones", [this]
		{
			Scalar->SetEditState(EBRSettingsEditState::Unsupported, FText::FromString(TEXT("no")));
			Discrete->SetEditState(EBRSettingsEditState::DependencyUnmet, FText::FromString(TEXT("later")));

			TArray<UBRSettingsDataObject*> Rows;
			Collection->FlattenVisible(Rows);

			TestEqual(TEXT("row count"), Rows.Num(), 1);
			TestEqual(TEXT("the kept one"), Rows[0], static_cast<UBRSettingsDataObject*>(Discrete.Get()));
		});

		It("is dirty when any child is", [this]
		{
			TestFalse(TEXT("clean"), Collection->IsDirty());

			Scalar->SetValue(0.9f);
			TestTrue(TEXT("dirty through child"), Collection->IsDirty());

			Collection->CaptureBaseline();
			TestFalse(TEXT("clean after re-baseline"), Collection->IsDirty());
		});

		It("shows no value of its own", [this]
		{
			TestEqual(TEXT("display"), Collection->GetDisplayValue().ToString(), FString(TEXT("—")));
		});
	});

	Describe("The registry, built with no local player", [this]
	{
		BeforeEach([this]
		{
			Registry.Reset(NewObject<UBRSettingsRegistry>(GetTransientPackage()));
			Registry->Build(nullptr);
		});

		It("builds every tab", [this]
		{
			TestTrue(TEXT("is built"), Registry->IsBuilt());
			TestNotNull(TEXT("Gameplay"), Registry->FindTab(UBRSettingsRegistry::TabGameplay));
			TestNotNull(TEXT("Video"), Registry->FindTab(UBRSettingsRegistry::TabVideo));
			TestNotNull(TEXT("Audio"), Registry->FindTab(UBRSettingsRegistry::TabAudio));
			TestNotNull(TEXT("Controls"), Registry->FindTab(UBRSettingsRegistry::TabControls));
		});

		It("produces rows for a tab, headers included", [this]
		{
			TArray<UBRSettingsDataObject*> Rows;
			Registry->GetRowsForTab(UBRSettingsRegistry::TabAudio, Rows);

			// One section header plus four volume leaves.
			TestEqual(TEXT("audio row count"), Rows.Num(), 5);
			TestTrue(TEXT("first row is the section header"), Rows[0]->IsA<UBRSettingsCollection>());
		});

		It("has an empty Controls tab when there is no Enhanced Input profile", [this]
		{
			// A legitimate state, not a failure: the tab exists so its absence never reads as a
			// build error, and it simply has nothing under it.
			TArray<UBRSettingsValue_KeyRemap*> Bindings;
			Registry->GetKeyRemapRows(Bindings);
			TestEqual(TEXT("no bindings"), Bindings.Num(), 0);
		});

		It("reports no unsaved changes on a freshly built tree", [this]
		{
			TestFalse(TEXT("clean"), Registry->HasUnsavedChanges());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
