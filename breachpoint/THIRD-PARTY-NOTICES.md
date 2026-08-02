# Third-party notices — BREACHPOINT

**This file must ship inside the packaged game.** It is not a repo-only record. Several of the
licences below require their notice to travel with every copy of the software, so a build that
omits this file is a licence violation regardless of what the repository contains.

**Staged as:** `Content/Legal/THIRD-PARTY-NOTICES.txt` (see `Config/DefaultGame.ini`
`[/Script/UnrealEd.ProjectPackagingSettings]`). Verification is
`Tools/verify_notices.py`, and it is a rung-2 grep gate — see `docs/contracts/testing.md`.

**Adding a dependency?** Add its notice here *in the same commit that adds the dependency*, and
add its verification token to `Tools/verify_notices.py`. A notice added later is a period during
which the game shipped without it.

---

## Lucide (icons)

**49 Lucide icons are in the Breachpoint UI**, imported into the Figma file under
`Art / UI Glyphs` (98 components across the 24px and 40px tiers) and destined for
`Content/UI/Icons/`. Their licence comments were stripped on import to reduce payload size, so
the notice does not travel with the artwork and must appear here.

```
Lucide — https://lucide.dev

Copyright (c) for portions of Lucide are held by Cole Bemis 2013-2022 as part of
Feather (MIT). All other copyright (c) for Lucide are held by Lucide Contributors 2022.

Licensed under the ISC License.

Permission to use, copy, modify, and/or distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright notice
and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.
```

**Modifications we made, which the licence permits:** `stroke-linecap: round → square` and
`stroke-linejoin: round → miter`, to match our design system. Geometry is unchanged. The 40px
tier uses source `stroke-width: 1.2` so it renders at 2px absolute — scaling the 24px artwork
would give 3.33px and break the system's stroke rule (`docs/ui/ASSET-METHODS.md`).

---

## Unreal Engine 5.8

Unreal® Engine, Copyright 1998–2026, Epic Games, Inc. All rights reserved. Unreal® is a
trademark or registered trademark of Epic Games, Inc. in the United States of America and
elsewhere. Used under the Unreal Engine End User Licence Agreement.

Includes engine-distributed third-party software; see the engine's own
`Engine/Source/ThirdParty` notices, which ship with the engine's runtime redistributables.

---

## Epic Games — First Person template content

Art assets originating in Epic's First Person template (mannequin meshes, animations, weapon
meshes, prototyping materials) are used under the Unreal Engine EULA, which permits their use in
projects built with the engine.

---

## Not third-party, and deliberately so

**No Halo asset, string, typeface, icon, emblem or brand mark ships in Breachpoint.** Halo is a
trademark of Microsoft Corporation; Halo Infinite and Halo: Campaign Evolved are the work of
343 Industries / Halo Studios. Breachpoint's UI follows *layout and behaviour conventions* — a
functional design vocabulary shared across the FPS genre — and its artwork is original.

Reference material was studied and is credited in `docs/UI-DESIGN-SYSTEM.md` §1.

**Internal development is a separate matter from shipping, deliberately.** Original assets are
used during development for reference, measurement and placeholder work; they live in
`Content/Reference/`, which is excluded from git and from cook, and `Tools/verify_notices.py`
verifies both exclusions plus the one escape route that defeats them (a hard reference from
shipping content). The removal of Halo-owned strings and assets from *shipping* files is tracked
in `docs/ui/ART-PASS-STAGE-3.md` and is **not yet complete** — that pass gates shipping, and the
paragraph above is a claim about a packaged build, not about the working tree.
