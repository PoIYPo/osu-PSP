# Third-party notices

## osu!lazer (MIT)

Slider path maths (`SliderPath`: adaptive bezier flattening, perfect-circle arcs,
catmull-rom approximation), slider tick spacing and the difficulty formulas
(preempt/fade from AR, hit windows from OD) were written by re-implementing the
behaviour of osu!lazer, which is MIT licensed:

    Copyright (c) 2022 ppy Pty Ltd <contact@ppy.sh>
    https://github.com/ppy/osu

No lazer source code is copied verbatim; the implementations are independent C++
ports of publicly documented behaviour.

## pspsdk / pspdev (BSD-ish / GPL-compatible)

The PSP build links against PSPSDK headers and libraries (pspgu, pspctrl, sceMp3
bindings, etc.). PSPSDK is distributed under a permissive licence that is
compatible with MIT for the purpose of linking.

## Fonts and assets

None are bundled yet. Any font or skin added later must carry its own licence
(OFL/Apache for fonts, CC0 for generated art).

## Beatmaps

Beatmaps are user-created content and are **not** part of this repository. The
`testdata/` directory is git-ignored and populated on demand by developer scripts
for parser testing only.
