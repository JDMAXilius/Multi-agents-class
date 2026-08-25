"""BN11 step 1 — WBP_BNKillfeedEntry: the feed's three parts.

Referee: 00-HUD-MEASURED.md, Event Feed `30:21` — rows 340x20, Killer x8 y2 h14,
Weapon Glyph x78 y6 22x8, Victim x110 y2 h14.

The row is a HorizontalBox (not a canvas), so the measured x are reached with slot
padding + minDesiredWidth instead of absolute offsets:
  KillerText pad L8, minW 64  -> 8..72   then WeaponIcon pad L6 -> glyph at x78 (22 wide)
  WeaponIcon ends 100, pad R6 -> 106     then VictimText pad L4 -> x110
Child order LineText(0), KillerText(1), WeaponIcon(2), VictimText(3): in the fallback
layout Killer/Victim are Collapsed (zero-size in an SBoxPanel) so the row still draws
exactly [LineText][6][glyph] as it does today.

Colour: NONE set. C++ tints the whole row (SetColorAndOpacity). Idempotent.
"""
import json
import bn11_lib as B

WBP = 'WBP_BNKillfeedEntry'
TREE = '/Game/BN/UI/%s.%s:WidgetTree' % (WBP, WBP)
BOX = TREE + '.EntryBox'
TEXT = '/Script/CommonUI.CommonTextBlock'

PLAN = [
    # display, childIndex, widget props, slot props
    ('KillerText', 1,
     {"text": "", "font": 14, "minDesiredWidth": 64, "visibility": "Collapsed",
      "justification": "Left"},
     {"padding": {"left": 8, "top": 0, "right": 0, "bottom": 0},
      "verticalAlignment": "VAlign_Center"}),
    ('VictimText', -1,
     {"text": "", "font": 14, "minDesiredWidth": 0, "visibility": "Collapsed",
      "justification": "Left"},
     {"padding": {"left": 4, "top": 0, "right": 0, "bottom": 0},
      "verticalAlignment": "VAlign_Center"}),
]

for display, index, wprops, sprops in PLAN:
    print('== %s' % display)
    info = B.ensure(WBP, display, TEXT, BOX, index)
    w, s = info['widget'], info['slot']
    B.props(w); B.props(s)                       # discovery call, per the toolset workflow
    wprops = dict(wprops, font=B.font_sized(w, wprops['font']))
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
