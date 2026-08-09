#pragma once
// Centralized extern declarations for all debug overlay globals.
// Definitions live in OSCheatManager.cpp. CVars, checkboxes, and reset logic are there too.
// Include this header instead of writing bare extern declarations in individual files.

// Debug overlay toggles — always defined (even in shipping, as zero-init dead data).
// Usage sites are #if !UE_BUILD_SHIPPING guarded; the globals themselves are not.
extern bool GDebugShowHealthBars;
extern bool GDebugShowNameplates;
extern bool GDebugPunchingBag;
extern bool GDebugMantleViz;
extern bool GDebugAttackTraceViz;
extern bool GDebugRotationViz;
extern bool GDebugTargetingViz;
extern bool GDebugGrabViz;
