# Original terrain investigation

Inspected the supplied EarthSiege 2 Hi-Rez ISO and its extracted resources on
2026-10-18. This pass did not change runtime rendering, simulation, audio,
cockpit, or HERC model code. The terrain integration gate remains closed until
the resource relationships below can be verified against the original game.

## Resources found

`ZONES.VOL` contains paired `DAT/ZONE*.DAT` and `DBA/ZONE*.DBA` records, plus
mission data such as `MSN/TRAIN1.MSN` and `MSN/C1_01.MSN`. There are 39 pairs in
the local archive. The existing bounded graphics reader decodes the DBA files
as single 8-bit indexed images: most are 128×128 and five are 256×256. For
example, `ZONE111.DBA` decodes to 128×128 pixels. Its pixel values span 0–159
and a grayscale preview resembles relief. That is a useful candidate, not
proof that its values are elevations, what scale they use, or which mission
uses it.

The matching `ZONE111.DAT` is 16 bytes and reads as four little-endian
integers `(7, 7, 14, 200)`. Other zone records vary. The field meanings and the
relationship between these values and the image dimensions are unresolved.

`SIMVOL0.VOL` contains `WORLD0.WLD` through `WORLD9.WLD`, matching `.RMP`
files and `WORLD0.DPL` through `WORLD9.DPL` palettes. The WLD files are 310–313
bytes. In `WORLD0.WLD`, the observed initial 16-bit values are `2, 208, 6,
16`; its trailing strings identify `world24`, `clouds2`, `impact0`, and
`urban.tex` (stored as `urban` and `tex`). Other records name snow, volcanic,
ice, and lunar ground sets. A pre-existing Herc Works `WorldData.java` reader
describes fields 2/4/6 as sky palette and horizon values, and records these
strings, but marks most of the layout unknown and unfinished. Treat those
meanings as a lead, not a verified renderer specification.

Each RMP is 98,312 bytes and starts with two 32-bit values `(32, 12)`. The
remaining data is not a recognized image or the current DBM/DBA format. No
standalone `.TEX` files were present in the extracted ISO tree, so the named
ground sets cannot be loaded directly with the current bitmap reader. The
database palette and RMP contents may hold part of the missing texture/render
mapping, but their relationship has not been established.

## Why terrain was not wired into gameplay

The mission files are structured binary data rather than readable manifests.
The small `TRAIN1.MSN` is 3,808 bytes; its companion `.ENG` contains two
training messages. The local research and OpenSiege / ES2 Toolkit references
do not provide a verified parser for this terrain relationship. No confirmed
link yet connects a particular `.MSN` mission to a `ZONE*.DBA`, WLD/RMP pair,
coordinate origin, or playable world extent.

Using a relief-looking DBA in the current 2.4 km missions would require guessing
at least its height units, orientation, bounds, mission association, palette,
and texture mapping. A guessed elevation function would also make rendering
disagree with the existing movement, collision, navigation, and HERC foot
grounding. For that reason, the current analytic terrain and its collision
behavior remain the working fallback. The HERC pair, 4:3 cockpit, existing HUD,
and all audio mappings are untouched.

## Smallest viable next step

Build an offline resource inspector for the actual records, then compare its
height and environment output with one mission running in the original game.
The minimum evidence needed before runtime integration is:

1. A decoded mission reference that identifies the exact zone and world IDs.
2. Verified DAT fields, DBA pixel interpretation, height unit/range, row
   direction, map origin, and world bounds.
3. The corresponding ground texture/material mapping and palette behavior.
4. A WLD/RMP decode that explains the horizon and terrain rendering inputs.
5. Known positions and elevations from the reference mission for validating
   player/enemy ground contact and navigation.

Once those facts are established, the existing simulation height query can
sample the same source grid used by the terrain renderer. The current graphics
reader already handles the observed DBA bitmap container; no new runtime
dependency is indicated. Keep imported resources and preview images under
ignored `imported/` or `assets/original/` paths.

## References

- The locally inspected `WorldData.java` is from [Herc Works MDK at pinned
  revision](https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/data/file/dbsim/WorldData.java).
- [OpenSiege bitmap definitions](https://github.com/open-siege/siege-studio/blob/53e7efb42a18bea053749f88d3c9059f76a613a0/siege-modules/content/siege-content-3space/include/siege/content/bmp/bitmap.hpp) and [bitmap reader](https://github.com/open-siege/siege-studio/blob/53e7efb42a18bea053749f88d3c9059f76a613a0/siege-modules/content/siege-content-3space/src/bmp/bitmap.cpp) document the generic bitmap container; they do not establish what the ZONE grids mean.

The technical uncertainty is the original mission/terrain linkage and grid
semantics, not ISO access or generic DBA image decoding.
