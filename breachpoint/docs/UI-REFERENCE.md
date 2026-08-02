# UI Reference — founder decision, 2 Aug 2026

**The front end follows the layout system of the Figma community file "Halo Infinite UI
Rework (Community)"** (`figma.com/design/Kn87U5sy2VD0lP8K7h4LcQ`). Decided by the founder
after reviewing rendered comparisons; supersedes the Campaign-Evolved-first plan for all
FRONT-END screens.

## What "follows" means (the boundary)

- **Layout, geometry, hierarchy, behaviour: matched.** The file was read through the Figma
  MCP; element positions were extracted from node metadata (1280×720 base) and are the
  numbers our mockups and WBP layouts use. Key measures: left rail x=69 w=349; menu rows
  h=28 pitch 40; roster panel w=349, header h=31, rows h=30 pitch 35; profile bar 1280×50
  at y=670; nav tabs at y=45 h=30.
- **Art: original, always.** No Halo emblem, rank iconography, typeface, or brand artwork
  is reproduced — Breachpoint ships on Steam. Chrome is the file's *language* (flat panels,
  sharp corners, 1px white borders, uppercase letter-spaced labels, white header bars,
  per-player colour rows), drawn fresh.

## Scope

- Covers: main menu, lobby/roster, settings, matchmaking/loading, post-game layers.
- Does NOT cover the in-match HUD — the file is front-end only. HUD keeps the existing
  spec (Campaign Evolved language + Infinite's competitive-element behaviour) in the
  screen-pass artifact and `Source/Breachpoint/UI/` bindings.

## The system built from it

- **`.claude/skills/ui-presentation/SKILL.md`** — the method: Presentation as a discipline,
  design-system-first, VISR colour semantics, the measured grid, the Figma MCP commands, the
  Chromium render loop, and the Figma→UE handoff rules.
- **`docs/UI-DESIGN-SYSTEM.md`** — the reference tables: colour tokens with their meanings,
  measured geometry, the twelve-component inventory with Figma↔UE names, which reference
  governs which surface, and the four C++ gaps that block binding.

## Where things are

- Rendered mockups + field-by-field ViewModel binding tables: the "BREACHPOINT — UI Screen
  Pass 01" artifact (claude.ai, founder's account) and the session scratchpad pipeline.
- The C++ gaps the UI pass found (per-player stat block, reticle target-state, respawn
  countdown, lobby ViewModel) are BP10/BP04/BP03/BP11 work and are unchanged by this
  decision.
