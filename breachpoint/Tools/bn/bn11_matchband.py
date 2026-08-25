"""BN11 step 3 — WBP_BNMatchBand: the two score bars.

Referee: 00-HUD-MEASURED.md, Match State `42:6` x474.67 y622 302x22 —
  bar self  `42:8`  x24  y7 60x8
  bar them  `42:14` x240 y7 44x8
The band's canvas IS 302x22 (RootSizeBox), so these are top-left-anchored absolute
offsets, exactly the idiom MyKillsText/ClockText/ScoreLimitText already use.

Both default to Hidden: BNMatchBand.cpp hides them until MatchDataState == Live, and an
empty bar is the "score is zero" lie the band's dashes exist to avoid. No colour is set.

Idempotent.
"""
import json
import bn11_lib as B

WBP = 'WBP_BNMatchBand'
CANVAS = '/Game/BN/UI/%s.%s:WidgetTree.BandCanvas' % (WBP, WBP)
BAR = '/Script/UMG.ProgressBar'


def topleft(x, y, w, h):
    return {"offsets": {"left": x, "top": y, "right": w, "bottom": h},
            "anchors": {"minimum": {"x": 0.0, "y": 0.0}, "maximum": {"x": 0.0, "y": 0.0}},
            "alignment": {"x": 0.0, "y": 0.0}}


PLAN = [
    ('SelfScoreBar', topleft(24.0, 7.0, 60.0, 8.0)),
    ('TopScoreBar', topleft(240.0, 7.0, 44.0, 8.0)),
]

for display, layout in PLAN:
    print('== %s' % display)
    info = B.ensure(WBP, display, BAR, CANVAS, -1)
    w, s = info['widget'], info['slot']
    print('  widget props:', sorted(json.loads(B.props(w)).keys()))
    B.props(s)
    wprops = {"visibility": "Hidden", "percent": 0.0}
    sprops = {"layoutData": layout, "bAutoSize": False, "zOrder": 0}
    print('  before w:', B.get(w, list(wprops)))
    print('  before s:', B.get(s, list(sprops)))
    print('  set    w:', B.setp(w, wprops))
    print('  set    s:', B.setp(s, sprops))
    print('  after  w:', B.get(w, list(wprops)))
    print('  after  s:', B.get(s, list(sprops)))

print('== compile + save')
print(B.compile_and_save(WBP))
print('== readback')
print(B.widgets(WBP)[1])
