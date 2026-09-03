#!/usr/bin/env python3
"""BN44 — WBP_BNScreen_Scoreboard + WBP_BNScoreRow laid out 1:1 to the FOUNDER'S CAPTURE of the
shipped post-game SCOREBOARD tab (2 Sep 2026; 2560x1440, scale 2.0 -> 1280x720 design).

    python3 Tools/bn/bn51_scoreboard_1to1.py --selftest   # geometry only, no editor
    python3 Tools/bn/bn51_scoreboard_1to1.py              # needs the live editor (MCP)

Every number is a measurement off that capture, listed in TICKET_BN44 §"founder's 1:1 target".
Where the capture and Figma `43:2` disagree the capture wins (founder's call); the earlier Figma
leaves this script placed (ColScore, HeaderTick, ColumnTintA/B, the x33 ModeIcon, the Figma
team-card positions) are moved or retired here.

NO COLOUR IS STORED (ASSET-RULES §5). Every tinted leaf is resolved BY NAME and coloured in
`UBNScreen_Scoreboard::RefreshHeader` / `UBNScoreRow::SetRow`. Leaves a C++ BindWidget owns are
NOT Blueprint variables (the UMG compiler would try to create the property twice).

Idempotent: `ensure()` skips widgets already present; every write is a fixed target.
"""
import json
import sys

sys.path.insert(0, __file__.rsplit('/', 1)[0])
import bn11_lib as B

BOARD = 'WBP_BNScreen_Scoreboard'
ROW = 'WBP_BNScoreRow'
CANVAS = '/Game/BN/UI/%s.%s:WidgetTree.BoardCanvas' % (BOARD, BOARD)
IMG = '/Script/UMG.Image'
TEXT = '/Script/CommonUI.CommonTextBlock'
HBOX = '/Script/UMG.HorizontalBox'
BTN = '/Game/UI/Components/Buttons/WBP_ButtonDefault.WBP_ButtonDefault_C'
PROMPT = '/Game/BN/UI/WBP_BNPromptButton.WBP_BNPromptButton_C'
SCROLL = '/Game/UI/Components/WBP_ScrollBar.WBP_ScrollBar_C'
PROFILE = '/Game/BN/UI/WBP_BNProfileBar.WBP_BNProfileBar_C'
ART = '/Game/BN/UI/Art/'
FIGMA = '/Game/BN/UI/Figma/'


def not_variable(wbp, widget):
    """ToggleWidgetAsVariable returns null on SUCCESS; bn11_lib reads a null returnValue as a
    refusal. So it is called through the raw transport and only a real error is raised."""
    try:
        B.mcp.call(B.UMG, 'ToggleWidgetAsVariable', widgetBlueprint=B.wbp(wbp), widget=widget, bIsVariable=False)
    except RuntimeError as e:
        if '"returnValue":null' not in str(e) and "'returnValue': None" not in str(e):
            raise


def tex(path):
    name = path.rsplit('/', 1)[1]
    return {"refPath": path + '.' + name}


def topleft(x, y, w, h):
    return {"offsets": {"left": x, "top": y, "right": w, "bottom": h},
            "anchors": {"minimum": {"x": 0.0, "y": 0.0}, "maximum": {"x": 0.0, "y": 0.0}},
            "alignment": {"x": 0.0, "y": 0.0}}


# Table geometry (design px): x 332.5..947.5, rows from y160, 17px rows.
TX, TW, TY = 332.5, 615.0, 160.0
CELL_W = 72.5
CELLS_X = [661.5, 735.0, 807.5, 882.5]          # KILLS DEATHS ASSISTS KDA, right-aligned to 955
ROW_H = 17.0

# name, class, x, y, w, h, z, extra  — extra: font_size / text / just / italic / brush / flip / auto
BOARD_PLAN = [
    # -- tabs: fixed boxes on the canvas, as in the capture (an HBox was tried and the shared
    #    button reports its 311 design width inside one). 130 wide: our tab type overflows at the
    #    capture's 111.5, so the bar keeps its left edge and grows right by 37. Logged in BN44.
    ('TabRecap',   BTN,  52.5, 31.0, 130.0, 20.0, 2, {"style": "/Script/Breachpoint.BRButtonStyle_NavTab"}),
    ('TabLineup',  BTN, 189.0, 31.0, 130.0, 20.0, 2, {"style": "/Script/Breachpoint.BRButtonStyle_NavTab"}),
    ('TabBoard',   BTN, 325.5, 31.0, 128.0, 20.0, 2, {"style": "/Script/Breachpoint.BRButtonStyle_NavTab"}),
    # -- header, right edge at x923: an HBox for line 1 (mode · map), one text for line 2
    ('HeaderLine1', HBOX, 400.0, 58.0, 523.0, 18.0, 2, {"just_box": True}),
    ('ResultLineText', TEXT, 400.0, 78.0, 523.0, 12.0, 2, {"font_size": 11, "just": "Right"}),
    ('ModeIcon',   IMG, 930.5, 59.5, 14.0, 14.0, 2, {"brush": tex('/Game/BN/UI/Assets/BN_Scoreboard_ModeIcon')}),
    # -- column headers (centred in their cells)
    ('ColKills',   TEXT, CELLS_X[0], 139.0, CELL_W, 14.0, 2, {"font_size": 11, "text": "KILLS", "just": "Center"}),
    ('ColDeaths',  TEXT, CELLS_X[1], 139.0, CELL_W, 14.0, 2, {"font_size": 11, "text": "DEATHS", "just": "Center"}),
    ('ColAssists', TEXT, CELLS_X[2], 139.0, CELL_W, 14.0, 2, {"font_size": 11, "text": "ASSISTS", "just": "Center"}),
    ('ColKda',     TEXT, CELLS_X[3], 139.0, CELL_W, 14.0, 2, {"font_size": 11, "text": "KDA", "just": "Center"}),
    # -- table rules and the row block
    ('ListTopRule',     IMG, TX, TY - 1.0, TW, 1.0, 1, {}),
    ('RowContainer',    None, TX, TY, TW, 294.0, 1, {}),
    ('TableBottomRule', IMG, TX, 455.0, TW, 1.0, 1, {}),
    ('TeamDivider',     IMG, TX, 307.0, TW, 1.0, 2, {}),
    ('ScrollTrack',     IMG, 956.5, TY, 1.0, 294.0, 2, {}),   # the shared bar ignores its height; a 1px track, tinted by C++
    # -- team cards (mine above theirs, relative law)
    ('MyTeamCaret',      IMG,  22.0, 258.0,  8.0, 11.0, 3, {"brush": tex(ART + 'T_BN_CarouselArrow'), "flip": -1}),
    ('MyTeamRankText',   TEXT, 30.0, 246.0, 12.0, 35.0, 3, {"font_size": 12, "italic": True, "valign": True}),
    ('MyTeamFill',       IMG,  48.5, 246.0, 212.5, 35.0, 1, {}),
    ('MyTeamEmblemBox',  IMG,  48.5, 246.0,  39.5, 35.0, 2, {}),
    ('MyTeamEmblem',     IMG,  55.0, 250.5,  26.0, 26.0, 3, {"brush": tex(FIGMA + 'T_BN_Fig_Emblem_01')}),
    ('MyTeamNameText',   TEXT, 92.0, 246.0, 117.0, 35.0, 3, {"font_size": 16, "valign": True}),
    ('MyTeamScoreBlock', IMG, 209.0, 246.0,  52.0, 35.0, 2, {}),
    ('MyTeamScoreText',  TEXT, 209.0, 246.0, 52.0, 35.0, 3, {"font_size": 16, "italic": True, "just": "Center", "valign": True}),
    ('EnemyTeamRankText',   TEXT, 30.0, 291.0, 12.0, 35.0, 3, {"font_size": 12, "italic": True, "valign": True}),
    ('EnemyTeamFill',       IMG,  48.5, 291.0, 212.5, 35.0, 1, {}),
    ('EnemyTeamEmblemBox',  IMG,  48.5, 291.0,  39.5, 35.0, 2, {}),
    ('EnemyTeamEmblem',     IMG,  55.0, 295.5,  26.0, 26.0, 3, {"brush": tex(FIGMA + 'T_BN_Fig_Emblem_02')}),
    ('EnemyTeamNameText',   TEXT, 92.0, 291.0, 117.0, 35.0, 3, {"font_size": 16, "valign": True}),
    ('EnemyTeamScoreBlock', IMG, 209.0, 291.0,  52.0, 35.0, 2, {}),
    ('EnemyTeamScoreText',  TEXT, 209.0, 291.0, 52.0, 35.0, 3, {"font_size": 16, "italic": True, "just": "Center", "valign": True}),
    # -- page dots (4th active) with arrows, centred on the table
    ('DotsArrowL', IMG, 596.0, 468.0, 6.0, 6.0, 2, {"brush": tex(ART + 'T_BN_CarouselArrow')}),
    ('DotsArrowR', IMG, 684.0, 468.0, 6.0, 6.0, 2, {"brush": tex(ART + 'T_BN_CarouselArrow'), "flip": -1}),
] + [
    ('Dot%d' % i, IMG, 608.0 + 12.0 * i, 468.0, 6.0, 6.0, 2,
     {"brush": tex(ART + ('T_CarouselDot_Active' if i == 3 else 'T_CarouselDot_Inactive'))})
    for i in range(6)
] + [
    # -- bottom band + legend + profile card
    ('BottomBand',        IMG, 0.0, 521.5, 1280.0, 198.5, 0, {}),
    ('ClosePrompt',       PROMPT,  55.0, 535.0, 0.0, 0.0, 3, {"auto": True}),
    ('ViewPrompt',        PROMPT, 134.0, 535.0, 0.0, 0.0, 3, {"auto": True}),
    ('MatchmakingPrompt', PROMPT, 191.0, 535.0, 0.0, 0.0, 3, {"auto": True}),
    ('ReportPrompt',      PROMPT, 342.0, 535.0, 0.0, 0.0, 3, {"auto": True}),   # +16: our type is wider than the capture's
    ('CyclePrompt',       PROMPT, 462.0, 535.0, 0.0, 0.0, 3, {"auto": True}),
    ('ProfileBar',        PROFILE, 0.0, 670.0, 1280.0, 50.0, 3, {}),
]

# Leaves the Figma pass placed that the capture has no equivalent for.
RETIRE = ['ScrollBar', 'HeaderTick', 'ColScore', 'ColumnTintA', 'ColumnTintB', 'HeaderRule', 'HeaderRuleStrong',
          'MyTeamAccent', 'EnemyTeamAccent', 'HeaderSep', 'BannerText', 'OutcomeText', 'OutcomeAccent']

# Row-local, 615 x 17.
ROW_PLAN = [
    ('RowFill',   IMG, 0.0, 0.0, TW, ROW_H, 0, {}),
    ('SelfBar',   IMG, -2.5, 0.0, 2.0, ROW_H, 2, {}),
    ('SelfCaret', IMG, -12.0, 3.0, 8.0, 11.0, 2, {"brush": tex(ART + 'T_BN_CarouselArrow'), "flip": -1}),
    ('Emblem',    IMG, 7.5, 0.5, 16.0, 16.0, 2, {}),
    ('NameRow',   HBOX, 23.0, 0.0, 300.0, ROW_H, 2, {}),
] + [
    ('CellTint%d' % i, IMG, CELLS_X[i] - TX, 0.0, CELL_W, ROW_H, 1, {}) for i in range(4)
] + [
    ('KillsText',   TEXT, CELLS_X[0] - TX, 1.0, CELL_W, 15.0, 2, {"font_size": 11, "italic": True, "just": "Center"}),
    ('DeathsText',  TEXT, CELLS_X[1] - TX, 1.0, CELL_W, 15.0, 2, {"font_size": 11, "italic": True, "just": "Center"}),
    ('AssistsText', TEXT, CELLS_X[2] - TX, 1.0, CELL_W, 15.0, 2, {"font_size": 11, "italic": True, "just": "Center"}),
    ('KdaText',     TEXT, CELLS_X[3] - TX, 1.0, CELL_W, 15.0, 2, {"font_size": 11, "italic": True, "just": "Center"}),
]
ROW_RETIRE = ['HighlightFill', 'HighlightAccent', 'ScoreText']


def _overlaps(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah


def selftest():
    b = {n: (x, y, w, h) for n, _, x, y, w, h, _, _ in BOARD_PLAN}
    assert b['TabRecap'][0] < b['TabLineup'][0] < b['TabBoard'][0] and b['TabRecap'][1] == b['TabBoard'][1]
    assert b['ResultLineText'][0] + b['ResultLineText'][2] == 923.0, 'header right edge'
    for n in ('ColKills', 'ColDeaths', 'ColAssists', 'ColKda'):
        assert b[n][1] == 139.0 and b[n][2] == CELL_W
    assert CELLS_X[3] + CELL_W == 955.0, 'last cell ends at the table edge (+7.5 scrollbar gutter)'
    assert b['RowContainer'][0] == b['ListTopRule'][0] == b['TableBottomRule'][0] == TX
    for leaf in ('Fill', 'EmblemBox', 'Emblem', 'NameText', 'ScoreBlock', 'ScoreText', 'RankText'):
        mine, theirs = b['MyTeam' + leaf], b['EnemyTeam' + leaf]
        assert mine[1] < theirs[1] and mine[0] == theirs[0], leaf
    assert b['BottomBand'][1] < b['ClosePrompt'][1] < b['ProfileBar'][1]
    r = {n: (x, y, w, h) for n, _, x, y, w, h, _, _ in ROW_PLAN}
    assert r['RowFill'] == (0.0, 0.0, TW, ROW_H)
    assert r['SelfBar'][0] < 0 and r['SelfCaret'][0] < r['SelfBar'][0]
    for i in range(4):
        assert r['CellTint%d' % i][0] == [r['KillsText'], r['DeathsText'], r['AssistsText'], r['KdaText']][i][0]
    print('selftest OK: %d board leaves, %d row leaves' % (len(BOARD_PLAN), len(ROW_PLAN)))


def place(wbp, canvas, plan):
    ws, _ = B.widgets(wbp)
    for display, cls, x, y, w, h, z, extra in plan:
        print('== %s' % display)
        if cls is None:                      # an existing leaf we only move
            if display not in ws:
                print('  missing, skipped'); continue
            info = ws[display]
        else:
            info = B.ensure(wbp, display, cls, canvas, -1)
            not_variable(wbp, info['widget'])
        widget, slot = info['widget'], info['slot']
        wprops = {}
        if 'font_size' in extra or extra.get('italic'):
            f = B.font_sized(widget, extra.get('font_size', 12))
            if extra.get('italic'):
                f['typefaceFontName'] = 'Italic'
            wprops['font'] = f
        if 'text' in extra:
            wprops['text'] = extra['text']
        if 'just' in extra:
            wprops['justification'] = extra['just']
        if 'style' in extra:
            wprops['style'] = {"refPath": extra['style']}
        if 'brush' in extra:
            wprops['brush'] = {"drawAs": "Image", "resourceObject": extra['brush'], "imageSize": {"x": w, "y": h}}
        if extra.get('flip'):
            wprops['renderTransform'] = {"scale": {"x": extra['flip'], "y": 1.0}}
        if cls == IMG:
            wprops['visibility'] = 'HitTestInvisible'
        if wprops:
            print('  set w:', B.setp(widget, wprops))
        sprops = {"layoutData": topleft(x, y, w, h), "bAutoSize": bool(extra.get('auto')), "zOrder": z}
        print('  set s:', B.setp(slot, sprops))


def retire(wbp, names):
    ws, _ = B.widgets(wbp)
    for n in names:
        if n in ws and ws[n].get('parent') not in (None, 'None'):
            print('  remove:', n, B.mcp.call(B.UMG, 'RemoveWidget', widgetBlueprint=B.wbp(wbp), widget=ws[n]['widget']))


def header_line(canvas):
    """HeaderLine1: an HBox holding ModeText · MapText, right-justified inside its 523px box."""
    ws, _ = B.widgets(BOARD)
    box = ws['HeaderLine1']
    boxref = box['widget']['refPath']
    # Right-justify the HBox's content by padding a spacer first? Simpler: the two texts are
    # right-aligned by making the HBox's own slot fixed and its children fill from the right:
    # a filler Spacer with Fill size, then the texts.
    spacer = B.ensure(BOARD, 'HeaderSpacer', '/Script/UMG.Spacer', boxref, -1)
    B.setp(spacer['slot'], {"size": {"value": 1.0, "sizeRule": "Fill"}})
    for name, text, size in (('ModeText', None, 16), ('HeaderSep', ' · ', 16), ('MapText', None, 16)):
        info = ws.get(name)
        if info is None or info.get('parent') in (None, 'None') or 'HeaderLine1' not in str(info.get('parent')):
            if info is not None and info.get('parent') not in (None, 'None'):
                B.mcp.call(B.UMG, 'MoveWidget', widgetBlueprint=B.wbp(BOARD), widget=info['widget'], newParent={"refPath": boxref}, childIndex=-1)
                ws, _ = B.widgets(BOARD); info = ws[name]
            else:
                info = B.ensure(BOARD, name, TEXT, boxref, -1)
        not_variable(BOARD, info['widget'])
        props = {"font": B.font_sized(info['widget'], size)}
        if text is not None:
            props['text'] = text
        B.setp(info['widget'], props)
        B.setp(info['slot'], {"verticalAlignment": "VAlign_Center", "size": {"value": 1.0, "sizeRule": "Automatic"}})


def name_row():
    """NameRow: NameText then a dim TagText, laid end to end like the capture's `Name [TAG]`."""
    ws, _ = B.widgets(ROW)
    box = ws['NameRow']['widget']['refPath']
    for name, size, pad in (('NameText', 11, 0.0), ('TagText', 10, 6.0)):
        info = ws.get(name)
        if info is not None and 'NameRow' not in str(info.get('parent')):
            B.mcp.call(B.UMG, 'MoveWidget', widgetBlueprint=B.wbp(ROW), widget=info['widget'], newParent={"refPath": box}, childIndex=-1)
            ws, _ = B.widgets(ROW); info = ws[name]
        elif info is None:
            info = B.ensure(ROW, name, TEXT, box, -1)
        not_variable(ROW, info['widget'])
        B.setp(info['widget'], {"font": B.font_sized(info['widget'], size)})
        B.setp(info['slot'], {"verticalAlignment": "VAlign_Center", "padding": {"left": pad, "top": 0, "right": 0, "bottom": 0},
                              "size": {"value": 1.0, "sizeRule": "Automatic"}})


def row_canvas():
    ws, _ = B.widgets(ROW)
    for name, w in ws.items():
        if w['widgetClassPath']['refPath'].endswith('.CanvasPanel'):
            return '/Game/BN/UI/%s.%s:WidgetTree.%s' % (ROW, ROW, name), ws
    raise RuntimeError('WBP_BNScoreRow has no CanvasPanel')


def main():
    retire(BOARD, RETIRE)
    place(BOARD, CANVAS, BOARD_PLAN)
    header_line(CANVAS)
    print(B.compile_and_save(BOARD))
    canvas, ws = row_canvas()
    retire(ROW, ROW_RETIRE)
    place(ROW, canvas, ROW_PLAN)
    name_row()
    # the row is 17 tall now
    for name, w in ws.items():
        if w['widgetClassPath']['refPath'].endswith('.SizeBox'):
            B.setp(w['widget'], {"bOverride_HeightOverride": True, "heightOverride": ROW_H})
    print(B.compile_and_save(ROW))


if __name__ == '__main__':
    selftest()
    if '--selftest' not in sys.argv:
        main()
