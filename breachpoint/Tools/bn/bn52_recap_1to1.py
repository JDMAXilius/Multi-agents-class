#!/usr/bin/env python3
"""BN45 — WBP_BNScreen_PostMatch: the PLAYER RECAP page, 1:1 to the founder's capture of the
shipped post-game recap (1920x1080, scale 1.5 -> 1280x720 design), plus a VICTORY / DEFEAT line
the founder asked for ("add the win lose somewhere") — top-right, above the header block.

    python3 Tools/bn/bn52_recap_1to1.py --selftest   # geometry only, no editor
    python3 Tools/bn/bn52_recap_1to1.py              # needs the live editor (MCP)

Every number is a measurement off that capture (TICKET_BN45). Values the game does not track
print as dashes from C++ (assists, medals); this script places the structure only.
NO COLOUR IS STORED; every tinted leaf is resolved by name and coloured in UBNScreen_PostMatch.
Leaves a C++ BindWidget owns are not Blueprint variables.
"""
import sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import bn11_lib as B

WBP = 'WBP_BNScreen_PostMatch'
CANVAS = '/Game/BN/UI/%s.%s:WidgetTree.RecapCanvas' % (WBP, WBP)
IMG = '/Script/UMG.Image'
TEXT = '/Script/CommonUI.CommonTextBlock'
HBOX = '/Script/UMG.HorizontalBox'
BAR = '/Script/UMG.ProgressBar'
BTN = '/Game/UI/Components/Buttons/WBP_ButtonDefault.WBP_ButtonDefault_C'
PROMPT = '/Game/BN/UI/WBP_BNPromptButton.WBP_BNPromptButton_C'
PROFILE = '/Game/BN/UI/WBP_BNProfileBar.WBP_BNProfileBar_C'
ART = '/Game/BN/UI/Art/'
FIGMA = '/Game/BN/UI/Figma/'
NAVTAB = '/Script/Breachpoint.BRButtonStyle_NavTab'


def tex(path):
    return {"refPath": path + '.' + path.rsplit('/', 1)[1]}


def topleft(x, y, w, h):
    return {"offsets": {"left": x, "top": y, "right": w, "bottom": h},
            "anchors": {"minimum": {"x": 0.0, "y": 0.0}, "maximum": {"x": 0.0, "y": 0.0}},
            "alignment": {"x": 0.0, "y": 0.0}}


STRIP_X, STRIP_Y, STRIP_W, STRIP_H = 66.7, 213.3, 846.7, 170.0
COL_W = STRIP_W / 6.0
MEDAL_Y, MEDAL_H = 394.7, 92.0
MEDAL_W = STRIP_W / 8.0

PLAN = [
    # -- tabs with LB / RB pills
    ('NavPillLeft',  IMG,  38.0, 45.0, 30.0, 16.0, 1, {"rounded": True}),
    ('NavPromptLeft', TEXT, 38.0, 46.0, 30.0, 14.0, 2, {"font_size": 9, "text": "LB", "just": "Center"}),
    ('TabRecap',     BTN,  66.7, 38.0, 144.7, 28.7, 2, {"style": NAVTAB}),
    ('TabLineup',    BTN, 226.7, 38.0, 132.0, 28.7, 2, {"style": NAVTAB}),
    ('TabBoard',     BTN, 370.0, 38.0, 138.7, 28.7, 2, {"style": NAVTAB}),
    ('NavPillRight', IMG, 525.0, 45.0, 30.0, 16.0, 1, {"rounded": True}),
    ('NavPromptRight', TEXT, 525.0, 46.0, 30.0, 14.0, 2, {"font_size": 9, "text": "RB", "just": "Center"}),
    # -- the win/lose line + the three header lines, right-aligned to x1214.7
    ('OutcomeText',  TEXT, 700.0, 14.0, 514.7, 22.0, 2, {"font_size": 20, "just": "Right"}),
    ('HeaderLine1',  TEXT, 700.0, 38.7, 514.7, 12.0, 2, {"font_size": 11, "just": "Right"}),
    ('HeaderLine2',  TEXT, 700.0, 56.7, 514.7, 12.0, 2, {"font_size": 11, "just": "Right"}),
    ('HeaderLine3',  TEXT, 700.0, 73.0, 514.7, 12.0, 2, {"font_size": 11, "just": "Right"}),
    # -- SCORE
    ('ScoreLabel',   TEXT, 66.7, 105.3, 120.0, 12.0, 2, {"font_size": 11, "text": "SCORE"}),
    ('ScoreRule',    IMG,  53.3, 122.0, 296.7, 1.0, 1, {}),
    ('ScoreValueText', TEXT, 66.7, 135.0, 300.0, 55.0, 2, {"font_size": 46}),
    # -- RANK
    ('RankLabel',    TEXT, 660.0, 105.3, 120.0, 12.0, 2, {"font_size": 11, "text": "RANK"}),
    ('RankRule',     IMG,  640.0, 122.0, 290.0, 1.0, 1, {}),
    ('RankIcon',     IMG,  640.0, 130.0, 73.0, 67.0, 2, {"brush": tex(FIGMA + 'T_BN_Fig_RankCrest_01')}),
    ('RankLineText', TEXT, 763.3, 140.0, 240.0, 22.0, 2, {"font_size": 18}),
    ('RankProgress', BAR,  730.0, 176.7, 183.3, 6.7, 2, {}),
    # -- stats strip: plate, six columns (label + value), the highlighted column plate + caret + rule
    ('StatsPlate',   IMG, STRIP_X, STRIP_Y, STRIP_W, STRIP_H, 0, {}),
    ('StatsHighlight', IMG, STRIP_X + 3 * COL_W, STRIP_Y, COL_W, STRIP_H, 1, {}),
    ('StatsCaret',   IMG, STRIP_X + 3.5 * COL_W - 5.0, 288.0, 10.0, 6.0, 2, {"brush": tex(ART + 'T_BN_CarouselArrow'), "rot": 90}),
    ('StatsHighlightRule', IMG, STRIP_X + 3 * COL_W, STRIP_Y + STRIP_H - 1.0, COL_W, 1.0, 2, {}),
] + [
    ('StatLabel%d' % i, TEXT, STRIP_X + i * COL_W, 235.0, COL_W, 14.0, 2, {"font_size": 11, "just": "Center"}) for i in range(6)
] + [
    ('StatValue%d' % i, TEXT, STRIP_X + i * COL_W, 305.0, COL_W, 40.0, 2, {"font_size": 30, "just": "Center"}) for i in range(6)
] + [
    # -- medals: eight cells on one plate, the first framed
    ('MedalsPlate',  IMG, STRIP_X, MEDAL_Y, STRIP_W, MEDAL_H, 0, {}),
    ('MedalFrame',   IMG, STRIP_X, MEDAL_Y, MEDAL_W, MEDAL_H, 1, {"box": True}),
] + [
    ('Medal%d' % i, IMG, STRIP_X + i * MEDAL_W + (MEDAL_W - 46.7) / 2, MEDAL_Y + 15.0, 46.7, 46.7, 2,
     {"brush": tex('/Game/BN/UI/Assets/BN_Feedback_MedalChip')}) for i in range(8)
] + [
    ('MedalCount%d' % i, TEXT, STRIP_X + i * MEDAL_W, MEDAL_Y + 72.0, MEDAL_W, 14.0, 2, {"font_size": 10, "just": "Center"}) for i in range(8)
] + [
    ('MedalNameText', TEXT, 66.7, 528.0, 500.0, 22.0, 2, {"font_size": 18}),
    ('MedalDescText', TEXT, 66.7, 556.0, 500.0, 14.0, 2, {"font_size": 11, "italic": True}),
    # -- nameplate bottom-right
    ('NameplateArt',  IMG, 960.0, 523.3, 250.0, 43.3, 1, {"brush": tex(FIGMA + 'T_BN_Fig_Nameplate_01')}),
    ('NameplateRank', TEXT, 985.0, 528.0, 30.0, 34.0, 2, {"font_size": 28, "italic": True}),
    ('NameplateName', TEXT, 1025.0, 528.0, 180.0, 16.0, 2, {"font_size": 12}),
    ('NameplateSub',  TEXT, 1025.0, 545.0, 180.0, 14.0, 2, {"font_size": 10, "italic": True}),
    # -- bottom band + legend + profile card
    ('BottomBand',   IMG, 0.0, 656.7, 1280.0, 63.3, 0, {}),
    ('ClosePrompt',  PROMPT, 36.7, 683.0, 0.0, 0.0, 3, {"auto": True}),
    ('MatchmakingPrompt', PROMPT, 113.3, 683.0, 0.0, 0.0, 3, {"auto": True}),
    ('ProfilePrompt', PROMPT, 286.7, 683.0, 0.0, 0.0, 3, {"auto": True}),
    ('ProfileBar',   PROFILE, 0.0, 670.0, 1280.0, 50.0, 3, {}),
]


def selftest():
    p = {n: (x, y, w, h) for n, _, x, y, w, h, _, _ in PLAN}
    assert abs(p['OutcomeText'][0] + p['OutcomeText'][2] - 1214.7) < 0.01
    assert p['HeaderLine1'][1] < p['HeaderLine2'][1] < p['HeaderLine3'][1] < p['ScoreLabel'][1]
    assert abs(6 * COL_W - STRIP_W) < 0.01 and abs(8 * MEDAL_W - STRIP_W) < 0.01
    for i in range(6):
        assert p['StatLabel%d' % i][0] == p['StatValue%d' % i][0]
    assert p['StatsHighlight'][0] == p['StatLabel3'][0], 'the highlighted column is the fourth (KILLS in the capture)'
    assert p['MedalFrame'][:2] == p['MedalsPlate'][:2]
    assert p['BottomBand'][1] < p['ClosePrompt'][1] and p['ProfileBar'][1] > p['BottomBand'][1]
    print('selftest OK: %d leaves' % len(PLAN))


def not_variable(widget):
    try:
        B.mcp.call(B.UMG, 'ToggleWidgetAsVariable', widgetBlueprint=B.wbp(WBP), widget=widget, bIsVariable=False)
    except RuntimeError as e:
        if 'null' not in str(e) and 'None' not in str(e):
            raise


def main():
    for display, cls, x, y, w, h, z, extra in PLAN:
        print('== %s' % display)
        info = B.ensure(WBP, display, cls, CANVAS, -1)
        not_variable(info['widget'])
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
        if extra.get('rounded'):
            wprops['brush'] = {"drawAs": "RoundedBox", "outlineSettings": {"cornerRadii": {"x": 8, "y": 8, "z": 8, "w": 8}, "roundingType": "FixedRadius", "width": 0}}
        if extra.get('box'):
            wprops['brush'] = {"drawAs": "Border", "margin": {"left": 2, "top": 2, "right": 2, "bottom": 2}}
        if extra.get('rot'):
            wprops['renderTransform'] = {"angle": extra['rot']}
        if cls in (IMG, BAR):
            wprops['visibility'] = 'HitTestInvisible'
        if wprops:
            print('  set w:', B.setp(widget, wprops))
        print('  set s:', B.setp(slot, {"layoutData": topleft(x, y, w, h), "bAutoSize": bool(extra.get('auto')), "zOrder": z}))
    print(B.compile_and_save(WBP))


if __name__ == '__main__':
    selftest()
    if '--selftest' not in sys.argv:
        main()
