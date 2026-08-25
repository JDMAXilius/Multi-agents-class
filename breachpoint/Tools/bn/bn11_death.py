"""BN11 step 2 — WBP_BNScreen_Death: weapon silhouette, big countdown, respawn bar.

Referee: 00-HUD-MEASURED.md, HUD/Death frame `36:2` (canvas 1280x720):
  Killing Weapon `36:8` x540 y340 200x34 -> silhouette `36:9` x0 y8 84x20
                                         -> name       `36:10` x96 y8 118x19  (abs x636)
  Respawn ring   `36:12/11` x588 y410 104x104
  Countdown      `36:13`    x588 y432 104x71

Every element uses the asset's existing centre-anchor idiom (anchors 0.5,0 / alignment
0.5,0), so Offsets.Left is (designX + width/2) - 640:
  WeaponIcon   540 -> -58   RespawnRing 588 ->  0   CountdownText 588 -> 0

The ring ships as a UProgressBar (ticket's note: the radial sweep is a Tier-4 material and
the VM steps once a second). It is laid out as the ring box's TOP EDGE, 104 x 8 at y410 —
inside the ring's 104x104 footprint and clear of the numeral at y432, so the material can
later reclaim the full box under the same bind name with no layout churn.

No colour is set anywhere: BNScreen_Death.cpp owns KilledByText/RespawnText tints and the
ProgressBar keeps its engine default until C++ claims it.

Idempotent.
"""
import json
import bn11_lib as B

WBP = 'WBP_BNScreen_Death'
TREE = '/Game/BN/UI/%s.%s:WidgetTree' % (WBP, WBP)
CANVAS = TREE + '.DeathCanvas'
TEXT = '/Script/CommonUI.CommonTextBlock'


def anchor(left, top, w, h):
    """Centre-anchored canvas layout, the idiom WeaponText already uses in this asset."""
    return {"offsets": {"left": left, "top": top, "right": w, "bottom": h},
            "anchors": {"minimum": {"x": 0.5, "y": 0.0}, "maximum": {"x": 0.5, "y": 0.0}},
            "alignment": {"x": 0.5, "y": 0.0}}


PLAN = [
    ('WeaponIcon', '/Script/UMG.Image',
     {"visibility": "Collapsed"},                       # brush handled below
     {"layoutData": anchor(-58.0, 348.0, 84.0, 20.0), "bAutoSize": False, "zOrder": 0}),
    ('RespawnRing', '/Script/UMG.ProgressBar',
     {"visibility": "Hidden", "percent": 0.0},
     {"layoutData": anchor(0.0, 410.0, 104.0, 8.0), "bAutoSize": False, "zOrder": 0}),
    ('CountdownText', TEXT,
     {"text": "", "font": 56, "justification": "Center", "visibility": "Hidden"},
     {"layoutData": anchor(0.0, 432.0, 104.0, 71.0), "bAutoSize": False, "zOrder": 0}),
]

for display, cls, wprops, sprops in PLAN:
    print('== %s' % display)
    info = B.ensure(WBP, display, cls, CANVAS, -1)
    w, s = info['widget'], info['slot']
    print('  widget props:', sorted(json.loads(B.props(w)).keys()))
    B.props(s)
    if 'font' in wprops:
        wprops = dict(wprops, font=B.font_sized(w, wprops['font']))
    if display == 'WeaponIcon':
        brush = json.loads(B.get(w, ['brush']))['brush']
        brush['imageSize'] = {"x": 84.0, "y": 20.0}
        wprops = dict(wprops, brush=brush)
    print('  before w:', B.get(w, list(wprops)))
    print('  before s:', B.get(s, list(sprops)))
    print('  set    w:', B.setp(w, wprops))
    print('  set    s:', B.setp(s, sprops))
    print('  after  w:', B.get(w, list(wprops)))
    print('  after  s:', B.get(s, list(sprops)))

# The one NON-additive edit, deliberate and cited: WeaponText sits centred on x636, i.e.
# 577..695, which the 540..624 silhouette would overlap. `36:10` measures the NAME at
# x636..754 with the silhouette beside it and the PAIR centred on 640. Offset -4 -> 55.
print('== WeaponText (measured `36:10`: abs x636, was centred ON 636)')
wt = {'refPath': TREE + '.WeaponText'}
wts = json.loads(B.widgets(WBP)[0]['WeaponText']['slot']['refPath'])if False else B.widgets(WBP)[0]['WeaponText']['slot']
B.props(wts)
print('  before s:', B.get(wts, ['layoutData']))
print('  set    s:', B.setp(wts, {"layoutData": anchor(55.0, 348.0, 118.0, 19.0)}))
print('  after  s:', B.get(wts, ['layoutData']))

print('== compile + save')
print(B.compile_and_save(WBP))
print('== readback')
print(B.widgets(WBP)[1])
