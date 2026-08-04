#!/usr/bin/env python3
"""The committed plan half of the WBP generator. Pure CPython — imports no engine.

This file is the reviewable artifact R37.1 requires: a WBP's layout expressed as
diffable text. `build_wbp.py` executes it; nothing else decides what a WBP contains.

The load-bearing idea: a widget marked `bind: True` MUST correspond to a
`meta = (BindWidget)` member on the C++ parent, and that correspondence is checked
HERE, against the real header, before an editor is ever opened.

`BindWidget` desync is otherwise UE's nastiest UI failure — the C++ renames a member,
the WBP keeps the old name, the widget fails to compile *at asset load* rather than at
build time, so rung 1 stays green and the HUD is simply empty in PIE. Checking it at
plan time turns that into a text-mode error nobody can miss.

GEOMETRY COMES FROM `figma_hud_layout.json`, WHICH IS AUTHORITATIVE FOR POSITION. Every
number in the HUD section below is measured from Figma node `30:2`, the SCREEN frame, at
the 1280x720 authoring base. The DPI curve does 1080; nothing here is ever multiplied by
1.5. Where a component's own export disagrees with the screen frame (vitals 277x35 vs
273.33x34, ammo 190x40 vs the frame's 218x60 sub-rect) the two are recorded and NOT
averaged — see the per-asset notes.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent

# Class paths, named once. A typo here is a silent "widget not created".
OVERLAY = "/Script/UMG.Overlay"
CANVAS = "/Script/UMG.CanvasPanel"
STACK = "/Script/CommonUI.CommonActivatableWidgetStack"
VBOX = "/Script/UMG.VerticalBox"
HBOX = "/Script/UMG.HorizontalBox"
TEXT = "/Script/CommonUI.CommonTextBlock"
IMAGE = "/Script/UMG.Image"
SIZEBOX = "/Script/UMG.SizeBox"
PROGRESS = "/Script/UMG.ProgressBar"
# UTileView. The view OWNS its entries - it creates, recycles and virtualises them, so a
# repeated tile is NEVER N hand-placed children (SCREEN-MANIFEST §7.1).
TILEVIEW = "/Script/UMG.TileView"
# UBRPanel::Content is a NamedSlot, not a panel: an INSTANCED panel's caller drops content
# through the named-slot API, which is the only way one WBP serves 45 measured variants.
NAMED_SLOT = "/Script/UMG.NamedSlot"
# The wrapped engine bar. UBRScrollBar COMPOSES it rather than deriving from it - UScrollBar
# is UCLASS(Experimental, MinimalAPI) with GENERATED_UCLASS_BODY, so a cross-module subclass
# fails at LINK, not at compile. The composition costs exactly this one line.
SCROLLBAR = "/Script/UMG.ScrollBar"
# UBRScrim is a UBRHairlineBorder with zero edges and a fill - a UWidget over a Slate leaf,
# NOT a UserWidget, so it is placed DIRECTLY and needs no generated child, like HAIRLINE/RULE.
# It self-tints from a token in its own constructor and ships Collapsed, so it can never
# render BP70 D2's blank white rectangle.
SCRIM = "/Script/Breachpoint.BRScrim"
# UScaleBox. Scales a FIXED design-space box uniformly to whatever the viewport is. This is
# how a 1280x720 frame full of absolute coordinates becomes correct at 1920x1080 and every
# other resolution: the numbers stay the frame's, and one widget does the arithmetic.
# Default Stretch is ScaleToFit, which preserves aspect - the design and the common screen
# are both 16:9, so 1280x720 -> 1920x1080 is an exact 1.5x with no letterboxing.
SCALEBOX = "/Script/UMG.ScaleBox"

# The design space every absolute coordinate in this file is measured in.
DESIGN_W, DESIGN_H = 1280.0, 720.0

# The hairline primitives. Both are `UWidget` over a Slate leaf, NOT UserWidgets — so unlike
# every UBR* class in this plan they are placed DIRECTLY and need no generated child. That is
# the whole reason `WBP_PanelBorder` does not exist and never will (BRHairlineBorder.h states
# the arithmetic: ~4500 widgets across the front end if it were a four-UImage composite).
HAIRLINE = "/Script/Breachpoint.BRHairlineBorder"
RULE = "/Script/Breachpoint.BRRule"

# Platform title-safe. Used as a screen root instead of a padded box: a padded box is a
# guess at every console's overscan, and this is the engine's answer to the same question.
SAFEZONE = "/Script/UMG.SafeZone"

# CommonUI's platform-glyph leaf. Draws the CURRENT input device's icon for a bound input
# action - zero brushes, zero per-platform art. This is why WBP_ButtonPrompt's glyph is
# NOT a UImage: a brushless UImage is BP70 D2's blank white rectangle AND a mouse-only lie
# about what the player is actually holding.
ACTION_GLYPH = "/Script/CommonUI.CommonActionWidget"

# Declared ahead of use (HUD-D): the doctrine wants an InvalidationBox around the weapon
# tray's static frame. Whether that wraps ONE merged tray widget or the shipped two siblings
# is an open founder DECIDE (figma_hud_layout.json measures the frame as one 280x110 unit);
# the class path is here so the plan edit is one line when it is ruled.
INVALIDATION = "/Script/UMG.InvalidationBox"

FILL = {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill",
        "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}}

# An Overlay slot that centres its child and lets it OVERFLOW. The reticle needs exactly
# this: `UBRReticleWidget::ApplyArt` calls `SetDesiredSizeOverride` with the weapon's
# authored edge length (Magnum 36, AR/BR 43, Shotgun 52, Sniper 58), and the size IS the
# spread readout. Any slot that constrained the child would silently delete the only
# accuracy cue the centre of the screen carries.
CENTER = {"horizontalAlignment": "HAlign_Center", "verticalAlignment": "VAlign_Center",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}}


def margin(left=0.0, top=0.0, right=0.0, bottom=0.0) -> dict:
    return {"left": float(left), "top": float(top),
            "right": float(right), "bottom": float(bottom)}


def inset(amount: float, h="HAlign_Fill", v="VAlign_Fill") -> dict:
    """An Overlay slot inset uniformly — Figma's `(2, 2) 246 x 24` inside a 250 x 28 shell.

    Written as an INSET rather than as a 246x24 size on purpose: the row's width is not 250 on
    any real screen (349 on the front-end rail, 536 in the grid stack), so the only number that
    survives is the 2px border gap. Authoring 246 would pin the plate to a width its parent no
    longer has.
    """
    return {"horizontalAlignment": h, "verticalAlignment": v,
            "padding": margin(amount, amount, amount, amount)}


def box_slot(fill: float | None = None, padding: dict | None = None,
             h="HAlign_Fill", v="VAlign_Fill") -> dict:
    """A Horizontal/VerticalBox slot. `fill=None` is Auto (hug), a float is Fill at that weight.

    `size` is `FSlateChildSize`, whose `sizeRule` is `ESlateSizeRule` — "Automatic" or "Fill".
    The value is only read when the rule is Fill, and it is written either way because a
    partial struct write is the kind of thing `write_verified` reports as a read-back mismatch
    rather than as a helpful error.
    """
    return {"size": {"sizeRule": "Automatic" if fill is None else "Fill",
                     "value": float(fill or 1.0)},
            "padding": padding or margin(),
            "horizontalAlignment": h, "verticalAlignment": v}

# ---------------------------------------------------------------------------
# ART: `font` and `brush`, the two node keys that make a correct tree VISIBLE
# ---------------------------------------------------------------------------
# Before these, every generated WBP was structurally right and visually empty: eleven
# CommonTextBlocks reading "Text Block" in the engine default face, images with no brush.
#
# NOTHING HERE IS A LITERAL. A `font` node key is a STYLE NAME out of figma_tokens.json,
# which was read live from the Figma variables; family, weight, size and letter spacing all
# come from that row. Typing 12 or "Rajdhani" into this file would be the same offence as
# typing hex into a WBP (`figma_tokens.json:_law`) — one more place a re-theme breaks silently.

TOKENS = json.loads((HERE / "figma_tokens.json").read_text())
TYPE_STYLES = {s["name"]: s for s in TOKENS["type"]["styles"]}

# family -> (composite font asset, the typeface names that asset actually contains).
#
# The typeface names are the Figma weight strings BY CONSTRUCTION — `import_fonts.py` built
# each composite font's typeface array from these same rows — so `style["weight"]` goes
# straight into `FSlateFontInfo::TypefaceFontName` with no mapping table in between. That
# also means a Figma weight this asset does not carry is a real, catchable error, which is
# what the set below is for. `Medium Italic` is ONE FName containing a space; it is not a
# family plus an italic flag, and UMG has no italic flag to set.
FONT_ASSETS = {
    "Rajdhani":         ("/Game/UI/Fonts/F_Rajdhani",       {"Regular", "Bold", "SemiBold"}),
    "Roboto Condensed": ("/Game/UI/Fonts/F_RobotoCondensed", {"Medium", "Medium Italic", "SemiBold"}),
}

# Which node classes each art key is meaningful on. A `font` on a UImage is not a harmless
# no-op — it is a set_properties call against a property that does not exist, which this
# server reports as TEXT rather than as an error, so it would read as a silent pass.
_FONT_CLASSES = {TEXT}
_BRUSH_CLASSES = {IMAGE}


def _obj_ref(game_path: str) -> dict:
    """`/Game/UI/Fonts/F_Rajdhani` -> the full `/Game/UI/Fonts/F_Rajdhani.F_Rajdhani` ref.

    The short form makes the MCP server drop the whole argument object and report "input
    params Json is empty" — build_wbp.py's docstring note 2, learned the expensive way.
    """
    return {"refPath": f"{game_path}.{game_path.rsplit('/', 1)[-1]}"}


def font_properties(style_name: str) -> dict:
    """A Figma text style -> the exact camelCase payload `ObjectTools.set_properties` takes.

    THE LETTER SPACING IS THE WHOLE POINT OF THIS FUNCTION. Figma records PERCENT; UMG's
    `FSlateFontInfo::LetterSpacing` is 1/1000 em, so 15% is 150, not 15 and not 0.15. The
    conversion is NOT done here: `figma_tokens.json` already carries both `ls_pct` and
    `ls_umg`, computed once at read time, and this reads `ls_umg`. Recomputing it would put
    a second copy of the rule in the repo, and the failure mode of a silently dropped
    letter-spacing is the nastiest one on this surface — every number "matches", the chrome
    just stops reading as military UI (`figma_tokens.json:type._letter_spacing`).

    Every value is an INT on purpose. `Size` and `LetterSpacing` are integral in the token
    rows already, and a JSON float landing on an int16 property is a rejection this server
    would report as text.

    NO ARRAYS IN THIS PAYLOAD, also on purpose. `set_properties` refuses a write that changes
    an array's SIZE and its ELEMENTS in one call and reports the refusal as text, dropping the
    whole property — that is what ate a font write earlier (it was building a composite font's
    typeface array). Writing only these four scalars keeps this path clear of that trap; if a
    future style ever needs `outlineSettings` or a fallback array, it must be grown one entry
    per call.

    `style["case"]` is deliberately NOT applied. UMG has no per-widget case transform, and the
    string comes from C++/the ViewModel — so UPPER is a contract on whoever formats the FText,
    not something a WBP default can express. Recorded rather than silently dropped.
    """
    s = TYPE_STYLES[style_name]
    asset, _ = FONT_ASSETS[s["family"]]
    return {"font": {"fontObject": _obj_ref(asset),
                     "typefaceFontName": s["weight"],
                     "size": int(s["size"]),
                     "letterSpacing": int(s["ls_umg"])}}


def brush(texture: str, width: float, height: float) -> dict:
    """A design-time `FSlateBrush`: which texture, drawn at what size.

    `width`/`height` are the AUTHORED size at the 1280x720 base, never multiplied by 1.5.
    """
    return {"texture": texture, "size": (float(width), float(height))}


def brush_properties(spec: dict) -> dict:
    return {"brush": {"resourceObject": _obj_ref(spec["texture"]),
                      "imageSize": {"x": spec["size"][0], "y": spec["size"][1]}}}


def texture_problem(spec: dict) -> str | None:
    """Why this brush's texture cannot be used yet — or None if it can.

    A brush pointing at an asset that is not there is worse than no brush: it compiles, it
    saves, and it renders a blank the next reader blames on layout. So the target is checked
    ON DISK at plan time and a miss is a SKIP with a loud note, never a plan error and never a
    dead asset — the HUD texture set is being extended by another lane right now, and this
    generator must keep producing correct assets while that lands.

    The LFS check is the same reason in a different costume: a pointer stub is a 130-byte text
    file wearing a .uasset name, and importing one yields a corrupt texture rather than an
    error (see the repo's LFS-stub gate).
    """
    f = REPO / "Content" / (spec["texture"].removeprefix("/Game/") + ".uasset")
    if not f.exists():
        return f"no texture at {f.relative_to(REPO)}"
    if f.read_bytes()[:32].lstrip().startswith(b"version https://git-lfs"):
        return f"{f.relative_to(REPO)} is a Git LFS pointer stub, not a texture"
    return None


def art_properties(node: dict) -> tuple[dict, list[str]]:
    """(properties to write on this widget, notes about what was skipped and why)."""
    props: dict = {}
    notes: list[str] = []
    if node.get("font"):
        props.update(font_properties(node["font"]))
    if node.get("brush"):
        why = texture_problem(node["brush"])
        if why:
            notes.append(f"brush not written — {why}")
        else:
            props.update(brush_properties(node["brush"]))
    return props, notes


UI_FOLDER = "/Game/UI"

# Feature-first folders, mirroring Source/Breachpoint/UI/. Flat /Game/UI was fine for three
# assets and is not fine for thirty: an asset picker sorted alphabetically interleaves a HUD
# surface, a menu component and a screen, and the naming law (strip UBR, prefix WBP_) then
# carries the ENTIRE burden of saying what a thing is.
#
# Set now, deliberately, because this generator deletes-then-creates - so a folder move costs
# nothing today and is nearly impossible later: BP18 proved MCP CANNOT rename an asset (the
# rename modal auto-cancels). Every asset has to be born at its final path.
#
# Content/UI/Icons stays exactly where it is and is NOT reorganised: Tools/verify_notices.py
# hard-codes that glob for the Lucide ISC notice, so moving it silently escapes a licence gate.
UI_LAYOUTS    = UI_FOLDER + "/Layouts"      # root + HUD layout: the two things pushed onto layers
UI_HUD        = UI_FOLDER + "/HUD"          # in-match surfaces
UI_COMPONENTS = UI_FOLDER + "/Components"   # reusable parts, never pushed directly
UI_SCREENS    = UI_FOLDER + "/Screens"      # activatable screens and modals
# The main menu gets its own folder (founder, 3 Aug 2026). Only the SCREEN moves here — the
# rail, nav bar, roster panel and rows stay in Components/ because they are shared: WBP_MenuRow
# serves the options modal too, and a component that two screens use does not belong in one
# screen's folder.
UI_MAINMENU   = UI_FOLDER + "/MainMenu"
# The crosshair gets its own folder (founder, 3 Aug 2026). The WIDGET moves; the
# HUD_Reticle_* TEXTURES deliberately do NOT - see the WBP_ReticleWidget entry.
UI_CROSSHAIR  = UI_FOLDER + "/Crosshair"

# asset -> folder, declared ABOVE both wbp_class() and PLAN because both need it and neither
# can be the source. wbp_class() runs DURING PLAN's construction (a host's child reference is
# evaluated as the dict literal is built), so reading PLAN from it is a NameError. One table,
# two readers, no cycle - and adding an asset to PLAN without routing it here is a KeyError at
# import, which is exactly when you want to hear about it.
ASSET_FOLDER = {
    "WBP_RootLayout":          UI_LAYOUTS,
    "WBP_HUDLayout":           UI_LAYOUTS,
    "WBP_VitalsWidget":        UI_HUD,
    "WBP_AmmoBlock":           UI_HUD,
    "WBP_ReticleWidget":       UI_CROSSHAIR,
    "WBP_MatchBand":           UI_HUD,
    "WBP_Killfeed":            UI_HUD,
    "WBP_KillfeedEntryWidget": UI_HUD,
    "WBP_EquipmentTray":       UI_HUD,
    "WBP_ProgressBar":         UI_COMPONENTS,
    "WBP_MenuRow":             UI_COMPONENTS,
    # The menu tier (BP72 step 4). All components except the screen itself.
    "WBP_NavTab":              UI_COMPONENTS,
    "WBP_ButtonPrompt":        UI_COMPONENTS,
    "WBP_RosterHeader":        UI_COMPONENTS,
    "WBP_RosterRow":           UI_COMPONENTS,
    "WBP_NavBar":              UI_COMPONENTS,
    "WBP_FeatureCard":         UI_COMPONENTS,
    "WBP_RecordPanel":         UI_COMPONENTS,
    "WBP_RosterPanel":         UI_COMPONENTS,
    "WBP_LeftRail":            UI_COMPONENTS,
    "WBP_MainMenu":            UI_MAINMENU,
    # The item/table tier. WBP_ItemTile MUST precede WBP_ItemGrid in PLAN — the tile is not
    # a tree child of the grid (a UTileView owns its entries), so validate_all()'s
    # host-ordering rule never fires and would not catch a wrong order.
    "WBP_HighlightButton":     UI_COMPONENTS,
    "WBP_ItemTile":            UI_COMPONENTS,
    "WBP_ItemGrid":            UI_COMPONENTS,
    "WBP_TableRow":            UI_COMPONENTS,
    # The chrome tier: containers and title bands, all leaves.
    "WBP_Panel":               UI_COMPONENTS,
    "WBP_ScrollBar":           UI_COMPONENTS,
    "WBP_PageTitle":           UI_COMPONENTS,
    "WBP_ItemTitle":           UI_COMPONENTS,
    "WBP_SmallHeader":         UI_COMPONENTS,
    # Screens and modals. WBP_Modal_Warning hosts WBP_HighlightButton and
    # WBP_Screen_DeathRespawn hosts WBP_MatchBand — both hosts must follow their child.
    "WBP_GearDetail":          UI_COMPONENTS,
    "WBP_Modal_Options":       UI_SCREENS,
    "WBP_Modal_Warning":       UI_SCREENS,
    "WBP_Panel_Toast":         UI_SCREENS,
    "WBP_Screen_Scoreboard":   UI_SCREENS,
    "WBP_Screen_DeathRespawn": UI_SCREENS,
    # The settings tier (BP78). WBP_SettingsRow is a COMPONENT even though only one screen
    # instances it -- it is created at runtime from a soft class, never placed in a tree, so
    # it is not "part of" the screen in any sense the folder convention recognises.
    "WBP_SettingsRow":         UI_COMPONENTS,
    "WBP_Modal_KeyRemap":      UI_SCREENS,
    "WBP_Screen_Settings":     UI_SCREENS,
    # Founder-requested, 4 Aug 2026. Its own folder because the Button Border texture set
    # imported alongside it lives there too -- the widget and the art it may use stay together.
    "WBP_ButtonDefault":       UI_COMPONENTS + "/Buttons",
}


def wbp_class(asset: str) -> str:
    """The GENERATED class of another asset in this plan: `/Game/UI/WBP_X.WBP_X_C`.

    Both `_C` and the full `/Game/Dir/Name.Name_C` form are load-bearing: the short form
    makes the MCP server drop the whole argument object (build_wbp.py docstring, note 2),
    and the path without `_C` names the Blueprint asset rather than the class you can
    instance.

    Every `BR` UI class is `UCLASS(Abstract)`, so a `BindWidget` typed `UBRProgressBar` can
    ONLY ever be satisfied by a generated child like this — never by the C++ class path.

    The folder is looked up from PLAN, NOT assumed to be UI_FOLDER. Hardcoding the root was
    invisible while every asset sat flat, and became three `high` findings the moment they
    moved into Layouts/, HUD/ and Components/: a host still asked for
    `/Game/UI/WBP_ProgressBar.WBP_ProgressBar_C` and the editor answered "not valid Class for
    property 'WidgetClass'" — correctly, because nothing lives there any more. A KeyError
    here is the right failure: a host referencing an asset outside the plan can never be
    built, and learning that at plan time beats learning it at call 40 with a half-built tree.
    """
    return f"{ASSET_FOLDER[asset]}/{asset}.{asset}_C"


def canvas_slot(left, top, width, height, anchor=(0.0, 0.0), align=(0.0, 0.0)):
    """A CanvasPanelSlot pinned to a point anchor.

    With min == max the anchor is a POINT, and UE then reads Offsets.Right/Bottom as the
    widget's SIZE, not as margins. That asymmetry is the classic source of a widget that
    looks right at 1280 and drifts at every other aspect ratio, so it is written once here
    rather than in every call site.

    `left`/`top` position the widget's ALIGNMENT point relative to the anchor point. So a
    bottom-right surface is anchor=(1,1), align=(1,1), left = (x + w) - 1280, and
    top = (y + h) - 720 — both negative, both measured, neither guessed.
    """
    return {"layoutData": {
        "offsets": {"left": float(left), "top": float(top),
                    "right": float(width), "bottom": float(height)},
        "anchors": {"minimum": {"x": float(anchor[0]), "y": float(anchor[1])},
                    "maximum": {"x": float(anchor[0]), "y": float(anchor[1])}},
        "alignment": {"x": float(align[0]), "y": float(align[1])}}}


def canvas_stretch(left=0.0, top=0.0, right=0.0, bottom=0.0) -> dict:
    """A CanvasPanelSlot stretched to its canvas: anchors (0,0)->(1,1).

    The OTHER canvas slot. With min != max the anchor is a BOX, and UE reads
    Offsets.Right/Bottom as MARGINS, not as a size - the exact inverse of `canvas_slot`.
    Written as its own function because the two disagree about what four identical floats
    mean, which is precisely the confusion `canvas_slot`'s docstring exists to prevent.
    """
    return {"layoutData": {
        "offsets": {"left": float(left), "top": float(top),
                    "right": float(right), "bottom": float(bottom)},
        "anchors": {"minimum": {"x": 0.0, "y": 0.0}, "maximum": {"x": 1.0, "y": 1.0}},
        "alignment": {"x": 0.0, "y": 0.0}}}


# ---------------------------------------------------------------------------
# SHARED TREES
# ---------------------------------------------------------------------------
# A tree defined once and used by more than one asset. There is exactly one today and it
# earns its keep: `WBP_MenuRow` and `WBP_SettingsRow` are the SAME widget tree under two
# different C++ parents, because `UBRSettingsRow` adds behaviour (it holds a data object and
# turns left/right into a value change) and not one new `BindWidget`. Copying the tree would
# mean every future COMPONENT-SPECS correction had to be made twice, and the second copy is
# the one that gets missed.

MENU_ROW_TREE = [
    {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},

    # Overlay child order IS z-order. Background line behind, then the plate that goes
    # solid white on hover, then the strokes ON the plate, then the text on top.
    {"name": "RowOverlay", "class": OVERLAY, "parent": "RootSizeBox", "slot": FILL},

    # COMPONENT-SPECS §2 hover: "an extra Background Line (opacity 0.3) appears behind".
    # Collapsed at idle by NativeOnInitialized — it exists to be shown, not to be seen.
    {"name": "BackgroundLine", "class": RULE, "parent": "RowOverlay",
     "slot": FILL, "bind": True},

    # The plate. NO BRUSH: `ApplyInvertedState` drives it with SetColorAndOpacity from
    # a token (None -> transparent idle, SurfaceInverted -> white on hover), so the
    # engine's default white brush is the correct input to that tint. Authoring a brush
    # here would be a second source for one colour.
    {"name": "TextFrameFill", "class": IMAGE, "parent": "RowOverlay",
     "slot": inset(2.0), "bind": True},

    {"name": "Border", "class": HAIRLINE, "parent": "RowOverlay",
     "slot": FILL, "bind": True},

    # COMPONENT-SPECS §2: padding T0 R10 B0 L10, counterAxis CENTER. The gap 10 is
    # expressed as padding on the children, not as a spacer (LAYOUT-DOCTRINE §1).
    {"name": "TextFrame", "class": HBOX, "parent": "RowOverlay",
     "slot": inset(2.0), "bind": True},

    # Fill 1.0 so the label takes the row's real width and `Selection` hugs the right
    # edge — which is what makes one row serve both a menu list and a settings list.
    {"name": "Label", "class": TEXT, "parent": "TextFrame",
     "slot": box_slot(fill=1.0, padding=margin(left=10.0), v="VAlign_Center"),
     "font": "Label/Button", "bind": True},

    # COMPONENT-SPECS §2: the right-aligned value on a settings row. Auto width, and
    # `SetSelectionText` collapses it when empty so a menu row shows nothing here.
    {"name": "Selection", "class": TEXT, "parent": "TextFrame",
     "slot": box_slot(padding=margin(left=10.0, right=10.0), h="HAlign_Right",
                      v="VAlign_Center"),
     "font": "Label/Button", "bind": True},
]


def unbound(nodes: list[dict]) -> list[dict]:
    """The same tree with every `bind` flag dropped, for a subclass that INHERITS its binds.

    `required_bind_widgets` parses ONE header sliced to ONE class and cannot see a base
    class's `BindWidget` members (the limitation `WBP_GearDetail` and `WBP_ItemTitle` already
    document). So a WBP whose parent inherits all its binds must create the widgets BY EXACT
    NAME but claim none of them: `bind: True` would be rejected as "declares no such
    BindWidget", while omitting the widgets would fail the asset at LOAD time.

    Naming them correctly satisfies UMG — `BindWidget` resolves by name at widget-compile,
    not by anything this plan writes — and satisfies the validator. It is the documented
    workaround, applied through a function so the next inherited-bind case does not re-derive
    it from scratch.
    """
    return [{k: v for k, v in node.items() if k != "bind"} for node in nodes]


# ---------------------------------------------------------------------------
# THE PLAN
# ---------------------------------------------------------------------------
# tree order IS z-order for an Overlay: earlier children render behind later ones.
# The layer order below is the whole point of the root layout — Game at the back,
# Modal in front — and it matches FBRUITags' declaration order in BRUITypes.h.
#
# PLAN ORDER IS BUILD ORDER. `build_wbp.py` walks this dict in insertion order, so any
# asset that HOSTS another (WBP_HUDLayout hosts six; three widgets host WBP_ProgressBar)
# must come after the thing it hosts. `validate_all()` enforces it rather than trusting it.

PLAN = {
    "WBP_RootLayout": {
        "folder": ASSET_FOLDER["WBP_RootLayout"],
        "parent_class": "/Script/Breachpoint.BRRootLayout",
        "class": "UBRRootLayout",
        "header": "Source/Breachpoint/UI/BRRootLayout.h",
        "notes": "Four CommonActivatableWidgetStacks in an Overlay. Overlay child "
                 "order is z-order: Game behind, Modal in front.",
        "tree": [
            {"name": "RootOverlay", "class": OVERLAY, "parent": None},
            {"name": "GameLayerStack", "class": STACK, "parent": "RootOverlay",
             "slot": FILL, "bind": True},
            {"name": "GameMenuLayerStack", "class": STACK, "parent": "RootOverlay",
             "slot": FILL, "bind": True},
            {"name": "MenuLayerStack", "class": STACK, "parent": "RootOverlay",
             "slot": FILL, "bind": True},
            {"name": "ModalLayerStack", "class": STACK, "parent": "RootOverlay",
             "slot": FILL, "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_ProgressBar — a PREREQUISITE, not a feature.
    #
    # `UBRProgressBar` is `UCLASS(Abstract)`, and three non-optional BindWidgets below are
    # typed to it (ShieldBar, HealthBar, CooldownBar). An abstract UserWidget cannot be
    # placed in a widget tree, so without ONE concrete generated child none of those three
    # assets can be built at all. This is that child, and it is the whole reason it exists.
    #
    # It carries exactly the one non-optional member `UBRProgressBar` declares. `Track`,
    # `Frame`, `ValueText` and `LabelText` are optional and are pure ART (brushes, hairline
    # stroke, fonts) — and this generator can only write SLOT properties, never brushes, so
    # authoring them here would produce named-but-blank widgets. They belong to the art pass.
    # ------------------------------------------------------------------
    "WBP_ProgressBar": {
        "folder": ASSET_FOLDER["WBP_ProgressBar"],
        "parent_class": "/Script/Breachpoint.BRProgressBar",
        "class": "UBRProgressBar",
        "header": "Source/Breachpoint/UI/Components/BRProgressBar.h",
        "notes": "The one concrete UBRProgressBar. Fill only; track/frame/readout are the "
                 "art pass. Must build BEFORE vitals and the equipment tray.",
        "tree": [
            {"name": "Fill", "class": PROGRESS, "parent": None, "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_MenuRow — the atom. SCREEN-MANIFEST §5 Tier 0: unblocks 26 of 31 screens.
    #
    # Geometry is COMPONENT-SPECS §2, measured off the reference file's live nodes:
    #   COMPONENT   250 x 28   (width is NOT authored — see below)
    #   Text Frame  (2,2) 246 x 24, auto-layout HORIZONTAL, gap 10, padding L10 R10,
    #               primaryAxis MIN, counterAxis CENTER
    #   Border      four vector lines, stroke align CENTER, side ticks 20 tall
    #
    # WIDTH IS DELIBERATELY NOT AUTHORED. The row is 250 in the component board and 349, 536 or
    # a column's Fill everywhere it is actually used. `UBRMenuRow::ApplyRowType` overrides width
    # ONLY for `IconOnly` (40x40) and clears it otherwise, which is the C++ saying the same
    # thing. A 250 here would break every wider list, silently, in the direction that looks fine
    # in the designer.
    #
    # HEIGHT IS C++-DRIVEN, and the asset therefore looks collapsed in the editor until it runs.
    # `SetHeightOverride` is a WIDGET property and this generator writes SLOT properties, fonts
    # and brushes only. Recording it rather than working around it: the height belongs on the
    # Type axis (28 / 40 / 60 / 120), so C++ is where it has to live regardless.
    #
    # FOUR OPTIONAL BINDS ARE DELIBERATELY OMITTED, and each omission is a decision:
    #   Icon, FilterButton — a UImage with no brush renders as a BLANK WHITE RECTANGLE. That is
    #     BP70's D2 defect exactly, and shipping it into the one component 26 screens instance
    #     would multiply it by 26. They land with their art, not before it.
    #   TypeSwitcher — one child per EBRMenuRowType, i.e. the other nine bodies. Those are
    #     Settings and MatchComposer (wave 6), and `ApplyRowType` already no-ops on a null
    #     switcher. The Default body is what the main menu needs.
    #   InvertAnim / DisclosureAnim — BindWidgetAnimOptional. A WidgetAnimation is not a widget
    #     and this generator cannot author one; `ApplyInvertedState` already guards on null and
    #     the inversion is fully correct without it (the animation only tweens it).
    # ------------------------------------------------------------------
    "WBP_MenuRow": {
        "folder": ASSET_FOLDER["WBP_MenuRow"],
        "parent_class": "/Script/Breachpoint.BRMenuRow",
        "class": "UBRMenuRow",
        "header": "Source/Breachpoint/UI/Components/BRMenuRow.h",
        "notes": "The 250x28 atom, Default type. Width unauthored (the row fills its rail); "
                 "height and the Type axis come from UBRMenuRow::ApplyRowType.",
        # SHARED WITH WBP_SettingsRow -- see MENU_ROW_TREE above. Edit it there, once.
        "tree": MENU_ROW_TREE,
    },

    # ==================================================================
    # THE MENU TIER (BP72 step 4). Order is load-bearing: validate_all()
    # requires a hosted asset to appear in PLAN BEFORE its host.
    #   leaves  -> NavTab, ButtonPrompt, RosterHeader, RosterRow
    #   hosts   -> NavBar, FeatureCard, RecordPanel, RosterPanel
    #   roots   -> LeftRail, Screen_FrontEnd
    # ==================================================================

    # ------------------------------------------------------------------
    # WBP_NavTab — one tab of the 666x30 bar. SCREEN-MANIFEST §5 Tier 1: 18 of 31 screens.
    #
    # NO SIZE IS AUTHORED, and that is C++ saying so: BRNavBar.cpp writes
    # SetWidthOverride(TabWidth=138)/SetHeightOverride(TabHeight=26) into RootSizeBox on
    # NativeOnInitialized. A 138 typed here would be a second source for a number four Menu
    # variants share. The asset therefore looks collapsed in the editor until it runs.
    #
    # `Icon` IS DELIBERATELY OMITTED. It is an art-bearing UImage with no texture, and a
    # brushless UImage renders as a blank white rectangle (BP70 D2) — 24x24 of white on
    # every tab of 18 screens. SetTabIcon COLLAPSES the slot for an unset soft ref anyway,
    # so the icon lands with its art or not at all.
    # ------------------------------------------------------------------
    "WBP_NavTab": {
        "folder": ASSET_FOLDER["WBP_NavTab"],
        "parent_class": "/Script/Breachpoint.BRNavTab",
        "class": "UBRNavTab",
        "header": "Source/Breachpoint/UI/Components/BRNavBar.h",
        "notes": "One 138x26 tab, closed rectangle border. Size unauthored — UBRNavTab "
                 "owns TabWidth/TabHeight. Icon omitted (art-bearing UImage, no texture).",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            # Overlay order is z-order: frame behind, label on top.
            {"name": "TabOverlay", "class": OVERLAY, "parent": "RootSizeBox", "slot": FILL},
            {"name": "Border", "class": HAIRLINE, "parent": "TabOverlay",
             "slot": FILL, "bind": True},
            # COMPONENT-SPECS §3: text (13,5) 120x14 in 138x26 -> padding L13 T5 R5 B7.
            # Written as PADDING, not a 120x14 size: the tab is 138 in the main bar and the
            # SubLevel bar is 516 wide, so the inset survives and the width does not.
            {"name": "Label", "class": TEXT, "parent": "TabOverlay",
             "slot": {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill",
                      "padding": margin(13.0, 5.0, 5.0, 7.0)},
             "font": "Label/Tab", "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_ButtonPrompt — glyph + verb. SCREEN-MANIFEST §5 Tier 0: on all 31 screens.
    #
    # WIDTH IS NEVER AUTHORED. The measured 62/146/253 at one, two and three prompts is the
    # OUTPUT of hugging, not an input. Height is C++'s (PromptHeight=20).
    #
    # `ActionGlyph` IS A UCommonActionWidget, NOT A UImage, and that is the whole component:
    # it draws the CURRENT input device's icon, so the prompt swaps keyboard<->gamepad with
    # no per-platform art and no raw key anywhere in the UI.
    #
    # `Verb` HAS NO FONT, AND THAT IS A RECORDED GAP. BP73 closed the DECIDE at Roboto
    # Condensed **Bold** 14, which is unexpressible twice over: figma_tokens.json has no
    # such row, and F_RobotoCondensed carries only {Medium, MediumItalic, SemiBold} — there
    # is no Bold typeface in the composite at all. Naming Body/Name here would be inventing
    # a measurement the ticket already measured differently.
    # ------------------------------------------------------------------
    "WBP_ButtonPrompt": {
        "folder": ASSET_FOLDER["WBP_ButtonPrompt"],
        "parent_class": "/Script/Breachpoint.BRButtonPrompt",
        "class": "UBRButtonPrompt",
        "header": "Source/Breachpoint/UI/Components/BRButtonPrompt.h",
        "notes": "Platform glyph + verb, hugging width, h20 from C++. Verb font is OFF-SYSTEM "
                 "(Roboto Cond Bold 14, BP73) and deliberately unwritten.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            # Hug, not fill: this box's width IS the prompt's width.
            {"name": "PromptHBox", "class": HBOX, "parent": "RootSizeBox", "slot": FILL},
            {"name": "ActionGlyph", "class": ACTION_GLYPH, "parent": "PromptHBox",
             "slot": box_slot(v="VAlign_Center"), "bind": True},
            # BP73 DECIDE 2, closed at 10: glyph ends x=20, verb starts x=30. Expressed as
            # the verb's left padding — LAYOUT-DOCTRINE §1 forbids a Spacer.
            {"name": "Verb", "class": TEXT, "parent": "PromptHBox",
             "slot": box_slot(padding=margin(left=10.0), v="VAlign_Center"), "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_RosterHeader — the group band above a roster's rows. 349 x 31 in-panel.
    #
    # The 31 IS NOT ONE BAND: BP73 read 28 (text) + 3 (gap) + 1 (line), so the separator
    # belongs to the header. RootSizeBox stays unauthored — C++ writes HeaderHeight=31 once.
    #
    # `BottomLine` IS NOT BOUND. UBRRosterHeader declares four BindWidgets and this is not
    # one of them, so bind:True would be a hard plan error. MCP-BUILD-PLANS §A4 draws it as
    # `bind?` and the doc is wrong. It ships as a pure-layout UBRRule.
    #
    # `Ground` CARRIES NO BRUSH — C++ tints the engine default white, which is the legal
    # case for a brushless UImage. DIVERGENCE: BP73 measured a vertical gradient white
    # 0.10 -> 0.30 and C++ applies a FLAT tint. Neither a plan nor a WBP can express a
    # gradient; filed, not faked.
    #
    # BOTH FONTS OFF-SYSTEM AND UNWRITTEN (BP73: Rajdhani Bold 18 / SemiBold 18; the token
    # ladder jumps Heading/Panel 16 -> Display/Title 20). Reaching for Heading/Caption is
    # precisely the guess BP73 caught — it was 6px short.
    # ------------------------------------------------------------------
    "WBP_RosterHeader": {
        "folder": ASSET_FOLDER["WBP_RosterHeader"],
        "parent_class": "/Script/Breachpoint.BRRosterHeader",
        "class": "UBRRosterHeader",
        "header": "Source/Breachpoint/UI/Components/BRRosterPanel.h",
        "notes": "Group band + 3px gap + 1px rule = the 31 C++ drives. Ground is brushless. "
                 "Both fonts OFF-SYSTEM (BP73), left unwritten.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            {"name": "HeaderVBox", "class": VBOX, "parent": "RootSizeBox", "slot": FILL},
            {"name": "BandOverlay", "class": OVERLAY, "parent": "HeaderVBox",
             "slot": box_slot(padding=margin(bottom=3.0))},
            {"name": "Ground", "class": IMAGE, "parent": "BandOverlay",
             "slot": FILL, "bind": True},
            {"name": "HeaderHBox", "class": HBOX, "parent": "BandOverlay",
             "slot": {"horizontalAlignment": "HAlign_Fill",
                      "verticalAlignment": "VAlign_Center",
                      "padding": margin(10.0, 0.0, 10.0, 0.0)}},
            {"name": "Label", "class": TEXT, "parent": "HeaderHBox",
             "slot": box_slot(v="VAlign_Center"), "bind": True},
            # Fill 1.0 + HAlign_Right is HOW the two share the band; a fixed-width count box
            # is the thing §A4's MUST-NOT names.
            {"name": "Count", "class": TEXT, "parent": "HeaderHBox",
             "slot": box_slot(fill=1.0, h="HAlign_Right", v="VAlign_Center"), "bind": True},
            # UNBOUND ON PURPOSE — no such member. Horizontal is UBRRule's default.
            {"name": "BottomLine", "class": RULE, "parent": "HeaderVBox", "slot": box_slot()},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_RosterRow — one player line. COMPONENT-SPECS §4 `Player Buttons`.
    #
    # WIDTH FILLS and is never authored (317 in the main-menu panel, different elsewhere).
    # Height is C++'s RowHeight=30; the pitch 35 is the PANEL's slot padding.
    #
    # SEVEN OPTIONAL BINDS OMITTED, ALL FOR ONE REASON: Emblem, RankFrame, RankInsignia,
    # MicSwitcher, ExternalIcons, PartyLeaderIcon, CurrentPlayerIcon are every one an
    # art-bearing UImage with no art — six blank white rectangles on every row of a six-row
    # panel is BP70 D2 multiplied by 36. C++ guards all seven on null.
    #
    # NO TEXT COLOUR ANYWHERE. The black/white flip is ComputeTextTone — one C++ decision,
    # not a design-time hex.
    # ------------------------------------------------------------------
    "WBP_RosterRow": {
        "folder": ASSET_FOLDER["WBP_RosterRow"],
        "parent_class": "/Script/Breachpoint.BRRosterRow",
        "class": "UBRRosterRow",
        "header": "Source/Breachpoint/UI/Components/BRRosterPanel.h",
        "notes": "Team plate + frame + gamertag. Width fills, height is C++'s RowHeight. "
                 "Seven art-bearing optional binds omitted until their art exists.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            {"name": "RowOverlay", "class": OVERLAY, "parent": "RootSizeBox", "slot": FILL},
            # ContentInset=2: COMPONENT-SPECS §4 puts TeamFill and Content both at (2,2).
            # The inset survives every row width; a 313 or a 345 would not.
            {"name": "TeamFill", "class": IMAGE, "parent": "RowOverlay",
             "slot": inset(2.0), "bind": True},
            {"name": "Border", "class": HAIRLINE, "parent": "RowOverlay",
             "slot": FILL, "bind": True},
            # Bound as UWidget, so an HBox satisfies it. Its own padding rides on the
            # children — a box has no padding property, only slots.
            {"name": "Content", "class": HBOX, "parent": "RowOverlay",
             "slot": inset(2.0), "bind": True},
            # Body/Name — gamertags are mixed-case proper nouns, so they take the BODY face,
            # never the all-caps Rajdhani chrome. Same reasoning as the killfeed's names.
            {"name": "Gamertag", "class": TEXT, "parent": "Content",
             "slot": box_slot(fill=1.0, padding=margin(left=5.0, right=15.0),
                              v="VAlign_Center"),
             "font": "Body/Name", "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_NavBar — SCREEN-MANIFEST §5 Tier 1: 18 of 31 screens.
    #
    # NO TABS IN THIS TREE. UBRNavBar::SetTabs creates every UBRNavTab from the soft
    # TabWidgetClass and slots it into TabContainer. A hand-placed tab would bind by name
    # into that array and double-render — the killfeed lesson exactly. An EMPTY container
    # is the finished state, not an unfinished one.
    #
    # `TabContainer` IS DECLARED UPanelWidget AND MUST STILL BE AN HBOX: BRNavBar.cpp casts
    # the new child's slot to UHorizontalBoxSlot to apply TabGap (12 = pitch 150 - tab 138).
    # An Overlay or VBox satisfies the bind type and SILENTLY drops the gap.
    #
    # THE 15px BUMPER/TAB OVERLAP IS NOT REPRODUCED. DECIDE row 9: bumper at x=27 and tabs
    # at x=39 overlap by 15, and an HBox lays children side by side — it cannot. Founder
    # call (an Overlay), not a number to fudge. No left padding invented to chase x=39
    # either: the `FirstTabOffsetX` the doc cites does NOT exist in BRNavBar.
    # ------------------------------------------------------------------
    "WBP_NavBar": {
        "folder": ASSET_FOLDER["WBP_NavBar"],
        "parent_class": "/Script/Breachpoint.BRNavBar",
        "class": "UBRNavBar",
        "header": "Source/Breachpoint/UI/Components/BRNavBar.h",
        "notes": "666x30 bar: two hosted button prompts around an EMPTY tab container. Size "
                 "unauthored — C++ picks 666 or the 516 sub-level width off bSubLevel.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            {"name": "BarHBox", "class": HBOX, "parent": "RootSizeBox", "slot": FILL},
            {"name": "BumperPrev", "class": wbp_class("WBP_ButtonPrompt"), "parent": "BarHBox",
             "slot": box_slot(padding=margin(left=27.0), h="HAlign_Left", v="VAlign_Center"),
             "bind": True},
            {"name": "TabContainer", "class": HBOX, "parent": "BarHBox",
             "slot": box_slot(fill=1.0), "bind": True},
            {"name": "BumperNext", "class": wbp_class("WBP_ButtonPrompt"), "parent": "BarHBox",
             "slot": box_slot(h="HAlign_Right", v="VAlign_Center"), "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_FeatureCard — the `News Button`, 349 x 222.
    #
    # The caption is OVERLAID on the art, not stacked under it (BP73 row 5 correction):
    # image full-bleed at inset 7, caption anchored to the BOTTOM of that same box. A VBox
    # would put the caption on a solid strip below the art, which is a different card.
    #
    # `ImageBox` IS CREATED even though MCP-BUILD-PLANS §B2's tree omits it: it is a
    # NON-OPTIONAL BindWidget and a missing one fails the WBP at ASSET LOAD — rung 1 stays
    # green and the card is simply blank. Doc defect, filed.
    #
    # ITS SLOT IS FILL, AND THAT IS A DECISION. C++ writes SetHeightOverride(196.7), a
    # constant traced to a HIDDEN Figma layer, while the visible art band is 186. A Fill
    # slot makes the measured 186 win and the suspect override inert; VAlign_Top would
    # honour 196.7 and overflow by 3.7. The constant is BP71's to fix.
    #
    # NO `Scrim` (the doc asks for one; no such member exists, it needs a Tier-4 gradient
    # material, and a brushless UImage is BP70 D2). NO `CaptionBox` (nothing binds it and
    # this generator cannot write SetHeightOverride — an unsized SizeBox is dead layout);
    # its measured px-20/py-10 is folded into the caption's own padding on top of inset 7.
    # ------------------------------------------------------------------
    "WBP_FeatureCard": {
        "folder": ASSET_FOLDER["WBP_FeatureCard"],
        "parent_class": "/Script/Breachpoint.BRFeatureCard",
        "class": "UBRFeatureCard",
        "header": "Source/Breachpoint/UI/Components/BRFeatureCard.h",
        "notes": "349x222 news card: caption overlaid on inset-7 art, dots rail below. "
                 "Height and every colour come from C++.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            {"name": "CardVBox", "class": VBOX, "parent": "RootSizeBox", "slot": FILL},
            # Fill 1.0 -> 200 tall (222 - the dots' 22). Overlay order IS z-order.
            {"name": "ButtonRegion", "class": OVERLAY, "parent": "CardVBox",
             "slot": box_slot(fill=1.0)},
            {"name": "Border", "class": HAIRLINE, "parent": "ButtonRegion",
             "slot": FILL, "bind": True},
            # NO brush: C++ tints PanelGround50 onto the engine default white.
            {"name": "Ground", "class": IMAGE, "parent": "ButtonRegion",
             "slot": inset(7.0), "bind": True},
            {"name": "ImageBox", "class": SIZEBOX, "parent": "ButtonRegion",
             "slot": inset(7.0), "bind": True},
            # NO brush: the VM pushes a soft texture path and C++ ships this Hidden (not
            # Collapsed — the band keeps its space) until it resolves.
            {"name": "FeatureImage", "class": IMAGE, "parent": "ImageBox",
             "slot": FILL, "bind": True},
            # Label/Button is the one font of BP73's four that is ON-SYSTEM. Its token case
            # is UPPER, which UMG cannot apply — the FText arrives upper-cased from the VM.
            {"name": "Caption", "class": TEXT, "parent": "ButtonRegion",
             "slot": {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Bottom",
                      "padding": margin(27.0, 10.0, 27.0, 17.0)},
             "font": "Label/Button", "bind": True},
            # The 72x10 switcher rail. EMPTY: dots are per-carousel-entry, the screen fills
            # this. Its measured h22 is not authored — an empty panel has no height to
            # override and the dots bring their own 6x6.
            {"name": "DotsContainer", "class": HBOX, "parent": "CardVBox",
             "slot": box_slot(padding=margin(12.0, 2.0, 12.0, 2.0), h="HAlign_Center"),
             "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_RecordPanel — SAME PARENT CLASS AS WBP_FEATURECARD, ON PURPOSE.
    # UBRScreen_FrontEnd::ProgressionButton is already TObjectPtr<UBRFeatureCard>, so this
    # is one asset and ZERO new classes. Because the parent is shared, the BindWidget
    # contract is IDENTICAL — same five required names, same two optional. Only art and
    # slot values differ, and a rename in BRFeatureCard.h breaks both entries in one run.
    #
    # THE 334 x 115 IS NOT AUTHORED, AND CANNOT BE: this generator writes no size overrides,
    # and UBRFeatureCard::NativeOnInitialized unconditionally sets CardHeight=222 and clears
    # the width. No constant for 334x115 exists anywhere in Source/. So this asset runs 222
    # tall until C++ gains a per-instance height. Typing 115 here would be a number invented
    # under deadline. contract_gap filed.
    # ------------------------------------------------------------------
    "WBP_RecordPanel": {
        "folder": ASSET_FOLDER["WBP_RecordPanel"],
        "parent_class": "/Script/Breachpoint.BRFeatureCard",
        "class": "UBRFeatureCard",
        "header": "Source/Breachpoint/UI/Components/BRFeatureCard.h",
        "notes": "The SERVICE RECORD panel — WBP_FeatureCard's tree with a Heading/Panel "
                 "title. Same C++ class, same binds; 334x115 has no C++ owner yet (gap).",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            {"name": "CardVBox", "class": VBOX, "parent": "RootSizeBox", "slot": FILL},
            {"name": "ButtonRegion", "class": OVERLAY, "parent": "CardVBox",
             "slot": box_slot(fill=1.0)},
            {"name": "Border", "class": HAIRLINE, "parent": "ButtonRegion",
             "slot": FILL, "bind": True},
            {"name": "Ground", "class": IMAGE, "parent": "ButtonRegion",
             "slot": inset(7.0), "bind": True},
            {"name": "ImageBox", "class": SIZEBOX, "parent": "ButtonRegion",
             "slot": inset(7.0), "bind": True},
            # NO brush — T_Logo_Mark is the art pass, and the 167x94 halves are the ART's
            # internal layout, not two widgets.
            {"name": "FeatureImage", "class": IMAGE, "parent": "ImageBox",
             "slot": FILL, "bind": True},
            # THE ONE ART DELTA FROM WBP_FeatureCard: this caption is a TITLE, not a news
            # line. The drop shadow COMPONENT-SPECS names is a style property, not a second
            # text widget, and this generator writes neither.
            {"name": "Caption", "class": TEXT, "parent": "ButtonRegion",
             "slot": {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Bottom",
                      "padding": margin(27.0, 10.0, 27.0, 17.0)},
             "font": "Heading/Panel", "bind": True},
            {"name": "DotsContainer", "class": HBOX, "parent": "CardVBox",
             "slot": box_slot(padding=margin(12.0, 2.0, 12.0, 2.0), h="HAlign_Center"),
             "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_RosterPanel — `Party List`, 349 x 273.
    #
    # NO ROWS IN THIS TREE. RowPool (FUserWidgetPool) claims WBP_RosterRows from the soft
    # RowWidgetClass into RowContainer and owns the measured pitch 35 as slot padding.
    # Hand-placed rows would be cleared by the first rebuild. RowContainer MUST be a
    # UVerticalBox — the bind is typed to it and the pool casts the slot.
    #
    # `Ground`'S SLOT IS FILL, NOT inset(3). BRRosterPanel.cpp pushes FMargin(3) into the
    # Overlay slot itself from ApplyPanelGeometry, which NativePreConstruct also calls — so
    # the designer sees it too. Typing 3 here would be a second source for one measured
    # number. MCP-BUILD-PLANS §B4 says inset 3; the doc is wrong.
    #
    # CONTENT INSET IS 16, NOT 3 — the doc conflates the two. 349 - 2*16 = the measured 317
    # header, and HeaderY + HeaderHeight + RowGap = the measured FirstRowY of 52 (there is a
    # file-scope static_assert). Nothing in the .cpp consumes ContentInset, so the WBP is the
    # only place it can live.
    #
    # NO `GradientFill`: needs a Tier-4 gradient material, and a brushless UImage would be a
    # blank white rectangle over the WHOLE panel. Optional bind, C++ guards null.
    # ------------------------------------------------------------------
    "WBP_RosterPanel": {
        "folder": ASSET_FOLDER["WBP_RosterPanel"],
        "parent_class": "/Script/Breachpoint.BRRosterPanel",
        "class": "UBRRosterPanel",
        "header": "Source/Breachpoint/UI/Components/BRRosterPanel.h",
        "notes": "349x273 party list: hosted header over an EMPTY pooled row column plus the "
                 "honest empty-state line. Size, ground inset and row pitch are C++-owned.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            {"name": "PanelOverlay", "class": OVERLAY, "parent": "RootSizeBox", "slot": FILL},
            {"name": "Ground", "class": IMAGE, "parent": "PanelOverlay",
             "slot": FILL, "bind": True},
            {"name": "PanelBorder", "class": HAIRLINE, "parent": "PanelOverlay",
             "slot": FILL, "bind": True},
            {"name": "ContentVBox", "class": VBOX, "parent": "PanelOverlay",
             "slot": {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill",
                      "padding": margin(16.0, 16.0, 16.0, 0.0)}},
            # The 5px bottom padding is UBRRosterRow::RowGap and it is the HEADER's to carry:
            # C++ only pads ROWS, and 16 + 31 + 5 = the measured first row y of 52.
            {"name": "Header", "class": wbp_class("WBP_RosterHeader"), "parent": "ContentVBox",
             "slot": box_slot(padding=margin(bottom=5.0)), "bind": True},
            {"name": "RowContainer", "class": VBOX, "parent": "ContentVBox",
             "slot": box_slot(fill=1.0), "bind": True},
            # TWO DIFFERENT FACTS, ONE WIDGET: the dash while Unknown, "NO PLAYERS" when the
            # feed says live-but-empty, Collapsed otherwise. C++ owns all three, so this
            # ships visible-and-dashed and never lies during travel or a mid-match join.
            {"name": "EmptyStateLabel", "class": TEXT, "parent": "ContentVBox",
             "slot": box_slot(padding=margin(top=8.0), h="HAlign_Center"),
             "font": "Body/Flavor", "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_LeftRail — the `Menu Combo`, and the ONE component-root CanvasPanel in the menu.
    #
    # The canvas is REQUIRED, not tolerated (LAYOUT-DOCTRINE law 6's documented exception):
    # ApplyCaret writes SelectionCaret's position and size through a canvas slot — 3 x 65 at
    # rail-local x = -4, y computed from the focused row. A box layout cannot express "an
    # arbitrary y", and the header's LAYOUT CONTRACT names the requirement outright. Nobody
    # may "fix" this by flattening it.
    #
    # NOTHING SIZES THE RAIL HERE. ApplyRailLayout writes RootSizeBox's width (349) AND its
    # height (ComputeRailHeight(RowCount, FeatureCard != nullptr)), plus MenuBorderBox's
    # height from the row count. All are widget properties this generator cannot write.
    #
    # NO ROWS AND NO DESCRIPTION TEXT: a FUserWidgetPool fills MenuRowSlot and C++ fills
    # DescriptionSlot. A hand-placed row would bind by name into the pool's array and
    # double-render — the killfeed lesson.
    #
    # RevealNotchTop/Bottom OMITTED: chamfer ART whose x is unmeasured by the header's own
    # admission, and the wipe originating from them is a BindWidgetAnimOptional this
    # generator cannot author. They land with the art pass and the animation, together.
    # ------------------------------------------------------------------
    "WBP_LeftRail": {
        "folder": ASSET_FOLDER["WBP_LeftRail"],
        "parent_class": "/Script/Breachpoint.BRLeftRail",
        "class": "UBRLeftRail",
        "header": "Source/Breachpoint/UI/Components/BRLeftRail.h",
        "notes": "Feature card / bordered menu panel / description strip in one 349-wide "
                 "instrument. Root CanvasPanel is the documented law-6 exception.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            {"name": "RailCanvas", "class": CANVAS, "parent": "RootSizeBox", "slot": FILL},
            # Stretched so the rail's C++-driven height flows into it; the caret is a
            # SIBLING of this box, not a child, so it can sit at x = -4.
            {"name": "RailVBox", "class": VBOX, "parent": "RailCanvas",
             "slot": canvas_stretch()},
            # Only the 10px FeatureCardGap is expressible here. Its PRESENCE is the single
            # source of truth for whether the 222+10 block is in the stack.
            {"name": "FeatureCard", "class": wbp_class("WBP_FeatureCard"), "parent": "RailVBox",
             "slot": box_slot(padding=margin(bottom=10.0)), "bind": True},
            # Auto: C++ calls SetHeightOverride(ComputeMenuBorderHeight(RowCount)) — 186 at
            # 4 rows. Authoring 186 would pin the panel at a row count that is data.
            {"name": "MenuBorderBox", "class": SIZEBOX, "parent": "RailVBox",
             "slot": box_slot(), "bind": True},
            {"name": "MenuBorderOverlay", "class": OVERLAY, "parent": "MenuBorderBox",
             "slot": FILL},
            {"name": "MenuBorder", "class": HAIRLINE, "parent": "MenuBorderOverlay",
             "slot": FILL, "bind": True},
            # DECIDE 8, CLOSED (BP73): the inset is 16 UNIFORM, not 16/22 — the unread
            # MenuRowSlotInsetRight=22 was deleted from the header for disagreeing with the
            # file. One constant, so this is inset(), not four numbers.
            {"name": "MenuRowSlot", "class": VBOX, "parent": "MenuBorderOverlay",
             "slot": inset(16.0), "bind": True},
            # DescriptionGap 55. The strip's own 37 comes from the UBRDescriptionStrip
            # another packet drops in here.
            {"name": "DescriptionSlot", "class": OVERLAY, "parent": "RailVBox",
             "slot": box_slot(padding=margin(top=55.0)), "bind": True},
            # AUTHORED LAST: canvas child order is z-order and the caret hugs the panel from
            # OUTSIDE it (x = -4), so it must draw in front. A PLACEHOLDER at the header's
            # constants — ApplyCaret rewrites position and size on every focus change.
            {"name": "SelectionCaret", "class": RULE, "parent": "RailCanvas",
             "slot": canvas_slot(-4.0, 0.0, 3.0, 65.0), "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_MainMenu — FE_Play, 1:1. LAST in PLAN: hosts the rail, the nav bar, the record
    # panel and the party list.
    #
    # *** ROOT CanvasPanel — a DOCUMENTED EXCEPTION to LAYOUT-DOCTRINE §6, founder-granted
    # 3 Aug 2026, in the same shape as UBRLeftRail's. ***
    #
    # This screen was FIRST built as bands and columns and it was wrong, structurally rather
    # than numerically. FE_Play (working file yznvnVdOFDADaugZSeomfP, node 21:32824, read
    # 3 Aug) places `Progression Button` at y=55 while the `Navigation Bar` occupies y=45..75:
    # THE TWO OVERLAP VERTICALLY. A VBox stacks, so no padding, alignment or fill can put a
    # sibling level with an earlier sibling — the band shape could not express the design at
    # any set of numbers. §6 exists to stop a canvas being the DEFAULT, not to stop it where
    # the design IS absolute geometry, which this frame is.
    #
    # THE COLUMNS WERE A GUIDE, NOT THE LAYOUT. `Grid - 3 Collumn` (21:32868) is HIDDEN in
    # the frame, as is `Grid - 4 Collumn`. The first build read the guide as the design. That
    # also explains the 869 vs 862 below: the two right-hand elements are NOT left-aligned
    # with each other, which no single column with one padding can produce.
    #
    # Every coordinate here is frame-local against 1280x720 and comes from that one read.
    # canvas_slot() is point-anchored, so Right/Bottom are SIZE, not margins.
    #
    # NOT IN THIS TREE, and each for its own reason:
    #   Background (21:32825) — an art instance with no texture in Content/UI; a brushless
    #     UImage is BP70 D2's blank white rectangle across the whole screen.
    #   Player (21:32827, 480,118 320x602) — the 3D subject's CAMERA box, not a widget.
    #   Profile Bar (21:32862) + Button Prompts (21:32863) — root-layout chrome by
    #     SCREEN-MANIFEST §7.1, and UBRScreen_FrontEnd declares no member for either. Their
    #     absence is why a render looks emptier than the frame even where geometry is exact.
    #
    # KNOWN DISAGREEMENT, RECORDED NOT SILENCED: the frame gives ProgressionButton 334x115,
    # and UBRFeatureCard::NativeOnInitialized unconditionally writes SetHeightOverride(222)
    # and clears the width. So C++ WILL override this slot's 115 at runtime. The slot carries
    # the measured value because that is what the design says; the override is the open gap
    # (no per-instance height on UBRFeatureCard), and it is visible rather than papered over.
    # ------------------------------------------------------------------
    "WBP_MainMenu": {
        "folder": ASSET_FOLDER["WBP_MainMenu"],
        "parent_class": "/Script/Breachpoint.BRScreen_FrontEnd",
        "class": "UBRScreen_FrontEnd",
        "header": "Source/Breachpoint/UI/Screens/BRScreen_FrontEnd.h",
        "notes": "FE_Play 1:1 from node 21:32824. Root CanvasPanel by founder exception to "
                 "§6 — the frame is absolute geometry and Progression Button overlaps the "
                 "nav bar, which a band layout cannot express.",
        "tree": [
            # THE SCALE CHAIN, and it is the whole reason the first canvas build rendered
            # "smaller" than the reference. canvas_slot() is POINT-anchored: offsets are
            # absolute pixels from the top-left. At 1280x720 that is exact; at 1920x1080 the
            # entire layout stays at 1280x720 scale in the corner and the screen is mostly
            # empty. Absolute coordinates are only meaningful WITH the space they were
            # measured in, so the space is declared here and scaled once.
            #
            # ScaleBox(ScaleToFit, the default) > SizeBox(1280x720) > CanvasPanel. The canvas
            # always gets exactly its design space, every child lands on its measured pixel,
            # and one widget maps that space onto the viewport. 1280x720 -> 1920x1080 is 1.5x
            # exactly, both 16:9, so nothing is letterboxed and nothing is cropped.
            #
            # NOT normalized anchors (x/1280, y/720): those stretch NON-uniformly, so a 21:9
            # display would smear the nav bar wide while leaving it 30 tall. Uniform scale is
            # what "1:1 with the frame" actually means.
            {"name": "ScreenScale", "class": SCALEBOX, "parent": None},
            # The design space itself. widthOverride/heightOverride verified by
            # ObjectTools.list_properties before writing - a guessed camelCase name fails
            # SILENTLY here and would leave the box hugging its canvas, i.e. zero-sized.
            # NO SLOT: a UScaleBoxSlot rejects the standard HAlign/VAlign/padding write and
            # reads back null - the same measured behaviour as USafeZoneSlot. A ScaleBox
            # scales its single child anyway, so the write was redundant as well as
            # impossible. Do not "restore" it.
            {"name": "DesignSpace", "class": SIZEBOX, "parent": "ScreenScale",
             "properties": {"widthOverride": DESIGN_W, "heightOverride": DESIGN_H}},
            {"name": "ScreenCanvas", "class": CANVAS, "parent": "DesignSpace", "slot": FILL},
            # Navigation Bar 21:32864 — 33, 45, 666x30.
            {"name": "NavBar", "class": wbp_class("WBP_NavBar"), "parent": "ScreenCanvas",
             "slot": canvas_slot(33.0, 45.0, 666.0, 30.0), "bind": True},
            # Menu Combo 21:32877 — 69, 138, 349x510. The 510 is a CROSS-CHECK, not a second
            # source: ComputeRailHeight(4 rows, with feature card) = 222+10+186+38+55+37 =
            # 510, so C++ and the frame already agree and the slot restates the agreement.
            {"name": "LeftRail", "class": wbp_class("WBP_LeftRail"), "parent": "ScreenCanvas",
             "slot": canvas_slot(69.0, 138.0, 349.0, 510.0), "bind": True},
            # Progression Button 21:32826 — 869, 55, 334x115. See the height note above.
            {"name": "ProgressionButton", "class": wbp_class("WBP_RecordPanel"),
             "parent": "ScreenCanvas",
             "slot": canvas_slot(869.0, 55.0, 334.0, 115.0), "bind": True},
            # Party List 21:32861 — 862, 397, 349x273. Seven pixels left of the Progression
            # Button, which is the measurement and not a typo.
            {"name": "PartyList", "class": wbp_class("WBP_RosterPanel"),
             "parent": "ScreenCanvas",
             "slot": canvas_slot(862.0, 397.0, 349.0, 273.0), "bind": True},
            # WHICH KIND OF NOTHING: never-told (VM null) versus told-and-empty are two
            # different facts and ApplyEmptyState renders them differently. The frame
            # measures no rect for it — it is a state the design does not draw — so it is
            # centred on the screen and that is a stated choice, not a measurement.
            {"name": "EmptyStateLabel", "class": TEXT, "parent": "ScreenCanvas",
             "slot": canvas_slot(0.0, 0.0, 1280.0, 720.0),
             "font": "Body/Flavor", "bind": True},
        ],
    },

    # ==================================================================
    # THE CHROME + ITEM TIER. WBP_ItemTile MUST precede WBP_ItemGrid: the tile is NOT a tree
    # child of the grid (a UTileView owns its entries), so validate_all()'s host-ordering
    # rule never fires and would not catch a wrong order.
    # ==================================================================

    # ------------------------------------------------------------------
    # WBP_Panel — the bordered container 45 measured variants collapse into.
    #
    # THERE IS NO `Ground` IMAGE AND THAT IS THE CLASS'S WHOLE ARGUMENT. UBRHairlineBorder
    # draws the token-coloured fill BEHIND its own strokes (FBRHairlineStyle::FillToken), so
    # a panel's ground and its border are ONE widget and one Slate draw element. A UImage
    # plate here would fork the paint path the stroke budget is measured against — and it
    # would be a brushless UImage (BP70 D2) in the one component every screen wraps.
    #
    # NO SIZE, AND NOT FOR THE USUAL REASON. SetPanelHeight writes SetHeightOverride but
    # NOTHING CALLS IT at init — NativeOnInitialized only styles the frame. So an unsized
    # RootSizeBox is a pass-through and the panel hugs whatever the caller drops in Content,
    # which is correct: width is layout and height is per-variant. RootSizeBox is created
    # anyway because it is the ONLY thing that makes SetPanelHeight work — omit it and the
    # API silently no-ops on null.
    # ------------------------------------------------------------------
    "WBP_Panel": {
        "folder": ASSET_FOLDER["WBP_Panel"],
        "parent_class": "/Script/Breachpoint.BRPanel",
        "class": "UBRPanel",
        "header": "Source/Breachpoint/UI/Components/BRPanel.h",
        "notes": "Ground + border in ONE UBRHairlineBorder, plus a NamedSlot for content. No "
                 "size — SetPanelHeight is the only writer and nothing calls it at init.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            {"name": "PanelOverlay", "class": OVERLAY, "parent": "RootSizeBox", "slot": FILL},
            {"name": "Frame", "class": HAIRLINE, "parent": "PanelOverlay",
             "slot": FILL, "bind": True},
            {"name": "Content", "class": NAMED_SLOT, "parent": "PanelOverlay",
             "slot": FILL, "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_ScrollBar — two widgets, both required, nothing optional. The smallest entry here.
    #
    # NEITHER NUMBER IS AUTHORED. ApplyWeight writes the track thickness onto the SizeBox and
    # the thumb thickness onto the bar. Typing 8 or 13 would let a screen half-adopt the
    # design — the class exists precisely so picking a weight gets you BOTH.
    #
    # THIS ASSET SCROLLS NOTHING BY ITSELF, by the class's own contract — the owning list
    # drives it via GetScrollBar(). Its one C++ consumer today (UBRItemGrid::ScrollBar) is
    # COLLAPSED and left collapsed, because UMG exposes no public path from a UTileView to an
    # external bar. So this builds correct and renders nowhere; that gap is BRItemGrid.h's.
    # ------------------------------------------------------------------
    "WBP_ScrollBar": {
        "folder": ASSET_FOLDER["WBP_ScrollBar"],
        "parent_class": "/Script/Breachpoint.BRScrollBar",
        "class": "UBRScrollBar",
        "header": "Source/Breachpoint/UI/Components/BRScrollBar.h",
        "notes": "The wrapped engine bar in its rail. Both measured thicknesses are C++-owned "
                 "(ApplyWeight), so the asset carries no size at all.",
        "tree": [
            {"name": "Track", "class": SIZEBOX, "parent": None, "bind": True},
            {"name": "ScrollBar", "class": SCROLLBAR, "parent": "Track",
             "slot": FILL, "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_PageTitle — the 1280 x 75 title band. SCREEN-MANIFEST §5 Tier 1: 15 of 31 screens.
    #
    # THE `class` KEY IS LOAD-BEARING. BRPageTitle.h declares UBRPageTitle AND UBRItemTitle;
    # without the slice this entry inherits the item title's RarityTagBorder / RarityLabel /
    # EquippedCheck. All three are Optional, so the plan would PASS while quietly asking a
    # page title to grow a rarity tag — the BRHUDLayout.h bug in a costume that does not error.
    #
    # NO WIDTH, EVER: the band is full-bleed and its width comes from the screen's top-stretch
    # anchor (a 1280 override breaks every aspect wider than 16:9). The HEIGHT is C++'s
    # GetBandHeight(), the ONE virtual UBRItemTitle overrides.
    #
    # EVERY INTERNAL OFFSET IS UNMEASURED AND NONE IS INVENTED. The header is explicit that
    # the band's outer size is all the reference gives. So this authors STRUCTURE and ZERO
    # padding numbers. The one structural call not backed by a read is the stack order — 75px
    # of band for a 24px title is room for two lines, and a breadcrumb is the parent LEVEL,
    # not a prefix. Flip it when the node is read.
    # ------------------------------------------------------------------
    "WBP_PageTitle": {
        "folder": ASSET_FOLDER["WBP_PageTitle"],
        "parent_class": "/Script/Breachpoint.BRPageTitle",
        "class": "UBRPageTitle",
        "header": "Source/Breachpoint/UI/Components/BRPageTitle.h",
        "notes": "Full-bleed 1280x75 band: breadcrumb over title, vertically centred. Height "
                 "is C++'s GetBandHeight(); no internal offset is authored — none is measured.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            # VAlign_Center on the GROUP, so the pair stays centred in a 75 band and in the
            # 105 one UBRItemTitle swaps in, without a second number.
            {"name": "TitleVBox", "class": VBOX, "parent": "RootSizeBox",
             "slot": {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Center",
                      "padding": margin()}},
            # NO FONT. The ramp records PAGE TITLE and ITEM TITLE and records nothing for a
            # breadcrumb. Label/Micro would be a near-token substituted for a measurement —
            # the move BP73 caught 6px short on the roster header.
            {"name": "Breadcrumb", "class": TEXT, "parent": "TitleVBox",
             "slot": box_slot(h="HAlign_Left"), "bind": True},
            # Display/Page Title is Rajdhani SemiBold 24 @100/1000em — the EXACT pair the
            # header records as TitleFontSizePx / TitleLetterSpacingPerMille.
            {"name": "Title", "class": TEXT, "parent": "TitleVBox",
             "slot": box_slot(h="HAlign_Left"),
             "font": "Display/Page Title", "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_ItemTitle — the 1280 x 105 band. SAME HEADER AS WBP_PageTitle, SECOND CLASS.
    #
    # READ THIS BEFORE TOUCHING THE `bind` FLAGS. class_body() slices ONE class and this
    # validator has NO base-class chain. So required_bind_widgets(BRPageTitle.h,
    # "UBRItemTitle") sees only the three rarity/equipped members — every one Optional — and
    # reports ZERO required binds. UMG does NOT agree: `Title` is a non-optional BindWidget on
    # UBRPageTitle, it is INHERITED, and a WBP_ItemTitle without one FAILS AT ASSET LOAD with
    # rung 1 still green.
    # Hence Title, RootSizeBox and Breadcrumb are CREATED but NOT marked bind:True — marking
    # them fails the plan ("declares no such BindWidget"), omitting them fails the engine.
    # They bind by NAME at compile, which is what CompileWidgetBlueprint proves.
    # contract_gap: this validator needs to walk base classes. It affects EVERY future
    # subclass entry — UBRGearDetail : UBRPanel inherits a required `Frame` the same way.
    # ------------------------------------------------------------------
    "WBP_ItemTitle": {
        "folder": ASSET_FOLDER["WBP_ItemTitle"],
        "parent_class": "/Script/Breachpoint.BRItemTitle",
        "class": "UBRItemTitle",
        "header": "Source/Breachpoint/UI/Components/BRPageTitle.h",
        "notes": "The 105 band: item name + rarity tag. Title/RootSizeBox/Breadcrumb are "
                 "INHERITED binds this validator cannot see — created unbound on purpose.",
        "tree": [
            # UNBOUND, INHERITED, AND REQUIRED BY THE ENGINE ANYWAY — see the block above.
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None},
            {"name": "TitleVBox", "class": VBOX, "parent": "RootSizeBox",
             "slot": {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Center",
                      "padding": margin()}},
            {"name": "Breadcrumb", "class": TEXT, "parent": "TitleVBox",
             "slot": box_slot(h="HAlign_Left")},
            # Name and tag on ONE line: SetItem writes both in a single call, so a title can
            # never show one item's name over another item's rarity.
            {"name": "TitleRow", "class": HBOX, "parent": "TitleVBox", "slot": box_slot()},
            # Display/Item Title — Rajdhani REGULAR 48 @100/1000em. Regular, not SemiBold:
            # the item title is the one place the ramp drops the chrome weight.
            {"name": "Title", "class": TEXT, "parent": "TitleRow",
             "slot": box_slot(v="VAlign_Center"), "font": "Display/Item Title"},
            {"name": "RarityTag", "class": OVERLAY, "parent": "TitleRow",
             "slot": box_slot(v="VAlign_Center")},
            {"name": "RarityTagBorder", "class": HAIRLINE, "parent": "RarityTag",
             "slot": FILL, "bind": True},
            # NO FONT: the rarity word has no measured size and no token row. C++ ships it
            # Collapsed until SetItem supplies a label, so the fallback face is invisible
            # until an item is actually pushed.
            {"name": "RarityLabel", "class": TEXT, "parent": "RarityTag",
             "slot": {"horizontalAlignment": "HAlign_Center", "verticalAlignment": "VAlign_Center",
                      "padding": margin()}, "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_SmallHeader — the 1180 x 27 column-caption band above a table.
    #
    # THE `class` SLICE IS THE WORST CASE IN THE PLAN. BRTableRow.h declares THREE classes,
    # and parsing it whole hands this band UBRTableRow's Border / RowFill / NameText as
    # NON-OPTIONAL — three table-row widgets the generator would dutifully create inside a
    # caption band, one of them a brushless UImage. Measured both ways, not assumed.
    #
    # `CaptionContainer` IS OMITTED. Nothing in C++ ever touches it, and the column stops its
    # children need are UNMEASURED sentinels — UBRTableRow makes them NEGATIVE so they cannot
    # be quietly passed to SetWidthOverride and look plausible. An empty panel with no filler
    # and no measurements is dead layout. It lands in the packet that reads the node and
    # drives the row columns, because the two MUST come from one set of numbers.
    # ------------------------------------------------------------------
    "WBP_SmallHeader": {
        "folder": ASSET_FOLDER["WBP_SmallHeader"],
        "parent_class": "/Script/Breachpoint.BRSmallHeader",
        "class": "UBRSmallHeader",
        "header": "Source/Breachpoint/UI/Components/BRTableRow.h",
        "notes": "1180x27 caption band. Size is C++'s; the caption container waits on the "
                 "unmeasured column stops and the font on the band's own read.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            {"name": "HeaderOverlay", "class": OVERLAY, "parent": "RootSizeBox", "slot": FILL},
            # VAlign_Bottom, not Fill: a UBRRule's desired size is its own THICKNESS on both
            # axes, so Fill on the major axis stretches it to 1180 while the minor stays 1px
            # and the line sits ON the band's lower edge. A Fill slot would stretch it to 27
            # tall and draw its top edge at the TOP.
            {"name": "Rule", "class": RULE, "parent": "HeaderOverlay",
             "slot": {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Bottom",
                      "padding": margin()}, "bind": True},
            # No padding: the band's internal inset is unmeasured and a guessed 10 or 16 would
            # read as measurement. Authored AFTER the rule — overlay order is z-order.
            {"name": "HeaderText", "class": TEXT, "parent": "HeaderOverlay",
             "slot": {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Center",
                      "padding": margin()}, "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_HighlightButton — the page-level CTA (SCREEN-MANIFEST §5 Tier 1: 11 of 31).
    #
    # NO SIZE ANYWHERE, AND NO RootSizeBox EITHER. The header states it: the reference lists
    # `Highlight Buttons` WITHOUT measured dimensions, filed as a contract gap. There is no
    # C++ member for a root box, so an unbound SizeBox would be an UNSIZED SizeBox — dead
    # layout, the thing WBP_FeatureCard's CaptionBox was cut for. The Overlay hugs the label.
    # HONEST CONSEQUENCE: the button is text-sized, so a host wanting the reference's box
    # must slot it.
    #
    # `Fill`'s SLOT IS FILL, NOT inset(2). WBP_MenuRow's 2px is a MEASURED border gap on a
    # 250x28 row; nothing measures this component, so borrowing the 2 would be a number with
    # no source here — the one thing this file never does.
    #
    # Icon OMITTED: an art-bearing UImage whose .cpp never references it — no tint, no
    # collapse — so unlike `Fill` there is nothing making a brushless image legal. It would
    # be BP70 D2 on all 11 screens. TypeSwitcher OMITTED: six bodies, five identical (Event
    # and Premium have no accent token), and ApplyButtonType guards null.
    # ------------------------------------------------------------------
    "WBP_HighlightButton": {
        "folder": ASSET_FOLDER["WBP_HighlightButton"],
        "parent_class": "/Script/Breachpoint.BRHighlightButton",
        "class": "UBRHighlightButton",
        "header": "Source/Breachpoint/UI/Components/BRHighlightButton.h",
        "notes": "Invertible CTA: plate + 4-line border + label. NO box is authored — the "
                 "reference measures none, so the button hugs its label (contract gap).",
        "tree": [
            # Overlay child order IS z-order: plate behind, chrome on it, label on top.
            {"name": "ButtonOverlay", "class": OVERLAY, "parent": None},
            # NO BRUSH: ApplyInvertedState drives this with SetColorAndOpacity from a token,
            # so the engine default white brush is the correct input to that tint.
            {"name": "Fill", "class": IMAGE, "parent": "ButtonOverlay",
             "slot": FILL, "bind": True},
            {"name": "Border", "class": HAIRLINE, "parent": "ButtonOverlay",
             "slot": FILL, "bind": True},
            # 10px L/R is the row family's shared text inset — a PADDING, not a size, so it
            # survives whatever box eventually gets measured.
            {"name": "Label", "class": TEXT, "parent": "ButtonOverlay",
             "slot": {"horizontalAlignment": "HAlign_Center",
                      "verticalAlignment": "VAlign_Center",
                      "padding": margin(left=10.0, right=10.0)},
             "font": "Label/Button", "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_ItemTile — the 114x114 cell, and a UTileView ENTRY. BEFORE WBP_ItemGrid.
    #
    # NO SIZE IS AUTHORED: C++ writes both overrides from the Size axis (114, or mini 30) so
    # `mini` is not a second WBP. A 114 here would be a second source for the number the
    # grid's pitch arithmetic is static_asserted against.
    #
    # inset(7) IS MEASURED and static_asserted (7 + 100 + 7 == 114). Written as an inset, not
    # a 100x100 size, because at Size=mini the interior is UNMEASURED — the header forbids
    # scaling 7/100 by 30/114 and calling it measured. An inset degrades honestly.
    #
    # `Art` IS BRUSHLESS AND SAFE, unlike every other bare UImage here: it is NON-OPTIONAL (a
    # missing one fails at ASSET LOAD with rung 1 green), and ApplyItemData COLLAPSES it when
    # there is no art — with TileType defaulting to Empty, it ships collapsed.
    #
    # `RarityTint` IS OMITTED, AND IT IS THE ONE OMISSION THAT COSTS SOMETHING. C++ does tint
    # it — but the token resolves to OPAQUE WHITE for all four rarities today. Sitting above
    # `Ground` it would ERASE the measured black plate and paint a flat white square wherever
    # art has not streamed, while the recorded artefact is a 0->1 ALPHA GRADIENT (a Tier-4
    # material that does not exist). It is optional and guarded; it lands with the material.
    #
    # FIVE BADGES OMITTED: all art-bearing, none with a texture, all guarded and collapsed.
    # A virtualised grid would otherwise draw five blanks per visible cell.
    # ------------------------------------------------------------------
    "WBP_ItemTile": {
        "folder": ASSET_FOLDER["WBP_ItemTile"],
        "parent_class": "/Script/Breachpoint.BRItemTile",
        "class": "UBRItemTile",
        "header": "Source/Breachpoint/UI/Components/BRItemTile.h",
        "notes": "One 114x114 grid cell: ground plate, art, 3-edge border and the rarity "
                 "bottom line. Size is C++'s (114 / mini 30); badges wait on their art.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            # Overlay child order IS z-order: plate, art, then chrome ON the art.
            {"name": "TileOverlay", "class": OVERLAY, "parent": "RootSizeBox", "slot": FILL},
            # BRUSHLESS AND TINTED — the legal case. C++ paints black@0.5 onto engine white.
            {"name": "Ground", "class": IMAGE, "parent": "TileOverlay",
             "slot": inset(7.0), "bind": True},
            {"name": "Art", "class": IMAGE, "parent": "TileOverlay",
             "slot": inset(7.0), "bind": True},
            {"name": "Border", "class": HAIRLINE, "parent": "TileOverlay",
             "slot": FILL, "bind": True},
            # The rarity signal, and the ONLY node whose colour moves — which is why it is a
            # separate UBRRule and not Border's bottom edge (one FBRHairlineStyle carries one
            # stroke token for all four of its edges). A UBRRule's desired height IS the
            # stroke weight, so a VAlign_Bottom slot puts the line on the tile's bottom edge
            # with no hand-typed size. The 1px L/R padding is the measured 112 inside 114,
            # expressed as the gap so it survives the mini variant.
            {"name": "RarityLine", "class": RULE, "parent": "TileOverlay",
             "slot": {"horizontalAlignment": "HAlign_Fill",
                      "verticalAlignment": "VAlign_Bottom",
                      "padding": margin(left=1.0, right=1.0)},
             "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_ItemGrid — the 536x388 grid. AFTER WBP_ItemTile.
    #
    # THE TILE VIEW IS EMPTY AND THAT IS THE FINISHED STATE. A repeated tile is a UTileView
    # with an entry WBP, NEVER N hand-placed children. The view owns creation, recycling and
    # virtualisation; a tile authored here would bind by name into that and double-render —
    # the killfeed lesson exactly.
    #
    # !! GAP — THIS ASSET IS NOT FINISHED BY THIS GENERATOR. TileView.EntryWidgetClass must
    # point at WBP_ItemTile, and build_wbp.py writes SLOT properties, fonts and brushes ONLY.
    # There is no plan key for a widget property, so the built grid COMPILES and renders ZERO
    # tiles until EntryWidgetClass is set. That is why WBP_ItemTile is ordered first, and why
    # this is NOT expressed with wbp_class(): the tile is not a tree child, so validate_all()
    # never fires on it and would not catch a wrong order. Close it by teaching the generator
    # a `properties` node key. DO NOT hand-place a tile to "fix" the blank grid.
    #
    # NO ROOT SIZE BOX: the same class ships as the 536 panel and the 504x374 off-grid
    # module, so the outer box belongs to the CALLER's slot. C++ owns pitch and column count.
    #
    # TileViewBox IS HAlign_Left, NOT Fill: C++ overrides its WIDTH to N*130, and an
    # HAlign_Fill slot would hand the SBox the parent's full width and quietly defeat the
    # override — which is the whole column-count mechanism.
    # ------------------------------------------------------------------
    "WBP_ItemGrid": {
        "folder": ASSET_FOLDER["WBP_ItemGrid"],
        "parent_class": "/Script/Breachpoint.BRItemGrid",
        "class": "UBRItemGrid",
        "header": "Source/Breachpoint/UI/Components/BRItemGrid.h",
        "notes": "An EMPTY UTileView in a C++-width-driven box, plus the honest empty-state "
                 "line. EntryWidgetClass -> WBP_ItemTile is NOT writable here (gap).",
        "tree": [
            # Overlay, not a box: the placeholder sits OVER the view's area, not beside it.
            {"name": "GridOverlay", "class": OVERLAY, "parent": None},
            {"name": "TileViewBox", "class": SIZEBOX, "parent": "GridOverlay",
             "slot": {"horizontalAlignment": "HAlign_Left",
                      "verticalAlignment": "VAlign_Fill",
                      "padding": margin()},
             "bind": True},
            # EMPTY ON PURPOSE. Entry size is C++'s: SetEntryWidth/Height = TilePitch, so the
            # CELL is the pitch and the tile inside it stays 114 at every aspect ratio.
            #
            # `entryWidgetClass` IS NOT OPTIONAL — UMG refuses to COMPILE a UListViewBase
            # without one ("required for any UListViewBase to function"), so this asset could
            # not be built at all until the generator learned to write plain widget
            # properties. That is why WBP_ItemTile must precede this entry in PLAN, and it is
            # now enforced by more than a comment: wbp_class() raises a KeyError if the tile
            # is not in the plan at all.
            {"name": "TileView", "class": TILEVIEW, "parent": "TileViewBox",
             "slot": FILL, "bind": True,
            # `{"refPath": ...}`, NOT a bare string. A class reference is an OBJECT at this
            # layer: the bare form is accepted and stored correctly, but reads back as
            # {"refPath": ...} and the verified-write comparison then fails on a value that
            # is actually right. Writing the shape the engine returns is what makes the
            # read-back meaningful instead of a permanent false negative.
             "properties": {"entryWidgetClass": {"refPath": wbp_class("WBP_ItemTile")}}},
            # TWO DIFFERENT FACTS, ONE WIDGET: an em dash while Unknown (nothing feeds this
            # grid — there is no customization ViewModel at all), "NO ITEMS" when a push
            # arrived empty, Collapsed once there are tiles. C++ owns all three, so it never
            # renders a confident empty inventory during travel or a pending fetch.
            {"name": "EmptyStateLabel", "class": TEXT, "parent": "GridOverlay",
             "slot": CENTER, "font": "Body/Flavor", "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_TableRow — Table Buttons, 660x28. The Browser's leaf (8 frames blocked on it).
    #
    # `class` MATTERS HERE MORE THAN ANYWHERE. BRTableRow.h declares THREE classes. Parsed
    # whole-file it yields 12 binds including UBRSmallHeader's NON-OPTIONAL `HeaderText` —
    # this plan would demand a header caption inside a table row and build_wbp.py would
    # faithfully create it. The slice cuts it to the 9 that are actually UBRTableRow's.
    #
    # !! GAP — THIS ROW HAS ONE COLUMN, DELIBERATELY. How the 660 divides into name / author
    # / players / rating is recorded NOWHERE: the header carries the four widths as NEGATIVE
    # SENTINELS precisely so a guessed layout cannot be passed off as measured. Authoring the
    # other columns means choosing stops — an HBox of three equal Fills IS a guess at 220
    # each, and it would look plausible forever. SetColumnText guards every one on null, so a
    # row showing only a name is visibly incomplete rather than confidently wrong.
    #
    # NO TEXT COLOUR IS AUTHORED: ApplyInvertedState repaints NameText on every hover,
    # selection and release. A design-time colour would be overwritten before the first frame
    # and would only invite someone to "fix" the row by editing dead data.
    # ------------------------------------------------------------------
    "WBP_TableRow": {
        "folder": ASSET_FOLDER["WBP_TableRow"],
        "parent_class": "/Script/Breachpoint.BRTableRow",
        "class": "UBRTableRow",
        "header": "Source/Breachpoint/UI/Components/BRTableRow.h",
        "notes": "660x28 browser row: invertible plate, 4-line border, ONE column. The other "
                 "three columns wait on the unmeasured stops.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None, "bind": True},
            {"name": "RowOverlay", "class": OVERLAY, "parent": "RootSizeBox", "slot": FILL},
            # NO BRUSH: ApplyInvertedState drives it with SetColorAndOpacity.
            {"name": "RowFill", "class": IMAGE, "parent": "RowOverlay",
             "slot": inset(2.0), "bind": True},
            {"name": "Border", "class": HAIRLINE, "parent": "RowOverlay",
             "slot": FILL, "bind": True},
            # Column 1, the only cell both Types are guaranteed to carry. The cell never
            # collapses when empty: SetColumnText writes the unknown dash instead, because a
            # collapsed cell shifts every column to its right.
            {"name": "NameText", "class": TEXT, "parent": "RowOverlay",
             "slot": {"horizontalAlignment": "HAlign_Left",
                      "verticalAlignment": "VAlign_Center",
                      "padding": margin(left=10.0, right=10.0)},
             "font": "Label/Button", "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_KillfeedEntry — the contract LANDED (BP66 closed, HUD-CPP-AUDIT packet C).
    #
    # UBRKillfeedEntryWidget now declares the exact BindWidget members this plan
    # pre-committed: KillerNameText / VictimNameText required, SpotterLineText /
    # WeaponIcon optional. SetEntry writes the texts directly — the old
    # BlueprintImplementableEvent path (unimplementable under R18: a BIE needs a
    # graph node, WBPs have empty graphs) is gone, and `validate()` below now
    # enforces the header/plan match it was designed for.
    # ------------------------------------------------------------------
    # Named WBP_KillfeedEntryWidget, not WBP_KillfeedEntry: the naming law derives the
    # asset name from the C++ class by stripping `UBR`, and the class is
    # `UBRKillfeedEntryWidget`. ROADMAP §0.4 already ruled that the CODE name wins over
    # the doc's `UBRKillfeedRow` — renaming a compiled class to satisfy a doc is the tail
    # wagging the dog. BP18's asset was named against the law and nobody noticed, because
    # the MCP cannot rename an asset. A GENERATED asset renames for free: change this key,
    # re-run, and the delete-then-create path does it.
    #
    # `"class"` matters HERE more than anywhere: BRHUDLayout.h declares TWO classes, and
    # without the slice this entry inherited `UBRHUDLayout`'s `KillfeedContainer` bind.
    "WBP_KillfeedEntryWidget": {
        "folder": ASSET_FOLDER["WBP_KillfeedEntryWidget"],
        "parent_class": "/Script/Breachpoint.BRKillfeedEntryWidget",
        "class": "UBRKillfeedEntryWidget",
        "header": "Source/Breachpoint/UI/BRHUDLayout.h",
        "notes": "One killfeed row, binds live (BP66 closed). WeaponIcon ships collapsed "
                 "from C++ until glyph art exists. Row box is the frame's 340x20.",
        # TYPOGRAPHY. Player names are proper nouns in mixed case, so they take the BODY face
        # (Roboto Condensed), not the all-caps Rajdhani chrome — `Body/Name` is literally the
        # style named for this. Row height is 20 and the face is 14, which fits.
        #
        # NO `color` ANYWHERE IN THIS ASSET, AND THAT IS LOAD-BEARING. `UBRKillfeed::…` tints
        # the WHOLE ROW via `UUserWidget::ColorAndOpacity` (BRKillfeed.cpp:179), which
        # MULTIPLIES down the tree. A leaf colour authored here would be multiplied by the
        # row tint and every killfeed line would come out darker than its token — the classic
        # double-tint. White leaves are the correct input to that multiply.
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None},
            {"name": "Row", "class": HBOX, "parent": "RootSizeBox"},
            {"name": "KillerNameText", "class": TEXT, "parent": "Row",
             "font": "Body/Name", "bind": True},
            # frame: killfeed.row.glyph [78,6,22,8] — a WEAPON glyph, and no weapon glyph
            # texture exists in Content/UI yet (Icons/ carries front-end UI glyphs, not
            # weapons). No brush rather than a wrong one — C++ ships it Collapsed until
            # art lands, so the brushless slot can never render BP70 D2's blank rectangle.
            {"name": "WeaponIcon", "class": IMAGE, "parent": "Row", "bind": True},
            {"name": "VictimNameText", "class": TEXT, "parent": "Row",
             "font": "Body/Name", "bind": True},
            # The Spotter line reserves its slot and renders EMPTY when the string is
            # empty — it never collapses layout and never waits on the LLM.
            # Offline ⇒ identical HUD minus flavour (ue5-ui-architecture §5).
            #
            # `Body/Flavor Small`, and the ITALIC is doing real work: it is the one visual
            # difference between a FACT the server sent (the two names) and a GENERATED line.
            # Roboto Condensed is also the only family here that HAS an italic — Rajdhani
            # ships none, which is exactly why the token file splits body off the chrome face.
            {"name": "SpotterLineText", "class": TEXT, "parent": "Row",
             "font": "Body/Flavor Small", "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_VitalsWidget — TOP-CENTRE, and it is an ARC.
    #
    # figma_hud_layout.json corrects the top-left placement asserted from the component
    # page: x=503.33 w=273.33 puts its centre at 640.0 exactly, so the canvas slot is a
    # centre anchor at left=0.
    #
    # THE ARC IS NOT FAKED HERE. The frame is "arc, not trapezoid", constant thickness 16,
    # sag 2.7. A straight rectangle would be the wrong primitive, and this generator cannot
    # write a material anyway. So the plan creates the BIND SLOTS at the measured rects and
    # the curvature stays a Tier-4 fill material on `WBP_ProgressBar`'s `Fill`.
    #
    # NO RootSizeBox, DELIBERATELY. `UBRVitalsWidget::NativeOnInitialized` sets it to the
    # component-page 277x35 if it exists, which would put a second geometry source in the
    # asset fighting the screen frame's 273.33x34. The JSON records that ~3.7px conflict and
    # says "do not average them"; omitting the optional SizeBox leaves exactly ONE source of
    # geometry — the canvas slot below — and the C++ guard (`if (RootSizeBox)`) no-ops.
    # ------------------------------------------------------------------
    "WBP_VitalsWidget": {
        "folder": ASSET_FOLDER["WBP_VitalsWidget"],
        "parent_class": "/Script/Breachpoint.BRVitalsWidget",
        "class": "UBRVitalsWidget",
        "header": "Source/Breachpoint/UI/HUD/BRVitalsWidget.h",
        "notes": "Shield arc over the hidden-until-damaged health bar, 273.33x34 measured. "
                 "Arc curvature is a Tier-4 material, not geometry authored here.",
        "tree": [
            {"name": "VitalsCanvas", "class": CANVAS, "parent": None},
            # The full box: the shield arc IS the surface.
            {"name": "ShieldBar", "class": wbp_class("WBP_ProgressBar"), "parent": "VitalsCanvas",
             "slot": canvas_slot(0, 0, 273.33, 34), "bind": True},
            # frame: health { y: 20, h: 5 }. No x/w is measured — it is drawn nested inside
            # the arc — so it takes the arc's width. Flagged rather than invented.
            {"name": "HealthBar", "class": wbp_class("WBP_ProgressBar"), "parent": "VitalsCanvas",
             "slot": canvas_slot(0, 20, 273.33, 5), "bind": True},
            # frame: centre_tick { x: 135.9, y: 20, w: 1.33, h: 10 }. Its visibility is a
            # C++ decision (present intact, absent broken), which is why it is bound at all.
            #
            # NO BRUSH, AND IT THEREFORE STILL DRAWS NOTHING. A 1.33x10 hairline is exactly the
            # thing `Content/UI/Icons/README.md` says never to export ("anything UMG can draw"),
            # so the right answer is a textureless solid fill — a `drawAs` other than `Image` —
            # and this generator only writes brushes that name a real texture. Filed as a gap
            # rather than pointed at a texture that does not exist.
            {"name": "CentreTick", "class": IMAGE, "parent": "VitalsCanvas",
             "slot": canvas_slot(135.9, 20, 1.33, 10), "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_AmmoBlock — the lower-right two thirds of the ONE 280x110 "Loadout Tray" frame.
    #
    # The JSON's own warning: the tray is one frame holding grenades AND equipment AND
    # weapon AND ammo AND the stowed weapon, and splitting it across `UBRAmmoBlock` and
    # `UBREquipmentTray` "is a decision nobody has recorded". The split used here is read
    # off the frame's children, not assumed: everything under `weapon` and `stowed` is
    # ammo, everything under `grenades` and `equipment` is the tray.
    #
    # Widget rect = the union of the ammo children, tray-local (60,44)-(278,104) => 218x60,
    # global (1000,624). The class constants say 190x40 (the component export). Recorded,
    # not averaged, and `RootSizeBox` is omitted for the same reason as vitals.
    #
    # NO StateSwitcher AND NO BatteryPercentText. Both are optional, and the class doc is
    # explicit that `Battery` is unreachable today — `UBRVM_Combat` has no charge field. A
    # switcher whose second page can never be shown is dead layout, which is precisely what
    # `ui-presentation` §8.3 forbids. They arrive with the ViewModel field.
    # ------------------------------------------------------------------
    "WBP_AmmoBlock": {
        "folder": ASSET_FOLDER["WBP_AmmoBlock"],
        "parent_class": "/Script/Breachpoint.BRAmmoBlock",
        "class": "UBRAmmoBlock",
        "header": "Source/Breachpoint/UI/HUD/BRAmmoBlock.h",
        "notes": "Weapon caption, mag / reserve and the ghosted stowed weapon. Rects are "
                 "the loadout-tray frame's own children, re-based to this widget's origin.",
        # TYPOGRAPHY. This readout is ONE family (Rajdhani) at THREE sizes, because the Figma
        # rects say the hierarchy is size, not face: mag 43 tall, reserve 26, stowed label 12.
        # Splitting mag and reserve across two families would break a single readout in half.
        #
        # NO COLOURS AUTHORED HERE. `UBRAmmoBlock::ApplyState` writes MagazineText,
        # ReserveText and StowedWeaponText colours from BR::Tokens on every state change
        # (BRAmmoBlock.cpp:172-197) — a design-time colour would be repainted before the first
        # frame, so authoring one only invites someone to "fix" the HUD by editing a value
        # nothing reads. ActiveWeaponText is the exception C++ never colours; see the gap.
        "tree": [
            {"name": "AmmoCanvas", "class": CANVAS, "parent": None},
            # tray-local weapon.name [60,44,87,14] — an all-caps chrome caption in a 14px box.
            # `Heading/Caption` is Rajdhani Bold 12 at 100 (10%), so it clears 14 with room for
            # descenders. Its token `case` is UPPER, which UMG cannot apply — see font_properties.
            {"name": "ActiveWeaponText", "class": TEXT, "parent": "AmmoCanvas",
             "slot": canvas_slot(0, 0, 87, 14), "bind": True,
             "font": "Heading/Caption"},
            # tray-local weapon.mag [74,58,36,43] — THE hero readout of the HUD.
            # `Display/Heading 2` (Rajdhani Bold 32, spacing 0) is the largest style that fits
            # 43; `Display/Item Title` is 48 and would overflow, which validate() now proves
            # rather than leaves to the eye. Spacing 0 matters as much as the size: two digits
            # at 10% spacing do not fit 36px.
            {"name": "MagazineText", "class": TEXT, "parent": "AmmoCanvas",
             "slot": canvas_slot(14, 14, 36, 43), "bind": True,
             "font": "Display/Heading 2"},
            # tray-local weapon.reserve [138,70,28,26]. The `div` glyph between them is art.
            # `Display/Title` — the same Rajdhani Bold, one step down, spacing 0. This is the
            # tightest fit on the surface: a three-digit reserve at 20 is close to the 28px box.
            {"name": "ReserveText", "class": TEXT, "parent": "AmmoCanvas",
             "slot": canvas_slot(78, 26, 28, 26), "bind": True,
             "font": "Display/Title"},
            # tray-local stowed.label [120,92,93,12]. The stowed BAR is art.
            # `Label/Micro` (SemiBold 10): the smallest text on the HUD gets the style named
            # for it. `Heading/Caption` at 12 would exactly equal the 12px box, leaving nothing
            # for the descender of a lowercase weapon name.
            {"name": "StowedWeaponText", "class": TEXT, "parent": "AmmoCanvas",
             "slot": canvas_slot(60, 48, 93, 12), "bind": True,
             "font": "Label/Micro"},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_ReticleWidget — dead centre, and NOT size-clamped.
    #
    # Both binds are optional and BOTH are built: `ApplyArt` writes the brush and the size
    # onto these two images, so a WBP without them renders nothing at all. That is the test
    # for an optional bind — not "is it optional" but "does C++ need it to draw".
    #
    # An Overlay, not a SizeBox: the art is five sizes because SIZE ENCODES SPREAD, and
    # `SetDesiredSizeOverride` only wins where the layout lets it. HAlign/VAlign Center lets
    # a 58px sniper reticle overflow the 40x40 anchor box, which the JSON says is what the
    # box is: "the anchor box, not a clamp".
    #
    # HitMarkerImage is authored AFTER the reticle: Overlay order is z-order, and the
    # confirm draws in front of the reticle it confirms.
    # ------------------------------------------------------------------
    "WBP_ReticleWidget": {
        "folder": ASSET_FOLDER["WBP_ReticleWidget"],
        "parent_class": "/Script/Breachpoint.BRReticleWidget",
        "class": "UBRReticleWidget",
        "header": "Source/Breachpoint/UI/HUD/BRReticleWidget.h",
        "notes": "Reticle + hit marker, centred, unclamped. C++ drives the size at runtime.",
        "tree": [
            {"name": "ReticleOverlay", "class": OVERLAY, "parent": None},
            # THIS BRUSH IS A DESIGN-TIME CONVENIENCE AND C++ NEVER READS IT.
            # `UBRReticleWidget::ApplyArt` writes both the brush resource and the size onto
            # this image from `FBRReticleArt` the moment a weapon is known, and it also pins
            # the tint to BR::Tokens::Shield. So this default only decides what the asset
            # looks like in the editor, and it is the AR at its authored 43 — the size that
            # encodes AR spread — so the designer sees a truthful default rather than a blank.
            # Nobody should ever "fix the reticle" by editing this.
            {"name": "ReticleImage", "class": IMAGE, "parent": "ReticleOverlay",
             "slot": CENTER, "bind": True,
             "brush": brush("/Game/UI/HUD/HUD_Reticle_AR", 43, 43)},
            # No brush: the hit-marker art is not in Content/UI/HUD. `HUD_Feedback_DamageDir`
            # exists as an export but has not been imported, and C++ drives this image from
            # `HitMarkerArtByKind` anyway — an empty default is honest, a wrong one is not.
            {"name": "HitMarkerImage", "class": IMAGE, "parent": "ReticleOverlay",
             "slot": CENTER, "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_MatchBand — BOTTOM-centre, and NOT exactly centred.
    #
    # figma_hud_layout.json corrects two things at once: the band is at y=622, not the top
    # of the screen, and x=474.67 w=302 puts its centre at 625.67 — 14.33px LEFT of 640.
    # That offset is carried through as measured. Silently rounding it to centre would
    # destroy the evidence of whichever it is: an intent, or a slip for the frame's owner.
    #
    # NO RocketCountdownText / RocketCountdownRoot, though both are optional and both are
    # real features (GDD §2.9). The screen frame draws NO rocket countdown — it has no
    # measured rect anywhere — and this file's whole discipline is that geometry is read,
    # not invented. Filed as a gap for whoever owns node 30:2.
    #
    # The mode icons, team bars and separators are art with no C++ member behind them, and
    # the JSON warns the bar widths are sample values because the bars are proportional to
    # score. Nothing here creates them.
    # ------------------------------------------------------------------
    "WBP_MatchBand": {
        "folder": ASSET_FOLDER["WBP_MatchBand"],
        "parent_class": "/Script/Breachpoint.BRMatchBand",
        "class": "UBRMatchBand",
        "header": "Source/Breachpoint/UI/HUD/BRMatchBand.h",
        "notes": "Ally score / clock / enemy score, band-local rects from the frame's own "
                 "children. Band centre is 625.67, measured, not 640.",
        # TYPOGRAPHY. All three are NUMERALS in 20px-tall boxes, so all three take
        # `Display/Title` — Rajdhani Bold 20, LETTER SPACING 0. The spacing is the decision
        # here, not the size: `Heading/Small` is Rajdhani Bold at 200 (20%), and 20% on
        # `12:00` in a 43px box overflows the band outright. Digits are also the one case
        # where extreme tracking costs legibility instead of buying character.
        #
        # `AllyScoreText` and `EnemyScoreText` are recoloured by C++ every Refresh
        # (BRMatchBand.cpp:28/33), so no colour is authored. ClockText is NOT — see the gap.
        "tree": [
            {"name": "BandCanvas", "class": CANVAS, "parent": None},
            # band-local ScoreSelf [90,1,34,20]
            {"name": "AllyScoreText", "class": TEXT, "parent": "BandCanvas",
             "slot": canvas_slot(90, 1, 34, 20), "bind": True,
             "font": "Display/Title"},
            # band-local Timer [138,1,43,20]. Five glyphs (`M:SS` / `--:--`) in 43px is why
            # this is the widest child of the band.
            {"name": "ClockText", "class": TEXT, "parent": "BandCanvas",
             "slot": canvas_slot(138, 1, 43, 20), "bind": True,
             "font": "Display/Title"},
            # band-local ScoreThem [200,1,34,20]
            {"name": "EnemyScoreText", "class": TEXT, "parent": "BandCanvas",
             "slot": canvas_slot(200, 1, 34, 20), "bind": True,
             "font": "Display/Title"},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_Killfeed — BOTTOM-LEFT, one widget, and that is the correct size.
    #
    # `UBRKillfeed` creates its own rows: the pool is built once into `EntryContainer` and
    # thereafter claimed by index. There is nothing for a plan to author inside it, and a
    # row authored here would be deleted by the first `EnsurePool`. Row height 20 at pitch
    # 24 is the ROW's padding, and rows are C++-created, so the 4px is not expressible here
    # either — it belongs to WBP_KillfeedEntryWidget's own box.
    # ------------------------------------------------------------------
    "WBP_Killfeed": {
        "folder": ASSET_FOLDER["WBP_Killfeed"],
        "parent_class": "/Script/Breachpoint.BRKillfeed",
        "class": "UBRKillfeed",
        "header": "Source/Breachpoint/UI/HUD/BRKillfeed.h",
        "notes": "The pooled feed's container and nothing else — C++ owns every row.",
        "tree": [
            {"name": "EntryContainer", "class": VBOX, "parent": None, "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_EquipmentTray — the TOP-LEFT strip of the loadout tray, BOTTOM-RIGHT on screen.
    #
    # figma_hud_layout.json corrects the bottom-LEFT placement the class doc still asserts
    # ("the BOTTOM-LEFT HUD surface"). It is bottom-right, sharing one frame with the ammo
    # block. Widget rect = the union of the grenade glyphs and the equipment slot,
    # tray-local (0,0)-(140,34), global (940,580).
    #
    # ONE grenade count and ONE cooldown, per the class doc and BP69: the frame draws two
    # grenade glyphs and the sim has one untyped count. Authoring a second count would
    # settle a founder call by being the next person to open the HUD.
    # ------------------------------------------------------------------
    "WBP_EquipmentTray": {
        "folder": ASSET_FOLDER["WBP_EquipmentTray"],
        "parent_class": "/Script/Breachpoint.BREquipmentTray",
        "class": "UBREquipmentTray",
        "header": "Source/Breachpoint/UI/HUD/BREquipmentTray.h",
        "notes": "Grenade count + grapple cooldown bar. Rects are the loadout-tray frame's "
                 "grenade/equipment children, re-based to this widget's origin.",
        "tree": [
            {"name": "TrayCanvas", "class": CANVAS, "parent": None},
            # grenades.count_text gives an ORIGIN [14,8] and no size; the FRAG glyph it sits
            # in is [0,0,40,34], so the box is the remainder of that glyph. Derived, not
            # measured — the only number on this surface that is.
            #
            # `Display/Title`, the SAME style as the scores and the clock, deliberately: one
            # numeral style across the whole HUD is what makes a count read as a count. 20 in
            # a 26 box, spacing 0.
            {"name": "GrenadeCountText", "class": TEXT, "parent": "TrayCanvas",
             "slot": canvas_slot(14, 8, 26, 26), "bind": True,
             "font": "Display/Title"},
            # equipment.slot [100,0,40,34] + equipment.cooldown_band [1,20,38,13].
            # The band "FILLS UP" — a UBRProgressBar with a radial fill, per the class doc.
            {"name": "CooldownBar", "class": wbp_class("WBP_ProgressBar"), "parent": "TrayCanvas",
             "slot": canvas_slot(101, 20, 38, 13), "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_HUDLayout — LAST, because it hosts six other generated assets.
    #
    # Every slot below is `figma_hud_layout.json`, node 30:2, at the 1280x720 authoring
    # base. Nothing is multiplied by 1.5; the DPI curve does 1080.
    #
    # `KillfeedContainer` IS GONE, AND THAT IS A DECISION, NOT AN OVERSIGHT.
    # There are two killfeed implementations in Source/: `UBRHUDLayout::RebuildKillfeed`
    # (an inline FUserWidgetPool into `KillfeedContainer`) and `UBRKillfeed` (a pooled,
    # index-stable projection of the same `UBRVM_Match` ring, with drop logging). They read
    # the SAME array, so authoring both draws the feed twice. `KillfeedContainer` is
    # BindWidgetOptional and `RebuildKillfeed` early-returns when it is null, so omitting it
    # is legal and inert — it does not crash, it does not warn, it simply leaves the feed to
    # the class written for it. Filed as a contract_gap: ONE of the two implementations
    # should be deleted in C++, and that is not a plan file's call to make.
    # ------------------------------------------------------------------
    "WBP_HUDLayout": {
        "folder": ASSET_FOLDER["WBP_HUDLayout"],
        "parent_class": "/Script/Breachpoint.BRHUDLayout",
        "class": "UBRHUDLayout",
        "header": "Source/Breachpoint/UI/BRHUDLayout.h",
        "notes": "Root canvas hosting the six HUD surfaces at their measured screen-frame "
                 "positions. Six of W1's surfaces are live; the motion tracker, damage "
                 "wheel, interaction prompt and objective waypoint have no C++ class.",
        "tree": [
            {"name": "RootCanvas", "class": CANVAS, "parent": None},

            # TOP-CENTRE. 503.33 + 273.33/2 = 640.0 exactly, so left is 0 off a centre anchor.
            {"name": "Vitals", "class": wbp_class("WBP_VitalsWidget"), "parent": "RootCanvas",
             "slot": canvas_slot(left=0, top=66, width=273.33, height=34,
                                 anchor=(0.5, 0.0), align=(0.5, 0.0))},

            # DEAD CENTRE. 620+20 = 640, 340+20 = 360. The 40x40 is an anchor box; the
            # child overflows it at 52 and 58 on purpose.
            {"name": "Reticle", "class": wbp_class("WBP_ReticleWidget"), "parent": "RootCanvas",
             "slot": canvas_slot(left=0, top=0, width=40, height=40,
                                 anchor=(0.5, 0.5), align=(0.5, 0.5))},

            # BOTTOM-LEFT (60,455) 340x76. left = 60; top = (455+76) - 720.
            {"name": "Killfeed", "class": wbp_class("WBP_Killfeed"), "parent": "RootCanvas",
             "slot": canvas_slot(left=60, top=-189, width=340, height=76,
                                 anchor=(0.0, 1.0), align=(0.0, 1.0))},

            # BOTTOM-RIGHT, loadout tray top strip (940,580) 140x34.
            # left = (940+140) - 1280 = -200; top = (580+34) - 720 = -106.
            {"name": "EquipmentTray", "class": wbp_class("WBP_EquipmentTray"), "parent": "RootCanvas",
             "slot": canvas_slot(left=-200, top=-106, width=140, height=34,
                                 anchor=(1.0, 1.0), align=(1.0, 1.0))},

            # BOTTOM-RIGHT, loadout tray weapon/ammo block (1000,624) 218x60.
            # left = (1000+218) - 1280 = -62; top = (624+60) - 720 = -36.
            {"name": "AmmoBlock", "class": wbp_class("WBP_AmmoBlock"), "parent": "RootCanvas",
             "slot": canvas_slot(left=-62, top=-36, width=218, height=60,
                                 anchor=(1.0, 1.0), align=(1.0, 1.0))},

            # BOTTOM-CENTRE, and NOT centred: measured centre 625.67 is 14.33 left of 640.
            # left = 625.67 - 640; top = (622+22) - 720. Authored LAST, so the band draws in
            # front of the tray if a wide aspect ever brings them together.
            {"name": "MatchBand", "class": wbp_class("WBP_MatchBand"), "parent": "RootCanvas",
             "slot": canvas_slot(left=-14.33, top=-76, width=302, height=22,
                                 anchor=(0.5, 1.0), align=(0.5, 1.0))},
        ],
    },

    # ==================================================================
    # SCREENS AND MODALS. Appended LAST so every hosted child precedes its host:
    # WBP_Modal_Warning hosts WBP_HighlightButton; WBP_Screen_DeathRespawn hosts WBP_MatchBand.
    # ==================================================================

    # ------------------------------------------------------------------
    # WBP_GearDetail — the customization detail strip, 586 x 161 / 586 x 125.
    #
    # TWO WIDGETS ARE CREATED WITHOUT bind:True AND THAT IS NOT AN OVERSIGHT. `Frame`
    # (NON-OPTIONAL) and `RootSizeBox` are declared on the BASE, UBRPanel, and
    # required_bind_widgets() parses ONE header sliced to ONE class — it cannot see an
    # inherited bind. bind:True would be rejected as "declares no such BindWidget", while
    # omitting the widgets would fail the WBP at ASSET LOAD. Creating them by exact name
    # satisfies UMG (BindWidget resolves by name at compile) and the validator both.
    # Same defect as WBP_ItemTitle's inherited `Title`. Filed: walk the base chain.
    #
    # NO HEIGHT, EVER: ApplyVariant calls SetPanelHeight(161 or 125). NO WIDTH EITHER, and
    # that one is a GAP — the .cpp says 586 "is authored in the WBP and never driven", but
    # this generator writes no size overrides, so 586 has no owner in either place.
    #
    # `MakerMark` IS BUILT despite being art-bearing: NativeOnInitialized -> ClearDetail ->
    # SetMakerMark({}) -> Collapsed, so the brushless slot is never drawn. `MakerRow` is
    # bound as UWidget, so an HBox satisfies it — and its collapse IS the 161->125 variant.
    # ------------------------------------------------------------------
    "WBP_GearDetail": {
        "folder": ASSET_FOLDER["WBP_GearDetail"],
        "parent_class": "/Script/Breachpoint.BRGearDetail",
        "class": "UBRGearDetail",
        "header": "Source/Breachpoint/UI/Components/BRGearDetail.h",
        "notes": "586-wide detail strip: name, description, and the maker row whose collapse "
                 "IS the 161->125 variant. Frame/RootSizeBox are inherited binds, unbound.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None},
            {"name": "DetailOverlay", "class": OVERLAY, "parent": "RootSizeBox", "slot": FILL},
            # INHERITED, NON-OPTIONAL. Ground AND border in one widget and one draw element.
            {"name": "Frame", "class": HAIRLINE, "parent": "DetailOverlay", "slot": FILL},
            {"name": "ContentVBox", "class": VBOX, "parent": "DetailOverlay",
             "slot": inset(16.0)},
            {"name": "ItemName", "class": TEXT, "parent": "ContentVBox",
             "slot": box_slot(), "font": "Display/Item Title", "bind": True},
            # Empty is never blank — ApplyFieldText writes the em dash.
            {"name": "Description", "class": TEXT, "parent": "ContentVBox",
             "slot": box_slot(fill=1.0, padding=margin(top=16.0)),
             "font": "Body/Name", "bind": True},
            {"name": "MakerRow", "class": HBOX, "parent": "ContentVBox",
             "slot": box_slot(padding=margin(top=16.0)), "bind": True},
            # NO BRUSH: the caller resolves the soft ref and pushes an FSlateBrush (law 3),
            # and C++ collapses the mark whenever there is no resource.
            {"name": "MakerMark", "class": IMAGE, "parent": "MakerRow",
             "slot": box_slot(v="VAlign_Center"), "bind": True},
            {"name": "MakerName", "class": TEXT, "parent": "MakerRow",
             "slot": box_slot(fill=1.0, padding=margin(left=16.0), v="VAlign_Center"),
             "font": "Heading/Caption", "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_Modal_Options — one class, two variants. Pushes to Layer.Modal.
    #
    # NO ROWS IN THIS TREE, AND THAT IS THE WHOLE ENTRY. RebuildRows resolves the SOFT
    # RowWidgetClass (wired to WBP_MenuRow in DefaultGame.ini), creates one UBRMenuRow per
    # payload row and AddChild's it. ReleaseRows calls RowContainer->ClearChildren() on every
    # activation. A hand-placed row would bind by name AND be destroyed by the first rebuild.
    #
    # NO SIZE: NativeOnInitialized writes 451x682 onto RootSizeBox precisely so the modal
    # cannot be stretched to 21:9 from a details panel. The x=48 anchor IS the WBP's job and
    # is the one geometry number this entry authors.
    # ------------------------------------------------------------------
    "WBP_Modal_Options": {
        "folder": ASSET_FOLDER["WBP_Modal_Options"],
        "parent_class": "/Script/Breachpoint.BRModal_Options",
        "class": "UBRModal_Options",
        "header": "Source/Breachpoint/UI/Screens/BRModal_Options.h",
        "notes": "451x682 popup at x=48 over a full scrim. Row list is EMPTY — C++ builds it "
                 "from the soft RowWidgetClass. Size is C++-owned; only x=48 lives here.",
        "tree": [
            {"name": "ModalRoot", "class": OVERLAY, "parent": None},
            {"name": "Scrim", "class": SCRIM, "parent": "ModalRoot",
             "slot": FILL, "bind": True},
            # PopupOriginX=48 is a C++ constant that C++ never applies; spent here.
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": "ModalRoot",
             "slot": {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Center",
                      "padding": margin(left=48.0)},
             "bind": True},
            {"name": "PopupOverlay", "class": OVERLAY, "parent": "RootSizeBox", "slot": FILL},
            # UNBOUND ON PURPOSE — no frame member is declared, so bind:True is a plan error.
            {"name": "PopupFrame", "class": HAIRLINE, "parent": "PopupOverlay", "slot": FILL},
            # 16 is the house content inset; this modal's inner anatomy is UNMEASURED.
            {"name": "ContentVBox", "class": VBOX, "parent": "PopupOverlay",
             "slot": inset(16.0)},
            # C++ sets the text and COLLAPSES it when empty, so an untitled filter panel
            # draws no empty band.
            {"name": "TitleText", "class": TEXT, "parent": "ContentVBox",
             "slot": box_slot(padding=margin(bottom=16.0)),
             "font": "Display/Options Header", "bind": True},
            # EMPTY. Declared UPanelWidget so a scroll box can replace it later without a C++
            # change; a VerticalBox is what it must be TODAY.
            {"name": "RowContainer", "class": VBOX, "parent": "ContentVBox",
             "slot": box_slot(fill=1.0), "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_Modal_Warning — full-page confirm. Pushes to Layer.Modal.
    #
    # FULL PAGE, SO NO BOX AND NO NUMBERS: the header records 1280x720 "for reference only —
    # the WBP anchors 0,0 -> 1,1 with zero offsets and is therefore correct at any aspect
    # without either number being typed into a slot". An Overlay root IS that anchoring.
    #
    # ConfirmLabelText / CancelLabelText ARE DELIBERATELY OMITTED. They exist only because
    # UCommonButtonBase has no label setter; WBP_HighlightButton draws its OWN Label, so
    # authoring them puts a second label beside every button. CONSEQUENCE, FILED: the
    # request's ConfirmLabel/CancelLabel are silently dropped until ApplyRequest casts to
    # UBRHighlightButton and calls SetLabelText — a one-line C++ fix, not a WBP fix.
    #
    # ORDER IS accept-then-decline, but FOCUS IS NOT: NativeGetDesiredFocusTarget returns
    # CancelButton, because cancel is the safe answer.
    # ------------------------------------------------------------------
    "WBP_Modal_Warning": {
        "folder": ASSET_FOLDER["WBP_Modal_Warning"],
        "parent_class": "/Script/Breachpoint.BRModal_Warning",
        "class": "UBRModal_Warning",
        "header": "Source/Breachpoint/UI/Screens/BRModal_Warning.h",
        "notes": "Full-page confirm over a scrim: title, body, confirm/cancel pair. No box and "
                 "no size — the overlay IS the 0,0->1,1 anchor. Inner gaps are UNMEASURED (16).",
        "tree": [
            {"name": "WarningRoot", "class": OVERLAY, "parent": None},
            {"name": "Scrim", "class": SCRIM, "parent": "WarningRoot",
             "slot": FILL, "bind": True},
            {"name": "ContentVBox", "class": VBOX, "parent": "WarningRoot",
             "slot": {"horizontalAlignment": "HAlign_Center",
                      "verticalAlignment": "VAlign_Center", "padding": margin()}},
            {"name": "TitleText", "class": TEXT, "parent": "ContentVBox",
             "slot": box_slot(padding=margin(bottom=16.0), h="HAlign_Center"),
             "font": "Display/Page Title", "bind": True},
            # UNMEASURED CHOICE, STATED: no token is named for a prose paragraph, and
            # Body/Name is the ONLY upright body style (the other two are italic, and italic
            # is this project's mark for a generated line). Leaving it unwritten would render
            # the one sentence the player must read in the ENGINE DEFAULT face, which is not
            # in the family set at all — a worse lie than an on-system default.
            {"name": "BodyText", "class": TEXT, "parent": "ContentVBox",
             "slot": box_slot(padding=margin(bottom=16.0), h="HAlign_Center"),
             "font": "Body/Name", "bind": True},
            {"name": "ButtonRow", "class": HBOX, "parent": "ContentVBox",
             "slot": box_slot(h="HAlign_Center")},
            {"name": "ConfirmButton", "class": wbp_class("WBP_HighlightButton"),
             "parent": "ButtonRow",
             "slot": box_slot(padding=margin(right=16.0), v="VAlign_Center"), "bind": True},
            {"name": "CancelButton", "class": wbp_class("WBP_HighlightButton"),
             "parent": "ButtonRow", "slot": box_slot(v="VAlign_Center"), "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_Panel_Toast — 349 x 60, Layer.Modal, and NON-BLOCKING:
    # bSupportsActivationFocus is false, so nothing here may be focusable and no input config
    # is declared. It dismisses on a TIMER (law 4).
    #
    # SIZE IS C++'S so the toast stays on the 3x349 column grid. The Overlay root is what
    # makes those overrides bite — a bare SizeBox root would be stretched by the layer's slot.
    #
    # x = 69 is Column1OriginX, a C++ constant C++ never applies. y IS UNMEASURED: the frame
    # carries geometry only, so this authors NO top offset rather than inventing one.
    # ------------------------------------------------------------------
    "WBP_Panel_Toast": {
        "folder": ASSET_FOLDER["WBP_Panel_Toast"],
        "parent_class": "/Script/Breachpoint.BRPanel_Toast",
        "class": "UBRPanel_Toast",
        "header": "Source/Breachpoint/UI/Screens/BRPanel_Toast.h",
        "notes": "349x60 non-blocking message at column-1 origin. Size is C++-owned; the rule "
                 "under the text is unbound by contract; vertical placement is UNMEASURED.",
        "tree": [
            {"name": "ToastRoot", "class": OVERLAY, "parent": None},
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": "ToastRoot",
             "slot": {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Top",
                      "padding": margin(left=69.0)},
             "bind": True},
            {"name": "ToastVBox", "class": VBOX, "parent": "RootSizeBox",
             "slot": inset(16.0)},
            # A toast is a SENTENCE, so it takes the body face, not the all-caps chrome.
            {"name": "MessageText", "class": TEXT, "parent": "ToastVBox",
             "slot": box_slot(fill=1.0, v="VAlign_Center"),
             "font": "Body/Name", "bind": True},
            # UNBOUND BY CONTRACT — "C++ binds only what C++ drives".
            {"name": "MessageRule", "class": RULE, "parent": "ToastVBox", "slot": box_slot()},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_Screen_Scoreboard — DELIBERATELY ALMOST EMPTY.
    #
    # NO CanvasPanel. The header's "canvas 465, 191" describes an ORIGIN, not a canvas: the
    # .cpp places the highlight and its overhanging accent with SetRenderTranslation — "a
    # RENDER transform, so it moves nothing else in the overlay and no layout pass can undo
    # it". The C++ was written to AVOID needing one, so law 6 needs no exception here.
    #
    # THE ROSTER HAS NO SOURCE (the header's own CONTRACT GAP — ApplyRosterState hard-codes
    # bHasRosterSource = false). So RosterContainer is created and left EMPTY, and the only
    # thing this screen can honestly draw today is the em dash in RosterUnavailableText.
    #
    # LocalHighlightRow / LocalAccentBar OMITTED: SetLocalPlayerRow has ZERO callers
    # repo-wide, so both ship Collapsed forever. Their documented sizes are inexpressible
    # here too, so authoring them means two permanently-collapsed widgets at a wrong size.
    # ------------------------------------------------------------------
    "WBP_Screen_Scoreboard": {
        "folder": ASSET_FOLDER["WBP_Screen_Scoreboard"],
        "parent_class": "/Script/Breachpoint.BRScreen_Scoreboard",
        "class": "UBRScreen_Scoreboard",
        "header": "Source/Breachpoint/UI/Screens/BRScreen_Scoreboard.h",
        "notes": "Header rule + the table body inside a SafeZone. No CanvasPanel. The roster is "
                 "an EMPTY container and an em dash: UBRVM_Match has no per-player collection.",
        "tree": [
            {"name": "ScreenSafeZone", "class": SAFEZONE, "parent": None},
            # NO SLOT — a USafeZoneSlot rejects the write and reads back null.
            {"name": "BandsVBox", "class": VBOX, "parent": "ScreenSafeZone"},
            # x is padding so the rule spans the content box at any width. A UBRRule is a
            # Slate leaf, not art: it needs no texture, which is why it is the ONE piece of
            # this frame that can be drawn honestly today.
            {"name": "HeaderRule", "class": RULE, "parent": "BandsVBox",
             "slot": box_slot(padding=margin(left=100.0, top=67.0, right=121.0))},
            # Left is absolute; top is the MEASURED GAP from the header rule's bottom, because
            # a VBox stacks by rendered height, not absolute y.
            {"name": "TableSizeBox", "class": SIZEBOX, "parent": "BandsVBox",
             "slot": box_slot(padding=margin(left=465.0, top=122.0),
                              h="HAlign_Left", v="VAlign_Top"),
             "bind": True},
            {"name": "TableOverlay", "class": OVERLAY, "parent": "TableSizeBox", "slot": FILL},
            # TOP-LEFT, ZERO PADDING — the header says "the WBP supplies no x/y of its own for
            # these three or the overhang is lost". EMPTY on purpose.
            {"name": "RosterContainer", "class": VBOX, "parent": "TableOverlay",
             "slot": {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Top",
                      "padding": margin()},
             "bind": True},
            # Body/Flavor, italic: the project's mark for a line that is NOT a fact the
            # server sent.
            {"name": "RosterUnavailableText", "class": TEXT, "parent": "TableOverlay",
             "slot": CENTER, "font": "Body/Flavor", "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_Screen_DeathRespawn — three centred bands, no CanvasPanel.
    #
    # VERTICAL RHYTHM IS THE FRAME'S GAPS, NOT ITS ABSOLUTE Y, because a VBox stacks by
    # RENDERED height. KNOWN DRIFT, RECORDED: the frame's killer band is 59 tall and a
    # Body/Name line renders about 18, so everything below rides up by that difference.
    # Padding a fake 41px bottom would be a size nobody measured — the frame exports no type
    # style for this node. Horizontal IS exact: both bands centre on 640.
    #
    # THE SWEEP BRUSH IS LOAD-BEARING, NOT DECORATION, and it is the ONE exception to "omit
    # art-bearing UImage binds": the .cpp calls RespawnRingSweep->GetDynamicMaterial(), which
    # returns null unless the brush resource IS a material. Without it, the only line in the
    # project that names the `Sweep` parameter silently no-ops. The asset exists on disk and
    # its package carries a scalar parameter named `Sweep`, matching the C++ constant exactly.
    # It still ships HIDDEN until BP23 replicates a respawn value — a visible zero-sweep would
    # be a claim about a timer that does not exist.
    #
    # KillingWeaponRoot OMITTED: collapsed unconditionally at init and, per the header's gap
    # 2, stays collapsed "for the whole lifetime of this class as written".
    # ------------------------------------------------------------------
    "WBP_Screen_DeathRespawn": {
        "folder": ASSET_FOLDER["WBP_Screen_DeathRespawn"],
        "parent_class": "/Script/Breachpoint.BRScreen_DeathRespawn",
        "class": "UBRScreen_DeathRespawn",
        "header": "Source/Breachpoint/UI/Screens/BRScreen_DeathRespawn.h",
        "notes": "Killer line / respawn ring / persisting match band as three centred bands in "
                 "a SafeZone. The sweep carries M_UI_RadialSweep so GetDynamicMaterial resolves.",
        "tree": [
            {"name": "ScreenSafeZone", "class": SAFEZONE, "parent": None},
            {"name": "BandsVBox", "class": VBOX, "parent": "ScreenSafeZone"},
            # Body/Name is the RULED face for a gamertag — the same decision as the killfeed
            # and the roster row, and this string comes out of the killfeed ring, so it is
            # literally the same data. NO COLOUR: Refresh writes it every time.
            {"name": "KillerNameText", "class": TEXT, "parent": "BandsVBox",
             "slot": box_slot(padding=margin(top=276.0), h="HAlign_Center"),
             "font": "Body/Name", "bind": True},
            # An Overlay, because the countdown sits INSIDE the ring, not under it.
            {"name": "RespawnRing", "class": OVERLAY, "parent": "BandsVBox",
             "slot": box_slot(padding=margin(top=75.0), h="HAlign_Center")},
            {"name": "RespawnRingSweep", "class": IMAGE, "parent": "RespawnRing",
             "slot": CENTER, "bind": True,
             "brush": brush("/Game/UI/Materials/M_UI_RadialSweep", 104, 104)},
            # Display/Heading 2 is the project's hero numeral style (the ammo magazine) — and
            # the ZERO letter spacing is the decision, not the size: tracking on digits costs
            # legibility.
            {"name": "RespawnCountdownText", "class": TEXT, "parent": "RespawnRing",
             "slot": CENTER, "font": "Display/Heading 2", "bind": True},
            # ONE UBRMatchBand, not a second set of bindings. It auto-binds UBRVM_Match itself
            # and this class never touches it, so the band and the death screen cannot disagree.
            {"name": "MatchState", "class": wbp_class("WBP_MatchBand"), "parent": "BandsVBox",
             "slot": box_slot(padding=margin(top=146.0), h="HAlign_Center"), "bind": True},
        ],
    },

    # ==================================================================
    # THE SETTINGS TIER (BP78). Order: the row is a leaf, the rebind modal is a leaf, the
    # screen hosts WBP_NavBar (declared far above) and therefore comes last.
    #
    # NOTHING HERE IS MEASURED FROM FIGMA, and that is stated rather than hidden. The
    # settings screen has no frame in the reference file — SCREEN-MANIFEST does not carry
    # one. Every number below is either a house constant already used elsewhere in this
    # plan (the 16 content inset, the 40 row pitch) or a deliberate structural choice. When
    # a settings frame IS measured, these are the numbers that get replaced.
    # ==================================================================

    # ------------------------------------------------------------------
    # WBP_SettingsRow — the settings list row. SAME TREE AS WBP_MenuRow.
    #
    # `UBRSettingsRow` derives from `UBRMenuRow` and declares ZERO new BindWidgets: it adds
    # a data object, a subscription and left/right handling. So this asset reuses
    # MENU_ROW_TREE verbatim through `unbound()` — see that function for why the binds are
    # dropped rather than repeated (inherited binds are invisible to the validator).
    #
    # It is a SEPARATE ASSET rather than a reuse of WBP_MenuRow because a WBP's parent class
    # is fixed at creation: WBP_MenuRow's parent is UBRMenuRow, and nothing can retarget it
    # to UBRSettingsRow without rebuilding it. The tree is shared; the asset cannot be.
    # ------------------------------------------------------------------
    "WBP_SettingsRow": {
        "folder": ASSET_FOLDER["WBP_SettingsRow"],
        "parent_class": "/Script/Breachpoint.BRSettingsRow",
        "class": "UBRSettingsRow",
        "header": "Source/Breachpoint/UI/Components/BRSettingsRow.h",
        "notes": "MENU_ROW_TREE under a UBRSettingsRow parent. All eight binds are INHERITED "
                 "from UBRMenuRow, so they are created by name and claimed by none.",
        "tree": unbound(MENU_ROW_TREE),
    },

    # ------------------------------------------------------------------
    # WBP_Modal_KeyRemap — the "press a key" capture. Pushes to Layer.Modal.
    #
    # FULL PAGE OVER A SCRIM, no box and no measured size: the screen exists to swallow every
    # key press, so it must cover the viewport. An Overlay root anchored 0,0 -> 1,1 is that,
    # at every aspect, without a number being typed.
    #
    # Both binds are OPTIONAL in C++ and are authored anyway — `UpdatePromptText` writes the
    # instruction line AND every failure message ("that key cannot be bound", "already in
    # use"). Omitting them would leave the player pressing keys at a modal that never says
    # anything back, which is the exact failure the C++ was written to avoid.
    # ------------------------------------------------------------------
    "WBP_Modal_KeyRemap": {
        "folder": ASSET_FOLDER["WBP_Modal_KeyRemap"],
        "parent_class": "/Script/Breachpoint.BRScreen_KeyRemap",
        "class": "UBRScreen_KeyRemap",
        "header": "Source/Breachpoint/UI/Screens/BRScreen_KeyRemap.h",
        "notes": "Full-page capture over a scrim. No size authored anywhere — the root "
                 "Overlay stretches, and the centred VBox hugs its text.",
        "tree": [
            {"name": "ModalRoot", "class": OVERLAY, "parent": None},
            # UNBOUND: UBRScreen_KeyRemap declares no Scrim member. It is here because a
            # capture modal that does not visibly take over the screen reads as "nothing
            # happened" while it silently eats every key.
            {"name": "Scrim", "class": SCRIM, "parent": "ModalRoot", "slot": FILL},
            {"name": "PromptVBox", "class": VBOX, "parent": "ModalRoot",
             "slot": {"horizontalAlignment": "HAlign_Center",
                      "verticalAlignment": "VAlign_Center", "padding": margin()}},
            # The action being rebound, above the instruction — the player needs to know WHAT
            # they are changing before they are told how.
            {"name": "ActionText", "class": TEXT, "parent": "PromptVBox",
             "slot": box_slot(padding=margin(bottom=16.0), h="HAlign_Center"),
             "font": "Display/Heading 2", "bind": True},
            {"name": "PromptText", "class": TEXT, "parent": "PromptVBox",
             "slot": box_slot(h="HAlign_Center"),
             "font": "Body/Flavor", "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_Screen_Settings — the settings screen. Pushes to Layer.Menu.
    #
    # HOSTS WBP_NavBar RATHER THAN AUTHORING A TAB STRIP. `UBRNavBar` already owns tab
    # definitions, exclusive selection through a UCommonButtonGroupBase, and bumper
    # navigation; a second strip here would fork the gamepad routing that component exists
    # to own. This is why this entry sits at the END of PLAN — validate_all() requires a
    # hosted asset to be built first, and WBP_NavBar is declared in the menu tier above.
    #
    # RowContainer IS EMPTY, exactly like WBP_Modal_Options's. `UBRScreen_Settings::RebuildRows`
    # fills it from the soft row class on every tab change; a child authored here would be a
    # widget the C++ neither owns nor clears.
    #
    # DescriptionText is a BindWidgetOptional that is authored: without it the screen renders
    # but every setting's explanation — and, more importantly, every "why is this greyed out"
    # reason — has nowhere to go.
    # ------------------------------------------------------------------
    # ------------------------------------------------------------------
    # WBP_ButtonDefault — the Menu Row `Type=Default` button. Founder-requested against
    # Figma `12:724` (Menu Row), 4 Aug 2026.
    #
    # SAME TREE AS WBP_MenuRow, AND THAT IS THE POINT. `Type=Default` is not a different
    # widget from a menu row — it IS the row's default type, one value on `EBRMenuRowType`,
    # and `UBRMenuRow::ApplyRowType` drives the shell from it. So this shares `MENU_ROW_TREE`
    # rather than copying it; a COMPONENT-SPECS correction lands in both or neither.
    #
    # BINDS ARE CLAIMED HERE, unlike `WBP_SettingsRow`. The parent is `UBRMenuRow` itself, which
    # DECLARES all eight `BindWidget` members, so `required_bind_widgets` can see them and
    # `bind: True` validates. `WBP_SettingsRow` had to use `unbound()` only because
    # `UBRSettingsRow` inherits them.
    #
    # WHY A SECOND ASSET AT ALL, when WBP_MenuRow exists and is identical: a WBP's identity is
    # its path, and screens reference it by soft class. Having a `WBP_ButtonDefault` under
    # `Components/Buttons` lets button-shaped callers point at a button-shaped asset without
    # every one of them having to know that a button is a menu row underneath. If that
    # indirection stops earning its keep, delete this and repoint at `WBP_MenuRow` — nothing
    # else changes, because the tree is the same object.
    # ------------------------------------------------------------------
    "WBP_ButtonDefault": {
        "folder": ASSET_FOLDER["WBP_ButtonDefault"],
        "parent_class": "/Script/Breachpoint.BRMenuRow",
        "class": "UBRMenuRow",
        "header": "Source/Breachpoint/UI/Components/BRMenuRow.h",
        "notes": "Menu Row Type=Default as a standalone button asset, with the exported Button "
                 "Border textures layered under the procedural hairline. 250x28 shell.",
        # MENU_ROW_TREE plus four textured edge images.
        #
        # THE TEXTURES DO NOT REPLACE `Border`, AND THEY CANNOT. `UBRMenuRow` declares
        # `Border` as a NON-OPTIONAL `BindWidget` typed `UBRHairlineBorder`; drop it and the
        # widget fails to compile at asset load. So the exported art is layered as four extra
        # UImages BENEATH the hairline (tree order is z-order in an Overlay), and the hairline
        # is still what `NativeOnInitialized` drives. Making the texture the ONLY border is a
        # C++ change to `UBRMenuRow`, not a WBP change.
        #
        # GEOMETRY CAVEAT, STATED BECAUSE IT IS VISIBLE: these pieces are authored for the
        # 100x100 `Button Border` frame, not for a 250x28 row. `Top_Line`/`Bottom_Line` are
        # BRACKETS (`M1 11V1H99V11` — down, across, down) whose corner stubs are 10px on a
        # 100px frame; stretched across a 250x28 row they scale with it. The correct fix is to
        # export Menu Row's OWN border pieces through the same pipeline, or to author these as
        # nine-slice. Filed, not hidden.
        "tree": MENU_ROW_TREE + [
            {"name": "EdgeTop", "class": IMAGE, "parent": "RowOverlay",
             "slot": {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Top",
                      "padding": margin()},
             "brush": brush("/Game/UI/Components/Buttons/Assets/Sides/Default_NoFade_Default__Top_Line", 100, 11)},
            {"name": "EdgeBottom", "class": IMAGE, "parent": "RowOverlay",
             "slot": {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Bottom",
                      "padding": margin()},
             "brush": brush("/Game/UI/Components/Buttons/Assets/Sides/Default_NoFade_Default__Bottom_Line", 100, 12)},
            {"name": "EdgeLeft", "class": IMAGE, "parent": "RowOverlay",
             "slot": {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Center",
                      "padding": margin()},
             "brush": brush("/Game/UI/Components/Buttons/Assets/Sides/Default_NoFade_Default__Left_Line", 68, 2)},
            {"name": "EdgeRight", "class": IMAGE, "parent": "RowOverlay",
             "slot": {"horizontalAlignment": "HAlign_Right", "verticalAlignment": "VAlign_Center",
                      "padding": margin()},
             "brush": brush("/Game/UI/Components/Buttons/Assets/Sides/Default_NoFade_Default__Right_Line", 68, 2)},
        ],
    },

    "WBP_Screen_Settings": {
        "folder": ASSET_FOLDER["WBP_Screen_Settings"],
        "parent_class": "/Script/Breachpoint.BRScreen_Settings",
        "class": "UBRScreen_Settings",
        "header": "Source/Breachpoint/UI/Screens/BRScreen_Settings.h",
        "notes": "Title / tabs / row list / description. Hosts WBP_NavBar. Row list EMPTY — "
                 "C++ builds it from the soft SettingsRowClass. No Figma frame exists yet.",
        "tree": [
            {"name": "ScreenRoot", "class": OVERLAY, "parent": None},
            # UNBOUND: no member is declared for it. A full-bleed ground so the screen is not
            # transparent over whatever is on Layer.Game beneath it.
            {"name": "Ground", "class": SCRIM, "parent": "ScreenRoot", "slot": FILL},
            # 16 is the house content inset, the same one WBP_Modal_Options spends.
            {"name": "ContentVBox", "class": VBOX, "parent": "ScreenRoot",
             "slot": inset(16.0)},
            {"name": "TitleText", "class": TEXT, "parent": "ContentVBox",
             "slot": box_slot(padding=margin(bottom=16.0)),
             "font": "Display/Page Title", "bind": True},
            # The tab strip. C++ calls SetTabs on it in NativeOnActivated, so it is authored
            # with no tabs — the four come from the registry, never from this plan.
            {"name": "TabBar", "class": wbp_class("WBP_NavBar"), "parent": "ContentVBox",
             "slot": box_slot(padding=margin(bottom=16.0)), "bind": True},
            # Fill 1.0: the row list takes everything the title, tabs and description leave.
            {"name": "RowContainer", "class": VBOX, "parent": "ContentVBox",
             "slot": box_slot(fill=1.0), "bind": True},
            {"name": "DescriptionText", "class": TEXT, "parent": "ContentVBox",
             "slot": box_slot(padding=margin(top=16.0)),
             "font": "Body/Flavor", "bind": True},
        ],
    },
}


# ---------------------------------------------------------------------------
# Reading the C++ contract
# ---------------------------------------------------------------------------

_BIND_RE = re.compile(
    r"UPROPERTY\([^)]*\bmeta\s*=\s*\([^)]*\bBindWidget(?P<opt>Optional)?\b[^)]*\)[^)]*\)"
    r"\s*(?:TObjectPtr<\s*(?P<ptr>\w+)\s*>|(?P<raw>\w+)\s*\*)\s*(?P<name>\w+)\s*;",
    re.S,
)

_CLASS_DECL = re.compile(r"^class\s+(?:\w+_API\s+)?(?P<name>U\w+)\s*:", re.M)


def class_body(header_text: str, cpp_class: str) -> str:
    """The slice of a header belonging to ONE class.

    Not cosmetic, and not a second invention: this mirrors
    `Tools/audit_ui/wbp_expect.py::class_body`, which found the bug. `BRHUDLayout.h`
    declares BOTH `UBRKillfeedEntryWidget` and `UBRHUDLayout`, so parsing the whole file
    handed the killfeed row a `KillfeedContainer` bind belonging to the HUD. It passed only
    because that member is Optional; a non-optional sibling would make this plan demand a
    widget in the wrong asset, and `build_wbp.py` would faithfully create it.

    Two not-found cases, deliberately different:
      * the text declares NO classes at all — it is a synthetic fragment, not a header, so
        the whole text is the body (this is what the self-test's fake headers are);
      * the text declares classes but not THIS one — the plan names a class its header does
        not define, and returning "" makes every planned bind fail loudly. That is the
        answer we want; silently falling back would restore the exact bug above.
    """
    starts = [(m.start(), m.group("name")) for m in _CLASS_DECL.finditer(header_text)]
    if not starts:
        return header_text
    for i, (pos, name) in enumerate(starts):
        if name == cpp_class:
            end = starts[i + 1][0] if i + 1 < len(starts) else len(header_text)
            return header_text[pos:end]
    return ""


def required_bind_widgets(header_rel: str, cpp_class: str | None = None) -> dict[str, dict]:
    """{member name: {"class": "UCommonActivatableWidgetStack", "optional": bool}}

    Parsed from the header so the plan cannot drift from the code silently. `cpp_class`
    narrows the parse to that class's body; omitting it parses the whole file, which is only
    correct for a header that declares one class.
    """
    text = (REPO / header_rel).read_text()
    if cpp_class:
        text = class_body(text, cpp_class)
    out = {}
    for m in _BIND_RE.finditer(text):
        out[m.group("name")] = {
            "class": m.group("ptr") or m.group("raw"),
            "optional": bool(m.group("opt")),
        }
    return out


def _leaf_class(class_path: str) -> str:
    """A tree node's class path -> the C++ leaf name UMG will type-check against.

    /Script/CommonUI.CommonActivatableWidgetStack -> UCommonActivatableWidgetStack
    /Game/UI/WBP_ProgressBar.WBP_ProgressBar_C    -> UBRProgressBar

    The second form resolves THROUGH THIS PLAN, because a generated WBP class is its C++
    parent as far as a BindWidget is concerned — which is the only way an abstract
    `UBRProgressBar` bind can ever be satisfied.
    """
    if class_path.startswith("/Script/"):
        return "U" + class_path.rsplit(".", 1)[-1]
    spec = PLAN.get(_hosted_asset(class_path))
    if spec is None:
        return "U" + class_path.rsplit(".", 1)[-1]   # unknown -> fails the exact match below
    return "U" + spec["parent_class"].rsplit(".", 1)[-1]


def _hosted_asset(class_path: str) -> str:
    """/Game/UI/WBP_ProgressBar.WBP_ProgressBar_C -> WBP_ProgressBar"""
    return class_path.rsplit("/", 1)[-1].split(".", 1)[0]


# UMG accepts a SUBCLASS for a BindWidget member — `TObjectPtr<UPanelWidget> Foo` is
# satisfied by a UVerticalBox. This validator has no engine and therefore no reflection,
# so the base chain of every class the plan uses is declared here as data. Adding a widget
# class to a plan means adding its chain here; an unknown class falls back to exact match,
# which fails loudly rather than passing something wrong.
_BASES = {
    # WBP_Modal_Warning's Confirm/Cancel binds are declared UCommonButtonBase and are planned
    # as WBP_HighlightButton, so the check needs this chain to walk.
    "UBRHighlightButton": ["UCommonButtonBase", "UCommonUserWidget", "UUserWidget", "UWidget"],
    "UOverlay":        ["UPanelWidget", "UWidget"],
    "UCanvasPanel":    ["UPanelWidget", "UWidget"],
    "UVerticalBox":    ["UPanelWidget", "UWidget"],
    "UHorizontalBox":  ["UPanelWidget", "UWidget"],
    "USizeBox":        ["UContentWidget", "UPanelWidget", "UWidget"],
    "UImage":          ["UWidget"],
    "UProgressBar":    ["UWidget"],
    "UCommonTextBlock": ["UTextBlock", "UTextLayoutWidget", "UWidget"],
    "UCommonActivatableWidgetStack":
        ["UCommonActivatableWidgetContainerBase", "UWidget"],
    "UBRProgressBar":  ["UCommonUserWidget", "UUserWidget", "UWidget"],
    # Both are UWidget-over-a-Slate-leaf, NOT UserWidgets — the chain is one link long, and
    # that is the point of them (BRHairlineBorder.h). `UBRRule` derives from the border, so a
    # bind typed UBRHairlineBorder is satisfied by a rule; the reverse is correctly not.
    "UBRHairlineBorder": ["UWidget"],
    "UBRRule":           ["UBRHairlineBorder", "UWidget"],
}


def _satisfies(planned: str, declared: str) -> bool:
    return planned == declared or declared in _BASES.get(planned, [])


# ---------------------------------------------------------------------------
# Validation — every failure here is one that would otherwise surface in PIE
# ---------------------------------------------------------------------------

def _slot_height(node: dict) -> float | None:
    """The authored height of a canvas-slotted node, or None if it is not one.

    With min == max the anchor is a POINT and Offsets.Bottom IS the height — see canvas_slot.
    Any other slot kind (FILL, CENTER) sizes from its parent and cannot be checked here.
    """
    try:
        d = node["slot"]["layoutData"]
        if d["anchors"]["minimum"] != d["anchors"]["maximum"]:
            return None
        return float(d["offsets"]["bottom"])
    except (KeyError, TypeError):
        return None


def validate_art(asset: str, node: dict) -> list[str]:
    """The `font` / `brush` keys, checked before an editor is ever opened.

    Everything here fails the PLAN, not the asset — except a missing brush texture, which is
    deliberately not an error at all (see `texture_problem`) and is reported separately.
    """
    errs: list[str] = []
    name = node["name"]

    style_name = node.get("font")
    if style_name is not None:
        if node["class"] not in _FONT_CLASSES:
            errs.append(f"{asset}: '{name}' carries a font but is a "
                        f"{node['class'].rsplit('.', 1)[-1]}, which has no `font` property")
        elif style_name not in TYPE_STYLES:
            errs.append(f"{asset}: '{name}' names text style '{style_name}', which is not in "
                        f"figma_tokens.json ({len(TYPE_STYLES)} styles read from Figma)")
        else:
            style = TYPE_STYLES[style_name]
            family = style["family"]
            if family not in FONT_ASSETS:
                errs.append(f"{asset}: '{name}' wants family '{family}', which has no font "
                            f"asset in UE (have {sorted(FONT_ASSETS)})")
            else:
                asset_path, typefaces = FONT_ASSETS[family]
                if style["weight"] not in typefaces:
                    errs.append(f"{asset}: '{name}' wants typeface '{style['weight']}' from "
                                f"{asset_path}, which contains {sorted(typefaces)} — "
                                "TypefaceFontName must match a typeface in the composite font "
                                "or the text silently renders in the fallback face")
            # Does the style plausibly FIT the rect the layout measured for it? Only the
            # unambiguous case is an error: a face taller than its own box cannot fit at any
            # cap height. This is the check that would have caught reaching for
            # `Display/Item Title` (48) for a magazine readout measured at 43.
            h = _slot_height(node)
            if h is not None and style["size"] > h:
                errs.append(f"{asset}: '{name}' is {style['size']}px "
                            f"('{style_name}') in a {h}px-tall slot — it cannot fit")

    if node.get("brush") is not None and node["class"] not in _BRUSH_CLASSES:
        errs.append(f"{asset}: '{name}' carries a brush but is a "
                    f"{node['class'].rsplit('.', 1)[-1]}, which has no `brush` property")

    return errs


def validate(asset: str, spec: dict) -> list[str]:
    errs: list[str] = []
    tree = spec["tree"]
    names = [n["name"] for n in tree]

    # The `class` key is what slices the header, and it is derivable from `parent_class`.
    # Both are spelled out because both are read by humans and by wbp_expect.py — so the
    # redundancy is checked here rather than left free to drift.
    cpp = spec.get("class")
    want_cpp = "U" + spec["parent_class"].rsplit(".", 1)[-1]
    if cpp != want_cpp:
        errs.append(f"{asset}: class is '{cpp}' but parent_class {spec['parent_class']} "
                    f"names '{want_cpp}'")

    dupes = {n for n in names if names.count(n) > 1}
    if dupes:
        errs.append(f"{asset}: duplicate widget names {sorted(dupes)}")

    roots = [n for n in tree if n["parent"] is None]
    if len(roots) != 1:
        errs.append(f"{asset}: expected exactly 1 root widget, got {len(roots)}")

    seen: set[str] = set()
    for node in tree:
        p = node["parent"]
        if p is None:
            seen.add(node["name"])
            continue
        if p not in names:
            errs.append(f"{asset}: {node['name']} parents to unknown '{p}'")
        elif p not in seen:
            errs.append(f"{asset}: {node['name']} precedes its parent '{p}' — "
                        "tree order must be creation order")
        seen.add(node["name"])

    for node in tree:
        errs += validate_art(asset, node)

    # THE CHECK THIS FILE EXISTS FOR
    required = required_bind_widgets(spec["header"], cpp)
    planned = {n["name"]: n for n in tree if n.get("bind")}

    for name, info in required.items():
        if name in planned:
            want, got = info["class"], _leaf_class(planned[name]["class"])
            if not _satisfies(got, want):
                errs.append(f"{asset}: '{name}' is {want} in {spec['header']} "
                            f"but {got} in the plan, and {got} is not a {want}")
        elif not info["optional"]:
            errs.append(f"{asset}: '{name}' is a NON-OPTIONAL BindWidget in "
                        f"{spec['header']} and the plan does not create it — "
                        "the WBP will fail to compile at asset load")

    for name in planned:
        if name not in required:
            errs.append(f"{asset}: plan marks '{name}' bind:True but "
                        f"{spec['header']} declares no such BindWidget")

    return errs


def validate_all() -> list[str]:
    """Every asset, plus the one rule that only exists ACROSS assets: build order.

    `build_wbp.py` walks PLAN in insertion order and `AddWidget` resolves a hosted class by
    path. A parent authored before its child would ask the editor for a class that does not
    exist yet, and the failure ("could not resolve widgetClass") names the wrong cause.
    """
    errs: list[str] = []
    built: list[str] = []
    for asset, spec in PLAN.items():
        errs += validate(asset, spec)
        for node in spec["tree"]:
            if node["class"].startswith("/Script/"):
                continue
            child = _hosted_asset(node["class"])
            if child not in PLAN:
                errs.append(f"{asset}: hosts '{child}', which this plan does not build")
            elif child not in built:
                errs.append(f"{asset}: hosts '{child}', which PLAN lists AFTER it — "
                            "a hosted class must be generated first")
        built.append(asset)
    return errs


def skipped_brushes() -> list[str]:
    """Brushes the plan asks for whose texture is not on disk. NOTES, never errors.

    Kept out of `validate_all()` on purpose: the HUD texture set is being extended by another
    lane, so a plan that is correct today and blocked by a texture that lands tomorrow would
    stop every build for no defect. Loud in the run log, fatal to nothing.
    """
    out = []
    for asset, spec in PLAN.items():
        for node in spec["tree"]:
            if node.get("brush"):
                why = texture_problem(node["brush"])
                if why:
                    out.append(f"{asset}.{node['name']}: {why}")
    return out


if __name__ == "__main__":
    import sys
    problems = validate_all()
    for p in problems:
        print("ERROR:", p)
    if not problems:
        for asset, spec in PLAN.items():
            req = required_bind_widgets(spec["header"], spec["class"])
            print(f"{asset}: {len(spec['tree'])} widgets, parent {spec['parent_class']}")
            print(f"  BindWidget contract from {spec['header']} ({spec['class']}): "
                  + (", ".join(f"{k}:{v['class']}" + ("?" if v["optional"] else "")
                               for k, v in req.items()) or "(none declared)"))
            for node in spec["tree"]:
                if node.get("font"):
                    s = TYPE_STYLES[node["font"]]
                    print(f"  font {node['name']}: {node['font']} — {s['family']} "
                          f"{s['weight']} {s['size']}px, spacing {s['ls_umg']}/1000em "
                          f"({s['ls_pct']}% in Figma)")
                if node.get("brush"):
                    print(f"  brush {node['name']}: {node['brush']['texture']} "
                          f"@ {node['brush']['size'][0]}x{node['brush']['size'][1]}")
        for note in skipped_brushes():
            print("NOTE (not an error):", note)
        print("PLAN OK")
    sys.exit(1 if problems else 0)
