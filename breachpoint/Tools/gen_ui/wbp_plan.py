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

import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

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

FILL = {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill",
        "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}}

# An Overlay slot that centres its child and lets it OVERFLOW. The reticle needs exactly
# this: `UBRReticleWidget::ApplyArt` calls `SetDesiredSizeOverride` with the weapon's
# authored edge length (Magnum 36, AR/BR 43, Shotgun 52, Sniper 58), and the size IS the
# spread readout. Any slot that constrained the child would silently delete the only
# accuracy cue the centre of the screen carries.
CENTER = {"horizontalAlignment": "HAlign_Center", "verticalAlignment": "VAlign_Center",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}}

UI_FOLDER = "/Game/UI"


def wbp_class(asset: str) -> str:
    """The GENERATED class of another asset in this plan: `/Game/UI/WBP_X.WBP_X_C`.

    Both `_C` and the full `/Game/Dir/Name.Name_C` form are load-bearing: the short form
    makes the MCP server drop the whole argument object (build_wbp.py docstring, note 2),
    and the path without `_C` names the Blueprint asset rather than the class you can
    instance.

    Every `BR` UI class is `UCLASS(Abstract)`, so a `BindWidget` typed `UBRProgressBar` can
    ONLY ever be satisfied by a generated child like this — never by the C++ class path.
    """
    return f"{UI_FOLDER}/{asset}.{asset}_C"


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
        "folder": UI_FOLDER,
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
        "folder": UI_FOLDER,
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
    # WBP_KillfeedEntry — layout only, and NOTHING IN IT IS BINDABLE YET.
    #
    # UBRKillfeedEntryWidget declares ZERO BindWidget members. Its only update path
    # is BP_OnEntrySet, a BlueprintImplementableEvent — and implementing that needs
    # an event graph NODE, which R18/R26 forbid in a WBP. That is decision D1
    # (ROADMAP §1), still open, and it blocks this asset from being finished.
    #
    # The row geometry below is real design work and is worth landing. The widget
    # NAMES are chosen as the contract the C++ packet should adopt: adding
    # BindWidget members with these exact names makes this WBP correct with no
    # re-authoring, and `validate()` will then enforce the match automatically.
    # Filed as a contract_gap; do not paper over it with a property binding, which
    # is a per-frame poll wearing a different hat (law 4).
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
        "folder": UI_FOLDER,
        "parent_class": "/Script/Breachpoint.BRKillfeedEntryWidget",
        "class": "UBRKillfeedEntryWidget",
        "header": "Source/Breachpoint/UI/BRHUDLayout.h",
        "notes": "One killfeed row. Names are a proposed BindWidget contract; the C++ "
                 "parent declares none yet (D1 open). Row box is the frame's 340x20.",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None},
            {"name": "Row", "class": HBOX, "parent": "RootSizeBox"},
            {"name": "KillerNameText", "class": TEXT, "parent": "Row"},
            {"name": "WeaponIcon", "class": IMAGE, "parent": "Row"},
            {"name": "VictimNameText", "class": TEXT, "parent": "Row"},
            # The Spotter line reserves its slot and renders EMPTY when the string is
            # empty — it never collapses layout and never waits on the LLM.
            # Offline ⇒ identical HUD minus flavour (ue5-ui-architecture §5).
            {"name": "SpotterLineText", "class": TEXT, "parent": "Row"},
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
        "folder": UI_FOLDER,
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
        "folder": UI_FOLDER,
        "parent_class": "/Script/Breachpoint.BRAmmoBlock",
        "class": "UBRAmmoBlock",
        "header": "Source/Breachpoint/UI/HUD/BRAmmoBlock.h",
        "notes": "Weapon caption, mag / reserve and the ghosted stowed weapon. Rects are "
                 "the loadout-tray frame's own children, re-based to this widget's origin.",
        "tree": [
            {"name": "AmmoCanvas", "class": CANVAS, "parent": None},
            # tray-local weapon.name [60,44,87,14]
            {"name": "ActiveWeaponText", "class": TEXT, "parent": "AmmoCanvas",
             "slot": canvas_slot(0, 0, 87, 14), "bind": True},
            # tray-local weapon.mag [74,58,36,43]
            {"name": "MagazineText", "class": TEXT, "parent": "AmmoCanvas",
             "slot": canvas_slot(14, 14, 36, 43), "bind": True},
            # tray-local weapon.reserve [138,70,28,26]. The `div` glyph between them is art.
            {"name": "ReserveText", "class": TEXT, "parent": "AmmoCanvas",
             "slot": canvas_slot(78, 26, 28, 26), "bind": True},
            # tray-local stowed.label [120,92,93,12]. The stowed BAR is art.
            {"name": "StowedWeaponText", "class": TEXT, "parent": "AmmoCanvas",
             "slot": canvas_slot(60, 48, 93, 12), "bind": True},
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
        "folder": UI_FOLDER,
        "parent_class": "/Script/Breachpoint.BRReticleWidget",
        "class": "UBRReticleWidget",
        "header": "Source/Breachpoint/UI/HUD/BRReticleWidget.h",
        "notes": "Reticle + hit marker, centred, unclamped. C++ drives the size at runtime.",
        "tree": [
            {"name": "ReticleOverlay", "class": OVERLAY, "parent": None},
            {"name": "ReticleImage", "class": IMAGE, "parent": "ReticleOverlay",
             "slot": CENTER, "bind": True},
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
        "folder": UI_FOLDER,
        "parent_class": "/Script/Breachpoint.BRMatchBand",
        "class": "UBRMatchBand",
        "header": "Source/Breachpoint/UI/HUD/BRMatchBand.h",
        "notes": "Ally score / clock / enemy score, band-local rects from the frame's own "
                 "children. Band centre is 625.67, measured, not 640.",
        "tree": [
            {"name": "BandCanvas", "class": CANVAS, "parent": None},
            # band-local ScoreSelf [90,1,34,20]
            {"name": "AllyScoreText", "class": TEXT, "parent": "BandCanvas",
             "slot": canvas_slot(90, 1, 34, 20), "bind": True},
            # band-local Timer [138,1,43,20]
            {"name": "ClockText", "class": TEXT, "parent": "BandCanvas",
             "slot": canvas_slot(138, 1, 43, 20), "bind": True},
            # band-local ScoreThem [200,1,34,20]
            {"name": "EnemyScoreText", "class": TEXT, "parent": "BandCanvas",
             "slot": canvas_slot(200, 1, 34, 20), "bind": True},
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
        "folder": UI_FOLDER,
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
        "folder": UI_FOLDER,
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
            {"name": "GrenadeCountText", "class": TEXT, "parent": "TrayCanvas",
             "slot": canvas_slot(14, 8, 26, 26), "bind": True},
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
        "folder": UI_FOLDER,
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
}


def _satisfies(planned: str, declared: str) -> bool:
    return planned == declared or declared in _BASES.get(planned, [])


# ---------------------------------------------------------------------------
# Validation — every failure here is one that would otherwise surface in PIE
# ---------------------------------------------------------------------------

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
        print("PLAN OK")
    sys.exit(1 if problems else 0)
