// Centralized log categories for OnSight
// Usage: #include "OSLogCategories.h" in .cpp files that need logging.
// Console: "Log LogOSAttack Verbose" to enable verbose output for a category.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOSAttack, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOSCombat, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOSChooser, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOSDebug, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOSGASAttackTrace, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOSGASGrab, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOSGASGrabReaction, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOSGASHitReact, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOSMantle, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOSMovement, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOSTrackMotionWarpTarget, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOSDomination, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOSSpawn, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(HookMapSelect, Log, All);
