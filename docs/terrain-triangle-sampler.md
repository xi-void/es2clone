# Offline terrain triangle sampler

Validated 2026-10-20. `tools/sample_zone_terrain.py` samples the existing
reconstructed height grid and cell-mode inventory without changing either.
It implements modes **0, 1 and 2**, using integer internal coordinates and
the original instruction order. No game or renderer integration is included.

## Use

From the project root:

```sh
python3 tools/sample_zone_terrain.py \
  imported/terrain-validation/zone111-spacing/height-grid.json \
  imported/terrain-validation/zone111-cell-modes/cell-modes.json \
  --point 4096 4096 --point 12288 4096 --point 40960 8192 \
  --report imported/terrain-validation/zone111-triangle-example/report.json
```

Repeat `--point X Y` for additional grid-relative integer positions. The tool
prints JSON with sampled heights, selected cells, local offsets, modes, branch
addresses and vertex validation counts. `--report` optionally saves that JSON;
the file must not already exist. Keep reports under Git-ignored `imported/`.
Omitting points still runs vertex validation. Invalid inputs/queries exit nonzero.
Only Python's standard library and the existing Python tooling are required.

`TerrainSampler.from_files(grid_path, modes_path)` checks the inventory's source
SHA-256 against the exact grid JSON. It reuses `inventory_zone_cells.inventory`
to validate unsigned sample recovery, corner ordering, dimensions and the mode
array; the sampling then uses that checked array. Decoded spacing must match
`1 << shift_i32` and the supplied DAT word three. Shifts 0..30, power-of-two
dimensions and first-to-last spans within signed 32-bit coordinates are the
offline tool's explicit supported-input policy. No original parser is changed.

## Exact arithmetic evidence

The earlier reports established the branch conditions, but only gave one full
equation. This task inspected the complete **saved REA/Ghidra 12.1.4 assembly**
for `0x0046e07c`, including the pointer-to-corner mapping and all arithmetic
branches. No new analysis session was necessary. The evidence is:

- Executable SHA-256:
  `33600748810578667ca513841406bbb4f4990eee463a8a041ea09ebd12730df3`,
  checked against the unchanged local `DBSIM.EXE`.
- REA Evidence ID:
  `ev_241e271f464982dec7696d904a7f2e2504a58672935d7bc6f9810a6079c64b07`.
- Saved response:
  `imported/terrain-diagonal-analysis-ary0e4gg/height-query-46e07c.json`.
  Full generated assembly remains ignored, outside publishable source.

At `0x0046e28d`, `EDX=h00`, `EBX=h10`, stack `[EBP-0x4c]=h01` and
`[EBP-0x50]=h11`. The neighbor indices at `0x0046e110–0x0046e229` and height
loads at `0x0046e249–0x0046e28b` establish that mapping. Stack `[EBP-4]=L`,
`[EBP-0xc]=fx`, `[EBP-0x10]=fy`, with `L=1<<s`. These are coordinates after
the existing bitmap row reversal. Neither input is flipped again.

Define `I(v)` as signed interpretation of the low 32 bits and
`T(a,b,u) = I(I(a-b)*u) >> s`. The right shift is signed arithmetic (`SAR`),
so negative products round **down**, not toward zero. Define
`A(h,t1,t2) = I(I(t1+h)+t2)` to preserve the two additions.

| Mode and condition | Exact result | Arithmetic instructions |
| --- | --- | --- |
| 2, `fx <= fy` | `A(h01, T(h11,h01,fx), T(h00,h01,L-fy))` | `0x0046e29a–0x0046e2c5` |
| 2, `fx > fy` | `A(h10, T(h00,h10,L-fx), T(h11,h10,fy))` | `0x0046e2cc–0x0046e2f4` |
| 0, `L-fx >= fy` | `A(h00, T(h10,h00,fx), T(h01,h00,fy))` | `0x0046e30c–0x0046e32e` |
| 0, `L-fx < fy` | `A(h11, T(h01,h11,L-fx), T(h10,h11,L-fy))` | `0x0046e332–0x0046e362` |
| 1 | `A(h00, T(h10,h00,fx), T(h01,h00,fy))` | `0x0046e366–0x0046e388` |

Mode 2 uses signed `CMP`/`JG` at `0x0046e295–0x0046e298`; equality takes
the first branch. Mode 0 uses signed `CMP`/`JL` at `0x0046e307–0x0046e30a`;
equality also takes the first branch. Mode 1 reaches `0x0046e366` through
`TEST`/`JNZ` at `0x0046e2fb–0x0046e2ff`.

Each `SUB`, `IMUL` and `ADD` retains its low 32 bits. Single-operand `IMUL`
at `0x0046e327`/`0x0046e381` produces a high half in EDX, but the following
code uses only EAX, matching the other low-half products. There is **no IDIV**
in interpolation: the power-of-two divisions are the separate `SAR`s. The
HUD's quantized metre conversion is unrelated and is not applied here.

Rearranging a triangle into another mathematically equivalent form can change
the separately rounded terms. A regression fixture with mode 2, heights
`(0,0,2,1)`, `L=4`, `fx=fy=1` gives **-1** through the original instructions;
an alternative barycentric arrangement gives 0. The sampler neither clamps
to corner extrema nor replaces the result with floating-point interpolation.

## Domain and unresolved cases

`sample(x,y)` admits only `0 <= x < (width-1)*L` and the corresponding Y
interval. It obtains cell indices with `x>>s`, `y>>s` and subtracts the cell
origin to obtain offsets. An exact internal grid line selects the next cell,
as the original shifts do. Negative coordinates and outer strips are rejected,
not clamped, wrapped or assigned a fabricated zero height.

For tests, `cell(i,j,fx,fy)` explicitly selects an interior cell and admits
local offsets **0..L inclusive**. This evaluates that cell's equations at all
four endpoints. It does **not** claim that a global query at the last sample
row/column uses that cell: such global queries remain rejected. Thus all
discrete vertices can be compared without inventing an outer-boundary policy.

Mode 3 is rejected, even though the original consumer treats it like mode 1;
the known selector does not produce it. The inventory's original positive,
negative and zero comparison rules remain unchanged. Higher flag bits,
mission preprocessing (`0x00471190`), mutations, zone origins/joins, outer-strip
rendering and collision policy remain outside scope. See
[diagonal selection](terrain-diagonal-selection.md) for the observed original
flattened-index boundary behavior. Vertical metres remain independently
unverified; all outputs are internal units.

## Validation

```sh
python3 tests/zone_triangle_tests.py
cmake -S . -B build
ctest --test-dir build -R '^(zone_heightmap|zone_cell_modes|zone_triangles)$' --output-on-failure
```

The new **14 synthetic tests** cover positive/negative/zero sample differences,
every integer point in three small cells, all corners/edges, both diagonal
equalities and adjacent branches, unsigned bytes, negative multipliers,
separate rounding, wrapped subtraction/multiplication/addition, supported shift
limits, addressing, input consistency, boundaries, provenance and file preservation.
All **46 tests passed**: these 14 tests plus the existing 21 heightmap and
11 cell-mode tests, registered as three CTest targets.

A separate bounded evaluator read the saved Ghidra instruction sequence
`0x0046e28d–0x0046e388`, modeled its registers/branches/32-bit operations, and
compared **4,500 deterministic synthetic cases**, spanning modes 0/1/2 and
shifts 0/1/2/14/30, with **zero differences**. Its script and summary are kept
under ignored `imported/terrain-validation/triangle-check-pm1ksk49/`. This is
an independent arithmetic cross-check against retained static evidence, not
execution of the original binary or a general-purpose x86 emulator.

For the existing ZONE111 grid:

| Check | Result |
| --- | --- |
| Explicit cell corners | 64,516 comparisons; **0 mismatches** |
| Unique vertices covered by those corners | All 16,384 |
| Global vertex queries within the supported domain | 16,129 comparisons; **0 mismatches** |
| Sample report | 15 interior/diagonal points across modes 0, 1 and 2; all five branch addresses exercised |

The report is saved at Git-ignored
`imported/terrain-validation/zone111-triangle-samples/report.json`, including
both input hashes. Extreme synthetic overflow can break vertex equality; the
diagnostic reports such mismatches rather than concealing them. ZONE111's
tested values do not have that problem.

No original executable was run, and no original-game visual or live gameplay
verification is claimed. Original resources, existing grid/mode artifacts and
the C++ runtime remain unchanged. This task adds only the sampler, focused
tests, a CTest entry and documentation; no runtime dependencies were introduced.

## Smallest renderer experiment

Next, render one small interior patch (for example 8×8 cells) in an **opt-in
test harness**, using the existing vertices, internal spacing and verified
diagonals. Overlay a few sampler-height markers to check orientation and
surface alignment. Keep that experiment separate from mission terrain,
collision, camera and cockpit behavior, and omit outer strips. No renderer
changes have been made here.
