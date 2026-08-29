#include "Execution/AIBTreeAuthoring.h"

#if WITH_EDITOR

#include "AIBotModule.h"
#include "Data/AIBDataRows.h"
#include "Data/AIBTiers.h"
#include "Execution/AIBStateTreeTasks.h"

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
	// The paths DefaultGame.ini names. They are the contract: an asset built anywhere else
	// resolves null at runtime and every bot stands still, so these are literals here rather
	// than parameters — a caller must not be able to build the tree into the wrong package.
	const TCHAR* const BotStateTreePackage = TEXT("/Game/AIBot/AI/ST_AIBBot");
	const TCHAR* const BotStateTreeName    = TEXT("ST_AIBBot");
	const TCHAR* const TiersPackage        = TEXT("/Game/AIBot/Data/DT_AIBTiers");
	const TCHAR* const TiersName           = TEXT("DT_AIBTiers");

	/** Finds the asset at PackagePath, or creates it with Factory. Returning the EXISTING
	 *  object is what makes a rebuild idempotent AND keeps every reference to it alive —
	 *  deleting and re-creating would null the ini's soft path resolution until the editor
	 *  restarted. */
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

	/** Every completion transition lands back on Root, never on a sibling: Root re-runs the
	 *  whole selector, which is the only place the which-branch decision is made — the
	 *  executor MIRRORS arbitration there. The delay is not stylistic: an instantly-failing
	 *  branch whose gate still holds (Seek with no provider, Roam with no navmesh) would
	 *  otherwise re-select every frame, and that is a pathfind per bot per frame, forever. */
	void AddCompletionTransition(UStateTreeState& From, UStateTreeState& Root, EStateTreeTransitionTrigger Trigger, float DelaySeconds)
	{
		FStateTreeTransition& Transition = From.AddTransition(Trigger, EStateTreeTransitionType::GotoState, &Root);
		if (DelaySeconds > 0.f)
		{
			Transition.bDelayTransition = true;
			Transition.DelayDuration = DelaySeconds;
		}
	}
}

FString UAIBTreeAuthoring::BuildBotStateTree()
{
	TArray<FString> Report;

	UStateTreeFactory* Factory = NewObject<UStateTreeFactory>();
	// The AI schema guarantees an AIController context — the object every AIB node falls
	// back to when its Controller property is unbound, and bindings are the one part of a
	// StateTree that genuinely has no code-side authoring API.
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

	// Idempotent rebuild: the states are rewritten from scratch every run, so this function
	// is the single source of truth for the graph and a second run converges.
	EditorData->SubTrees.Reset();

	UStateTreeState& Root = EditorData->AddRootState();

	// ONE FLAT BRANCH PER AMBITION, each entered only while the brain currently wants it
	// (the per-branch gate) and exited the moment it stops (the sentinel riding beside the
	// work). A state runs ALL its tasks at once — which is the design, not a compromise:
	// move + face + fire together is the Halo read, and every mover is written to keep
	// station rather than complete on arrival, so nothing thrashes the selector.

	// ---- Engage: the brain wants the fight ---------------------------------------------
	UStateTreeState& Engage = Root.AddChildState(TEXT("Engage"));
	Engage.AddEnterCondition<FAIBGateEngageCondition>();
	Engage.AddTask<FAIBAmbitionSentinelTask>();
	Engage.AddTask<FAIBFaceBeliefTask>();
	Engage.AddTask<FAIBMoveNearBeliefTask>();
	Engage.AddTask<FAIBFireWhenAbleTask>();
	// Phase 4's footwork rides BESIDE the burst (the host's R9 shape): the movement
	// policy decides the rhythm, the task steps laterally only while station-keeping,
	// and it never completes — the fight's other tasks own the state's fate.
	Engage.AddTask<FAIBStrafeTask>();
	AddCompletionTransition(Engage, Root, EStateTreeTransitionTrigger::OnStateSucceeded, 0.f);
	// Failure here is a lost belief (both belief tasks fail on visibility loss). The beat
	// before re-selecting is the human "wait — where'd he go", not a tuning accident.
	AddCompletionTransition(Engage, Root, EStateTreeTransitionTrigger::OnStateFailed, 0.2f);

	// ---- Retreat: the brain wants out --------------------------------------------------
	UStateTreeState& Retreat = Root.AddChildState(TEXT("Retreat"));
	Retreat.AddEnterCondition<FAIBGateRetreatCondition>();
	Retreat.AddTask<FAIBAmbitionSentinelTask>();
	auto& Flee = Retreat.AddTask<FAIBFleeFromBeliefTask>();
	// A FIGHTING retreat, not a rout. Retreat used to be flee-and-nothing-else: the bot
	// turned its back and jogged away without a shot, which is a free kill for whoever
	// broke its shield and reads as panic rather than discipline. Halo's spartans
	// backpedal firing. Both tasks run BESIDE the flee (a state runs all its tasks),
	// so the mover still owns where the body goes — this only decides where it looks
	// and whether the trigger is pressed on the way out.
	//
	// bRequireTarget=false is load-bearing: FaceBelief fails on lost visibility, and in
	// Retreat that would collapse the branch a hurt bot depends on the moment it breaks
	// line of sight — which is the exact moment a retreat is WORKING. FireWhenAble needs
	// no such flag; it already gates every press on visibility and fails only when the
	// avatar door closes.
	Retreat.AddTask<FAIBFaceBeliefTask>().GetInstanceData().bRequireTarget = false;

	// ONE number, read by three tasks below — the whole of the defend band's arbitration.
	constexpr float DEFEND_RANGE_UU = 700.f;
	// The ADS band must start where the flee mover STOPS SPRINTING, or the two fight: below
	// the defend floor the mover holds sprint, the host refuses ADS while sprinting, and the
	// task re-presses Aim every 0.75s forever with nothing to show for it (aib-critic M2 —
	// the same F7 futile-press shape the ADS band was originally shaped to avoid).
	Retreat.AddTask<FAIBFireWhenAbleTask>().GetInstanceData().AimRangeUU = DEFEND_RANGE_UU;

	// DEFEND MODE, not a rout (founder, 28 Aug: "make them defend mode where they crouch
	// jump, evade, fire — everything they do on attacking mode but in defence mode").
	//
	// Retreat now has two halves separated by ONE number. Closer than DEFEND_RANGE the flee
	// mover breaks contact; at or beyond it the flee mover stands down and this footwork
	// owns the legs, so the bot circles, jukes and shoots exactly as it does in Engage.
	// The two ranges MUST agree — two movers issuing in one tick cancel per tick, which is
	// why this is one constant read twice rather than two tuned numbers.
	//
	// The upper bound is deliberately far wider than Engage's 900: a defending bot should
	// keep evading for as long as it can SEE the threat, not stop dancing at the range
	// where an attacker would stop closing.
	Flee.GetInstanceData().DefendRangeUU = DEFEND_RANGE_UU;
	FAIBStrafeTaskInstanceData& RetreatFootwork = Retreat.AddTask<FAIBStrafeTask>().GetInstanceData();
	RetreatFootwork.MinRangeUU = DEFEND_RANGE_UU;
	// The chord's re-normalisation clamps out to StandOffMinUU, and leaving that at its 280
	// default let the defend footwork ratchet straight through its own floor and back into the
	// flee mover's half of the band (aib-critic H1). The stand-off IS the band floor here.
	RetreatFootwork.StandOffMinUU = DEFEND_RANGE_UU;
	// EngageFadeEndUU, not 3000: every tier's LoseSightRadius is 1500, so a 3000 gate could
	// never fire — and if it somehow did it would open a hole where NEITHER mover owns the
	// legs (flee stands down past 700, strafe holds past 3000). A dead number that hides a
	// stall is worse than a small one (aib-critic L1).
	RetreatFootwork.FightRangeUU = AIB::EngageFadeEndUU;
	// Reaching the flee goal while still wanting Retreat re-selects and flees further.
	AddCompletionTransition(Retreat, Root, EStateTreeTransitionTrigger::OnStateSucceeded, 0.f);
	AddCompletionTransition(Retreat, Root, EStateTreeTransitionTrigger::OnStateFailed, 0.5f);

	// ---- Search: a fresh memory of someone ---------------------------------------------
	// Halo's legibility lesson: walk to where they WERE, stand, sweep — reads as hunting.
	// The mover stands at the post (never completes on arrival); the branch ends when the
	// memory stales (mover fails), someone appears (mover succeeds), or the want moves on.
	UStateTreeState& Search = Root.AddChildState(TEXT("Search"));
	Search.AddEnterCondition<FAIBGateSearchCondition>();
	Search.AddTask<FAIBAmbitionSentinelTask>();
	Search.AddTask<FAIBMoveToLastKnownTask>();
	Search.AddTask<FAIBSweepLookTask>();
	AddCompletionTransition(Search, Root, EStateTreeTransitionTrigger::OnStateSucceeded, 0.f);
	AddCompletionTransition(Search, Root, EStateTreeTransitionTrigger::OnStateFailed, 0.5f);

	// ---- Seek: I have somewhere to be — go there ---------------------------------------
	// The node SET is unchanged from the SeekWeapon authoring on purpose: both structs
	// keep their names (the probe list pins them), and what a branch MEANS lives in C++
	// virtuals, never in serialized node parameters. So this replacement needs no new
	// node and, strictly, no asset rebuild — see the ticket Log.
	UStateTreeState& Seek = Root.AddChildState(TEXT("Seek"));
	Seek.AddEnterCondition<FAIBGateSeekCondition>(); // gates AIBot.Ambition.Seek
	Seek.AddTask<FAIBAmbitionSentinelTask>();
	Seek.AddTask<FAIBSeekDestinationTask>();               // belief -> POI -> reachable point
	AddCompletionTransition(Seek, Root, EStateTreeTransitionTrigger::OnStateSucceeded, 0.f);
	// The mover cannot fail for want of a destination now; this covers the only case left
	// (no navmesh at all), and the delay keeps that quiet while the gate still holds.
	AddCompletionTransition(Seek, Root, EStateTreeTransitionTrigger::OnStateFailed, 2.0f);

	// ---- Roam: nothing better to want --------------------------------------------------
	// GATED AGAIN (the Phase-3 W-REVIEW ruling; supersedes the interim ungated Roam that
	// unblocked the first live PIE). The engine's "must always be able to select a state"
	// demand is real and TERMINAL when violated — a failed selection sets
	// TreeRunStatus=Failed and Tick early-outs forever (seven bots, seven errors, nothing
	// moved) — but the answer to it is the dedicated Fallback below, not ungating a real
	// want: an ungated Roam silently swallows every unmapped ambition, so a Phase-6 mode
	// bot would roam past the flag forever while the ambition log printed the correct
	// want. With the gate, the executor mirrors arbitration 1:1 for every real ambition.
	// The t=0 cold-start miss is closed at its root by Think-before-StartLogic.
	UStateTreeState& Roam = Root.AddChildState(TEXT("Roam"));
	Roam.AddEnterCondition<FAIBGateRoamCondition>();
	Roam.AddTask<FAIBAmbitionSentinelTask>();
	Roam.AddTask<FAIBWanderTask>();
	// Arrived: re-select, draw a new reachable point. The small success delay exists for
	// the degenerate nav island whose every draw lands inside acceptance — instant
	// succeed would be a pathfind per bot per FRAME (W-REVIEW P3 M4). A world with no
	// navmesh fails instead — that delay keeps it quiet, the log says why.
	AddCompletionTransition(Roam, Root, EStateTreeTransitionTrigger::OnStateSucceeded, 0.25f);
	AddCompletionTransition(Roam, Root, EStateTreeTransitionTrigger::OnStateFailed, 2.0f);

	// ---- Mode: the game mode's want (Phase 6) ------------------------------------------
	// ONE branch serves every AIBot.Ambition.Mode.* want (the gate widens matching to
	// the hierarchy — the one place exact-mirror equality is deliberately relaxed,
	// because a host's want is a child tag exact == can never equal). The mover resolves
	// WHICH objective from the current ambition's kind join; SweepLook keeps the hold
	// readable as guarding, not standing.
	UStateTreeState& Mode = Root.AddChildState(TEXT("Mode"));
	Mode.AddEnterCondition<FAIBGateModeCondition>();
	Mode.AddTask<FAIBAmbitionSentinelTask>();
	Mode.AddTask<FAIBMoveToObjectiveTask>();
	Mode.AddTask<FAIBSweepLookTask>();
	AddCompletionTransition(Mode, Root, EStateTreeTransitionTrigger::OnStateSucceeded, 0.f);
	AddCompletionTransition(Mode, Root, EStateTreeTransitionTrigger::OnStateFailed, 1.0f);

	// ---- Fallback: the want maps to NO branch (the Phase-3 W-REVIEW ruling) ------------
	// UNGATED and LAST — the always-selectable state the compiler demands, which is what
	// lets every REAL ambition above keep its gate. Selected only when nothing above is:
	// an unserved want stands still and SAYS SO at Warning (F7); the sentinel ends it the
	// moment the want becomes servable. A healthy spawn never walks through here (the
	// controller scores once before the tree starts).
	UStateTreeState& Fallback = Root.AddChildState(TEXT("Fallback"));
	Fallback.AddTask<FAIBAmbitionSentinelTask>();
	Fallback.AddTask<FAIBUnservedWantTask>();
	AddCompletionTransition(Fallback, Root, EStateTreeTransitionTrigger::OnStateSucceeded, 0.f);
	AddCompletionTransition(Fallback, Root, EStateTreeTransitionTrigger::OnStateFailed, 1.0f);

	Report.Add(TEXT("states     : Root > [Engage, Retreat, Search, Seek, Roam, Mode, Fallback] (every ambition gated — Mode by hierarchy — the ungated Fallback floor last, sentinel in each)"));
	Report.Add(TEXT("seek       : AIBot.Ambition.Seek — deliberate movement (belief -> POI -> reachable point). SeekWeapon is RETIRED: no pickups in this game."));

	// An uncompiled StateTree runs NOTHING — the asset would exist, the ini would resolve,
	// and every bot would still stand still. This turns editor data into bytecode.
	UStateTreeEditingSubsystem::ValidateStateTree(Tree);
	FStateTreeCompilerLog CompilerLog;
	const bool bCompiled = UStateTreeEditingSubsystem::CompileStateTree(Tree, CompilerLog);
	Report.Add(FString::Printf(TEXT("compile    : %s"), bCompiled ? TEXT("OK") : TEXT("FAILED")));
	if (!bCompiled)
	{
		CompilerLog.DumpToLog(LogAIBot);
		Report.Add(TEXT("             (compiler messages dumped to LogAIBot above)"));
	}

	FString SaveError;
	Report.Add(FString::Printf(TEXT("save       : %s"), SaveAsset(Tree, SaveError) ? TEXT("OK") : *SaveError));

	return FString::Join(Report, TEXT("\n"));
}

FString UAIBTreeAuthoring::BuildTierTable()
{
	TArray<FString> Report;

	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = FAIBTierRow::StaticStruct();

	FString How;
	UDataTable* Table = Cast<UDataTable>(FindOrCreateAsset(TiersPackage, TiersName, UDataTable::StaticClass(), Factory, How));
	if (!Table)
	{
		return FString::Printf(TEXT("FAIL: could not create %s.%s"), TiersPackage, TiersName);
	}
	Report.Add(FString::Printf(TEXT("asset      : %s (%s)"), *Table->GetPathName(), *How));

	Table->RowStruct = const_cast<UScriptStruct*>(FAIBTierRow::StaticStruct());
	Table->EmptyTable();

	// PHASE 8: the four tiers, MIRRORED from the C++ registry rather than restated —
	// AIBTiers:: is the one source of truth (the controller resolves from it at
	// runtime; a missing table changes nothing), so this table is inspection surface,
	// never authority. Plus the Default row, which is the unknown-name fallback.
	Table->AddRow(FName(TEXT("Default")), FAIBTierRow());
	TArray<FName> TierNames;
	AIBTiers::GetTierNames(TierNames);
	TArray<FString> RowWarnings;
	for (const FName& TierName : TierNames)
	{
		if (const FAIBTierRow* Row = AIBTiers::Find(TierName))
		{
			Table->AddRow(TierName, *Row);
			RowWarnings.Append(AIBTiers::ValidateRow(TierName, *Row));
		}
	}
	Report.Add(FString::Printf(TEXT("rows       : %d, mirrored from the AIBTiers C++ registry"), Table->GetRowMap().Num()));
	for (const FString& Warning : RowWarnings)
	{
		Report.Add(FString::Printf(TEXT("row warning: %s"), *Warning));
	}

	FString SaveError;
	Report.Add(FString::Printf(TEXT("save       : %s"), SaveAsset(Table, SaveError) ? TEXT("OK") : *SaveError));

	return FString::Join(Report, TEXT("\n"));
}

FString UAIBTreeAuthoring::BuildBotAssets()
{
	TArray<FString> Report;
	Report.Add(TEXT("=== AIBot assets: BUILD ==="));
	Report.Add(TEXT("-- DT_AIBTiers --"));
	Report.Add(BuildTierTable());
	Report.Add(TEXT("-- ST_AIBBot --"));
	Report.Add(BuildBotStateTree());
	Report.Add(TEXT("=== read-back ==="));
	Report.Add(AuditBotAssets());

	const FString Joined = FString::Join(Report, TEXT("\n"));
	UE_LOG(LogAIBot, Log, TEXT("%s"), *Joined);
	return Joined;
}

FString UAIBTreeAuthoring::AuditBotAssets()
{
	TArray<FString> Report;

	// The read-back is the deliverable, so it loads from the object path the INI names —
	// not from the pointer this process just built. A tree saved to the wrong package
	// passes every check that trusts its own handle, and fails the only one that matters.
	const FString TreePath = FString::Printf(TEXT("%s.%s"), BotStateTreePackage, BotStateTreeName);
	if (const UStateTree* Tree = LoadObject<UStateTree>(nullptr, *TreePath))
	{
		Report.Add(FString::Printf(TEXT("ST_AIBBot  : FOUND at %s"), *Tree->GetPathName()));
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
					if (Child)
					{
						Report.Add(FString::Printf(TEXT("    +- %s (%d enter conditions, %d tasks, %d transitions)"),
							*Child->Name.ToString(), Child->EnterConditions.Num(), Child->Tasks.Num(), Child->Transitions.Num()));

						// THE TASK NAMES AND THE NUMBERS THAT ARBITRATE THEM. A task COUNT is not
						// proof the branch behaves: Retreat read "5 tasks" while its defend band
						// could still have been a default 0, which would leave the flee mover
						// running forever and look exactly like no change at all. This codebase
						// has already been bitten once by an authored tree serialising a stale
						// instance value (see FAIBStrafeTaskInstanceData::FightRangeUU's rename
						// comment), so the read-back prints the values rather than trusting them.
						for (const FStateTreeEditorNode& TaskNode : Child->Tasks)
						{
							const UScriptStruct* NodeType = TaskNode.Node.GetScriptStruct();
							FString Line = FString::Printf(TEXT("       . %s"),
								NodeType ? *NodeType->GetName() : TEXT("<empty>"));

							if (const FAIBStrafeTaskInstanceData* Strafe = TaskNode.Instance.GetPtr<FAIBStrafeTaskInstanceData>())
							{
								Line += FString::Printf(TEXT("  MinRangeUU=%.0f FightRangeUU=%.0f"),
									Strafe->MinRangeUU, Strafe->FightRangeUU);
							}
							else if (const FAIBFleeFromBeliefTaskInstanceData* Flee = TaskNode.Instance.GetPtr<FAIBFleeFromBeliefTaskInstanceData>())
							{
								Line += FString::Printf(TEXT("  DefendRangeUU=%.0f"), Flee->DefendRangeUU);
							}
							else if (const FAIBFaceBeliefTaskInstanceData* Face = TaskNode.Instance.GetPtr<FAIBFaceBeliefTaskInstanceData>())
							{
								Line += FString::Printf(TEXT("  bRequireTarget=%s"),
									Face->bRequireTarget ? TEXT("true") : TEXT("false"));
							}
							Report.Add(Line);
						}
					}
				}
			}
		}
	}
	else
	{
		Report.Add(FString::Printf(TEXT("ST_AIBBot  : MISSING at %s - every bot will log 'failed to load' and stand still."), *TreePath));
	}

	const FString TablePath = FString::Printf(TEXT("%s.%s"), TiersPackage, TiersName);
	if (const UDataTable* Table = LoadObject<UDataTable>(nullptr, *TablePath))
	{
		Report.Add(FString::Printf(TEXT("DT_AIBTiers: FOUND at %s (row struct %s, %d rows)"),
			*Table->GetPathName(), *GetNameSafe(Table->RowStruct), Table->GetRowMap().Num()));
	}
	else
	{
		Report.Add(FString::Printf(TEXT("DT_AIBTiers: MISSING at %s - FAIBTierRow C++ defaults drive."), *TablePath));
	}

	return FString::Join(Report, TEXT("\n"));
}

#else // !WITH_EDITOR

namespace
{
	const TCHAR* const EditorOnlyMessage = TEXT("UAIBTreeAuthoring is editor-only - these assets are built in the editor, never at runtime.");
}

FString UAIBTreeAuthoring::BuildBotStateTree() { return EditorOnlyMessage; }
FString UAIBTreeAuthoring::BuildTierTable()    { return EditorOnlyMessage; }
FString UAIBTreeAuthoring::BuildBotAssets()    { return EditorOnlyMessage; }
FString UAIBTreeAuthoring::AuditBotAssets()    { return EditorOnlyMessage; }

#endif // WITH_EDITOR
