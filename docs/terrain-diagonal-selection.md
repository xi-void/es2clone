# Original terrain cell diagonal selection

Read-only REA/Ghidra 12.1.4 investigation, 2026-10-19. Target:
`imported/from-iso/iso/DBSIM.EXE`, SHA-256
`33600748810578667ca513841406bbb4f4990eee463a8a041ea09ebd12730df3`.
All addresses are virtual addresses at image base `0x00400000`.

**The interior selection rule is established.** Function `0x0046bed8` compares
the sums of opposite **unsigned sample bytes** and writes the low two bits of
the cell's flag byte. It does not use an alternating pattern or random choice.
The final row and column are skipped by this preparation step. Their initial
low bits are zero; the height query has distinct, somewhat irregular boundary
behavior detailed below. No interpolation or diagonal logic was added to the
reconstruction utility or game.

## Cell storage and initialization

The grid contains:

| Location | Observed use |
| --- | --- |
| `grid+0xec` | Pointer to an array of 16-byte cells |
| `grid+0x100`, `+0x104` | Width/height exponents; `W=1<<value`, `H=1<<value` |
| `grid+0xf0` | Separate auxiliary byte array; not the diagonal flag array |
| `cell+0x00` | Unsigned height sample |
| `cell+0x0f & 3` | Interpolation mode |
| Remaining bits of `cell+0x0f` | Other state; preserved by the diagonal writer |

Cell address is `grid.cells + 16 * (y*W+x)`; the preparation function computes
this at `0x0046beed–0x0046bf1e`. Its four samples are current, next row,
next row/column, and next column, loaded at `0x0046bf2a`, `0x0046bf71`,
`0x0046bfc2`, and `0x0046c010`. [D1]

The DBA loader `0x0046c3c0` allocates `W*H` cells of size `0x10`
(`0x0046c413–0x0046c437`) and reverses source bitmap rows as already documented.
It writes sample bytes at `0x0046c4db`. Calls to `0x00470c68` initialize the
**separate** auxiliary array: this helper subtracts the cell-array base, divides
the byte offset by 16, and writes one byte under `grid+0xf0`. [D2, D3]

Allocation does establish zeroed flags, rather than leaving them unspecified:

- Array allocator `0x004965b8` obtains memory through `0x004773e4` at
  `0x004965ef`, then invokes the supplied per-element callback at
  `0x00496633`. The loader supplies callback `0x004714d5` and mode 1.
- `0x004773e4` explicitly calls `_memset(pointer,0,size)` at `0x00477420`
  on its pool path. Its alternate path calls `0x00477370` at `0x0047742b`.
- `0x00477370` calls imported `CW3220.DLL::_calloc(1,size)` at
  `0x00477379`, which likewise supplies zero-initialized storage.
- The complete 14-byte callback at `0x004714d5–0x004714e2` only loads its
  argument, forms two addresses, and returns; it makes no memory writes.
  Ghidra did not recognize it as a procedure. REA `read_bytes` plus a bounded
  `objdump` decode verified its bytes; no function was created or patched. [D4]

The bitmap loader's flag stores at `0x0046c586`, `0x0046c58b` and
`0x0046c5c4` retain bit 1 (`&2`) and assign bits starting at bit 2. They do
not create a diagonal choice from the freshly zeroed bit 1. Those higher-bit
assignments include a random-number call, but **that call does not choose the
diagonal**. Their wider material semantics were not investigated. [D2]

## Exact selector

Use grid coordinates after the established source-row reversal:

```text
s00 = sample[x,   y]
s10 = sample[x+1, y]
s01 = sample[x,   y+1]
s11 = sample[x+1, y+1]
delta = (s00 + s11) - (s01 + s10)
```

At `0x0046c012–0x0046c029`, register clearing and `MOVZX` ensure unsigned
byte interpretation. `SUB EDX,ECX` at `0x0046c02b` computes delta. Its range
is -510..510, so neither sign-extension ambiguity nor 32-bit overflow affects
this decision. These are **sample** sums, before height multiplier/base.

| Condition | Exact branch/write evidence | Stored low bits | Meaning in the height query |
| --- | --- | --- | --- |
| `delta == 0` | `TEST`/`JNZ` at `0x0046c02d–0x0046c02f`; `AND 0xfc`, `OR 1`, store at `0x0046c034–0x0046c03a` | 1 | Coplanar samples; one plane suffices |
| `delta > 0` | Nonzero branch; signed `JLE` at `0x0046c069` is not taken; `AND byte [cell+0xf],0xfc` at `0x0046c06b` | 0 | Diagonal between `(x+1,y)` and `(x,y+1)` |
| `delta < 0` | Nonzero branch; signed `JLE` goes to `0x0046c0b0`; `AND 0xfc`, `OR 2`, store at `0x0046c0b5–0x0046c0bb` | 2 | Diagonal between `(x,y)` and `(x+1,y+1)` |

All cases preserve bits 2..7. This writer does not produce mode 3. It also
stores two sets of 16-bit coefficients at cell offsets `+1..+0xb`; their full
rendering/normal semantics are outside this investigation. [D1]

## Preparation call chain and timing

The observed chain is:

```text
mission setup 0x004253d8
  call 0x00471190 at 0x004263d9, passing grid [0x004a0bf8]
    call 0x0046c1dc at 0x00471250
      call 0x0046c2ec at 0x0046c237 for each cell
        call 0x0046bed8 at 0x0046c325 for interior cells only
```

`0x0046c1dc` iterates `0 <= x < W`, `0 <= y < H`; its second pass calls the
empty function `0x0046c3b8`. `0x0046c2ec` invokes the selector only when
`y < H-1` **and** `x < W-1`, using signed `JGE` exclusions at
`0x0046c30e` and `0x0046c320`. Its edge path writes coefficient vectors
`(0,0,0x800)` at `0x0046c351–0x0046c36d`, followed by writes to bytes
`+0xd/+0xe`; it does not assign `+0xf`. Thus the skipped edges retain low
bits 0 on the freshly loaded path. [D5–D7]

Before this call, `0x00471190` has a conditional preprocessing pass using
separate auxiliary flags read by `0x00470cf4` from `grid+0xf0`. When bit 0 is
set and bit 1 clear (`0x00471202–0x00471208`), it calls `0x00470edc` and
`0x0047101c`; those bodies were not expanded. The diagonal selector reads
whatever sample bytes exist **after** that pass. Do not claim that a bare DBA
inventory reproduces every mission's prepared terrain state. [D7, D8]

The same mission setup function contains an earlier height query at
`0x00425784`. Static addresses and this call chain do not imply that all height
queries occur after diagonal preparation. This report establishes the selector
and its setup path, not every runtime scheduling or terrain-mutation path. [D9]

## Height-query branch conditions

The existing evidence for `0x0046e07c` independently ties the flag to the
interpolation equations. With `L=1<<grid.shift`, `fx=x-(i<<shift)` and
`fy=y-(j<<shift)`, it reads `cell+0xf` and masks with 3 at
`0x0046e283–0x0046e288`:

| Low bits | Query branch | Corners used |
| --- | --- | --- |
| 2, `fx <= fy` | `CMP fx,fy; JG` at `0x0046e295–0x0046e298`, fallthrough | `h00,h01,h11` |
| 2, `fx > fy` | Branch to `0x0046e2cc` | `h00,h10,h11` |
| 0, `fx+fy <= L` | Compare `L-fx` with `fy` at `0x0046e307`; `JL` at `0x0046e30a` not taken | `h00,h10,h01` |
| 0, `fx+fy > L` | Branch to `0x0046e332` | `h10,h01,h11` |
| 1 or 3 | Nonzero low bits at `0x0046e2fb–0x0046e2ff`, branch to `0x0046e366` | `h00,h10,h01`, without a triangle-side test |

Here `h` denotes the byte × multiplier + base height. Products retain 32-bit
results and each is arithmetic-shifted separately. The mode-3 behavior is
observed in the consumer; no producer of mode 3 was established. No sampler
was implemented. [D10]

## Boundaries: exact local query behavior

The selector's caller excludes the final row/column, but the query first admits
cell indices `0 <= i < W`, `0 <= j < H`. It returns zero for indices outside
those bounds (`0x0046e0a0–0x0046e0cc`). It then obtains all four neighbors using
**flattened** indices:

```text
h00 index = j*W+i
h10 index = j*W+i+1
h01 index = (j+1)*W+i
h11 index = (j+1)*W+i+1
```

Each pointer is accepted only if its index is in `[0,W*H)`. The guards are at
`0x0046e11e–0x0046e148`, `0x0046e164–0x0046e192`,
`0x0046e1b2–0x0046e1e0`, and `0x0046e1fb–0x0046e229`.
The query returns zero if **any** pointer is null, before reading a mode or
performing interpolation (`0x0046e22c–0x0046e244`). [D10]

For 128×128, these instructions imply:

| Query cell index | Consequence |
| --- | --- |
| `0..126, 0..126` | All four neighbors valid; includes the final complete interior cell `(126,126)` |
| `127, 0..125` | `h10` aliases column 0 of the next row; `h11` aliases column 0 of the row after that; all pointers pass |
| `127,126` | `h11` index 16384 is rejected; returns zero |
| Any column, row 127 | `h01` is outside the array; returns zero |
| Negative index or index >=128 on either axis | Rejected by the initial guards; returns zero |

This is flattened-index aliasing, not evidence for intentional geographic
wrapping. It is not clamping. The fresh-load right-edge mode is 0 as traced
above, but exact renderer clipping, neighboring-zone joins, gameplay restrictions
on reaching these strips, and later mutations were not investigated. The
128×128 **discrete** grid remains valid even though its last row is not usable
as a query-cell origin. A new implementation must not silently substitute an
invented edge rule.

## Validation, limits, and next step

A read-only arithmetic tally over the existing reconstructed ZONE111 samples
found 16,129 interior cells: **6,619 positive-delta**, **6,654 negative-delta**,
and **2,856 zero-delta** cases. This checks the formula's application to the
available data, not actual original-game execution or equivalence after mission
preprocessing. No original executable or DLL was run.

The separate spacing change passed **21 Python heightmap tests**, **52 native
graphics checks**, and **6 graphics-importer tests**. These tests verify the
offline utility; they do not execute the original diagonal selector. Its
evidence is the static instruction and call-chain analysis above.

The smallest next implementation is an **offline interior-cell mode inventory**:
derive 0/1/2 from the four unsigned samples using the verified comparison, test
positive/negative/zero cases and values above 127, and keep edge cells explicitly
separate. Preserve original samples and upper flag bits as unknown rather than
inventing materials. Defer triangle sampling and rendering until that focused
inventory is independently checked. No further engine investigation is needed
to implement that small task.

## Evidence and scope

Complete responses, a closed-session snapshot, the callback's bounded byte
decode, and arithmetic-only checks are retained under Git-ignored
`imported/terrain-diagonal-analysis-ary0e4gg/`. A bounded `objdump` search within
the already identified terrain code located flag writes; the selector and
callers were then confirmed through REA/Ghidra. No annotations, source patches,
original assets, or generated decompiler output were added to tracked content.

| Reference | REA Evidence IDs (response filenames in the ignored directory) |
| --- | --- |
| D1 | `diagonal-writer-46bed8.json`: `ev_d92ebb28b59a7487416a2213b6a8cdf8f11a6e771f7112f1e54ace6c306231f2` |
| D2 | `bitmap-grid-loader-46c3c0.json`: `ev_910f9eee4a522c0ad58182d8c3f8a991800a2d09c7e9fc5914931838d4fd580b` |
| D3 | `cell-aux-470c68.json`: `ev_8e6c5fd685ed62b5fa8fe9b87d2af8bd2bb002787138a0d2347c055ce4adad6d` |
| D4 | `array-allocator-import.json`: `ev_3b34cca6fa8e09f418942a4279b3a755d9eae26aba60d77aa2449774f30c89b6`; `cell-memory-4773e4.json`: `ev_d0edfd7666229b316815a413b84da9b46e88f1a7167373e5240f7efb3b14757b`; `cell-memory-fallback-477370.json`: `ev_fa40ce6da15a208425c6881ba6056f5d590628878798267481ac0f95b70dfc6f`; `cell-initializer-bytes.json`: `ev_d846c175b6a4104f76141429b0d9ba825ab3660faa8604d43ce61c29eac70bbf` |
| D5 | `cell-preparation-46c2ec.json`: `ev_fa0ea4b0217c13c5e45f85e62460a259a7f5c3728b972d4a6c986e64484ac6aa` |
| D6 | `grid-preparation-46c1dc.json`: `ev_ede7b4c3e84742c79b9c463b2bddad7fbe37d2c4aa7faae1f107b00076966856` |
| D7 | `prepare-wrapper-471190.json`: `ev_5167ce3f9db2f549add41487bddd408846323dccb14410e831d504faf724f6d0` |
| D8 | `aux-flags-read-470cf4.json`: `ev_098de28906a90a6bf21b5ba23b872aa7856852b9b15c35be1a1187d6ad938c75` |
| D9 | `mission-preparation-4253d8.json` (reused): `ev_837e4cfb04a4d3b290b4defe2d2bcceea3f110f400ba49ad9012315aadc8ea29` |
| D10 | `height-query-46e07c.json` (reused): `ev_241e271f464982dec7696d904a7f2e2504a58672935d7bc6f9810a6079c64b07` |
