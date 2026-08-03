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

## Rajdhani and Roboto Condensed (typefaces)

**Both UI typefaces are SIL OFL 1.1 and they ship inside the packaged game.** The `.ttf`
files live in `Content/UI/Fonts/` and are imported as `FontFace` assets, so the font data is
cooked into the build and this notice has to travel with it. The OFL permits embedding and
commercial redistribution as part of software; it requires this notice to accompany every
copy, and forbids selling the fonts by themselves.

- **Rajdhani** — Regular, SemiBold, Bold. Chrome, labels, the display ramp.
- **Roboto Condensed** — Medium, Medium Italic, SemiBold. Body copy, and the only italic in
  the system, because Rajdhani ships none.

**Used unmodified.** Static instances from the Google Fonts catalogue, byte-for-byte: no
subsetting, no renaming, no hinting or metric edits. Neither family declares a Reserved Font
Name, so nothing here is a Modified Version and no name change is owed.

The upstream `OFL.txt` of each family is kept verbatim beside the fonts as
`Content/UI/Fonts/OFL-Rajdhani.txt` and `OFL-RobotoCondensed.txt`. The two licence bodies are
identical; only the copyright line differs, so both copyright lines appear above the single
copy of the licence below.

```
Copyright (c) 2014, Indian Type Foundry (info@indiantypefoundry.com).
Copyright 2011 The Roboto Project Authors (https://github.com/googlefonts/roboto-classic)

This Font Software is licensed under the SIL Open Font License, Version 1.1.
This license is copied below, and is also available with a FAQ at:
http://scripts.sil.org/OFL


-----------------------------------------------------------
SIL OPEN FONT LICENSE Version 1.1 - 26 February 2007
-----------------------------------------------------------

PREAMBLE
The goals of the Open Font License (OFL) are to stimulate worldwide
development of collaborative font projects, to support the font creation
efforts of academic and linguistic communities, and to provide a free and
open framework in which fonts may be shared and improved in partnership
with others.

The OFL allows the licensed fonts to be used, studied, modified and
redistributed freely as long as they are not sold by themselves. The
fonts, including any derivative works, can be bundled, embedded, 
redistributed and/or sold with any software provided that any reserved
names are not used by derivative works. The fonts and derivatives,
however, cannot be released under any other type of license. The
requirement for fonts to remain under this license does not apply
to any document created using the fonts or their derivatives.

DEFINITIONS
"Font Software" refers to the set of files released by the Copyright
Holder(s) under this license and clearly marked as such. This may
include source files, build scripts and documentation.

"Reserved Font Name" refers to any names specified as such after the
copyright statement(s).

"Original Version" refers to the collection of Font Software components as
distributed by the Copyright Holder(s).

"Modified Version" refers to any derivative made by adding to, deleting,
or substituting -- in part or in whole -- any of the components of the
Original Version, by changing formats or by porting the Font Software to a
new environment.

"Author" refers to any designer, engineer, programmer, technical
writer or other person who contributed to the Font Software.

PERMISSION & CONDITIONS
Permission is hereby granted, free of charge, to any person obtaining
a copy of the Font Software, to use, study, copy, merge, embed, modify,
redistribute, and sell modified and unmodified copies of the Font
Software, subject to the following conditions:

1) Neither the Font Software nor any of its individual components,
in Original or Modified Versions, may be sold by itself.

2) Original or Modified Versions of the Font Software may be bundled,
redistributed and/or sold with any software, provided that each copy
contains the above copyright notice and this license. These can be
included either as stand-alone text files, human-readable headers or
in the appropriate machine-readable metadata fields within text or
binary files as long as those fields can be easily viewed by the user.

3) No Modified Version of the Font Software may use the Reserved Font
Name(s) unless explicit written permission is granted by the corresponding
Copyright Holder. This restriction only applies to the primary font name as
presented to the users.

4) The name(s) of the Copyright Holder(s) or the Author(s) of the Font
Software shall not be used to promote, endorse or advertise any
Modified Version, except to acknowledge the contribution(s) of the
Copyright Holder(s) and the Author(s) or with their explicit written
permission.

5) The Font Software, modified or unmodified, in part or in whole,
must be distributed entirely under this license, and must not be
distributed under any other license. The requirement for fonts to
remain under this license does not apply to any document created
using the Font Software.

TERMINATION
This license becomes null and void if any of the above conditions are
not met.

DISCLAIMER
THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL THE
COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL
DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM
OTHER DEALINGS IN THE FONT SOFTWARE.
```

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
