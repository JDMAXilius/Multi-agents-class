# TICKET — BN12: two questions the engine has to answer before any traversal code is written

> STATUS: open — cut 25 Aug 2026 by the cloud lead, at the founder's direction. Needs the ENGINE
> ON DISK and nothing else — no live editor, no PIE. **This ticket writes no C++ and no ini.** It
> answers two questions and records the answers. That is the whole job.

Founder directive: bots must derive traversal — drop down, jump up, move platform to platform —
**from the navigation mesh itself**, with nothing placed in the level but the nav bounds volume
it already has. That approach is sound and it is how UE's own generator works. But it rests on
two engine facts that this repo cannot see: the cloud session has no engine, and
`dev.epicgames.com`, `docs.unrealengine.com` and `forums.unrealengine.com` are all blocked by its
egress proxy. Every API name below came from a **search summary**, which is exactly the source
that put three APIs into `BNDamageSpec` that had to be torn out again. The full research —
the mechanism, what Halo does, the plan these answers feed, and the complete unverified
inventory — is `docs/BREACHPOINT-NEXT-RESEARCH-TRAVERSAL.md`; read it first.

**So nothing gets written until these are transcribed from the headers on the machine that will
compile them.** Question 2 in particular can kill an entire approach, and it fails at PACKAGE
time rather than in PIE — which is the worst possible place to discover it.

## Kickoff (machine-checkable)

- requires: engine-installed
- `Tools/env.local` has a valid `ENGINE_ROOT` (`Tools/run-specs.sh` reads the same line)
- owner_path: `docs/tickets/TICKET_BN12_NAVLINK_PROBE.md`
  <!-- THIS FILE AND NOTHING ELSE. No Source/ edit, no Config/ edit, no Content/. If answering a
       question below seems to need one, that is a contract_gap: write it in the Log and STOP.
       A probe that starts implementing is no longer a probe, and its answers stop being
       trustworthy because they were shaped by what the implementer wanted to be true. -->

## QUESTION 1 — the built-in jump-down generator

UE 5.5+ generates navigation links from navmesh geometry during tile generation, configured by a
`Nav Link Jump Down Config` section on the navmesh. It is flagged **Experimental** through 5.8,
and there is an open community report that `Jump Max Depth` does not behave as documented — so
these are numbers to measure, never to trust.

Paste the **verbatim** answer to each. Struct definitions in full, not summarised.

1a. The config struct. Search the engine tree, do not assume a path:
```
grep -rn "NavLinkGenerationJumpDownConfig" "$ENGINE_ROOT/Engine/Source" --include=*.h
```
Paste the whole `USTRUCT` — **every** `UPROPERTY`, with its type, its name and its default.

1b. The navmesh property that turns it on, and the class that owns it:
```
grep -rn "JumpDownConfig\|bGenerateNavLinks\|GenerateNavLinks" "$ENGINE_ROOT/Engine/Source" --include=*.h
```

1c. **The ini section name, which is the crux of the C++-first path.** Find the `UCLASS(...)`
line for `ARecastNavMesh` and paste it. What is needed is whether it carries `config=`/
`defaultconfig` and which properties are `UPROPERTY(config)` — that is what decides whether
`Config/DefaultEngine.ini` can own these settings at all, or whether they are per-instance map
data. Report the section name in `[/Script/Module.Class]` form as it would be written.

1d. The proxy class to subclass:
```
grep -rn "GeneratedNavLinksProxy" "$ENGINE_ROOT/Engine/Source" --include=*.h
```
Is `UBaseGeneratedNavLinksProxy` a `UObject` or an `AActor`? Is it the class to derive from
directly, or is there a concrete subclass meant for that? What must a subclass override?

1e. Does it generate **upward** links, or downward only? Answer from the code and quote the lines
that settle it — the docs only ever describe jumping down, and the whole shape of the plan
depends on this being true.

## QUESTION 2 — can C++ read the navmesh border edges, in a SHIPPING target?

This is the question that can end an approach, so it gets answered before anything is built on it.

A navmesh edge with no neighbouring polygon is a ledge, by definition. Reading those edges is the
input to a link generator that derives everything from the mesh and places nothing in the level.
The route that appears to exist is `FRecastDebugGeometry` with `bGatherNavMeshEdges = true`, then
`ARecastNavMesh::GetDebugGeometry()` / `GetDebugGeometryForTile()`, reading `NavMeshEdges`.

Two things make that route doubtful, and both are load-bearing:

2a. **Is it exported?** There are reports of `LNK2019 unresolved external symbol` on
`GetDebugGeometry`. Find the declaration and paste it with its export macro and any
`#if WITH_EDITOR` / `#if !UE_BUILD_SHIPPING` / `ENABLE_DRAW_DEBUG` guard around it:
```
grep -rn "GetDebugGeometry" "$ENGINE_ROOT/Engine/Source" --include=*.h -B4 -A2
```
**State plainly whether a non-editor Game target can call this.** If it is editor-only or
debug-only, say so — that is a useful answer, not a failure, and it is the entire reason this
ticket exists.

2b. **Is there a better door?** If 2a says no, look for a supported way to walk polygons and
their neighbours — the candidates worth grepping are `GetPolyNeighbors`, `GetPolyEdges`,
`FNavigationPortalEdge`, `GetPolysInBox`, `BeginBatchQuery`. Paste what exists on
`ARecastNavMesh` with its export macro. Do not evaluate which is nicer; just report what is
callable.

2c. The registration side, needed either way. Confirm these exist and paste their declarations:
`ANavLinkProxy`, `UNavLinkCustomComponent`, `INavLinkCustomInterface`, and whichever
`UNavigationSystemV1` call registers a custom link. Also: **is there a delegate that fires when
navmesh generation finishes?** — that is when a generator would run.

## Three supporting facts, cheap while you are in there

- **Is this project's navmesh static or dynamic?** (`RuntimeGeneration` on the navmesh.) It
  decides whether rebuilding navigation writes into `BR_Arena01` and its external actor packages,
  or whether it is rebuilt at load and saved nowhere.
- **Does `BR_Arena01` actually have a navmesh?** The cloud clone stores every `.uasset` and
  `.umap` as a Git LFS pointer, so this could not be checked from there and **must not be assumed**
  — bots pathing today is strong evidence, not a read-back. `Tools/blockout/arena_plan.py` emits a
  `BR_NavBounds` actor, so the bounds volume should be there; confirm the `RecastNavMesh` is too.
- **Fall damage.** Does BN apply any damage on landing? The gantry lips sit at 8 m / 800 uu, and
  a drop rule that assumes survivability without checking is a bot that kills itself politely.

## Done when

- [x] Q1a–1e answered in the Log, structs quoted verbatim, 1e's verdict backed by quoted lines
- [x] Q1c states whether `DefaultEngine.ini` can own these settings, and under which section name
- [x] Q2a states plainly whether a Game (non-editor) target can call `GetDebugGeometry`
- [x] Q2b lists what else is callable on `ARecastNavMesh`, if 2a is no
- [x] Q2c confirms the registration classes and names the generation-finished delegate
- [x] The three supporting facts answered
- [x] **No file outside this one has changed** (`git status` pasted in the Log proves it)

## What this ticket does NOT do

It does not turn the generator on, write `UBNGeneratedNavLinks`, write `UBNNavLinkForge`, touch
`Config/`, or rebuild navigation. Those are BN13's, and BN13 cannot be written honestly until
this Log exists. If an answer here makes an approach impossible, **say so in the Log** — killing
a plan on an engine fact is this ticket succeeding, not failing.

## Log

_(terminal: the greps, verbatim, and the verdicts)_

### 25 Aug 2026 — terminal probe, transcribed from the engine on disk

Engine read: `/Users/Shared/Epic Games/UE_5.8/Engine/Source` (matches `Tools/env.local`
`ENGINE_ROOT=/Users/Shared/Epic Games/UE_5.8`). Every path below is relative to that
`Engine/Source` root. Nothing here comes from a search summary, a doc page, or memory.
No C++ written, no ini written, no asset touched.

**Headline, before the detail — the research doc's central premise is out of date.**
`FNavLinkGenerationJumpDownConfig` is **deprecated in 5.7**. UE 5.8 ships
`FNavLinkGenerationJumpConfig`, it lives in an **array** on the navmesh (many configs, not
one), and it generates **up as well as down**. The "downward only" claim — flagged in the
research as "the single most consequential unverified claim" — is **false in 5.8**. That
makes step 1 bigger and step 2 smaller than the plan assumed.

---

## QUESTION 1 — the built-in generator

### 1a. The config struct

`FNavLinkGenerationJumpDownConfig` **still exists but is deprecated**:

> `Runtime/NavigationSystem/Public/NavMesh/LinkGenerationConfig.h:159`
> ```cpp
> USTRUCT()
> struct UE_DEPRECATED(5.7, "Use FNavLinkGenerationJumpConfig instead.") FNavLinkGenerationJumpDownConfig
> ```
> and on the navmesh, `Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h:917-919`
> ```cpp
> UE_DEPRECATED(5.7, "Use the NavLinkJumpConfigs array instead.")
> UPROPERTY(config)
> FNavLinkGenerationJumpDownConfig NavLinkJumpDownConfig;
> ```
> — and that member sits inside `#if WITH_EDITORONLY_DATA` (RecastNavMesh.h:914-921). **Do
> not use it.** It is editor-only data on a deprecated path.

**The live struct**, verbatim and complete —
`Runtime/NavigationSystem/Public/NavMesh/LinkGenerationConfig.h:24-143`:

```cpp
/** Experimental configuration to generate vertical links. */
USTRUCT()
struct FNavLinkGenerationJumpConfig
{
	GENERATED_BODY()

	NAVIGATIONSYSTEM_API FNavLinkGenerationJumpConfig();

	// Note: We need to explicitly disable warnings on these constructors/operators for clang to be happy with deprecated variables
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	~FNavLinkGenerationJumpConfig() = default;
	FNavLinkGenerationJumpConfig(const FNavLinkGenerationJumpConfig&) = default;
	FNavLinkGenerationJumpConfig(FNavLinkGenerationJumpConfig&&) = default;
	FNavLinkGenerationJumpConfig& operator=(const FNavLinkGenerationJumpConfig&) = default;
	FNavLinkGenerationJumpConfig& operator=(FNavLinkGenerationJumpConfig&&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Should this config be used to generate links. */
	UPROPERTY(EditAnywhere, Config, Category = Settings)
	bool bEnabled = true;

	/** A name for this config */
	UPROPERTY(EditAnywhere, Config, Category = Settings)
	FName Name;
	
	/** Horizontal length of the jump.
	 * How far from the starting point we will look for ground. */ 
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(UIMin=0, ClampMin=0, UIMax=10000))
	float JumpLength = 150.f; 

	/** How far from the navmesh edge is the jump started. */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(UIMin=0, ClampMin=0))
	float JumpDistanceFromEdge = 10.f; 

	/** How far below the starting height we want to look for landing ground.
	 * A negative value can be used to generate a trajectory landing above the starting height
	 * (if negative, make sure to use a JumpHeight value big enough).
	 */
	UPROPERTY(EditAnywhere, Config, Category = Settings)
	float JumpMaxDepth = 150.f;

	/** Peak height relative to the height of the starting point. */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(UIMin=0, ClampMin=0))
	float JumpHeight = 50.f;

	/** Tolerance at both ends of the jump to find ground. */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(UIMin=0, ClampMin=0))
	float JumpEndsHeightTolerance = 80.f;

	/** Value multiplied by CellSize to find the distance between sampling trajectories. Default is 1.
     *  Larger values improve generation speed but might introduce sampling errors.  */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(UIMin=1, ClampMin=1))
	float SamplingSeparationFactor = 1.f;
	
	/** When filtering similar links, it's the distance used to compare between segment endpoints to match similar links.
	 * Use greater distance for more filtering (0 to deactivate filtering). */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta=(UIMin=0, ClampMin=0))
	float FilterDistanceThreshold = 80.f;

	/** Flags used to indicate how links will be added. */
	UPROPERTY(EditAnywhere, Config, Category = Settings, meta = (Bitmask, BitmaskEnum = "/Script/NavigationSystem.ENavLinkBuilderFlags"))
	uint16 LinkBuilderFlags = (uint16)ENavLinkBuilderFlags::CreateCenterPointLink;

#if WITH_EDITORONLY_DATA
	/** Area class for links generated by this configuration. */
	UE_DEPRECATED(5.6, "AreaClass is deprecated")
	UPROPERTY(Config, meta = (DeprecatedProperty, DeprecationMessage = "Use DownDirectionAreaClass and UpDirectionAreaClass instead."))
	TSubclassOf<UNavAreaBase> AreaClass_DEPRECATED;
#endif //WITH_EDITORONLY_DATA

	/**
	 * Area class for downward traversal of links generated by this configuration.
	 * @note If the value matches UpDirectionAreaClass, a single bidirectional link will be used to represent links generated by this configuration.
	 * If the value is null, no link will be generated in this direction
	 */
	UPROPERTY(EditAnywhere, Config, Category = Settings)
	TSubclassOf<UNavAreaBase> DownDirectionAreaClass;

	/**
	 * Area class for upward traversal of links generated by this configuration.
	 * @note If the value matches DownDirectionAreaClass, a single bidirectional link will be used to represent links generated by this configuration.
	 * If the value is null, no link will be generated in this direction
	 */
	UPROPERTY(EditAnywhere, Config, Category = Settings)
	TSubclassOf<UNavAreaBase> UpDirectionAreaClass;

	/** Class used to handle links made with this configuration.
	 * Using this allows to implement custom behaviors when using navlinks, for example during the pathfollow.
	 * Note that having a proxy is not required for successful navlink pathfinding,
	 * but it does allow for custom behavior at the start and the end of a given navlink.
	 * This implies that using LinkProxyClass is optional, and it can remain empty (the default value).
	 * @see INavLinkCustomInterface 
	 * @see UGeneratedNavLinksProxy
	 */
	UPROPERTY(EditAnywhere, Category= Settings)
	TSubclassOf<UBaseGeneratedNavLinksProxy> LinkProxyClass;

	/** Identifier used identify the current proxy handler. All links generated through this config will use the same handler. */
	UPROPERTY()
	FNavLinkId LinkProxyId;

	/** Current proxy. The proxy instance is build from the LinkProxyClass (provided it's not null).
	 * A proxy will be created if a @see LinkProxyClass is used.
	 */
	UPROPERTY(SkipSerialization)
	TObjectPtr<UBaseGeneratedNavLinksProxy> LinkProxy = nullptr;

	/** Is the link proxy registered to the navigation system CustomNavLinksMap.
	 * Registration occurs on PostRegisterAllComponents or on PostLoadPreRebuild if a new proxy was created. */
	UPROPERTY(SkipSerialization)
	bool bLinkProxyRegistered = false;

	/** Implemented for deprecated property cleanup purposes. */
	bool Serialize(FArchive& Ar);

#if WITH_RECAST	
	/** Copy configuration to dtNavLinkBuilderJumpConfig. */
	void CopyToDetourConfig(dtNavLinkBuilderJumpConfig& OutDetourConfig) const;
#endif //WITH_RECAST
};
```

Supporting enum, `LinkGenerationConfig.h:16-22`:

```cpp
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ENavLinkBuilderFlags : uint16
{
	CreateCenterPointLink		= 1 << 0,
	CreateExtremityLink			= 1 << 1,
};
```

Constructor defaults, `Runtime/NavigationSystem/Private/NavMesh/LinkGenerationConfig.cpp:70-74`:
```cpp
FNavLinkGenerationJumpConfig::FNavLinkGenerationJumpConfig()
{
	DownDirectionAreaClass = UNavArea_Default::StaticClass();
	UpDirectionAreaClass = UNavArea_Default::StaticClass();
}
```
**Both directions default to `UNavArea_Default`** — i.e. out of the box the generated link is
**bidirectional**, not one-way down.

Hard constraint from the Detour side,
`Runtime/Navmesh/Private/Detour/DetourNavLinkBuilderConfig.cpp:6-8`:
```cpp
void dtNavLinkBuilderJumpConfig::init()
{
	check(jumpHeight >= 0.f);
```
`JumpHeight` must be `>= 0` — a negative value is a `check()` failure, not a warning.
`JumpMaxDepth`, by contrast, carries **no `ClampMin`** in the live struct (the deprecated one
had `ClampMin=0`), and `cachedDownRatio = -jumpMaxDepth/jumpLength`
(`DetourNavLinkBuilderConfig.cpp:20`) is written to work with either sign.

### 1b. The navmesh properties that turn it on

`Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h:837-840` — **public** (the class has
no access specifier between `GENERATED_UCLASS_BODY()` at :573 and `protected:` at :907, and
the macro closes with `public:`):
```cpp
/** Experimental: if set, navlinks will be automatically generated.
 * @see FNavLinkGenerationJumpConfig */ 
UPROPERTY(EditAnywhere, Category=Generation, config)
uint32 bGenerateNavLinks : 1;
```
Default is **off** — `Runtime/NavigationSystem/Private/NavMesh/RecastNavMesh.cpp:523`:
`, bGenerateNavLinks(false)`.

`RecastNavMesh.h:923-925` — **`protected:`** (specifier at :907):
```cpp
/** Experimental configurations to generate jump links. */
UPROPERTY(EditAnywhere, Category=Generation, config)
TArray<FNavLinkGenerationJumpConfig> NavLinkJumpConfigs;
```
Public read-only accessor, `RecastNavMesh.h:1520`:
```cpp
const TArray<FNavLinkGenerationJumpConfig>& GetNavLinkJumpConfigs() const { return NavLinkJumpConfigs; }
```
(no `NAVIGATIONSYSTEM_API` needed — it is inline). **There is no public setter.** The array is
written by the config system or the details panel, not by game code.

There is also a global kill switch —
`Runtime/NavigationSystem/Private/NavMesh/RecastNavMesh.cpp:64-65, 774-777`:
```cpp
static bool bAllowLinkGeneration = true;
static FAutoConsoleVariableRef CVarAllowLinkGeneration(TEXT("ai.nav.AllowLinkGeneration"), bAllowLinkGeneration, TEXT("Set to false to force disabling link generation."), ECVF_Default);
...
bool ARecastNavMesh::IsGeneratingLinks() const
{
	return bGenerateNavLinks && UE::NavMesh::Private::bAllowLinkGeneration;
}
```
`ai.nav.AllowLinkGeneration 0` is a free A/B toggle for measurement.

**Runtime, not editor-only.** The whole generation path — `FRecastNavMeshGenerator::IsGeneratingLinks`
(`RecastNavMeshGenerator.cpp:5699`), `ResolveGeneratedLinkAreas` (:5704), the `dtNavLinkBuilder`
run (:4076), `AddGeneratedLinks` (:3876, called :4138) — sits inside **`#if WITH_RECAST` only**
(the block opens at `RecastNavMeshGenerator.cpp:21`). No `WITH_EDITOR`, no `!UE_BUILD_SHIPPING`.
`WITH_RECAST=1` whenever `Target.bCompileRecast` is true
(`Runtime/NavigationSystem/NavigationSystem.Build.cs:40-43`), and that defaults to `true`
(`Programs/UnrealBuildTool/Configuration/Rules/TargetRules.cs:1406-1407`).

### 1c. The ini section — **`DefaultEngine.ini` CAN own these settings**

`Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h:570-571`:
```cpp
UCLASS(config=Engine, defaultconfig, hidecategories=(Input,Rendering,Tags,Transformation,Actor,Layers,Replication), notplaceable, MinimalAPI)
class ARecastNavMesh : public ANavigationData
```

`config=Engine` + `defaultconfig`, and both `bGenerateNavLinks` (:839) and `NavLinkJumpConfigs`
(:924) are `UPROPERTY(..., config)`. So:

- **File:** `Config/DefaultEngine.ini`
- **Section:** `[/Script/NavigationSystem.RecastNavMesh]`

The research's guess was right, for the right reason. Array-of-struct config uses the `+` form,
e.g. `+NavLinkJumpConfigs=(bEnabled=True,Name="Drop",JumpLength=...)`. The `protected:` on the
array does not block this — the config loader writes through reflection.

**One field cannot come from ini: `LinkProxyClass`.** `LinkGenerationConfig.h:118-119` is
`UPROPERTY(EditAnywhere, Category= Settings)` with **no `Config` specifier**. Every other field
in the struct has `Config`; that one does not. Per its own comment (:110-117) the proxy is
**optional** — "having a proxy is not required for successful navlink pathfinding" — so step 1
can ship entirely from `DefaultEngine.ini` provided we do not need per-link callbacks. If we
ever do need `LinkProxyClass`, that is a contract_gap for BN13 to solve (details panel on the
navmesh actor, which Epic's own guidance says does not stick, or a C++ subclass of
`ARecastNavMesh`), not something ini can express.

### 1d. The proxy class

`Runtime/NavigationSystem/Public/BaseGeneratedNavLinksProxy.h:11-38` — it is a **`UObject`**,
not an `AActor`:
```cpp
/**
 * Experimental
 * Base class used to create generated navlinks proxy.
 * The proxy id is used to represent multiple links generated from the same configuration.
 */
UCLASS(Blueprintable, MinimalAPI)
class UBaseGeneratedNavLinksProxy : public UObject, public INavLinkCustomInterface
{
	GENERATED_UCLASS_BODY()
	
	// BEGIN INavLinkCustomInterface
	NAVIGATIONSYSTEM_API virtual void GetLinkData(FVector& LeftPt, FVector& RightPt, ENavLinkDirection::Type& Direction) const override;
	NAVIGATIONSYSTEM_API virtual FNavLinkId GetId() const override;
	NAVIGATIONSYSTEM_API virtual void UpdateLinkId(FNavLinkId ProxyId) override;
	NAVIGATIONSYSTEM_API virtual UObject* GetLinkOwner() const override;
	// END INavLinkCustomInterface

	void SetOwner(UObject* NewOwner) { Owner = NewOwner; }
	
protected:
	/** The LinkID will be the same for all navlinks using the proxy. */
	UPROPERTY(Transient)
	FNavLinkId LinkProxyId;

	/** Proxy owner. */
	UPROPERTY(Transient)
	TObjectPtr<UObject> Owner = nullptr;
};
```

There **is** a concrete subclass, and it lives in a different module —
`Runtime/AIModule/Classes/Navigation/GeneratedNavLinksProxy.h:11-40`:
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLinkReachedSignature, AActor*, MovingActor, const FVector, DestinationPoint);

/**
 * Experimental
 * Blueprintable class used to handle generated links as custom links.
 */
UCLASS(Blueprintable, BlueprintType, MinimalAPI)
class UGeneratedNavLinksProxy : public UBaseGeneratedNavLinksProxy
{
	GENERATED_UCLASS_BODY()

	virtual UWorld* GetWorld() const override;

	// BEGIN INavLinkCustomInterface
	AIMODULE_API virtual bool OnLinkMoveStarted(class UObject* PathComp, const FVector& DestPoint) override;
	// END INavLinkCustomInterface

	//////////////////////////////////////////////////////////////////////////
	// Blueprint interface for smart links
	
	/** Called when agent reaches smart link during path following. */
	UFUNCTION(BlueprintImplementableEvent)
	AIMODULE_API void ReceiveSmartLinkReached(AActor* Agent, const FVector Destination);

protected:
	void NotifySmartLinkReached(UObject* PathingAgent, const FVector DestPoint);
	
	UPROPERTY(BlueprintAssignable)
	FLinkReachedSignature OnSmartLinkReached;
};
```

Answers: it is a `UObject`. **Nothing is mandatory to override** — the base supplies working
`INavLinkCustomInterface` implementations for `GetLinkData` / `GetId` / `UpdateLinkId` /
`GetLinkOwner`, and the whole proxy is optional in the first place. If we want a callback when
a bot enters a generated link, derive from **`UGeneratedNavLinksProxy`** (AIModule, `GetWorld()`
already wired, `OnLinkMoveStarted` already overridden) and override
`INavLinkCustomInterface::OnLinkMoveStarted` / `OnLinkMoveFinished` — `AIModule` is already a
dependency of `BreachpointNext`. Deriving from `UBaseGeneratedNavLinksProxy` directly costs us
`GetWorld()`.

Related and worth knowing before BN13 plans anything clever:
`RecastNavMesh.h:1535-1550` declares `IsGeneratingLinks()`, `RegisterGeneratedLinksProxy()`,
`UnregisterGeneratedLinksProxy()` and `CreateAndRegisterJumpLinksProxy()` as **`protected` with
NO export macro**. A subclass of `ARecastNavMesh` in our module would fail to link against them
(this is the real shape of the LNK2019 class of problem, just on a different symbol).

### 1e. Up or down? — **BOTH, in 5.8. The research's "downward only" is wrong.**

Four independent lines settle it.

1. `JumpMaxDepth`'s own comment,
   `Runtime/NavigationSystem/Public/NavMesh/LinkGenerationConfig.h:58-63`:
   > ```cpp
   > /** How far below the starting height we want to look for landing ground.
   >  * A negative value can be used to generate a trajectory landing above the starting height
   >  * (if negative, make sure to use a JumpHeight value big enough).
   >  */
   > UPROPERTY(EditAnywhere, Config, Category = Settings)
   > float JumpMaxDepth = 150.f;
   > ```
   Note what is missing versus the deprecated struct at :186 — `meta=(UIMin=0, ClampMin=0)`.
   The clamp was **removed** so the value can go negative.

2. `UpDirectionAreaClass` exists as a first-class field
   (`LinkGenerationConfig.h:102-108`), and the constructor sets it to `UNavArea_Default`
   (`LinkGenerationConfig.cpp:73`), i.e. **on by default**.

3. The generator emits a distinct up-link.
   `Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp:3941-3968`:
   > ```cpp
   > // Both directions of the links share the same Left (owner) and Right (far) positions.
   > // They are distinguished by Direction: the "down" link uses LeftToRight, while the "up"
   > // link uses RightToLeft, which sets DT_OFFMESH_CON_REVERSED to swap traversal semantics.
   > // If ownership was swapped (bSwap), these directions are inverted.
   >
   > if (config.downDirArea != RECAST_NULL_AREA)
   > {
   > 	FGeneratedNavigationLink& DownLink = AddLinkSharedLambda(config);
   > 	...
   > }
   >
   > if (config.upDirArea != RECAST_NULL_AREA)
   > {
   > 	FGeneratedNavigationLink& UpLink = AddLinkSharedLambda(config);
   > 	UpLink.generatedLinkArea = config.upDirArea;
   > 	UpLink.generatedLinkPolyFlag = config.upDirPolyFlag;
   > 	UpLink.Left = OwnerPos;
   > 	UpLink.Right = FarPos;
   > 	UpLink.Direction = bDownIsRightToLeft
   > 		? ENavLinkDirection::LeftToRight
   > 		: ENavLinkDirection::RightToLeft;
   > }
   > ```

4. And when the two area classes match — the default — it does not even bother with two links,
   it emits one **bidirectional** one. `RecastNavMeshGenerator.cpp:3981` and the lambda comment
   at :3898:
   > ```cpp
   > // Used when we're provided with identical up and down areas. In this situation, a single bidirectional link is sufficient for our needs
   > ...
   > const bool bBidirectionalJumpDownLinks = jumpConfig.downDirArea == jumpConfig.upDirArea;
   > ```

**What this means concretely.** The sampler still seeds every trajectory from a navmesh
**border edge** (`dtNavLinkBuilder::initJumpRig` /`sampleEdge`,
`Runtime/Navmesh/Private/Detour/DetourNavLinkBuilder.cpp:670-758`) — that part of the research's
mechanism description is exactly right. What changed is that the parabola is no longer forced
downward, and the resulting off-mesh connection is two-way by default. A ledge-top edge with
`JumpMaxDepth=+800` gives the drop; a floor-level edge at a terrace base with
`JumpMaxDepth=-400` and a large enough `JumpHeight` gives the climb; and because it is an
**array** of configs, we can ship both in the same ini, each with its own budget and its own
`Name`.

**Caveat that must be measured, not assumed:** the generator has no idea what `UBNGA_Jump` can
actually do. It will happily emit an up-link the bot cannot physically make. `JumpHeight` and a
negative `JumpMaxDepth` are the only budget, and they are geometry, not gameplay. That is a
measurement job for BN13, and it is the same trap as the community's `Jump Max Depth` report.

---

## QUESTION 2 — reading navmesh border edges from a Game target

### 2a. `GetDebugGeometry` — **the name in the research does not exist on `ARecastNavMesh`**

`grep -rn "GetDebugGeometry(" --include=*.h --include=*.cpp` over the whole engine tree returns
exactly two hits, and **neither is `ARecastNavMesh::GetDebugGeometry`**:

- `Runtime/NavigationSystem/Public/NavMesh/RecastNavMeshGenerator.h:846-849`
  ```cpp
  #if UE_ENABLE_DEBUG_DRAWING
  	/** Converts data encoded in EncodedData.CollisionData to FNavDebugMeshData format */
  	static NAVIGATIONSYSTEM_API void GetDebugGeometry(const FNavigationRelevantData& EncodedData, FNavDebugMeshData& DebugMeshData);
  #endif  // UE_ENABLE_DEBUG_DRAWING
  ```
  A **static** on `FRecastNavMeshGenerator`, taking `FNavigationRelevantData` / `FNavDebugMeshData`
  — nothing to do with navmesh edges — and **guarded by `UE_ENABLE_DEBUG_DRAWING`**, which is off
  in Shipping. This is almost certainly the symbol behind the LNK2019 reports.
- its definition at `Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp:7649`.

**`ARecastNavMesh::GetDebugGeometry()` — NOT FOUND in UE 5.8 headers.** Flagging this plainly:
the research inventory named an API that does not exist. What does exist is
**`GetDebugGeometryForTile`**, and it is fine:

`Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h:1234-1247`, **public**:
```cpp
	/* Gather debug geometry.
	 * @params OutGeometry Output geometry.
	 * @params TileIndex Used to collect geometry for a specific tile, INDEX_NONE will gather all tiles
	 * @return True if done collecting.
	 */
	UE_DEPRECATED(5.5, "Use the version of the function that takes a FNavTileRef instead.")
	NAVIGATIONSYSTEM_API bool GetDebugGeometryForTile(FRecastDebugGeometry& OutGeometry, int32 TileIndex) const;

	/* Gather debug geometry.
	 * @params OutGeometry Output geometry.
	 * @params TileRef Used to collect geometry for a specific tile, an invalid FNavTileRef will gather all tiles
	 * @return True if done collecting.
	 */
	NAVIGATIONSYSTEM_API bool GetDebugGeometryForTile(FRecastDebugGeometry& OutGeometry, FNavTileRef TileRef) const;
```

**Guard audit (preprocessor stack computed mechanically, not eyeballed):**

| symbol | file:line | enclosing `#if` stack |
| --- | --- | --- |
| `ARecastNavMesh::GetDebugGeometryForTile` decl | `RecastNavMesh.h:1247` | `WITH_RECAST` (opens :1007) |
| `ARecastNavMesh::GetDebugGeometryForTile` def | `RecastNavMesh.cpp:2906` | `WITH_RECAST` (opens :499) |
| `FPImplRecastNavMesh::GetDebugGeometryForTile` def | `PImplRecastNavMesh.cpp:3069` | `WITH_RECAST` (opens :7) |
| `FRecastDebugGeometry::NavMeshEdges` | `RecastNavMesh.h:241` | `WITH_RECAST` (opens :135) |

**No `WITH_EDITOR`. No `!UE_BUILD_SHIPPING`. No `ENABLE_DRAW_DEBUG` / `UE_ENABLE_DEBUG_DRAWING`.**
Only `WITH_RECAST`, which is `1` in a default Game or Shipping target
(`NavigationSystem.Build.cs:40-43`; `TargetRules.cs:1406-1407` `bCompileRecast = true`).

> **Plain statement, as the ticket asks: YES — a non-editor Game target, including Shipping, can
> call `ARecastNavMesh::GetDebugGeometryForTile(FRecastDebugGeometry&, FNavTileRef)` and read
> `FRecastDebugGeometry::NavMeshEdges`.** It is exported `NAVIGATIONSYSTEM_API`, public, and
> unguarded beyond `WITH_RECAST`. **The load-bearing risk the research flagged does not
> materialise.** It fails only if someone sets `bCompileRecast=false`, which would also delete
> the navmesh itself.

The plumbing, for the record —
`Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h:240-241, 259-268`:
```cpp
	TArray<FVector> PolyEdges;
	TArray<FVector> NavMeshEdges;
...
	uint32 bGatherPolyEdges : 1;
	uint32 bGatherNavMeshEdges : 1;
	uint32 bMarkForbiddenPolys : 1;
	uint32 bGatherTileBuildTimesHeatMap : 1;
...
	FRecastDebugGeometry() : bGatherPolyEdges(false), bGatherNavMeshEdges(false), bMarkForbiddenPolys(false), bGatherTileBuildTimesHeatMap(false)
	{}
```
consumed at `Runtime/NavigationSystem/Private/NavMesh/PImplRecastNavMesh.cpp:3426-3428`:
```cpp
	if (OutGeometry.bGatherPolyEdges || OutGeometry.bGatherNavMeshEdges)
	{
		GetTilePolyEdges(Tile, !!OutGeometry.bGatherPolyEdges, !!OutGeometry.bGatherNavMeshEdges
```
`bGatherNavMeshEdges` confirmed to exist and to be the switch. Vertices come out **in pairs**,
one pair per edge.

### 2b. The better door — and it is much better

2a is a yes, so this is a bonus rather than a fallback, but it changes what BN13 should write.
There is a **non-debug, first-class API** for exactly the question "give me this tile's border
edges", all on `ARecastNavMesh`, all `NAVIGATIONSYSTEM_API`, all public, all inside the same
`WITH_RECAST`-only block:

```cpp
// RecastNavMesh.h:1173
	NAVIGATIONSYSTEM_API void GetAllNavMeshTiles(TArray<FNavTileRef>& OutRefs) const;

// RecastNavMesh.h:1433-1434
	/** Get all exterior nav mesh edges from tile */
	NAVIGATIONSYSTEM_API bool GetEdgesInTile(FNavTileRef TileRef, TArray<FNavigationWallEdge>& OutEdges) const;

// RecastNavMesh.h:1430-1431
	/** Get all  navmesh edges from tile that form the border of the passed in Filter */
	NAVIGATIONSYSTEM_API void GetTilePolyEdgesForFilter(FNavTileRef TileRef, FSharedConstNavQueryFilter Filter, TArray<FNavigationWallEdge>& OutEdges) const;

// RecastNavMesh.h:1427-1428
	/** Find up to 64 navmesh eges in up to 64 polys around the center */
	NAVIGATIONSYSTEM_API bool FindEdges(const NavNodeRef CenterNodeRef, const FVector Center, const FVector::FReal Radius, const FSharedConstNavQueryFilter Filter, TArray<FNavigationWallEdge>& OutEdges) const;
```

with, `RecastNavMesh.h:324-334`:
```cpp
struct FNavigationWallEdge
{
	FNavigationWallEdge() = default;
	FNavigationWallEdge(const FVector& InStart, const FVector& InEnd)
		: Start(InStart)
		, End(InEnd)
	{
	}
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
};
```

`GetEdgesInTile` is `GetDebugGeometryForTile`'s honest twin: its body
(`RecastNavMesh.cpp:2430-2457`) calls the same `FPImplRecastNavMesh::GetTilePolyEdges` with
`bGatherInternalEdges=false, bGatherExternalEdges=true` and hands back typed `Start`/`End` pairs
instead of a flat `TArray<FVector>`. **`GetAllNavMeshTiles` + `GetEdgesInTile` is the door BN13
should use** — no debug struct, no debug intent, same data, typed.

The rest of the ticket's grep list, all present, all public, all `NAVIGATIONSYSTEM_API`, all
inside the `WITH_RECAST` block:

| API | file:line |
| --- | --- |
| `bool GetPolyNeighbors(NavNodeRef PolyID, TArray<FNavigationPortalEdge>& Neighbors) const` | `RecastNavMesh.h:1378` |
| `bool GetPolyNeighbors(NavNodeRef PolyID, TArray<NavNodeRef>& Neighbors) const` | `RecastNavMesh.h:1381` |
| `bool GetPolyEdges(NavNodeRef PolyID, TArray<FNavigationPortalEdge>& Neighbors) const` | `RecastNavMesh.h:1384` |
| `bool GetPolysInTile(FNavTileRef TileRef, TArray<FNavPoly>& Polys) const` | `RecastNavMesh.h:1422` |
| `bool GetPolysInBox(const FBox&, TArray<FNavPoly>&, FSharedConstNavQueryFilter, const UObject*) const` | `RecastNavMesh.h:1425` |
| `virtual void BeginBatchQuery() const override` | `RecastNavMesh.h:1312` |
| `FBox GetNavMeshTileBounds(FNavTileRef TileRef) const` | `RecastNavMesh.h:1138` |
| `int32 GetNavMeshTilesCount() const` | `RecastNavMesh.h:1168` |
| `struct FNavigationPortalEdge` | `Runtime/Engine/Classes/AI/Navigation/NavigationTypes.h:219` |

Note `BeginBatchQuery` is declared on the base — `Runtime/NavigationSystem/Public/NavigationData.h:806`
`virtual void BeginBatchQuery() const {}` — and overridden on `ARecastNavMesh` at :1312.

### 2c. Registration side

All four exist.

```cpp
// Runtime/AIModule/Classes/Navigation/NavLinkProxy.h:33-34
UCLASS(Blueprintable, autoCollapseCategories=(SmartLink, Actor), hideCategories=(Input), MinimalAPI)
class ANavLinkProxy : public AActor, public INavLinkHostInterface, public INavRelevantInterface
```
(AIModule. Carries `TArray<FNavigationLink> PointLinks` at :40 and one private
`UNavLinkCustomComponent* SmartLinkComp` at :50 — "There can only be at most one smart link per
ANavLinkProxy instance", :30.)

```cpp
// Runtime/NavigationSystem/Public/NavLinkCustomComponent.h:27-28
UCLASS(MinimalAPI)
class UNavLinkCustomComponent : public UNavRelevantComponent, public INavLinkCustomInterface
```
with, :68-69:
```cpp
	/** set basic link data: end points and direction */
	NAVIGATIONSYSTEM_API void SetLinkData(const FVector& RelativeStart, const FVector& RelativeEnd, ENavLinkDirection::Type Direction);
```

```cpp
// Runtime/NavigationSystem/Public/NavLinkCustomInterface.h:27-35
UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UNavLinkCustomInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class INavLinkCustomInterface
{
	GENERATED_IINTERFACE_BODY()
```
Its header comment (:20-24) names the registration contract: *"Owner is responsible for
registering and unregistering links in NavigationSystem: RegisterCustomLink /
UnregisterCustomLink"*.

The `UNavigationSystemV1` calls, `Runtime/NavigationSystem/Public/NavigationSystem.h:1020-1028`,
all public (specifier at :1003), unguarded:
```cpp
	//----------------------------------------------------------------------//
	// Custom navigation links
	//----------------------------------------------------------------------//
	NAVIGATIONSYSTEM_API virtual void RegisterCustomLink(INavLinkCustomInterface& CustomLink);
	NAVIGATIONSYSTEM_API void UnregisterCustomLink(INavLinkCustomInterface& CustomLink);
	int32 GetNumCustomLinks() const { return CustomNavLinksMap.Num(); }

	static NAVIGATIONSYSTEM_API void RequestCustomLinkRegistering(INavLinkCustomInterface& CustomLink, UObject* OwnerOb);
	static NAVIGATIONSYSTEM_API void RequestCustomLinkUnregistering(INavLinkCustomInterface& CustomLink, UObject* ObjectOb);
```

**The generation-finished delegate — yes, it exists.**
`Runtime/NavigationSystem/Public/NavigationSystem.h:56` and `:443-444`, public (specifier :426),
unguarded:
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNavDataGenericEvent, ANavigationData*, NavData);
...
	UPROPERTY(BlueprintAssignable, Transient, meta = (displayname = OnNavigationGenerationFinished))
	FOnNavDataGenericEvent OnNavigationGenerationFinishedDelegate;
```
It is a **dynamic** multicast, so a listener needs a `UFUNCTION()`. Fired from
`UNavigationSystemV1::OnNavigationGenerationFinished(ANavigationData& NavData)`
(`NavigationSystem.h:1103`). Two neighbours worth knowing: `FOnNavigationInitDone OnNavigationInitDone;`
(`NavigationSystem.h:58, 446` — plain, non-dynamic) and
`ARecastNavMesh::OnNavMeshTilesUpdated(const TArray<FNavTileRef>& ChangedTiles)`
(`RecastNavMesh.h:1205`, virtual, per-tile), which is the finer-grained hook if a whole-mesh
rebuild is too coarse.

---

## The three supporting facts

**1. Static or dynamic? — DYNAMIC.** Read back from the actor's own external package, which is a
real binary in this clone, not an LFS pointer (42,856 bytes):
`Content/__ExternalActors__/Maps/BR_Arena01/6/E3/HCY7SNQKXA9K2PTJH9IDD5.uasset` contains
`/Script/NavigationSystem.RecastNavMesh`,
`/Game/Maps/BR_Arena01.BR_Arena01:PersistentLevel.RecastNavMesh_UAID_C0BFBEEA8D43D0F302-Default`,
the property name `RuntimeGeneration`, and exactly one `ERuntimeGenerationType::` value —
**`ERuntimeGenerationType::Dynamic`** (`::Static` and `::DynamicModifiersOnly` do not appear).
Enum values are `Static / DynamicModifiersOnly / Dynamic / LegacyGeneration`,
`Runtime/NavigationSystem/Public/NavigationData.h:523-531`.

Consequence, and it is a good one: the navmesh is **rebuilt at runtime**, so turning on
`bGenerateNavLinks` does **not** require re-saving `BR_Arena01` or its external actor packages
for the links to exist in a game session. No binary asset lock needed for step 1. (An editor
rebuild would still dirty tile data if anyone triggers one, so BN13 should still not rebuild
navigation casually.)

**2. Does `BR_Arena01` have a navmesh? — YES, read back, not assumed.** Two external actor
packages carry it:
- `Content/__ExternalActors__/Maps/BR_Arena01/6/E3/HCY7SNQKXA9K2PTJH9IDD5.uasset` — the
  `RecastNavMesh` actor above.
- `Content/__ExternalActors__/Maps/BR_Arena01/A/8G/VESQVSTD8RALYCPOJSVFB3.uasset` — strings
  `BR_NavBounds` and `NavMeshBoundsVolume`, i.e. the bounds volume
  `Tools/blockout/arena_plan.py` emits.

The map itself (`Content/Maps/BR_Arena01.umap`) carries `/Script/NavigationSystem`,
`NavigationSystemConfig`, `NavigationSystemModuleConfig`, `NavigationSystemV1`. 51 external actor
packages total. Both halves the research could not check from the cloud are confirmed present.

**3. Fall damage — NONE. BN applies no damage on landing.** `grep` across
`Source/BreachpointNext` for `FallDamage|TakeFallingDamage|ApplyDamageMomentum|Landed` finds
exactly one landing handler, `Source/BreachpointNext/AbilitySystem/Abilities/BNMovementAbilities.cpp:81-88`:
```cpp
void UBNGA_Jump::OnLanded(const FHitResult& Hit)
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->StopJumping();
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
```
`StopJumping` and `EndAbility`, nothing else. No `TakeFallingDamage` override anywhere, and
`BNDamage::ApplyDamage` (`AbilitySystem/Effects/BNDamage.h:55`) is only reached from
`BNGA_Fire` / `BNGA_Melee` / `BNGA_Grenade` / `BNProjectile` / `BNGA_Death`. **The 8 m / 800 uu
gantry drop is free.** A drop rule may treat any survivable-by-geometry fall as survivable
outright — the only cost of a big drop is travel time. If fall damage is ever added, this fact
inverts and BN13's budgets become load-bearing; that belongs in the risk register.

---

## Verdict: does the planned approach survive?

**Yes — and it gets cheaper, not dearer. Both feared failure modes were false alarms; the real
correction is that the plan under-scoped step 1 and over-scoped step 2.**

- **Q2's kill shot did not land.** `GetEdgesInTile` / `GetDebugGeometryForTile` are exported,
  public, and guarded only by `WITH_RECAST`. Reading navmesh border edges from a **packaged
  Shipping Game target** is supported. Nothing about step 2 dies at package time. The specific
  API the research named (`ARecastNavMesh::GetDebugGeometry`) **does not exist**, but a better
  one does, so this is a rename, not a redesign.
- **Q1c's C++-first path exists.** `UCLASS(config=Engine, defaultconfig)` plus
  `UPROPERTY(config)` on both `bGenerateNavLinks` and `NavLinkJumpConfigs` means
  `Config/DefaultEngine.ini` `[/Script/NavigationSystem.RecastNavMesh]` owns the settings, with
  no asset edited and no binary lock. Only `LinkProxyClass` is out of reach from ini, and it is
  optional by design.
- **Step 1 is now bigger than "the downward half".** The built-in generator does up-links and
  bidirectional links, from an array of independently budgeted configs. The founder's ruling —
  derive from the mesh, place nothing in the level — is satisfied by the **engine's own
  generator for both directions**, from one ini section.
- **Step 2 (`UBNNavLinkForge`) should be deferred, not designed.** Its entire justification in
  the research was "the built-in thing is downward only." That premise is dead. BN13 should be
  **measure step 1 first**: turn on `bGenerateNavLinks`, ship two configs (one drop, one climb),
  and read back what got generated. Write `UBNNavLinkForge` only against a measured, named gap.
  Writing it now would be building a replacement for a system nobody has run yet.
- **Step 3 (`FBNDropDownTask`) is unaffected** and its stated ceiling still stands.

**Carry-forward risks for BN13's register, not blockers:**
1. `FNavLinkGenerationJumpConfig` is `USTRUCT()` (not `BlueprintType`) and still labelled
   *"Experimental"* in its own comment (`LinkGenerationConfig.h:24`). Numbers get measured.
2. `check(jumpHeight >= 0.f)` (`DetourNavLinkBuilderConfig.cpp:8`) — a negative `JumpHeight` in
   ini is a hard assert, not a clamp. Only `JumpMaxDepth` may go negative.
3. The generator budgets geometry, not gameplay. Nothing stops it emitting an up-link taller
   than `UBNGA_Jump` can clear. Bots will confidently path into links they cannot traverse until
   the numbers are measured against the real jump.
4. `ARecastNavMesh`'s link-proxy helpers (`RecastNavMesh.h:1535-1550`) are protected **and
   unexported** — subclassing `ARecastNavMesh` to reach them will LNK2019. If BN13 wants that,
   it is a contract_gap, not a workaround.
5. Epic's note that link building costs tile-generation time is real
   (`bRegenerateCompressedLayers` is forced when `bGenerateLinks`,
   `RecastNavMeshGenerator.cpp:1852`). With `RuntimeGeneration::Dynamic`, that cost lands at
   **runtime**. Measure nav build time on the listen server before and after.

**Free measurement lever:** `ai.nav.AllowLinkGeneration 0`
(`RecastNavMesh.cpp:64-65`) disables generation without touching config — an A/B that needs no
rebuild.

## NOT FOUND — stated plainly

- **`ARecastNavMesh::GetDebugGeometry`** — does not exist in UE 5.8. The only `GetDebugGeometry`
  in the engine is `static FRecastNavMeshGenerator::GetDebugGeometry(const FNavigationRelevantData&, FNavDebugMeshData&)`
  (`RecastNavMeshGenerator.h:848`), which is a different thing entirely and **is** guarded by
  `#if UE_ENABLE_DEBUG_DRAWING`. `GetDebugGeometryForTile` is the real, unguarded API and is a
  **substitution — flagged as such**, not a match for the name in the research.
- **`FNavLinkGenerationJumpDownConfig` as the live config** — exists, but `UE_DEPRECATED(5.7)`,
  and its navmesh member is `WITH_EDITORONLY_DATA`. Superseded by `FNavLinkGenerationJumpConfig`.
- **"Nav Link Jump Down Config" as a single struct on the navmesh** — superseded by
  `TArray<FNavLinkGenerationJumpConfig> NavLinkJumpConfigs`.
- **"Downward only"** — false in 5.8. See 1e.
- Everything else in the research's unverified inventory (`UBaseGeneratedNavLinksProxy`,
  `GeneratedNavLinksProxy`, `FRecastDebugGeometry::bGatherNavMeshEdges`, `NavMeshEdges`,
  `GetDebugGeometryForTile`, `ANavLinkProxy`, `UNavLinkCustomComponent`,
  `INavLinkCustomInterface`, the custom-link registration calls, the
  generation-finished delegate) — **confirmed**, file:line above.

## Owner-path / process note

Only `docs/tickets/TICKET_BN12_NAVLINK_PROBE.md` was written. No `Source/`, no `Config/`, no
`Content/`. The "`git status` pasted in the Log" box is **left unticked**: the dispatching lead
instructed this session to run no git command at all, and that instruction outranks the
checklist item. **The lead should paste `git status` and tick that box** — the claim being
proven is that nothing else changed, and this agent asserts that but has not run the command
that would demonstrate it.

### 25 Aug 2026 — lead, closing the one box the agent could not

The agent was instructed by the dispatching lead to run NO git command, which is why it left this
box open rather than ticking it on assertion. Ticked now on evidence. `git status --short` at the
moment BN12 finished, with the BN11 agent still running in a different owner path:

```
 M .claude/active-packet.json                      <- the claim file, written by the lead
 M docs/tickets/TICKET_BN12_NAVLINK_PROBE.md       <- this ticket
```

Filtered to `docs/`, `TICKET_BN12_NAVLINK_PROBE.md` is the ONLY change. No `Source/`, no
`Content/`, no ini. BN12 wrote nothing outside its owner_path, as required — the ticket that
writes no code wrote no code.
