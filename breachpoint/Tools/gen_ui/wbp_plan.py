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

# The hairline primitives. Both are `UWidget` over a Slate leaf, NOT UserWidgets — so unlike
# every UBR* class in this plan they are placed DIRECTLY and need no generated child. That is
# the whole reason `WBP_PanelBorder` does not exist and never will (BRHairlineBorder.h states
# the arithmetic: ~4500 widgets across the front end if it were a four-UImage composite).
HAIRLINE = "/Script/Breachpoint.BRHairlineBorder"
RULE = "/Script/Breachpoint.BRRule"

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
    "WBP_ReticleWidget":       UI_HUD,
    "WBP_MatchBand":           UI_HUD,
    "WBP_Killfeed":            UI_HUD,
    "WBP_KillfeedEntryWidget": UI_HUD,
    "WBP_EquipmentTray":       UI_HUD,
    "WBP_ProgressBar":         UI_COMPONENTS,
    "WBP_MenuRow":             UI_COMPONENTS,
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
        "tree": [
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
