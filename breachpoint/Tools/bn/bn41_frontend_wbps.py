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
BUTTON = '/Script/UMG.Button'
IMAGE = '/Script/UMG.Image'
TEXT = '/Script/CommonUI.CommonTextBlock'
SIZEBOX = '/Script/UMG.SizeBox'

# hud/panel #0A1018 at 0.88 and the scrim — the two grounds this file is allowed to tint
# (00-HUD-MEASURED colour table; the referee's notch language ships M2, a flat panel M1).
PANEL = {"r": 0.039, "g": 0.063, "b": 0.094, "a": 0.88}
BAND = {"r": 0.020, "g": 0.031, "b": 0.047, "a": 0.94}
SCRIM = {"r": 0.0, "g": 0.0, "b": 0.0, "a": 0.62}


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
        ('TitleText', TEXT, 'FrontEndCanvas',
         # Navigation Bar `21:32864` is 33,45 666x30. M1 has no tabs, so the wordmark stands
         # in for PLAY/CREATE/COMMUNITY/SHOP in that slot until M2 builds them.
         {"layoutData": topleft(33, 45, 666, 30), "bAutoSize": False},
         {"text": "BREACHPOINT"}, False, 30),
        ('NewsPanel', BORDER, 'FrontEndCanvas',
         {"layoutData": topleft(69, 138, 349, 222), "bAutoSize": False},  # News `I…7:7381`
         {"brushColor": PANEL, "padding": {"left": 12.0, "top": 12.0, "right": 12.0, "bottom": 12.0}},
         False, None),
        ('NewsTitleText', TEXT, 'NewsPanel',
         {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Bottom",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}},
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
        row_box('PlayRowBox'),
        ('PlayButton', BUTTON, 'PlayRowBox', BTN_FILL, None, True, None),
        ('PlayLabel', TEXT, 'PlayButton', None, {"text": "PLAY"}, False, 14),
        row_box('CustomsRowBox'),
        ('CustomsButton', BUTTON, 'CustomsRowBox', BTN_FILL, {"bIsEnabled": False}, False, None),
        ('CustomsLabel', TEXT, 'CustomsButton', None, {"text": "CUSTOM GAMES"}, False, 14),
        row_box('AcademyRowBox'),
        ('AcademyButton', BUTTON, 'AcademyRowBox', BTN_FILL, {"bIsEnabled": False}, False, None),
        ('AcademyLabel', TEXT, 'AcademyButton', None, {"text": "ACADEMY"}, False, 14),
        row_box('QuitRowBox', 0.0),
        ('QuitButton', BUTTON, 'QuitRowBox', BTN_FILL, None, True, None),
        ('QuitLabel', TEXT, 'QuitButton', None, {"text": "QUIT"}, False, 14),
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
        ('PromptText', TEXT, 'ProfileBar',
         {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Center",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}},
         {"text": "ENTER — SELECT      ESC — QUIT"}, False, 11),
    ]


def value_row(prefix, label):
    """One MAP/MODE/BOTS row: a bound Button whose child Overlay carries the label left
    and the BOUND value right — the reference UI's label-left / value-right row."""
    return [
        row_box(prefix + 'RowBox'),
        (prefix + 'Button', BUTTON, prefix + 'RowBox', BTN_FILL, None, True, None),
        # MEASURED 1 Sep: with the ButtonSlot left at its default (Center) the Overlay
        # shrink-wraps to its widest child, so HAlign_Left/Right resolve INSIDE that shrunk
        # box and label and value land on top of each other (MapLabel x97 vs MapValue x95).
        # Filling the ButtonSlot is what makes label-left / value-right an actual split.
        (prefix + 'Row', OVERLAY, prefix + 'Button', BTN_FILL, None, False, None),
        (prefix + 'Label', TEXT, prefix + 'Row',
         {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Center",
          "padding": {"left": 4.0, "top": 0.0, "right": 0.0, "bottom": 0.0}},
         {"text": label}, False, 14),
        (prefix + 'ValueText', TEXT, prefix + 'Row',
         {"horizontalAlignment": "HAlign_Right", "verticalAlignment": "VAlign_Center",
          "padding": {"left": 0.0, "top": 0.0, "right": 4.0, "bottom": 0.0}},
         {"text": ""}, True, 14),
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
         # BOUND as of the BN43 art pass: RefreshDisplay sets the brush from the selected
         # map's soft PreviewTexture and un-collapses it. Collapsed here is the no-plate
         # resting state, which is also what an empty ini entry leaves it at.
         {"visibility": "Collapsed"}, True, None),
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
        ('StartLabel', TEXT, 'StartButton', None, {"text": "START GAME"}, False, 14),
        ('BreakdownPanel', BORDER, 'SetupCanvas',
         {"layoutData": topleft(466, 76, 349, 332), "bAutoSize": False},  # Breakdown `21:43050`
         {"brushColor": PANEL, "padding": {"left": 16.0, "top": 16.0, "right": 16.0, "bottom": 16.0}},
         False, None),
        ('BreakdownTitleText', TEXT, 'BreakdownPanel',
         {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Top",
          "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}},
         {"text": "DETAILS"}, False, 16),
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
         # Button Prompts `21:32863`: x60 y685 h20. Width is NOT measured — the referee says
         # it varies with prompt count (62–227), so 150 is a bounded pick, not a reading.
         {"layoutData": topleft(60, 685, 150, 20), "bAutoSize": False, "zOrder": 1}, None, True, None),
        ('BackLabel', TEXT, 'BackButton', None, {"text": "ESC — BACK"}, False, 11),
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
