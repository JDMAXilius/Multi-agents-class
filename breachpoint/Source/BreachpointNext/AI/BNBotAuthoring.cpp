#include "AI/BNBotAuthoring.h"

#if WITH_EDITOR

#include "AI/BNBotBrain.h"
#include "AI/BNBotController.h"
#include "AI/BNBotStateTreeTasks.h"
#include "BreachpointNext.h"
#include "Data/BNDataRows.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "Engine/DataTable.h"
#include "Factories/DataTableFactory.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeEditorData.h"
#include "StateTreeFactory.h"
#include "StateTreeState.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	// The two paths DefaultGame.ini already names. They are the contract: an asset built anywhere
	// else resolves null at runtime and every bot stands still, so these are literals here rather
	// than parameters — a caller must not be able to build the tree into the wrong package.
	const TCHAR* const BotStateTreePackage = TEXT("/Game/BN/AI/ST_BNBot");
	const TCHAR* const BotStateTreeName    = TEXT("ST_BNBot");
	const TCHAR* const AmbitionsPackage    = TEXT("/Game/BN/Data/DT_BNBotAmbitions");
	const TCHAR* const AmbitionsName       = TEXT("DT_BNBotAmbitions");

	/** Finds the asset at PackagePath, or creates it with Factory. Returning the EXISTING object
	 *  is what makes a rebuild idempotent AND keeps every reference to it alive — deleting and
	 *  re-creating would null the ini's soft path resolution until the editor restarted. */
	UObject* FindOrCreateAsset(const TCHAR* PackagePath, const TCHAR* AssetName, UClass* AssetClass, UFactory* Factory, FString& OutHow)
	{
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), PackagePath, AssetName);
		if (UObject* Existing = StaticLoadObject(AssetClass, nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			OutHow = TEXT("reused existing");
			return Existing;
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		const FString PackageFolder = FPackageName::GetLongPackagePath(PackagePath);
		UObject* Created = AssetTools.CreateAsset(AssetName, PackageFolder, AssetClass, Factory);
		OutHow = Created ? TEXT("created") : TEXT("FAILED to create");
		return Created;
	}

	bool SaveAsset(UObject* Asset, FString& OutError)
	{
		if (!Asset)
		{
			OutError = TEXT("null asset");
			return false;
		}

		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Asset);

		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		const FSavePackageResultStruct Result = UPackage::Save(Package, Asset, *FileName, SaveArgs);
		if (Result.Result != ESavePackageResult::Success)
		{
			OutError = FString::Printf(TEXT("UPackage::Save returned %d for %s"), static_cast<int32>(Result.Result), *FileName);
			return false;
		}
		return true;
	}

	/** Every completion transition in this tree lands back on Root, never on the state it came
	 *  from. A GotoState aimed at Engage whose enter condition has since gone false does not fall
	 *  through to Roam — it simply fails to select, and the bot keeps running a state whose reason
	 *  has expired. Landing on Root re-runs the whole selector, which is the only place the
	 *  Engage-or-Roam decision is actually made. */
	void AddCompletionTransition(UStateTreeState& From, UStateTreeState& Root, EStateTreeTransitionTrigger Trigger, float DelaySeconds)
	{
		FStateTreeTransition& Transition = From.AddTransition(Trigger, EStateTreeTransitionType::GotoState, &Root);
		if (DelaySeconds > 0.f)
		{
			// NOT a stylistic pause. A frozen bot that still sees a target fails FireBurst
			// instantly; a level with no points fails Roam instantly. Without the delay the
			// selector re-enters every frame and issues a pathfind per bot per frame, forever.
			Transition.bDelayTransition = true;
			Transition.DelayDuration = DelaySeconds;
		}
	}
}

FString UBNBotAuthoring::BuildBotStateTree()
{
	TArray<FString> Report;

	UStateTreeFactory* Factory = NewObject<UStateTreeFactory>();
	// The schema decides which nodes the tree may contain and what context it publishes. The AI
	// one guarantees an AIController context — which is also the object every BN node falls back
	// to when its Controller property is unbound, and bindings are the one part of a StateTree
	// that genuinely has no code-side authoring API.
	Factory->SetSchemaClass(UStateTreeAIComponentSchema::StaticClass());

	FString How;
	UStateTree* Tree = Cast<UStateTree>(FindOrCreateAsset(BotStateTreePackage, BotStateTreeName, UStateTree::StaticClass(), Factory, How));
	if (!Tree)
	{
		return FString::Printf(TEXT("FAIL: could not create %s.%s"), BotStateTreePackage, BotStateTreeName);
	}
	Report.Add(FString::Printf(TEXT("asset      : %s (%s)"), *Tree->GetPathName(), *How));

	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(Tree->EditorData);
	if (!EditorData)
	{
		// A reused asset created by an older or other path may carry no editor data at all.
		EditorData = NewObject<UStateTreeEditorData>(Tree, FName(), RF_Transactional);
		Tree->EditorData = EditorData;
		Report.Add(TEXT("editordata : created fresh (asset had none)"));
	}

	if (!EditorData->Schema || EditorData->Schema->GetClass() != UStateTreeAIComponentSchema::StaticClass())
	{
		EditorData->Schema = NewObject<UStateTreeAIComponentSchema>(EditorData);
		Report.Add(TEXT("schema     : set to StateTreeAIComponentSchema"));
	}

	// Idempotent rebuild: the states are rewritten from scratch every run, so this function is
	// the single source of truth for the graph and a second run converges rather than duplicates.
	EditorData->SubTrees.Reset();

	UStateTreeState& Root = EditorData->AddRootState();

	// ---- Engage: there is someone to kill -------------------------------------------------
	UStateTreeState& Engage = Root.AddChildState(TEXT("Engage"));
	Engage.AddEnterCondition<FBNHasTargetCondition>();

	// Children are tried IN ORDER, and that order is the bot's combat priority: fix the gun
	// first, then shoot if it can see, else close the distance. Each child is one job, because a
	// StateTree runs all of a state's tasks at once — stacking reload and fire on one state would
	// run them simultaneously, not in sequence.

	// 1. Magazine low and a reserve to fill it from: reload, facing the target throughout.
	UStateTreeState& Rearm = Engage.AddChildState(TEXT("Rearm"));
	Rearm.AddEnterCondition<FBNNeedsReloadCondition>();
	Rearm.AddTask<FBNFaceTargetTask>();
	Rearm.AddTask<FBNReloadTask>();
	AddCompletionTransition(Rearm, Root, EStateTreeTransitionTrigger::OnStateSucceeded, 0.f);
	AddCompletionTransition(Rearm, Root, EStateTreeTransitionTrigger::OnStateFailed, 1.0f);

	// 2. Nothing to reload but nothing to fire either: change weapon. This state has no enter
	//    condition — BN Select Weapon succeeds immediately when the held weapon can already
	//    fight, and NextSelectableState is what carries that pass-through on to Shoot/Close
	//    instead of bouncing back to Root and re-selecting this same state forever.
	UStateTreeState& Arm = Engage.AddChildState(TEXT("Arm"));
	Arm.AddTask<FBNSelectWeaponTask>();
	Arm.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::NextSelectableState);
	// Failed means the whole loadout is dry. Two seconds before trying again — there is no pickup
	// system to fix it, so a tight retry would be a per-frame swap loop for the rest of the match.
	AddCompletionTransition(Arm, Root, EStateTreeTransitionTrigger::OnStateFailed, 2.0f);

	// 3. Get into a firing position. THIS COMES BEFORE SHOOTING, and the order is the whole
	//    lesson of the first PIE run: with Shoot ordered first, two bots stood at 2200uu and
	//    emptied magazine after magazine into a wall. Their eye-level line of sight to the target
	//    was genuinely clear — LineOfSightTo traces from the pawn's view point — while the
	//    weapon's own trace, from the muzzle along the control rotation, hit the geometry in
	//    between. "I can see it" and "I can shoot it" are different questions, and a bot that
	//    answers the first one and then pulls the trigger shoots masonry forever, because nothing
	//    in that loop ever asks it to move.
	//
	//    BN Move To Target succeeds only on "in range AND line of sight", so this state ENDS at a
	//    firing position rather than at a mere distance. Succeeding hands straight on to Shoot
	//    through NextSelectableState — the walk and the burst are one sequence, and being in
	//    position is decided in ONE place instead of being restated as a range check here.
	UStateTreeState& Close = Engage.AddChildState(TEXT("Close"));
	Close.AddTask<FBNFaceTargetTask>();
	Close.AddTask<FBNMoveToTargetTask>();
	Close.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::NextSelectableState);
	// Unreachable target: fail, and take the delay rather than re-pathing on the next frame.
	AddCompletionTransition(Close, Root, EStateTreeTransitionTrigger::OnStateFailed, 1.0f);

	// 4. In position: face it and fire a burst. The line-of-sight condition stays as a cheap
	//    guard — Close only hands over when sight is true, but the target can step behind cover
	//    in the frame between, and a burst that starts blind is exactly what step 3 is about.
	UStateTreeState& Shoot = Engage.AddChildState(TEXT("Shoot"));
	Shoot.AddEnterCondition<FBNHasLineOfSightCondition>();
	Shoot.AddTask<FBNFaceTargetTask>();
	Shoot.AddTask<FBNFireBurstTask>();
	AddCompletionTransition(Shoot, Root, EStateTreeTransitionTrigger::OnStateSucceeded, 0.f);
	AddCompletionTransition(Shoot, Root, EStateTreeTransitionTrigger::OnStateFailed, 1.0f);

	// ---- Roam: nothing better to want -----------------------------------------------------
	UStateTreeState& Roam = Root.AddChildState(TEXT("Roam"));
	Roam.AddTask<FBNMoveToPointOfInterestTask>();
	AddCompletionTransition(Roam, Root, EStateTreeTransitionTrigger::OnStateSucceeded, 0.f);
	// A level with no points fails this instantly; the delay is what keeps that cheap and quiet.
	AddCompletionTransition(Roam, Root, EStateTreeTransitionTrigger::OnStateFailed, 2.0f);

	Report.Add(TEXT("states     : Root > [Engage > [Rearm, Arm, Close, Shoot], Roam]"));

	// An uncompiled StateTree runs NOTHING — the asset would exist, the ini would resolve, and
	// every bot would still stand still. This is the step that turns editor data into bytecode.
	UStateTreeEditingSubsystem::ValidateStateTree(Tree);
	FStateTreeCompilerLog CompilerLog;
	const bool bCompiled = UStateTreeEditingSubsystem::CompileStateTree(Tree, CompilerLog);
	Report.Add(FString::Printf(TEXT("compile    : %s"), bCompiled ? TEXT("OK") : TEXT("FAILED")));
	if (!bCompiled)
	{
		CompilerLog.DumpToLog(LogBN);
		Report.Add(TEXT("             (compiler messages dumped to LogBN above)"));
	}

	FString SaveError;
	Report.Add(FString::Printf(TEXT("save       : %s"), SaveAsset(Tree, SaveError) ? TEXT("OK") : *SaveError));

	return FString::Join(Report, TEXT("\n"));
}

FString UBNBotAuthoring::BuildBotAmbitionsTable()
{
	TArray<FString> Report;

	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = FBNBotAmbitionRow::StaticStruct();

	FString How;
	UDataTable* Table = Cast<UDataTable>(FindOrCreateAsset(AmbitionsPackage, AmbitionsName, UDataTable::StaticClass(), Factory, How));
	if (!Table)
	{
		return FString::Printf(TEXT("FAIL: could not create %s.%s"), AmbitionsPackage, AmbitionsName);
	}
	Report.Add(FString::Printf(TEXT("asset      : %s (%s)"), *Table->GetPathName(), *How));

	Table->RowStruct = const_cast<UScriptStruct*>(FBNBotAmbitionRow::StaticStruct());
	Table->EmptyTable();

	// The rows MIRROR UBNBotBrain::DefaultRow rather than restating its numbers. The C++ defaults
	// are the fallback when this table is missing; a hand-typed copy here would be a second source
	// of truth that silently drifts from the first the day someone tunes one of them.
	const EBNBotAmbition Ambitions[] = { EBNBotAmbition::Fight, EBNBotAmbition::Survive, EBNBotAmbition::Roam };
	for (const EBNBotAmbition Ambition : Ambitions)
	{
		FBNBotAmbitionRow Row = UBNBotBrain::DefaultRow(Ambition);
		Table->AddRow(UBNBotBrain::AmbitionRowName(Ambition), Row);
	}
	Report.Add(FString::Printf(TEXT("rows       : %d, mirrored from UBNBotBrain::DefaultRow"), Table->GetRowMap().Num()));

	FString SaveError;
	Report.Add(FString::Printf(TEXT("save       : %s"), SaveAsset(Table, SaveError) ? TEXT("OK") : *SaveError));

	return FString::Join(Report, TEXT("\n"));
}

FString UBNBotAuthoring::BuildBotAssets()
{
	TArray<FString> Report;
	Report.Add(TEXT("=== BN bot assets: BUILD ==="));
	Report.Add(TEXT("-- DT_BNBotAmbitions --"));
	Report.Add(BuildBotAmbitionsTable());
	Report.Add(TEXT("-- ST_BNBot --"));
	Report.Add(BuildBotStateTree());
	Report.Add(TEXT("=== read-back ==="));
	Report.Add(AuditBotAssets());

	const FString Joined = FString::Join(Report, TEXT("\n"));
	UE_LOG(LogBN, Log, TEXT("%s"), *Joined);
	return Joined;
}

FString UBNBotAuthoring::AuditBotAssets()
{
	TArray<FString> Report;

	// The read-back is the deliverable, so it loads from the object path the INI names — not from
	// the pointer this process just built. A tree saved to the wrong package passes every check
	// that trusts its own handle, and fails the only one that matters.
	const FString TreePath = FString::Printf(TEXT("%s.%s"), BotStateTreePackage, BotStateTreeName);
	if (const UStateTree* Tree = LoadObject<UStateTree>(nullptr, *TreePath))
	{
		Report.Add(FString::Printf(TEXT("ST_BNBot   : FOUND at %s"), *Tree->GetPathName()));
		Report.Add(FString::Printf(TEXT("  schema   : %s"), *GetNameSafe(Tree->GetSchema())));
		Report.Add(FString::Printf(TEXT("  compiled : %s"), Tree->IsReadyToRun() ? TEXT("YES (ready to run)") : TEXT("NO - the tree will run nothing")));

		if (const UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(Tree->EditorData))
		{
			for (const UStateTreeState* SubTree : EditorData->SubTrees)
			{
				if (!SubTree)
				{
					continue;
				}
				Report.Add(FString::Printf(TEXT("  state    : %s (%d enter conditions, %d tasks, %d transitions)"),
					*SubTree->Name.ToString(), SubTree->EnterConditions.Num(), SubTree->Tasks.Num(), SubTree->Transitions.Num()));
				for (const UStateTreeState* Child : SubTree->Children)
				{
					if (!Child)
					{
						continue;
					}
					Report.Add(FString::Printf(TEXT("    +- %s (%d enter conditions, %d tasks, %d transitions)"),
						*Child->Name.ToString(), Child->EnterConditions.Num(), Child->Tasks.Num(), Child->Transitions.Num()));
					for (const UStateTreeState* GrandChild : Child->Children)
					{
						if (GrandChild)
						{
							Report.Add(FString::Printf(TEXT("       +- %s (%d enter conditions, %d tasks, %d transitions)"),
								*GrandChild->Name.ToString(), GrandChild->EnterConditions.Num(), GrandChild->Tasks.Num(), GrandChild->Transitions.Num()));
						}
					}
				}
			}
		}
	}
	else
	{
		Report.Add(FString::Printf(TEXT("ST_BNBot   : MISSING at %s - every bot will log 'failed to load' and stand still."), *TreePath));
	}

	const FString TablePath = FString::Printf(TEXT("%s.%s"), AmbitionsPackage, AmbitionsName);
	if (const UDataTable* Table = LoadObject<UDataTable>(nullptr, *TablePath))
	{
		Report.Add(FString::Printf(TEXT("DT_Ambitions: FOUND at %s (row struct %s)"), *Table->GetPathName(), *GetNameSafe(Table->RowStruct)));
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			const FBNBotAmbitionRow* Row = reinterpret_cast<const FBNBotAmbitionRow*>(Pair.Value);
			if (Row)
			{
				Report.Add(FString::Printf(TEXT("  %-8s base=%.2f health=%.2f target=%.2f dist=%.2f commit=%.1fs interruptBelow=%.2f"),
					*Pair.Key.ToString(), Row->BaseUtility, Row->HealthWeight, Row->TargetWeight,
					Row->DistanceWeight, Row->CommitSeconds, Row->InterruptBelowHealthNorm));
			}
		}
	}
	else
	{
		Report.Add(FString::Printf(TEXT("DT_Ambitions: MISSING at %s - UBNBotBrain::DefaultRow drives, after one warning."), *TablePath));
	}

	return FString::Join(Report, TEXT("\n"));
}

#else // !WITH_EDITOR

namespace
{
	const TCHAR* const EditorOnlyMessage = TEXT("UBNBotAuthoring is editor-only - these assets are built in the editor, never at runtime.");
}

FString UBNBotAuthoring::BuildBotStateTree()      { return EditorOnlyMessage; }
FString UBNBotAuthoring::BuildBotAmbitionsTable() { return EditorOnlyMessage; }
FString UBNBotAuthoring::BuildBotAssets()         { return EditorOnlyMessage; }
FString UBNBotAuthoring::AuditBotAssets()         { return EditorOnlyMessage; }

#endif // WITH_EDITOR
