#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AIBTreeAuthoring.generated.h"

/**
 * The module's two authored assets, BUILT IN CODE — /Game/AIBot/AI/ST_AIBBot and
 * /Game/AIBot/Data/DT_AIBTiers.
 *
 * WHY THIS IS C++ AND NOT A PYTHON SCRIPT (the host's R5 finding, verbatim reasoning):
 * a StateTree graph lives in UStateTreeEditorData::SubTrees and UStateTreeState::Children,
 * bare Instanced UPROPERTYs carrying neither CPF_Edit nor CPF_BlueprintVisible, so
 * PropertyAccessUtil refuses them to Python and the MCP ObjectTools alike; there is no
 * StateTree factory exposed to script; and UStateTreeEditingSubsystem::CompileStateTree is
 * a plain static C++ function, not a UFUNCTION — an uncompiled tree runs nothing. Every
 * one of those doors is open to C++.
 *
 * Idempotent: a rebuild converges (states rewritten from scratch, the asset object
 * reused so live soft-path references stay valid). The tier table MIRRORS FAIBTierRow's
 * C++ defaults — one direction of flow; the table exists to be retuned, never to
 * re-declare numbers C++ already ships.
 *
 * BlueprintCallable so the editor's Python console and the MCP bridge can reach it; the
 * bodies are WITH_EDITOR only (whole TU guarded — UBT feeds every module .cpp to the
 * Game and Server targets) and those targets get honest stubs.
 */
UCLASS()
class AIBOT_API UAIBTreeAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Builds (or rebuilds, idempotently) /Game/AIBot/AI/ST_AIBBot, compiles it, saves it. */
	UFUNCTION(BlueprintCallable, Category = "AIBot|Authoring")
	static FString BuildBotStateTree();

	/** Builds (or rebuilds) /Game/AIBot/Data/DT_AIBTiers — today one Default row mirroring
	 *  FAIBTierRow's C++ defaults; Phase 8 authors the four real tiers here. */
	UFUNCTION(BlueprintCallable, Category = "AIBot|Authoring")
	static FString BuildTierTable();

	/** Both of the above, then the read-back. The one call the terminal makes. */
	UFUNCTION(BlueprintCallable, Category = "AIBot|Authoring")
	static FString BuildBotAssets();

	/** Read-back only, from a fresh load: what is actually in the assets on disk. */
	UFUNCTION(BlueprintCallable, Category = "AIBot|Authoring")
	static FString AuditBotAssets();
};
