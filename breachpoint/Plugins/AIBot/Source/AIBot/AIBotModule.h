#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// One log category for the whole module. Verifier protocols grep these lines
// and count — a log line here is an instrument, not a comment.
AIBOT_API DECLARE_LOG_CATEGORY_EXTERN(LogAIBot, Log, All);
