# Third-party notices

Astral-Remake (by kdt255) bundles the components below. Each one stays under its own license; the
MIT + Commons Clause license in [`LICENSE`](LICENSE) only covers the first-party code.

| Component | Version | License | Files |
| --- | --- | --- | --- |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.92.9b (locally patched) | MIT | `third_party/imgui/` |
| stb_truetype, stb_rect_pack, stb_textedit | bundled with Dear ImGui | MIT / Public Domain | `third_party/imgui/imstb_*.h` |
| [FreeType](https://freetype.org) | 2.15.0 (static x64 /MD build) | FreeType License (FTL) | `third_party/freetype/` |
| [Inter](https://github.com/rsms/inter) SemiBold + Bold | 4.001 | SIL Open Font License 1.1 | `menu/src/astral/fonts/inter.h` |

## Dear ImGui

Copyright (c) 2014-2026 Omar Cornut. Full text: [`third_party/imgui/LICENSE.txt`](third_party/imgui/LICENSE.txt).

The bundled copy carries local patches (`imgui_anim_widgets.h`, `anim.h`, extra style colors); they are
distributed under the same MIT license.

## FreeType

Portions of this software are copyright © 2026 The FreeType Project (www.freetype.org). All rights reserved.

Distributed under the FreeType License: [`third_party/freetype/FTL.TXT`](third_party/freetype/FTL.TXT)
(see also [`third_party/freetype/LICENSE.TXT`](third_party/freetype/LICENSE.TXT)). FreeType's gzip module
contains a copy of zlib, which is under the zlib license.

## Inter

Copyright (c) 2016 The Inter Project Authors. The font data is embedded unmodified as a byte array in
`menu/src/astral/fonts/inter.h` and is licensed under the SIL Open Font License 1.1:
[`third_party/inter/OFL.txt`](third_party/inter/OFL.txt).
