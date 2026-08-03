# `Content/UI/Components/`

Reusable widget assets. **One WBP per class in `Source/Breachpoint/UI/Components/`, no more.**

**Naming:** `WBP_<Name>`, derived mechanically from the C++ parent by stripping `UBR`.
`UBRNavBar` → `WBP_NavBar`. No other name is legal; the derivation is what lets a script
assert every WBP's parent from its filename alone.

**Belongs here:** UMG layout only — widget tree, anchors, slots, widget animations.
Flat. No subfolders (`docs/ui/ue-frontend/STRUCTURE.md` §3).

**Never here:**
- A WBP with **no** C++ parent in `Source/Breachpoint/UI/Components/`. Every asset here is a
  child of a `UBR*` class; an orphan WBP is a Tier-4 asset holding gameplay nobody can diff (R18).
- Bindings, state, branches, event logic, or any gameplay number. Those live in the C++ parent
  and in `Content/Data/*.csv` (law 3, R26 condition 4).
- Screens, panels or modals — those go in `Content/UI/Screens/`.
- Textures. `T_*` never lives beside a widget.
