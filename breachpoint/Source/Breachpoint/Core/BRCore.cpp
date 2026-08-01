// Breachpoint. Log channels and collision channel aliases.

#include "Core/BRCore.h"

// One definition per declaration in BRCore.h. The collision aliases are compile-time
// constants and need no definition here; their counterpart lives in Config/DefaultEngine.ini.

DEFINE_LOG_CATEGORY(LogBRCombat);
DEFINE_LOG_CATEGORY(LogBRNet);
DEFINE_LOG_CATEGORY(LogBRAI);
DEFINE_LOG_CATEGORY(LogBROnline);
DEFINE_LOG_CATEGORY(LogBRUI);

// Input/ (§3.2). Added by BP01 step 4 under ruling R24 — see the header comment.
DEFINE_LOG_CATEGORY(LogBRInput);
