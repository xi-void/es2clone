# Original graphical resources

The first OpenEarthSiege 2 graphics increment reads the original **Tomahawk cockpit bitmap and palette directly**. It replaces the prototype frame when those files are installed. Original proportions are preserved in a 4:3 viewport; wide screens have side bars. The frame now uses linear sampling with premultiplied alpha, and the live HUD shares its 640×480 coordinates. See [cockpit corrections](battlefield.md#cockpit-corrections-and-limits) for layering, scaling and remaining source-pixel detail. Live instruments retain prototype behavior. The original frame does not change chassis statistics or claim to implement the original Tomahawk equipment.

## Import

From the project root:

```sh
python3 tools/import_original_graphics.py original/Earthsiege2_Freeware_HiRezStudios_1r0.iso
./run.sh
```

The importer reads only `SIMVOL0.VOL` from the ISO into a temporary directory and copies **two** resources byte-for-byte into ignored `assets/original/`: `TOMAHAWK.HB0` and `COCKPIT.DPL`. No original file is modified; the audio library, mappings, soundtrack, and settings are untouched. It requires Python 3 and `7z` for ISO input. Conflicting destination files cause an error rather than being overwritten.

Reuse the existing extraction without reading the ISO again:

```sh
python3 tools/import_original_graphics.py imported/from-iso/resources/VOL/SIMVOL0.VOL
```

A local `SIMVOL0.VOL` archive or an installation directory containing it also works. `assets/original/import.json` records source paths and hashes. If importing from extracted files, their hashes are recorded; the original archive hash is unavailable in that mode. The importer deliberately does not guess which of multiple installations to use.

`./build/earthsiege --original-dir /path/to/local/graphics` selects a different directory containing these two files. Otherwise source builds discover the project's `assets/original/`, and packages use the directory beside the executable. No absolute developer path is compiled in. Original art loads by default when available; `--prototype-cockpit` selects the previous artwork. Absent or corrupt original resources leave the game playable with that fallback. The executable name and existing pilot saves remain compatible.

## Verified format subset

| Resource | Implemented layout | Limits |
| --- | --- | --- |
| DPL | `0f 00 28 00`, payload length, 256 RGB/flags entries starting at byte 12; six-bit channels expanded by four | Flags are not interpreted as opacity; additional palette tables are not decoded |
| DBM | `0e 00 28 00`; 21-byte packed header; little-endian height, width, 8-bit depth, image length; row-major indices | Only uncompressed, zero-extra-data records with consistent lengths |
| DBA / HBA / HB0 and related views | `01 00 28 00`, payload length and count, then DBM records aligned to two bytes | File/decoded pixel/frame count limits; unsupported encodings rejected |

The DBM header is **21 bytes**, so the first pixel of a single-frame HB0 array is byte **33**, not 34. DPL's first color is at byte **12**, not 16. Padding is per bitmap record, not per scanline. These details were verified against Open Siege's [structures](https://github.com/open-siege/siege-studio/blob/53e7efb42a18bea053749f88d3c9059f76a613a0/siege-modules/content/siege-content-3space/include/siege/content/bmp/bitmap.hpp), [reader](https://github.com/open-siege/siege-studio/blob/53e7efb42a18bea053749f88d3c9059f76a613a0/siege-modules/content/siege-content-3space/src/bmp/bitmap.cpp) and [palette decoder](https://github.com/open-siege/siege-studio/blob/53e7efb42a18bea053749f88d3c9059f76a613a0/siege-modules/content/siege-content-3space/src/pal/palette.cpp), then checked against the supplied disc. No external extraction program or new runtime dependency is needed.

Transparency is an explicit cockpit rendering choice: index zero contains the Tomahawk window/background cutouts; instrument faces use other indices and remain opaque. The generic decoder preserves every index and can output fully opaque previews. This does not implement the original HD/ED clipping masks.

Local validation decoded **367 resources**: 10 DBM, 252 DBA, 51 HBA and 54 HB/DB view arrays, including all nine HB0 cockpits. `SIMVOL0.VOL/dba/CORNERS.DBA` declares a payload length inconsistent with the file; it remains unsupported and preserved. Decoding pixels does not establish the correct palette, animation, position, or gameplay meaning for every image. Only the verified Tomahawk/COCKPIT pair is currently used by the game.

## Inspection and tests

```sh
./build/graphics_probe \
  imported/from-iso/resources/VOL/SIMVOL0.VOL/hb0/TOMAHAWK.HB0 \
  imported/from-iso/resources/VOL/SIMVOL0.VOL/dpl/COCKPIT.DPL /tmp/tomahawk.ppm

./build/original_graphics_tests imported/from-iso/resources/VOL/SIMVOL0.VOL
ctest --test-dir build --output-on-failure
./tests/native-smoke.sh
```

The probe reports array dimensions and optionally writes the first frame as an opaque PPM. Supply a known matching palette; there is no automatic palette guessing. Public tests use synthetic fixtures and cover truncation, dimensions, record padding, invalid counts/encodings, transparency, conflicting imports, source preservation, audio isolation, and exclusion from packaging. The optional original-resource test checks all nine locally supplied frames. Neither public tests nor packages contain the original pixels.

The renderer uploads one 640×480 RGBA texture at startup (about 1.17 MiB), then draws one textured quad each frame. No frame-time filesystem I/O or image conversion is added. Frame and instruments use the existing `Gait::bounce`/`sway` values, with the same displacement as the prior console. The view retains `Combat::cameraEye`, `cameraAim`, and `cameraRoll`. There is no second gait clock, random shake, or audio timing change.

## Next compatibility work

Decode original gauge, font, clipping and view-layout resources before claiming complete cockpit fidelity. Verify terrain format, height units, tile orientation and mission references before replacing the shared visual/collision height function. DTS models, original mission scripts and gameplay values are subsequent increments. See the [compatibility assessment and roadmap](compatibility-roadmap.md).

`original/`, `imported/`, `assets/original/`, original graphics formats and local previews are ignored. Public staging and installation exclude the `original` asset directory, even if it contains text files. Keep screenshots containing original artwork local as well.
