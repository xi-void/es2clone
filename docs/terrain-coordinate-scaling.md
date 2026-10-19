# Original terrain coordinate scaling

Static analysis performed 2026-10-18; report completed 2026-10-19. REA's
Ghidra 12.1.4 provider analyzed the local PE32/i386 `DBSIM.EXE`, SHA-256
`33600748810578667ca513841406bbb4f4990eee463a8a041ea09ebd12730df3`.
Addresses below are virtual addresses at image base `0x00400000`.

**Confirmed:** ZONE111's horizontal sample spacing is **16,384 internal units**
on both axes. Its third DAT word, **14**, is the coordinate shift exponent.
The navigation display converts internal distance with
**`6 * trunc_toward_zero(distance / 1000)`**, followed by the literal ` M.`.
The nominal horizontal scale is therefore **0.006 metres per internal unit**,
giving **98.304 metres between samples**. The display itself is quantized to
six-metre increments and uses an approximate horizontal distance.

Terrain heights and object altitude share an internal Z coordinate scale.
Applying the horizontal metre ratio to Z is a supported **interpretation of
their common world coordinate system**, not an independently verified numeric
altitude readout. The inspected altimeter is graphical. This distinction matters
when using these findings outside the original display code.

The executable was never executed. No engine, resources, parser, reconstruction
utility, or tests were changed. Saved evidence from the earlier height analysis
was reused; this session followed only terrain consumers and navigation/range
display paths. Raw responses and the closed-session snapshot are under ignored
`imported/terrain-coordinate-analysis-zs31g7fv/`, not intended for publication.

## 1. Grid coordinates and sample spacing

The established ZONE loader `0x0042789c` reads successive DAT words. Its store
at `0x00427a09` puts the third word in grid field `+0x108`; `0x00427a24` puts
the fourth word in `+0x118`. For ZONE111 these are 14 and 200. [E1]

The height-query function `0x0046e07c` supplies the missing meaning of `+0x108`:

| Instructions | Observed operation |
| --- | --- |
| `0x0046e08b–0x0046e093` | Load `grid+0x108`; load position X; `SAR EDX,CL` |
| `0x0046e095–0x0046e09e` | Load the same shift; load position Y; `SAR EBX,CL` |
| `0x0046e0d1–0x0046e0de` | Construct cell width `L = 1 << shift` |
| `0x0046e0e1–0x0046e107` | Shift indices back and subtract from X/Y to obtain within-cell offsets |
| `0x0046e0a0–0x0046e0cc` | Reject negative/out-of-range indices and return zero |

Thus, for valid interior positions and this shift:

```text
s = 14
L = 1 << s = 16384
i = arithmetic_shift_right(x, s)
j = arithmetic_shift_right(y, s)
fx = x - (i << s)
fy = y - (j << s)
```

The operations use 32-bit coordinates; `SAR` is signed, and x86 takes its shift
count from the low five bits of CL. This establishes ZONE111's shift, not a
validation policy for arbitrary DAT values. [E2]

An independent geometry-producing path, function `0x0046d344`, agrees:
`0x0046d95c` and `0x0046d96d` shift the X and Y indices left by `grid+0x108`.
`0x0046d97d–0x0046d985` zero-extends the sample byte, multiplies by `+0x118`,
and adds base `+0x110`. `0x0046d99d–0x0046d9b7` adds the three components of
the grid origin at `+0xf4/+0xf8/+0xfc` to the resulting XYZ vector. There is
no extra per-axis scale in this vertex-construction block. [E3]

This resolves the third DAT word left deliberately uninterpreted by
[the discrete height reconstruction](zone111-heightmap-validation.md). Its raw
heights and row reversal remain correct and were not changed.

## 2. Trace to the displayed metres

The navigation drawing function is `0x0043c3e4`. Its association with
`HNavGauge` is supported by the type record at `0x0043e367` (name at
`0x0043e387`) and adjacent table data at `0x0049c148`, containing that type
pointer and function pointer `0x0043c3e4`. The function's own behavior supplies
the stronger evidence: it retrieves a route point, draws a bearing marker,
formats a point number and distance, and appends ` M.`. [E4, E5]

The relevant value flow is:

1. `0x0043c49b–0x0043c4d8`: call `0x00423b0c` using the route index plus one;
   copy the returned point's first two 32-bit coordinates into gauge `+0x3c/+0x40`.
   Route lifecycle and arrival handling were not investigated.
2. `0x0043c4f4–0x0043c50a`: copy the current object's X/Y at `+0x26/+0x2a`,
   then call `0x0047d588` with the two coordinate pairs.
3. `0x0047d588` calls `0x00492924`, which subtracts the two X/Y components
   (`0x0049292d–0x00492934`). It then calls `0x00492939`, through
   `0x0049298e`, to `0x0047dd40`. No unit conversion occurs in these wrappers.
4. `0x0047dd40` forms signed absolute coordinate differences and returns
   `max(abs(dx),abs(dy)) + (min(abs(dx),abs(dy)) >> 1)`.
   The absolute-value operations are at `0x0047dd44–0x0047dd54`, comparison
   at `0x0047dd56`, and half/minimum plus maximum at `0x0047dd5a–0x0047dd62`.
   This is an integer approximation, not a Euclidean square root. Z is absent.
5. `0x0043c71d–0x0043c727` passes that result to `0x00434228` and stores
   the converted value.
6. `0x0043c78c–0x0043c790` passes it to decimal `_itoa`;
   `0x0043c7a8–0x0043c7b1` appends the suffix at `0x0049c0fc`, whose bytes
   are `20 4d 2e 00` (` M.`). The assembled text reaches the drawing helper
   `0x00438920` at `0x0043c7c1`. [E4, E6–E8]

Conversion function `0x00434228` is particularly small and unambiguous:

| Address | Instruction and consequence |
| --- | --- |
| `0x0043422b` | Load the 32-bit distance argument into EAX |
| `0x0043422e` | `MOV ECX,0x3e8` — divisor 1000 |
| `0x00434233–0x00434234` | `CDQ; IDIV ECX` — signed division, truncating toward zero |
| `0x00434236` | `ADD EAX,EAX` — double the quotient |
| `0x00434238` | `LEA EAX,[EAX+EAX*2]` — triple that doubled quotient |

Consequently, for ordinary nonnegative distances:

```text
d = max(abs(dx), abs(dy)) + floor(min(abs(dx), abs(dy)) / 2)
displayed_metres = 6 * floor(d / 1000)
nominal_metres_per_internal_unit = 6 / 1000
```

The nominal ratio is useful for representing coordinates; it does **not**
replace the exact original display algorithm. Multiplying first, rounding to
the nearest metre, or substituting a Euclidean norm changes the result. The
32-bit absolute-value/addition paths have their usual overflow limits; the
formula above describes normal world-coordinate differences, not `INT_MIN`.

Examples calculated from these instructions, not observed gameplay:

| `dx`, `dy` (internal) | Approximate distance | Display |
| --- | --- | --- |
| 999, 0 | 999 | `0 M.` |
| 1000, 0 | 1000 | `6 M.` |
| 16384, 0 | 16384 | `96 M.` |
| 16384, 16384 | 24576 | `144 M.` |

The original `STRINGS0.STR` contains the external `WAYPOINT ` label at file
offset `0xad7`. No resource-ID mapping is needed to establish the conversion:
the decoded route-point flow and literal metre suffix already connect it to
the display. No unverified string-resource parser was introduced.

## 3. Independent consumers: range and altitude

The radar/range path provides a second horizontal consumer. Function
`0x0043ebe0` subtracts object and observer X/Y at `0x0043ed54–0x0043ed5d`,
calls the same approximate norm `0x0047dd40` at `0x0043ed74`, and passes the
selected object's distance to `0x00434228` at `0x0043ee71`. The result is stored
at widget `+0x520`. Function `0x0043eecc` formats that field at
`0x0043f256–0x0043f25d` and draws it at `0x0043f2a0`. It also converts its
configured range through `0x00434228` at `0x0043f1eb`. [E9]

This corroborates the navigation conversion. It does not establish the
behavior of every other target/MFD distance display.

The altitude consumer is `0x0043dd70`, called by HUD drawing paths
`0x0043d5c8` and `0x0043d6dc`. It:

- Reads current X/Y from object position `+0x26/+0x2a` and passes them with
  the terrain grid to `0x0046e07c` at **`0x0043de13`**.
- Saves terrain height at `0x0043de18`; reads the same object's Z (`+0x2e`)
  at `0x0043de1e`, saving it at `0x0043de26`.
- Uses `grid+0x110` as a lower bound and `grid+0x114 + 0x1388` as an upper
  bound (`0x0043dde6–0x0043ddf5`). The broader meaning of `+0x114` was not
  re-investigated here.
- Maps terrain height to the gauge with signed multiply/divide at
  `0x0043de9b–0x0043deaa`, then clamps to its screen limits.
- Maps object Z with the **same bounds and arithmetic** at
  `0x0043df51–0x0043df60`, then clamps it too. [E10]

Both markers therefore use the same internal vertical scale. The expression,
ignoring 32-bit intermediate overflow, is
`screen_bottom - trunc((z - lower) * pixel_span / (upper - lower))`.
This is a mapping into screen pixels, not a metre conversion. The later signed
division by **50** at `0x0043e060–0x0043e06d` feeds a remainder-based repeating
bitmap offset (`0x0043e072–0x0043e085`); treating that divisor as units per metre
would be incorrect.

Another direct height consumer, `0x004253d8`, calls the query at
`0x00425784` with an XYZ position and writes its return directly into the Z
slot at `0x0042578c`; a signed 16-bit object-definition offset is subsequently
added at `0x004257aa–0x004257ae`. No metre conversion intervenes. [E11]

**Axis conclusion:** X/Y use equal internal spacing. Sample-byte-to-Z scaling
is separately controlled by DAT word four, while grid-index-to-X/Y scaling is
controlled by word three. The vertex builder, direct Z assignment and altitude
gauge support a common XYZ world coordinate basis. A separate numeric
altitude-to-metres display was not recovered, so the vertical metre values below
are derived using that common-scale interpretation, not a second measured ratio.

## 4. Interpolation is present

`0x0046e07c` fetches four neighboring cell heights and performs **piecewise
planar triangular interpolation** using 32-bit integers. It is neither nearest
sample nor bilinear interpolation. [E2]

Let `h00`, `h10`, `h01`, `h11` be the heights at `(i,j)`, `(i+1,j)`,
`(i,j+1)`, `(i+1,j+1)`. All four are unsigned sample byte × multiplier + base
(`0x0046e24c–0x0046e286`). Cell byte `+0x0f`, masked with 3 at
`0x0046e283–0x0046e288`, chooses the branch:

| Low two bits | Selection evidence | Behavior |
| --- | --- | --- |
| 0 | `0x0046e2fb–0x0046e30a` compares `L-fx` with `fy` | Split at `fx+fy=L` |
| 2 | `0x0046e28d–0x0046e298` compares `fx` with `fy` | Split at `fx=fy`, the other diagonal |
| 1 or 3 | Branch to `0x0046e366` | Uses the `h00,h10,h01` plane without that triangle-selection test |

For example, bits 0 and `fx+fy <= L` yield:

```text
height = h00 + SAR32((h10-h00)*fx, s) + SAR32((h01-h00)*fy, s)
```

The relevant multiply/shift/add sequence is `0x0046e30c–0x0046e32e`.
Each product is shifted separately; negative `SAR` rounds downward, unlike
signed `IDIV`. Multiplications/additions retain the original low 32 bits.
Other triangles use the corresponding three corner heights, with `L-fx` or
`L-fy` where appropriate. The native arithmetic should be preserved in any
future sampler rather than replaced with floating-point blending.

The source and initialization of the low-two-bit cell flags, their broader
meaning, and the final row/column's neighbor policy remain unresolved here.
Do not invent a diagonal pattern or assume a clamped 127×127-cell surface.
The existing discrete grid contains no recovered flag metadata; it remains
insufficient by itself for exact between-sample playback.

## 5. ZONE111 implications and limits

| Quantity | Result | Status |
| --- | --- | --- |
| Grid dimensions | 128×128 samples | Previously validated DBA decoding |
| Horizontal step | `2^14 = 16384` internal units on X and Y | Confirmed query and vertex arithmetic |
| Nominal horizontal step in metres | 98.304 m | Derived from confirmed display ratio |
| First-to-last sample span on one axis | `127*16384 = 2080768` internal units; nominal 12484.608 m | Arithmetic derivation, not a claim about outer-edge topology |
| Height increment per sample byte | 200 internal units | Confirmed DAT multiplier |
| Discrete elevation range | 10800–31800 internal units | Previously validated original samples |
| Nominal vertical increment/range | 1.2 m per byte; 64.8–190.8 m | Common-XYZ-scale interpretation described above |

The 128-sample index bound in the height query corresponds to a nominal
`128*16384` coordinate interval, but does not prove how the outermost cells
are rendered or joined to neighbors. No north/east convention, full world
tiling, WLD/RMP layout, arbitrary-zone origin policy, or global renderer scale
was inferred. There was no live or visual original-game verification.

## 6. Smallest next implementation step

Extend **only the offline height reconstruction's metadata** to report
`grid_spacing_internal = 1 << DAT[2]` for the verified ZONE111 case and the
explicit nominal metre ratio `6/1000`, retaining the exact raw height grid.
Add focused synthetic checks for the shift and the display's divide-before-
multiply order. Keep nominal geometric coordinates distinct from the
approximate, quantized HUD distance. This is a recommendation, not an
implementation performed in this task.

Before implementing faithful interpolation, the next narrow static question is
which writes establish `cell+0x0f & 3` for ZONE111 and what happens at its final
row/column. There is no need to repeat the unit investigation or change the
renderer to answer that question.

## Evidence index and preservation

These are REA Evidence IDs, not executable symbol names. `FUN_...` names and
field descriptions are analysis labels. Conclusions above use assembly because
the decompiler sometimes loses stack-passed arguments in this binary.
Full generated disassembly/pseudocode remains in the ignored local evidence
directory; this report contains only the relevant manually summarized facts.

| ID | Local response file(s) and REA evidence |
| --- | --- |
| E1 | `prior-function-zone.json`: `ev_aaa32ca5f0b274e712f195bfd2005d0b64d237d54225ebd45e7a7188f54d90bf` |
| E2 | `prior-function-height-query.json`: `ev_241e271f464982dec7696d904a7f2e2504a58672935d7bc6f9810a6079c64b07` |
| E3 | `prior-function-grid-geometry.json`: `ev_9f0dfcf7589d347fe6a8628444170c4cb45cf0ab45d5d2bed979a73fc3960566` |
| E4 | `nav-gauge-43c3e4.json`: `ev_4d20b0e0503490206459d9a21d7cdf79489506d693e708ba07c35563cfa726fa` |
| E5 | `nav-type-record.json`: `ev_fb66d28f12b0dd37df0564273a7559f53243efd94f60f0b9ae61904498d2d136`; `nav-vtable.json`: `ev_e68ba5698c40465152dfbfa93d679f27c3431a5157404307f21458bcbb07babe` |
| E6 | `distance-conversion-434228.json`: `ev_eb0a723e49a476597c9cb3aa9b25d008eb35e8b121adc70e81d0d13f8d4c86f0`; `distance-suffix.json`: `ev_e9f81dde22a3ef76a9a4f7f604a76c705b97cab65326d39f3b383128e361f4e4` |
| E7 | `xy-distance-47d588.json`: `ev_4e2702be41ecd2b0ab8fe22b069505f5645a9a016d5bd08a5b8c852de97bc714`; `xy-subtract-492924.json`: `ev_6ca93daa4284b7dfc078ed7bc8059e3baa4d2c7e4b5896d25b8281ccf589600f` |
| E8 | `xy-length-492939.json`: `ev_bbb480b3b43c32069bb8b8d0be2ea02dad57dbf3b3df73b823a39632c6228d44`; `xy-length-core-49298e.json`: `ev_d2359d1b70e9661378ddbba3851b7c46c6be6c9fd4616ba43e19e6729cf91dfd`; `radar-length-47dd40.json`: `ev_be5330c319f42328c9dd1c02a9afeee06c0f4bcce4721b70eb3971259d0bb9de` |
| E9 | `target-range-43ebe0.json`: `ev_283f6240aae69e37c452b92f797c5ca847358d8bfa6b2d42ab74f1ed3bebd9e7`; `radar-display-43eecc.json`: `ev_914549c8abfe9d1b726effe08b9d31f1628008415f7194d11b6d22254b152580` |
| E10 | `hud-height-caller-43dd70.json`: `ev_3940caa5f7f47dab8af6e3cbb67b5f23609025ad1b146155cdd9e4d473a508cf`; callers retained in `hud-parent-43d5c8.json` and `hud-parent-43d6dc.json` |
| E11 | `height-caller-4253d8.json`: `ev_837e4cfb04a4d3b290b4defe2d2bcceea3f110f400ba49ad9012315aadc8ea29` |

REA closed the target and saved `analysis-snapshot.json` successfully. Hash
comparison against the session baseline verifies preservation of existing
project files and the original executable. The analysis directory is ignored
by Git. Only this report was added to publishable project content; existing
uncommitted work was preserved. No runtime tests were necessary or claimed
for this documentation-only investigation.
