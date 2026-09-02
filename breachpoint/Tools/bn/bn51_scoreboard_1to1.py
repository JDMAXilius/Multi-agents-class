#!/usr/bin/env python3
"""BN44 — WBP_BNScreen_Scoreboard + WBP_BNScoreRow laid out 1:1 to the measured frame `43:2`.

    python3 Tools/bn/bn51_scoreboard_1to1.py --selftest   # geometry only, no editor
    python3 Tools/bn/bn51_scoreboard_1to1.py              # needs the live editor (MCP)

EVERY NUMBER BELOW IS COPIED FROM `00-HUD-MEASURED.md` §`43:2` (1280x720 canvas, top-left
offsets on `BoardCanvas` — the idiom the board already uses). Nothing here is a judgement call
except the two things the section leaves implicit and which the selftest pins: the team cards'
NAME field spans the nameplate rect (x145 142x44) and the row's value columns are the header
columns shifted by the table's left edge (465): 816.5-465 = 351.5 ≈ the measured 356.5 row-local
SCORE x, so the ROW-LOCAL numbers from the row line (`43:40`) are used, not a derivation.

NO COLOUR IS STORED — ASSET-RULES §5. Every leaf that carries a tint is an Optional bind that
`UBNScreen_Scoreboard::RefreshHeader` / `UBNScoreRow::SetRow` colour at runtime (HeaderTick,
ColumnTintA/B, TeamDivider, My/EnemyTeamAccent, HighlightFill/Accent). Text sizes are set;
text colours are not.

TEAM FILLS `43:38/43:102` (694x88, one per team block) are NOT placed: they are sized to four
rows, and the row block is laid out by C++ per player count. Filed in the ticket as a follow-up
(C++-sized fills), not faked at a fixed height.

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
ASSETS = '/Game/BN/UI/Assets/'
FIGMA = '/Game/BN/UI/Figma/'


def topleft(x, y, w, h):
    return {"offsets": {"left": x, "top": y, "right": w, "bottom": h},
            "anchors": {"minimum": {"x": 0.0, "y": 0.0}, "maximum": {"x": 0.0, "y": 0.0}},
            "alignment": {"x": 0.0, "y": 0.0}}


# name, class, x, y, w, h, z, extra widget props
BOARD_PLAN = [
    # -- header `43:4..14`
    ('HeaderTick',        IMG,    5.0,   18.0,    3.0, 52.0, 1, {}),
    ('ModeIcon',          IMG,   33.0,   23.0,   44.0, 44.0, 1, {"brush": {"resourceObject": {"refPath": ASSETS + 'BN_Scoreboard_ModeIcon.BN_Scoreboard_ModeIcon'}, "imageSize": {"x": 44, "y": 44}}}),
    ('ModeText',          TEXT, 100.0,   34.0,  258.0, 33.0, 1, {"font_size": 26}),
    ('HeaderSep',         TEXT, 358.0,   34.0,   27.0, 33.0, 1, {"font_size": 26, "text": "·"}),
    ('MapText',           TEXT, 385.0,   34.0,  400.0, 33.0, 1, {"font_size": 26}),
    ('ResultLineText',    TEXT, 100.0,   73.0,  700.0, 17.0, 1, {"font_size": 13}),
    # -- column headers `43:29..32` (KILLS / DEATHS exist as ColKills / ColDeaths at these x)
    ('ColScore',          TEXT, 816.5,  157.0,  100.0, 15.0, 1, {"font_size": 12, "text": "SCORE", "justification": "Center"}),
    ('ColAssists',        TEXT, 983.0,  157.0,  100.0, 15.0, 1, {"font_size": 12, "text": "ASSISTS", "justification": "Center"}),
    # -- column tints `43:36/37` (behind the rows)
    ('ColumnTintA',       IMG,  825.0,  191.0,   83.0, 282.0, 0, {}),
    ('ColumnTintB',       IMG,  992.0,  191.0,   83.0, 282.0, 0, {}),
    # -- team cards `43:15..28`
    ('MyTeamAccent',      IMG,   96.0,  268.0,    4.0, 44.0, 1, {}),
    ('MyTeamEmblem',      IMG,  100.0,  268.0,   44.0, 44.0, 1, {"brush": {"resourceObject": {"refPath": FIGMA + 'T_BN_Fig_Emblem_01.T_BN_Fig_Emblem_01'}, "imageSize": {"x": 44, "y": 44}}}),
    ('MyTeamNameText',    TEXT, 145.0,  268.0,  142.0, 44.0, 1, {"font_size": 20}),
    ('MyTeamScoreText',   TEXT, 287.0,  268.0,   67.0, 44.0, 1, {"font_size": 26, "justification": "Center"}),
    ('EnemyTeamAccent',   IMG,   96.0,  335.0,    4.0, 44.0, 1, {}),
    ('EnemyTeamEmblem',   IMG,  100.0,  335.0,   44.0, 44.0, 1, {"brush": {"resourceObject": {"refPath": FIGMA + 'T_BN_Fig_Emblem_02.T_BN_Fig_Emblem_02'}, "imageSize": {"x": 44, "y": 44}}}),
    ('EnemyTeamNameText', TEXT, 145.0,  335.0,  142.0, 44.0, 1, {"font_size": 20}),
    ('EnemyTeamScoreText', TEXT, 287.0, 335.0,   67.0, 44.0, 1, {"font_size": 26, "justification": "Center"}),
    # -- team divider `43:101`
    ('TeamDivider',       IMG,  465.0,  367.0,  693.0,  1.0, 1, {}),
]

# Row-local (`43:40`, `43:39/57`): the row is 694x22.
ROW_PLAN = [
    ('HighlightFill',   IMG,    0.0, 0.0, 694.0, 22.0, 0, {}),
    ('HighlightAccent', IMG,   -5.0, 0.0,   4.0, 22.0, 1, {}),
    ('TagText',         TEXT,  97.0, 5.0,  53.0, 12.0, 1, {"font_size": 11}),
    ('ScoreText',       TEXT, 356.5, 4.0,  90.0, 14.0, 1, {"font_size": 12, "justification": "Center"}),
    ('AssistsText',     TEXT, 523.0, 4.0,  90.0, 14.0, 1, {"font_size": 12, "justification": "Center"}),
]

ROW_W, ROW_H = 694.0, 22.0


def _overlaps(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah


def selftest():
    """The measured invariants a careless edit would break silently."""
    b = {n: (x, y, w, h) for n, _, x, y, w, h, _, _ in BOARD_PLAN}
    # header: mode · sep · map sit on one baseline and do not collide
    assert b['ModeText'][1] == b['HeaderSep'][1] == b['MapText'][1] == 34.0
    assert not _overlaps(b['ModeText'], b['MapText'])
    # columns: SCORE < KILLS(900) < ASSISTS < DEATHS(1066.5), w100 each, all at y157
    assert b['ColScore'][0] < 900.0 < b['ColAssists'][0] < 1066.5
    assert b['ColScore'][1] == b['ColAssists'][1] == 157.0
    # team cards: mine ABOVE theirs (relative law), same x for every leaf
    for leaf in ('Accent', 'Emblem', 'NameText', 'ScoreText'):
        mine, theirs = b['MyTeam' + leaf], b['EnemyTeam' + leaf]
        assert mine[1] < theirs[1] and mine[0] == theirs[0], leaf
    # divider between the two team blocks of the table
    assert 191.0 < b['TeamDivider'][1] < 472.0
    # row: the accent stands proud of the left edge, fill spans the row exactly
    r = {n: (x, y, w, h) for n, _, x, y, w, h, _, _ in ROW_PLAN}
    assert r['HighlightFill'] == (0.0, 0.0, ROW_W, ROW_H)
    assert r['HighlightAccent'][0] < 0.0
    assert r['TagText'][0] < r['ScoreText'][0] < r['AssistsText'][0]
    print('selftest OK: %d board leaves, %d row leaves' % (len(BOARD_PLAN), len(ROW_PLAN)))


def place(wbp, canvas, plan):
    for display, cls, x, y, w, h, z, extra in plan:
        print('== %s' % display)
        info = B.ensure(wbp, display, cls, canvas, -1)
        widget, slot = info['widget'], info['slot']
        wprops = {}
        if 'font_size' in extra:
            wprops['font'] = B.font_sized(widget, extra['font_size'])
        if 'text' in extra:
            wprops['text'] = extra['text']
        if 'justification' in extra:
            wprops['justification'] = extra['justification']
        if 'brush' in extra:
            wprops['brush'] = dict(extra['brush'], drawAs='Image')
        if cls == IMG:
            wprops['visibility'] = 'HitTestInvisible'
        if wprops:
            print('  set w:', B.setp(widget, wprops))
        print('  set s:', B.setp(slot, {"layoutData": topleft(x, y, w, h), "bAutoSize": False, "zOrder": z}))


def row_canvas():
    ws, _ = B.widgets(ROW)
    for name, w in ws.items():
        if w['widgetClassPath']['refPath'].endswith('.CanvasPanel'):
            return '/Game/BN/UI/%s.%s:WidgetTree.%s' % (ROW, ROW, name)
    raise RuntimeError('WBP_BNScoreRow has no CanvasPanel to place leaves on')


def main():
    place(BOARD, CANVAS, BOARD_PLAN)
    print(B.compile_and_save(BOARD))
    place(ROW, row_canvas(), ROW_PLAN)
    print(B.compile_and_save(ROW))


if __name__ == '__main__':
    selftest()
    if '--selftest' not in sys.argv:
        main()
