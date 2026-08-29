# Halo Infinite layout/blueprint references — the curated list (BN28)

> 29 Aug 2026, cloud lead. Found by search; the cloud container's egress proxy
> BLOCKS every image-bearing domain (artstation, gfinity, halopedia, gamecoach,
> itch, reddit), so these are verified-to-exist but UNVIEWED from the cloud.
> Open them in a browser (founder or terminal). To let the cloud adapt our
> sheets to any of them: save screenshots into `docs/design/reference/` and
> commit — the cloud can view committed images.

## The one that matters most

1. **HCS 2022 official map layouts — Arturo Pérez (ArtStation)**
   https://www.artstation.com/artwork/Je9bna
   (mirror: https://arturbloodshot.artstation.com/projects/Je9bna)
   Top-view layouts of ALL Halo Infinite competitive maps, made for the Halo
   Championship Series "so the community could understand the maps and use
   them for strategy planning". This is the community-standard blueprint board
   — the exact genre our S1 sheets compete in. His album "Map Top-downs" has
   more: https://arturbloodshot.artstation.com/albums/3082563
   (Same artist also did Forge concept boards and MCC layout sheets — a whole
   catalogue of the drawing conventions we want.)

## Interactive / callout references

2. **Gfinity interactive maps + callouts, all maps**
   https://www.gfinityesports.com/halo-infinite/multiplayer-maps/
   Per-map overheads with every callout named (Aquarius, Recharge, Live Fire,
   Streets, Bazaar…). Our research digest's verbal topology came largely from
   these pages' text.
3. **GameCoach strategy boards** — https://gamecoach.gg/esports/haloinfinite
   Clean interactive overlay boards for rotations/setups — the "living" version
   of a strategy blueprint.
4. **Halopedia map pages** (e.g. https://www.halopedia.org/Aquarius) — each map
   page carries official overhead shots and layout prose.
5. **Ketul Majmudar's level-design analyses** (annotated layout diagrams):
   Aquarius: https://uxdesign.cc/halo-infinite-multiplayer-level-design-series-aquarius-e90be2907496
   Bazaar: https://ketul1776.medium.com/halo-infinite-multiplayer-level-design-series-bazaar-a69fb9828762

## What to do with them (the loop)

- Founder/terminal: open #1, screenshot the boards for Aquarius + Recharge +
  Live Fire, drop into `docs/design/reference/`, commit, say "get latest".
- Cloud: views them, extracts the conventions worth stealing (level tinting,
  weapon iconography, zone labeling, silhouette style) and re-tunes
  `gen_blueprint.py` so our sheets read like HCS boards while keeping the
  civil-engineer dimension discipline they lack.
