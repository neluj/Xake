# Third-Party Notices

This document applies to Xake 0.1.0. It is an engineering record, not legal
advice.

## Qt

Xake dynamically links to Qt 6.10.3 Core, Gui, Widgets, and Svg libraries. The
open-source Qt distribution makes these modules available under the GNU Lesser
General Public License version 3 and, depending on the module, the GNU General
Public License version 3. Commercial Qt terms are not used by this project.

- Project: https://www.qt.io/
- Source: https://download.qt.io/archive/qt/6.10/6.10.3/single/
- License information: https://doc.qt.io/qt-6/licensing.html
- LGPL-3.0 text: `licenses/LGPL-3.0.txt`
- Corresponding-source offer: `QT_SOURCE_OFFER.md`

Qt can contain third-party components under additional permissive licenses.
Their notices are maintained by Qt at:
https://doc.qt.io/qt-6/licenses-used-in-qt.html

## Chess Piece Sprite

`app/assets/Chess_Pieces_Sprite.png` is a rasterized derivative of
"Chess Pieces Sprite.svg" by jurgenwesterhof, adapted from work by Cburnett.
The source artwork is licensed under Creative Commons
Attribution-ShareAlike 3.0 Unported.

- Source: https://commons.wikimedia.org/wiki/File:Chess_Pieces_Sprite.svg
- License text: `licenses/CC-BY-SA-3.0.txt`
- Local SHA-256:
  `2138DCA74830ACD9EEEA66FAECFF807A0E1DC9074A603F956BAFAD6F2D64A2DB`

The local sprite is used to render the chess pieces and is redistributed under
the same CC BY-SA 3.0 terms. No endorsement by the original authors is implied.

## Perft Regression Data

`tests/data/perftsuite.epd` contains chess positions and expected node counts
used only by the source-tree test suite. The file was originally found online
and used by the related Akerbeltz project. Its original author, canonical
source, and license could not be identified. Xake does not claim authorship of
this data, and it is not installed in the Windows binary package.

## Xake Artwork

The following project artwork was supplied or created for Xake and is
distributed under GPL-3.0-only with the application:

- `app/assets/xake-logo.png`
- `app/assets/xake.ico` and the PNG icon variants derived from the logo
- `app/assets/color_w.svg`
- `app/assets/color_b.svg`

## Compiler Runtime

Windows packages built with MinGW may contain GCC, libstdc++, and MinGW-w64
runtime libraries installed by Qt's deployment tooling. Their license and
runtime-exception texts are included in the `licenses` directory. These files
apply only when the corresponding runtime DLLs are present in a package.
