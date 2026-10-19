# ZONE111 heightmap validation

Validated 2026-10-18; horizontal metadata checked 2026-10-19;
interior cell-mode inventory checked 2026-10-20.
The reconstructed **128×128** discrete height grid is
consistent with the verified original executable arithmetic: unsigned sample
bytes, reversed source rows, multiplier **200**, base **0**, and 32-bit results.
Its heights span **10,800–31,800 internal units**. This is an offline data
validation, not an original-game execution or a terrain renderer implementation.

## Reproduce

From the project root, using the existing configured build:

```sh
cmake --build build --target graphics_probe original_graphics_tests -j2
python3 tools/reconstruct_zone_heightmap.py \
  imported/from-iso/resources/VOL/ZONES.VOL/DAT/ZONE111.DAT \
  imported/from-iso/resources/VOL/ZONES.VOL/DBA/ZONE111.DBA \
  --output imported/terrain-validation/zone111-spacing
ctest --test-dir build -R '^(original_graphics|graphics_importer|zone_heightmap)$' --output-on-failure
```

The Python utility uses only the standard library and the existing C++17
`graphics_probe` executable. Pass `--probe PATH` for another build directory.
There is no JVM, Maven, REA, Ghidra, image-library, or game runtime requirement.
For an isolated helper build without configuring the game:

```sh
c++ -std=c++17 -Isrc tools/graphics_probe.cpp src/original_graphics.cpp -o /tmp/es2-graphics-probe
# Pass --probe /tmp/es2-graphics-probe to the Python utility.
```

Default outputs are under Git-ignored `imported/terrain-validation/zone111/`:

- `height-grid.json`: exact signed integer rows (`heights[y][x]`), dimensions,
  raw DAT words, source paths/hashes, horizontal spacing and summary metadata.
- `heightmap.png`: 128×128 grayscale visualization, linearly normalized from
  minimum height (black) to maximum (white). Top image row is grid row 0.
  **Black represents 10,800 here, not zero**; normalization never changes the
  numerical grid. No geographic axis direction is implied.
- `report.json`: concise dimensions, extrema, selected samples and provenance.

Outputs must use a new directory; repeat runs need another `--output` under
ignored `imported/`. Existing results and original resource directories cannot
be overwritten. No copyrighted data, preview or generated grid is added to Git.

## Reused decoding and scope

The Python graphics importer validates cockpit-specific headers; it is not a
general DBA decoder. There was no reusable Python ZONE terrain reader. The
existing `original::decodeBitmaps` in `src/original_graphics.cpp` already handles
the exact uncompressed 8-bit DBA/DBM layout used by ZONE111, including packed
rows, object alignment, container lengths and full file consumption.

`graphics_probe --indices-json BITMAP` now exposes the first frame's original
indices and dimensions, plus the total frame count, without a palette. The
decoder and its existing palette/PPM interface are unchanged. No competing DBA
parser was introduced. The utility reads the 16-byte DAT with little-endian
`struct.unpack`, retains both signed values and raw unsigned words, and uses
the fourth word as a multiplier and the third as the horizontal shift.
Words at offsets 0 and 4 remain uninterpreted; dimensions come from the bitmap,
not inferred DAT meanings.

The utility deliberately admits one frame with power-of-two dimensions. Other
frame counts, non-power-of-two terrain dimensions, unsupported encodings,
inconsistent lengths and truncated resources fail with a nonzero status. It
does not resize, invent padding, guess palette-derived elevations, apply an
origin adjustment, convert heights to metres or implement interpolation. The original
object-factory dispatch is not fully understood, but ZONE111's local serialized
bitmap is fully consumed by the existing supported decoder; that limitation
does not leave this file's pixel payload ambiguous.

## Evidence and original-file results

For the initial discrete reconstruction, the saved analysis report was read at
`/tmp/es2-rea-terrain-esxlpb70/report.md`; Ghidra was not rerun then. Its temporary
location is not a tool dependency. The relevant durable reference facts are:

| Evidence | Meaning used here |
| --- | --- |
| DBSIM SHA-256 `33600748810578667ca513841406bbb4f4990eee463a8a041ea09ebd12730df3` | Exact analyzed original executable; Ghidra 12.1.4 through REA |
| Zone loader `0x0042789c`, store `0x00427a24` | Fourth DAT word supplies grid `+0x118`, the multiplier |
| Bitmap-to-grid function `0x0046c3c0`, `0x0046c466–0x0046c498` | Source row `r` goes to grid row `height-1-r`; columns retain order |
| `0x0046c4bf–0x0046c4db`, `0x0046c609` | Byte samples; subtraction parameter zero in this path; base cleared to zero |
| Height function `0x0046e07c`, `0x0046e25a–0x0046e260` | Zero-extension of byte, 32-bit `IMUL`, then base `ADD` |

REA Evidence IDs: zone loader
`ev_aaa32ca5f0b274e712f195bfd2005d0b64d237d54225ebd45e7a7188f54d90bf`;
bitmap-to-grid
`ev_ac929b26fd27dd0e78b27a34700c9856122b593e01bbf9eac71b304d2d9d5daa`;
height query
`ev_241e271f464982dec7696d904a7f2e2504a58672935d7bc6f9810a6079c64b07`.
No decompiler output is copied into this repository.

Source identities:

- `ZONE111.DAT`: 16 bytes; words `(7, 7, 14, 200)`;
  SHA-256 `b0d0ea979ed75c750c82e63784467ee3ab81cf545e889e623c426af6d8f23cec`.
- `ZONE111.DBA`: 16,418 bytes; one 128×128 frame; unsigned sample range **54–159**;
  SHA-256 `5f05839e6fd3292418dd44c126490fad3707b50d8fc4c44cfba47fab9f0a31f5`.

Selected values, with coordinates expressed only as array indices:

| Grid `(x,y)` | Source `(x,row)` | Sample | Height, internal units |
| --- | --- | --- | --- |
| (0,0) | (0,127) | 84 | 16,800 |
| (127,0) | (127,127) | 82 | 16,400 |
| (0,127) | (0,0) | 82 | 16,400 |
| (127,127) | (127,0) | 99 | 19,800 |
| (64,64) | (64,63) | 147 | 29,400 |
| (56,14) | (56,113) | 54 | 10,800 |
| (32,48) | (32,79) | 159 | 31,800 |
| (42,94) | (42,33) | 159 | 31,800 |

The final row is the original analysis anchor: source file byte offset `0x10cb`.
There are multiple maxima; the report selects the first in reconstructed grid
order. Independently comparing all **16,384** reconstructed heights with the
verified original pixel region starting at file byte 33, using source offset
`33 + (127-y)*128 + x`, found **zero discrepancies** against byte × 200.
This cross-check uses the already validated one-file layout, not a new parser.

## Comparison and verification

- The older `docs/original-terrain.md` sample minimum of **0** is incorrect for
  this supplied file: both the reused decoder and direct pixel-region check
  give **54**. No zero-based height offset was introduced to match that note.
- The generic bitmap decoder correctly preserves file row order. Terrain
  reconstruction needs the additional vertical reversal; changing the shared
  decoder would incorrectly change other original graphics.
- Earlier relief-like previews did not establish elevation semantics. The
  executable arithmetic now establishes discrete sample heights for this path.
  Palette-expanded RGB or normalized preview brightness must not substitute
  for the raw sample bytes.
- Bitmap dimensions are preserved. Matching `(7,7)` to 128×128 is not used to
  assign field meanings. The third DAT word is preserved raw and now also
  interpreted as the horizontal shift, based on subsequent executable evidence.

Automated results after the spacing update: **21 synthetic tests passed**, **52 existing native
graphics checks passed**, and **6 existing graphics-importer tests passed**
(three CTest targets). New coverage includes unsigned samples above 127,
the 159 × 200 anchor, signed multiplier boundaries/32-bit overflow, row reversal,
non-square dimensions, unknown DAT words, malformed/truncated resources,
unsupported variants, PNG structure and pixels, source hashes/preservation,
non-overwriting outputs and the existing palette preview interface. Five added
tests cover verified spacing, non-square and single-sample spans, positive
shift limits, display quantization and unsupported shifts without loss of heights.

The generated PNG was opened and visually inspected as a valid grayscale
heightmap at the original dimensions. This does not establish original-game
visual fidelity, world orientation, interpolation, collision or physical scale.
The original executable and game were not run. Source/resource hash comparison
confirmed no changes to runtime code, existing graphics decoding, assets or
original inputs. Git ignore/public-tree checks passed.

Initial reconstruction files: `tools/reconstruct_zone_heightmap.py`,
`tools/graphics_probe.cpp`, `tests/zone_heightmap_tests.py`, the new CTest entry
in `CMakeLists.txt`, and this report. Existing terrain generation remains intact.

## Verified horizontal metadata

The [coordinate investigation](terrain-coordinate-scaling.md) resolves the
earlier unit question. The third DAT word is stored at `grid+0x108`
(`0x00427a09`); height queries shift X/Y right by it (`0x0046e093`,
`0x0046e09e`), and vertex construction shifts sample indices left
(`0x0046d95c`, `0x0046d96d`). The display helper `0x00434228` divides by
1000 **before** multiplying by 6. That report contains exact REA Evidence IDs.

Both JSON outputs now include an additive `horizontal_spacing` object:

| Field | ZONE111 value |
| --- | --- |
| `shift_i32` | 14, from zero-based DAT word index 2 |
| `spacing_internal` | 16384 on both X and Y |
| `spacing_metres_nominal` | 98.304 |
| `span_internal.x`, `.y` | 2080768 each: `(128 - 1) * 16384` |
| `span_metres_nominal.x`, `.y` | 12484.608 each |
| `nominal_metres_per_internal_unit` | Exact ratio numerator 6, denominator 1000 |
| `display_conversion.quantum_metres` | 6; `quantized` is true |
| `display_conversion.one_axis_step_metres` | 96, the quantized readout for one axial step |
| `vertical_metres_independently_verified` | false |

The nominal coordinates use the ratio without rounding. They are distinct
from the original HUD's `6 * trunc_toward_zero(distance_internal / 1000)` and
its approximate horizontal distance metric, both recorded in the metadata.
The spans measure the first to last **sample**, not the full query domain or
any inferred wrapping/clamping behavior. No vertical metre values are exported.

Shifts 0..30 admit a positive signed 32-bit step. This is the utility's
explicit supported-value policy, not a claim that the game validates DATs that
way. For other values, metadata reports `status: unsupported_shift` and the
original word, omitting guessed spacing; discrete heights still decode exactly
as before. Counts outside this range are not silently masked to x86 shift counts.

Schema version 1, every existing field name, height type, row order, source
identity and preview remain compatible. The existing descriptive list
`uninterpreted_dat_byte_offsets` is corrected from `[0,4,8]` to `[0,4]` because
offset 8 is now understood. Comparing the actual original-file run in ignored
`imported/terrain-validation/zone111-spacing/` against the prior `zone111/`
run found every other legacy field equal, all 16,384 heights unchanged, and
the PNG byte-identical. Neither original input was modified.

This update changes the Python utility, its focused tests and this report;
the C++ decoder and renderer remain unchanged. Integer triangular interpolation
is confirmed in the executable but remains unimplemented. The separate
[diagonal investigation](terrain-diagonal-selection.md) follows the relevant
cell flags and boundaries; no flag pattern is assumed by this utility.

## Offline interior cell-mode inventory

`tools/inventory_zone_cells.py` consumes the existing `height-grid.json`
without modifying it or the reconstruction utility. It uses only Python's
standard library and the existing signed-32-bit and PNG helpers. Run:

```sh
python3 tools/inventory_zone_cells.py \
  imported/terrain-validation/zone111-spacing/height-grid.json --preview
cmake -S . -B build
ctest --test-dir build -R '^(zone_heightmap|zone_cell_modes)$' --output-on-failure
```

The default new output directory is Git-ignored
`imported/terrain-validation/zone111-cell-modes/`. `cell-modes.json` contains
the 127×127 mode array, counts, percentages, corner offsets, diagonal endpoints,
scope and input JSON SHA-256. `--preview` additionally writes `cell-modes.png`;
omit it for numerical output only. Repeated runs require a fresh `--output`
directory under ignored `imported/`; existing results are never overwritten.

The selector compares unsigned **sample bytes**, not scaled heights. The tool
recovers each byte using a unique inverse of
`signed32(sample * multiplier_i32 + base_i32)`. This also handles negative
multipliers and signed overflow without changing the comparison's sign.
Ambiguous inverses, inconsistent dimensions, unsupported row order, missing
fields and invalid heights fail explicitly. It does not introduce another DBA
parser or reverse rows a second time.

Following [the verified selector and consumer branches](terrain-diagonal-selection.md),
corners are `s00=(x,y)`, `s10=(x+1,y)`, `s01=(x,y+1)`, `s11=(x+1,y+1)` and
`delta=(s00+s11)-(s01+s10)`. The selector is `0x0046bed8`, with zero selection
at `0x0046c02f` and the signed nonzero branch at `0x0046c069`. Diagonal endpoints
come from the height-query branches in `0x0046e07c`, not from sign-based naming.

| Difference | Mode (low two bits) | Diagonal endpoints, relative to cell | Count | Percentage |
| --- | --- | --- | --- | --- |
| Positive | 0 | `(1,0)` to `(0,1)` | 6,619 | 41.0379% |
| Negative | 2 | `(0,0)` to `(1,1)` | 6,654 | 41.2549% |
| Zero | 1 | Coplanar; no diagonal needed | 2,856 | 17.7072% |
| Total | | | 16,129 | 100% |

These counts agree with the prior arithmetic tally in the diagonal report.
The preview uses a fixed categorical legend: mode 0 black, mode 1 gray (127),
mode 2 white (255). It was opened and visually checked at 127×127 pixels.
`cell_modes[y][x]` uses the input grid's row order: X increases right and Y
down in the image; no geographic direction is implied. The last sample row
and column are neighbors for interior cells, never additional cell origins.
No edge mode, upper flag bits, material or interpolation is inferred.

Verification: **11 new synthetic tests and all 21 existing heightmap tests
passed** through the two CTest targets. Coverage includes each sign, unsigned
byte extremes, all four corner contributions, reflection, asymmetric grid
orientation, reuse of the existing row reversal, exact counts/percentages,
malformed inputs, sample-recovery ambiguity, fixed PNG colors, input metadata
preservation and non-overwriting CLI output. Hash checks confirmed that the
existing grids, spacing metadata, previews, original resources and runtime
files remained unchanged. Generated inventory artifacts are Git-ignored.

This is a bare-DBA-derived sample inventory. The conditional mission
preprocessing in `0x00471190` remains outside its scope. The original executable
was not run, and Ghidra was not rerun; no renderer or gameplay verification is
claimed. Changes are limited to the new utility, its test file, a CTest
registration and this report.

The smallest subsequent task is an **offline interior-only integer triangle
sampler**, using these modes and the documented arithmetic. Validate corners,
both sides of each diagonal, equality cases and separate signed arithmetic
shifts with synthetic fixtures before sampling ZONE111. Keep boundary strips
and mission preprocessing explicitly outside that experiment. No interpolation
has been implemented in this inventory task.

Follow-up: the [offline triangle sampler](terrain-triangle-sampler.md) now
validates all 64,516 interior-cell corners against this unchanged grid, with
zero mismatches. It documents exact branch arithmetic and keeps outer-strip
queries and renderer integration excluded.
