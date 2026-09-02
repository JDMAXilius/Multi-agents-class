"""BN41/BN42 — the two front-end screens' WBPs: the PLAN, and the batch FALLBACK.

R46 (founder, 1 Sep): editor jobs run through the unreal-mcp tools DIRECTLY from the
editor session; a Python driver is the fallback, not the path. This file's standing under
that ruling: `frontend_plan()` / `playsetup_plan()` are the committed, diffable widget
tables TICKET_BN42's direct MCP calls walk (and bn41_selftest.py checks against the C++),
and `build()` below is the batch fallback — run it only if the direct path is blocked,
and say so in the ticket's Log.

    (live editor with the MCP server up)
    python3 Tools/bn/bn41_frontend_wbps.py             # build both, delete-first
    python3 Tools/bn/bn41_frontend_wbps.py --verify    # read-only: tree vs plan

Referee: Source/BreachpointNext/UI/Content/BN/UI/Assets/01-MENU-MEASURED.md — every box
below carries its Figma node id in a comment; argue with the node, not with this file.

METHOD, all transcribed from the two proven builders (law: transcription over invention):
- transport + AddWidget/props/compile/save idioms: Tools/bn/bn11_lib.py (the BN lane)
- delete-first creation + root-strip + read-back verification: mcp-ui/gen_ui/build_wbp.py
  (its module docstring's three expensive lessons apply verbatim — FindPackage clobber
  guard is MEMORY not disk, refs need /Game/X/Y.Y, property names are camelCase and
  set_properties takes a JSON STRING)
- geometry idiom: docs/archive/BREACHPOINT-NEXT-TASK-R7-WBP-HUD.md — absolute 1280x720
  canvas offsets, top-left anchors; the DefaultEngine.ini DPI curve does every resolution,
  so NOTHING here is multiplied by 1.5.

STRINGS AND COLOURS. Bound texts stay empty — C++ owns them (the pause screen's law).
Static chrome strings (row labels, the wordmark) are set HERE because this file is the
committed, diffable artifact — the same standing the old wbp_plan.py had. Colours: only
panel grounds and the scrim are tinted here (a default UBorder is BLINDING white — the
one colour bn11 could avoid setting and this file cannot); every text keeps its default
and C++ tints what it owns.

BINDS MUST MATCH THE C++ BY NAME — BNScreen_FrontEnd.h / BNScreen_PlaySetup.h are the
contract; a rename there without one here is the empty-HUD-at-PIE failure the old plan
file documents. The compile step at the end is where the engine enforces it.
"""
import argparse
import json
import sys

import bn11_lib as B

FRONTEND = 'WBP_BNScreen_FrontEnd'
PLAYSETUP = 'WBP_BNScreen_PlaySetup'

CANVAS = '/Script/UMG.CanvasPanel'
BORDER = '/Script/UMG.Border'
OVERLAY = '/Script/UMG.Overlay'
VBOX = '/Script/UMG.VerticalBox'
# THE MEASURED MENU ROW, not /Script/UMG.Button. UBRButton is the component Figma sheet
# 12:724 was measured against: the Idle->Hover INVERSION, the 28-high row, the token palette.
# An engine UButton renders the grey capsule the founder was looking at.
BUTTON = '/Game/UI/Components/Buttons/WBP_ButtonDefault.WBP_ButtonDefault_C'
IMAGE = '/Script/UMG.Image'

# Level art. These are DESIGN-TIME brushes and they are not decoration: an Image with
# resourceObject None and visibility Collapsed shows NOTHING when you open the WBP, which is
# exactly what the founder hit ("I do not see the levels images here at all"). C++ still owns
# the runtime value - PlaySetup swaps the plate per map - but the asset now carries a real
# default so the widget is legible on its own.
NEWS_TEX = '/Game/BN/UI/Art/T_News_Spillway.T_News_Spillway'
PREVIEW_TEX = '/Game/BN/UI/Art/T_Preview_Spillway.T_Preview_Spillway'


def brush(tex, w, h):
    return {"resourceObject": {"refPath": tex}, "drawAs": "Image",
            "imageSize": {"x": float(w), "y": float(h)}}
TEXT = '/Script/CommonUI.CommonTextBlock'
SIZEBOX = '/Script/UMG.SizeBox'

# hud/panel #0A1018 at 0.88 and the scrim — the two grounds this file is allowed to tint
# (00-HUD-MEASURED colour table; the referee's notch language ships M2, a flat panel M1).
PANEL = {"r": 0.039, "g": 0.063, "b": 0.094, "a": 0.88}
BAND = {"r": 0.020, "g": 0.031, "b": 0.047, "a": 0.94}
SCRIM = {"r": 0.0, "g": 0.0, "b": 0.0, "a": 0.62}
# The chassis accent bars and the selection caret. Geometry is measured; these two tints are
# ours (the referee carries no colour for them) — same standing as the panel alphas.
NOTCH = {"r": 0.75, "g": 0.80, "b": 0.86, "a": 0.90}
CARET = {"r": 0.90, "g": 0.94, "b": 1.00, "a": 1.00}
DIM = {"r": 0.514, "g": 0.592, "b": 0.663, "a": 1.0}   # BNUIColors::InkDim, as a literal
BAR = '/Script/UMG.ProgressBar'


def topleft(x, y, w, h):
    # bn11_matchband's exact idiom: absolute top-left canvas layout in design space.
    return {"offsets": {"left": float(x), "top": float(y), "right": float(w), "bottom": float(h)},
            "anchors": {"minimum": {"x": 0.0, "y": 0.0}, "maximum": {"x": 0.0, "y": 0.0}},
            "alignment": {"x": 0.0, "y": 0.0}}


def stretch():
    return {"offsets": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0},
            "anchors": {"minimum": {"x": 0.0, "y": 0.0}, "maximum": {"x": 1.0, "y": 1.0}},
            "alignment": {"x": 0.0, "y": 0.0}}


# referee §1 "Menu in Border" `I…7:7383`: the 349 chassis carries a 3px border inset
# (Menu List 343 wide) and THEN a 16,16 content inset -> contents are 311 wide, which is
# exactly the §1 Text Button width. Both insets are this Border's padding, so 311 falls out
# of 349 - 2*MENU_PAD instead of being a magic number.
MENU_BORDER_INSET = 3.0     # §1: "3px border inset; Menu List 343 wide"
MENU_CONTENT_INSET = 16.0   # §1: "contents inset 16,16, width 311"
MENU_PAD = MENU_BORDER_INSET + MENU_CONTENT_INSET   # 19 -> 349 - 38 = 311


def menu_pad():
    return {"left": MENU_PAD, "top": MENU_PAD, "right": MENU_PAD, "bottom": MENU_PAD}


def row_pitch(bottom=12.0):
    # 28-high rows at pitch 40 (referee §1 Text Button): the 12 lives in the slot, and the
    # 28 lives in the row's SizeBox below — it is NOT free. MEASURED in the editor 1 Sep:
    # a bare UButton auto-sizing to its 14pt label renders 311x31, so four rows came to 160
    # and OVERFLOWED the 148 content box of the 186-high menu panel (186 - 2*19). With the
    # SizeBox: 4*28 + 3*12 = 148 exactly. That arithmetic is why 28 is load-bearing, not a nit.
    return {"padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": float(bottom)},
            "horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Top"}


ROW_H = 28.0   # referee §1 Text Button height; enforced, never inherited from the font


def row_box(name, bottom=12.0):
    """The SizeBox that PINS a menu row to 28. Wraps the button; the button then fills it."""
    return (name, SIZEBOX, 'MenuColumn', row_pitch(bottom),
            {"bOverride_HeightOverride": True, "heightOverride": ROW_H}, False, None)


BTN_FILL = {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill"}


# Node schema: (name, class, parent, slotProps, widgetProps, bind, fontSize)
# widgetProps go through the bn11 verified-write path; fontSize uses B.font_sized so the
# typeface object is never dropped by a partial write.

def frontend_plan():
    """FE_Play `21:32824` — referee §2. Menu Combo at (69,138): news 349x222, menu
    349x186 at y+232, description 349x37 at y+473."""
    return [
        ('FrontEndCanvas', CANVAS, None, None, None, False, None),
        ('Scrim', IMAGE, 'FrontEndCanvas', {"layoutData": stretch(), "bAutoSize": False},
         {"colorAndOpacity": SCRIM}, False, None),
        # THE NAVIGATION BAR, `21:32864` at 33,45 666x30. The wordmark that used to stand in
        # this slot is GONE — these are the real tabs. Read off the node: four "Menu Slider
        # Button" at 138x26, bar-local x 39/189/339/489, i.e. pitch 150 with a 12 gap; the
        # LB/RB prompt glyphs are 27x15 at bar-local 27 and 639.
        # Only PLAY has a screen. C++ selects it and disables the other three by name.
        ('NavPlayTab', BUTTON, 'FrontEndCanvas',
         {"layoutData": topleft(33 + 39, 47, 138, 26), "bAutoSize": False, "zOrder": 2}, None, True, None),
        ('NavCreateTab', BUTTON, 'FrontEndCanvas',
         {"layoutData": topleft(33 + 189, 47, 138, 26), "bAutoSize": False, "zOrder": 2}, None, True, None),
        ('NavCommunityTab', BUTTON, 'FrontEndCanvas',
         {"layoutData": topleft(33 + 339, 47, 138, 26), "bAutoSize": False, "zOrder": 2}, None, True, None),
        ('NavShopTab', BUTTON, 'FrontEndCanvas',
         {"layoutData": topleft(33 + 489, 47, 138, 26), "bAutoSize": False, "zOrder": 2}, None, True, None),
        ('NavPromptLeft', TEXT, 'FrontEndCanvas',
         {"layoutData": topleft(33 + 27, 52.5, 27, 15), "bAutoSize": False, "zOrder": 3},
         {"text": "LB", "colorAndOpacity": DIM}, False, 11),
        ('NavPromptRight', TEXT, 'FrontEndCanvas',
         {"layoutData": topleft(33 + 639, 52.5, 27, 15), "bAutoSize": False, "zOrder": 3},
         {"text": "RB", "colorAndOpacity": DIM}, False, 11),
        ('NewsPanel', BORDER, 'FrontEndCanvas',
         {"layoutData": topleft(69, 138, 349, 222), "bAutoSize": False},  # News `I…7:7381`
         # padding 0, NOT the 12 the panel used to carry: the art bleeds to the card edge
         # and the 12 inset moved onto the TITLE's slot below.
         {"brushColor": PANEL, "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}},
         False, None),
        # A Border holds ONE child, so the card's art and its headline share an Overlay.
        ('NewsStack', OVERLAY, 'NewsPanel',
         {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}}, None, False, None),
        # FIRST child = painted first = BEHIND the headline. Order is the z-order here.
        ('NewsImage', IMAGE, 'NewsStack',
         {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}},
         {"brush": brush(NEWS_TEX, 349, 222), "visibility": "HitTestInvisible"}, True, None),
        ('NewsTitleText', TEXT, 'NewsStack',
         {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Bottom",
          "padding": {"left": 12.0, "top": 12.0, "right": 12.0, "bottom": 12.0}},
         {"text": "NEW ARENA: SPILLWAY"}, False, 16),
        ('MenuPanel', BORDER, 'FrontEndCanvas',
         {"layoutData": topleft(69, 370, 349, 186), "bAutoSize": False},  # Menu in Border `I…7:7383`
         {"brushColor": PANEL, "padding": menu_pad()},
         False, None),
        ('MenuColumn', VBOX, 'MenuPanel',
         {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}}, None, False, None),
        # rows 311x28 pitch 40 (`I…21:32897..900`): PLAY live, two dead, QUIT live.
        # The SizeBox is what makes 28 true — see row_pitch's note; without it the row is 31.
        # No *Label children: the component owns its label and C++ sets it with SetLabelText,
        # so the WBP still types no strings and a menu row is ONE widget again. The two dead
        # slots are bound now too — C++ disables them, rather than an asset property doing it.
        row_box('PlayRowBox'),
        ('PlayButton', BUTTON, 'PlayRowBox', BTN_FILL, None, True, None),
        row_box('CustomsRowBox'),
        ('CustomsButton', BUTTON, 'CustomsRowBox', BTN_FILL, None, True, None),
        row_box('AcademyRowBox'),
        ('AcademyButton', BUTTON, 'AcademyRowBox', BTN_FILL, None, True, None),
        row_box('QuitRowBox', 0.0),
        ('QuitButton', BUTTON, 'QuitRowBox', BTN_FILL, None, True, None),
        ('DescriptionPanel', BORDER, 'FrontEndCanvas',
         {"layoutData": topleft(69, 611, 349, 37), "bAutoSize": False},   # Description `I…7:7384`
         {"brushColor": BAND, "padding": {"left": 20.0, "top": 10.0, "right": 20.0, "bottom": 10.0}},
         False, None),
        ('DescriptionText', TEXT, 'DescriptionPanel', None, {"text": ""}, True, 12),
        ('PartyPanel', BORDER, 'FrontEndCanvas',
         {"layoutData": topleft(862, 397, 349, 273), "bAutoSize": False},  # Party List `21:32861`
         {"brushColor": PANEL, "padding": {"left": 16.0, "top": 16.0, "right": 16.0, "bottom": 16.0}},
         False, None),
        ('PartyHeaderText', TEXT, 'PartyPanel',
         {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Top",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}},
         {"text": "FIRETEAM — LOCAL"}, False, 14),
        ('ProfileBar', BORDER, 'FrontEndCanvas',
         {"layoutData": topleft(0, 670, 1280, 50), "bAutoSize": False},    # Profile Bar `21:32862`
         # 15 top/bottom is derived, not chosen: Prompts `21:32863` sit at y685 h20 inside a
         # bar at y670 h50 -> 670+15=685 and 50-15-15=20. Same number on both screens.
         {"brushColor": BAND, "padding": {"left": 60.0, "top": 15.0, "right": 60.0, "bottom": 15.0}},
         False, None),
        # THE NOTCH LANGUAGE, read off `21:32824` rather than deferred to "M2". The panel
        # chassis in Figma is a rounded rect with two 88x4.73 bars biting its top and bottom
        # edges at DIFFERENT x (top 127, bottom 215) — the asymmetry is the design, not a
        # mistake. Menu in Border is at 69,370 and its Menu List insets 3, so the bars land at
        # absolute x 199 / 287. Plain tinted Images: no texture, no material, no Tier-4 asset.
        ('MenuNotchTop', IMAGE, 'FrontEndCanvas',
         {"layoutData": topleft(199, 373, 88, 4.73), "bAutoSize": False, "zOrder": 2},
         {"colorAndOpacity": NOTCH, "visibility": "HitTestInvisible"}, False, None),
        ('MenuNotchBottom', IMAGE, 'FrontEndCanvas',
         {"layoutData": topleft(287, 553, 88, 4.73), "bAutoSize": False, "zOrder": 2},
         {"colorAndOpacity": NOTCH, "visibility": "HitTestInvisible"}, False, None),
        # Selection caret `I…7:7398` "Rectangle 278": 3x65 at x-4 of the Menu List, i.e. it
        # HANGS OUTSIDE the panel's left edge. Static here; moving it with the focused row is
        # C++'s job the day the rail gets keyboard nav.
        ('MenuCaret', IMAGE, 'FrontEndCanvas',
         {"layoutData": topleft(68, 431, 3, 65), "bAutoSize": False, "zOrder": 2},
         {"colorAndOpacity": CARET, "visibility": "HitTestInvisible"}, False, None),
        # Party List carries the same two bars (`I21:32861;7:4097/4098`), 88x4, top x130.5
        # bottom x218.5 within a panel at 862,397.
        ('PartyNotchTop', IMAGE, 'FrontEndCanvas',
         {"layoutData": topleft(992.5, 400, 88, 4), "bAutoSize": False, "zOrder": 2},
         {"colorAndOpacity": NOTCH, "visibility": "HitTestInvisible"}, False, None),
        ('PartyNotchBottom', IMAGE, 'FrontEndCanvas',
         {"layoutData": topleft(1080.5, 667, 88, 4), "bAutoSize": False, "zOrder": 2},
         {"colorAndOpacity": NOTCH, "visibility": "HitTestInvisible"}, False, None),
        # THE PROGRESSION PANEL, `21:32826` at 869,55 334x115. The title sits ABOVE the border
        # (node y -5 relative, i.e. absolute 50) and the border is 334x94 at y76; the switcher
        # dots hang BELOW at 999,176 72x10 and the prompt glyph at 836,113 20x20.
        # There is no progression system: RankText and RankProgress are fed from ini and
        # collapse to nothing when unset, rather than printing an invented rank.
        ('ProgressionTitle', TEXT, 'FrontEndCanvas',
         {"layoutData": topleft(869, 50, 108, 21), "bAutoSize": False, "zOrder": 2},
         {"text": "CAREER RANK", "colorAndOpacity": DIM}, False, 12),
        ('ProgressionPanel', BORDER, 'FrontEndCanvas',
         {"layoutData": topleft(869, 76, 334, 94), "bAutoSize": False, "zOrder": 1},
         {"brushColor": PANEL, "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}},
         False, None),
        ('ProgressionNotchTop', IMAGE, 'FrontEndCanvas',
         {"layoutData": topleft(869 + 123, 76, 88, 4), "bAutoSize": False, "zOrder": 2},
         {"colorAndOpacity": NOTCH}, False, None),
        ('ProgressionNotchBottom', IMAGE, 'FrontEndCanvas',
         {"layoutData": topleft(869 + 123, 166, 88, 4), "bAutoSize": False, "zOrder": 2},
         {"colorAndOpacity": NOTCH}, False, None),
        # Right Side of the panel is 167 wide from x1036; the line and its bar live in it.
        ('RankText', TEXT, 'FrontEndCanvas',
         {"layoutData": topleft(1046, 92, 147, 22), "bAutoSize": False, "zOrder": 2},
         {"text": ""}, True, 14),
        ('RankProgress', BAR, 'FrontEndCanvas',
         {"layoutData": topleft(1046, 120, 147, 8), "bAutoSize": False, "zOrder": 2}, None, True, None),
        ('ProgressionSwitcher', IMAGE, 'FrontEndCanvas',
         {"layoutData": topleft(999, 176, 72, 10), "bAutoSize": False, "zOrder": 2},
         {"colorAndOpacity": DIM}, False, None),
        ('PromptText', TEXT, 'ProfileBar',
         {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Center",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}},
         {"text": "ENTER — SELECT      ESC — QUIT"}, False, 11),
    ]


def value_row(prefix, label):
    """One MAP/MODE/BOTS row — now a SINGLE widget.

    The Overlay plus a label CommonTextBlock plus a value CommonTextBlock is GONE. UBRButton
    already carries both halves of a settings row (Label left, Selection right), and
    BNScreen_PlaySetup drives them with SetLabelText / SetSelectionText. `label` survives as a
    parameter only so the caller still reads as a table of rows; the string itself is C++'s.
    """
    return [
        row_box(prefix + 'RowBox'),
        (prefix + 'Button', BUTTON, prefix + 'RowBox', BTN_FILL, None, True, None),
    ]


def playsetup_plan():
    """CG_Lobby `21:43019` — referee §4. Menu Combo at (69,76): preview 349x196.7, menu
    at y+206.7 -> (69,282.7), description at y+483 -> (69,559); Breakdown (466,76) 349x332."""
    plan = [
        ('SetupCanvas', CANVAS, None, None, None, False, None),
        ('Scrim', IMAGE, 'SetupCanvas', {"layoutData": stretch(), "bAutoSize": False},
         {"colorAndOpacity": SCRIM}, False, None),
        ('PageTitleText', TEXT, 'SetupCanvas',
         # Page Title `21:43048` is 0,0 1280x75; its Title Frame `I21:43048;577:4124` is
         # 70,15 630x54 and the title text inside it is h31 at y+11.5 -> absolute y 26.5.
         # Figma splits a breadcrumb ("CUSTOMIZE ▸ ARMOR HALL", second part at x134); M1
         # renders ONE title string, so we take the frame box, not the split.
         {"layoutData": topleft(70, 26, 630, 31), "bAutoSize": False},
         {"text": "PLAY  ▸  CUSTOM GAME"}, False, 22),
        ('PreviewPanel', BORDER, 'SetupCanvas',
         {"layoutData": topleft(69, 76, 349, 196.7), "bAutoSize": False},  # Preview `I…7:7382`
         {"brushColor": PANEL, "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}},
         False, None),
        ('PreviewImage', IMAGE, 'PreviewPanel',
         {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}},
         # BOUND as of the BN43 art pass: RefreshDisplay swaps this brush per selected map.
         # The default here is Spillway, the first roster entry, so the Designer shows the
         # real thing instead of an empty box.
         {"brush": brush(PREVIEW_TEX, 349, 196.7), "visibility": "HitTestInvisible"}, True, None),
        ('MenuPanel', BORDER, 'SetupCanvas',
         {"layoutData": topleft(69, 282.7, 349, 186), "bAutoSize": False},  # Menu in Border
         {"brushColor": PANEL, "padding": menu_pad()},
         False, None),
        ('MenuColumn', VBOX, 'MenuPanel',
         {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}}, None, False, None),
    ]
    plan += value_row('Map', 'MAP')
    plan += value_row('Mode', 'MODE')
    plan += value_row('Bots', 'PLAYERS')
    plan += [
        row_box('StartRowBox', 0.0),
        ('StartButton', BUTTON, 'StartRowBox', BTN_FILL, None, True, None),
        ('BreakdownPanel', BORDER, 'SetupCanvas',
         {"layoutData": topleft(466, 76, 349, 332), "bAutoSize": False},  # Breakdown `21:43050`
         {"brushColor": PANEL, "padding": {"left": 16.0, "top": 16.0, "right": 16.0, "bottom": 16.0}},
         False, None),
        # A Border holds ONE child, so the title and the per-map body share a column.
        ('BreakdownStack', VBOX, 'BreakdownPanel',
         {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}}, None, False, None),
        ('BreakdownTitleText', TEXT, 'BreakdownStack',
         {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Top",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 12.0}},
         {"text": "DETAILS"}, False, 16),
        # BOUND and EMPTY here: the lines are the selected map's, written by RefreshDisplay
        # from that map's ini Details, so the panel changes as the MAP row cycles.
        ('BreakdownText', TEXT, 'BreakdownStack',
         {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Top",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}},
         {"text": ""}, True, 14),
        ('DescriptionPanel', BORDER, 'SetupCanvas',
         {"layoutData": topleft(69, 559, 349, 37), "bAutoSize": False},
         {"brushColor": BAND, "padding": {"left": 20.0, "top": 10.0, "right": 20.0, "bottom": 10.0}},
         False, None),
        ('DescriptionText', TEXT, 'DescriptionPanel', None, {"text": ""}, True, 12),
        ('ProfileBar', BORDER, 'SetupCanvas',
         {"layoutData": topleft(0, 670, 1280, 50), "bAutoSize": False},   # Profile Bar `21:32862`
         # 15, same derivation as FrontEnd: Prompts `21:32863` y685 h20 inside 670..720.
         {"brushColor": BAND, "padding": {"left": 60.0, "top": 15.0, "right": 60.0, "bottom": 15.0}},
         False, None),
        ('BackButton', BUTTON, 'SetupCanvas',                              # bound OPTIONAL in C++
         # Button Prompts `21:32863` READ OFF THE NODE 1 Sep: 60,685 62x20. The 150 that stood
         # here was a bounded GUESS and it was simply wrong — the frame is 62 wide.
         {"layoutData": topleft(60, 685, 62, 20), "bAutoSize": False, "zOrder": 1}, None, True, None),
    ]
    return plan


PLANS = {
    FRONTEND: ('/Script/BreachpointNext.BNScreen_FrontEnd', frontend_plan),
    PLAYSETUP: ('/Script/BreachpointNext.BNScreen_PlaySetup', playsetup_plan),
}


def asset_path(name):
    return '/Game/BN/UI/%s' % name


def build(name, parent_class, plan):
    path = asset_path(name)
    full = '%s.%s' % (path, name)
    print('==== %s -> %s' % (name, full))

    # (1) delete first — build_wbp lesson 1: the create call's clobber guard is MEMORY.
    exists = B.mcp.call(B.AST, 'exists', path=full)
    if exists:
        B.mcp.call(B.AST, 'delete', path=full)
        still = B.mcp.call(B.AST, 'exists', path=full)
        if still:
            raise RuntimeError('%s: could not delete for a clean rebuild' % name)
        print('  deleted for a clean rebuild')

    # (2) create with the C++ parent — the BindWidget contract's other half.
    B.mcp.call(B.UMG, 'CreateWidgetBlueprint', folderPath='/Game/BN/UI', assetName=name,
               parentClass={'refPath': parent_class})
    wbp = {'refPath': full}

    # strip the settings-default root so the plan is the only author of the tree
    cur = B.mcp.call(B.UMG, 'GetWidgets', widgetBlueprint=wbp)
    for w in (cur or {}).get('widgets', []):
        if w.get('parent') in (None, 'None'):
            B.mcp.call(B.UMG, 'RemoveWidget', widgetBlueprint=wbp, widget=w['widget'])

    # (3) the tree, in plan order
    handles = {}
    for (node, cls, parent, sprops, wprops, bind, fsize) in plan:
        pref = handles[parent]['widget'] if parent else None
        info = B.mcp.call(B.UMG, 'AddWidget', widgetBlueprint=wbp, widgetClass={'refPath': cls},
                          widgetDisplayName=node, parentWidget=pref, childIndex=-1)
        if not (isinstance(info, dict) and info.get('widgetName') == node):
            raise RuntimeError('%s: AddWidget %s failed: %r' % (name, node, info))
        handles[node] = info
        if bind:
            B.mcp.call(B.UMG, 'ToggleWidgetAsVariable', widgetBlueprint=wbp,
                       widget=info['widget'], bIsVariable=True)
        if fsize is not None:
            wprops = dict(wprops or {}, font=B.font_sized(info['widget'], fsize))
        # verified writes, bn11 style: set, read back, print both — a silently-wrong
        # camelCase key must be visible in the transcript, not swallowed.
        if sprops and isinstance(info.get('slot'), dict):
            print('  %s slot  set: %s' % (node, B.setp(info['slot'], sprops)))
            print('  %s slot  now: %s' % (node, B.get(info['slot'], list(sprops))[:300]))
        if wprops:
            print('  %s props set: %s' % (node, B.setp(info['widget'], wprops)))
            print('  %s props now: %s' % (node, B.get(info['widget'], list(wprops))[:300]))

    # (4) compile — the engine enforces every BindWidget here — then save.
    c, s = B.compile_and_save(name)
    print('  compile: %s  save: %s' % (c, str(s)[:120]))

    # (5) tree read-back for the receipt
    _, desc = B.widgets(name)
    print(desc)


def verify(name, plan):
    ws, desc = B.widgets(name)
    want = {n for (n, *_rest) in plan}
    got = set(ws)
    print('==== VERIFY %s: %s' % (name, 'TREE MATCHES PLAN (%d widgets)' % len(got)
          if got == want else 'missing %s extra %s' % (sorted(want - got), sorted(got - want))))
    return got == want


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--verify', action='store_true', help='read-only: on-disk tree vs plan')
    ap.add_argument('--asset', choices=sorted(PLANS), help='just one of the two')
    args = ap.parse_args()

    ok = True
    for name, (parent, planf) in PLANS.items():
        if args.asset and name != args.asset:
            continue
        if args.verify:
            ok = verify(name, planf()) and ok
        else:
            build(name, parent, planf())
    sys.exit(0 if ok else 1)
