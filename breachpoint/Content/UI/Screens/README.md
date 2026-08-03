# `Content/UI/Screens/`

Full screens, hosted panels and modals — the things the UI manager pushes onto a CommonUI layer.

**Naming** (`docs/ui/ue-frontend/SCREEN-MANIFEST.md:122-124`), same strip-`UBR`-prefix-`WBP_`
derivation as everywhere else:

| C++ class | Asset |
|---|---|
| `UBRScreen_<Name>` | `WBP_Screen_<Name>` |
| `UBRPanel_<Name>`  | `WBP_Panel_<Name>` |
| `UBRModal_<Name>`  | `WBP_Modal_<Name>` |

**Belongs here:** layout, anchors, widget animations. Flat — no per-family subfolders
(`docs/ui/ue-frontend/STRUCTURE.md` §3).

**Never here:**
- A screen with no `UBRScreen_`/`UBRPanel_`/`UBRModal_` C++ parent.
- Logic. Navigation, data binding and state belong to the C++ parent and the ViewModels; a WBP
  carrying anything but layout is a contract violation (R18).
- Reusable components — those go in `Content/UI/Components/`. A thing used by two screens is a
  component, not a copy.
- One asset per Figma frame. Several frames are usually **one widget with a state enum**; the
  manifest's `WBP_*` column is the count that governs.
