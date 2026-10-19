# DTS skeletal animation inventory

Implemented 2026-10-18 following [the reference-only plan](reference-implementation-plan.md).
`tools/inspect_dts_animation.py` promotes the existing local
`imported/herc-model-audit/inspect_dts.py` traversal into a bounded, standalone
Python inspector. It retains the known animation layouts, skips mesh bodies,
and adds hierarchy/reference checks, source offsets, hashes and readable reports.
The scratch script, native decoder and original assets are unchanged.

## Usage

Python standard library only; no Java, Maven, extraction or rendering step:

```sh
python3 tools/inspect_dts_animation.py assets/original/models/TOMAHAWK.DTS assets/original/models/ACHILLES.DTS
mkdir -p imported/dts-animation-inventory
python3 tools/inspect_dts_animation.py assets/original/models/TOMAHAWK.DTS assets/original/models/ACHILLES.DTS --format json --output imported/dts-animation-inventory/new-inventory.json
python3 tests/dts_animation_tests.py
ES2_DTS_ORIGINAL_DIR=assets/original/models python3 tests/dts_animation_tests.py
```

Default output is text on stdout. `--format json` emits a list of model inventories
(schema version 1); `--output` exclusively creates a **new** file after all inputs
pass validation. Existing destinations, originals and symlinks are refused.
Failures name the input, top-level record and hexadecimal byte offset, and exit
nonzero. Limits follow the native proof scope: 4 MiB/file, 16 top-level records,
32 nesting levels, 64 bones/sequences, 1,024 frames and 8,192 transforms.

Full reports generated locally are `imported/dts-animation-inventory/inventory.txt`
and `inventory.json`. These contain extracted tables and must stay private and
ignored, alongside the already ignored original model files. This document
contains structural observations, not a distributable asset conversion.

## Confirmed source structures

All multibyte values are little-endian. These layouts agree with the scratch
inspector and existing native reader and were checked against both local files:

| Structure | Inventory representation |
| --- | --- |
| Tagged object | u32 tag, u32 payload length excluding the eight-byte header; exact parent/file bounds |
| Graphical part prefix | i16 bone reference, i16 part ID, four retained i16 bounding words; bounds are **not** a pose translation |
| ANShape `0x001e0003` | Nested part containment, two counted raw word maps, tagged animation list |
| ANAnimList `0x001e0002` | Counted tagged sequences, four-word transition rows, six-word transforms, default indices and parent/child pairs |
| Transform | Six i16: rotation XYZ then translation XYZ; raw units preserved |
| Skeleton | u16 default-transform index per node; signed i16 parent/child relations; parent -1 means a recorded root |
| ANSequence `0x001e0001`, ANCyclicSequence `0x001e0004` | u16 duration, two opaque u16 header words, u16 frame count; frames contain tick duration and two opaque words |
| Sequence bindings | u16 part count/list of node indices, then frame-major u16 transform references; frame count × part count entries |

Counts and reference indices use unsigned words, including values above 32767;
rotations, translations and parent links use signed words. Opaque words are
retained as unsigned bit patterns; no signed interpretation or meaning is
assigned. JSON includes table offsets, ends, counts, strides and struct formats.
Sequence IDs are zero-based table positions; no clip names were recovered.

The report separates **object containment** from the **animation skeleton**.
Part IDs are not skeleton node indices. Objects are linked back to their numeric
bone references, including markers and alternate cell children. Sequence motion
classification compares actual transform values, not just differing indices:
`varying_rotation`, `varying_translation`, `held`, and `unlisted`. A held local
pose can differ from the default; unlisted nodes have no samples in that sequence.
Either can still move through its parent. This does not evaluate world poses.

## Human/Cybrid inventory

Read locally: `assets/original/models/TOMAHAWK.DTS` (human, 70,556 bytes) and
`assets/original/models/ACHILLES.DTS` (Cybrid, 75,642 bytes).

| Top-level record | Tomahawk: nodes / sequences / transforms / transitions | Achilles: nodes / sequences / transforms / transitions |
| --- | --- | --- |
| 0 | 12 / 8 / 233 / 26 | 12 / 8 / 239 / 26 |
| 1 | 12 / 1 / 20 / 0 | 12 / 1 / 22 / 0 |
| 2 | 12 / 8 / 233 / 26 | 12 / 8 / 239 / 26 |
| 3 | 12 / 1 / 20 / 0 | 12 / 1 / 22 / 0 |
| 4 | 10 / 1 / 17 / 0 | 10 / 1 / 18 / 0 |
| 5 | 8 / 1 / 14 / 0 | 8 / 1 / 14 / 0 |
| 6 | 7 / 1 / 12 / 0 | 8 / 1 / 14 / 0 |

Seven ANShape records consume each complete file. They are reported separately;
their exact original selection/LOD roles are not established. Records 1, 3–6
omit a relation for node 0. The inspector retains `parent: null` for that node;
it does not adopt the runtime's inferred root. Other missing relations, invalid
parents/children, duplicate children and cycles are errors.

The record-0 skeletons have identical parent-child relationships:

```text
root: 0
root: 1
      1 -> 2 -> 9 -> {5, 6}
      1 -> 3 -> 10 -> {7, 8}
      1 -> 4 -> 11
```

This supports two articulated leg branches and an upper-body branch, consistent
with the existing renderer's use of these nodes. Anatomical labels remain
interpretations: MDK's `DTSBoneId` labels apply to **part IDs**, not directly to
these node numbers, and contain unknowns. The report therefore uses numeric
identifiers rather than inventing bone names.

Record-0 sequence observations (same frame counts/ticks in both files):

| ID | Tag class | Frames / ticks | Observed local changes |
| --- | --- | --- | --- |
| 0 | Cyclic | 8 / 800 | Node 4 rotation; node 0 held |
| 1 | Cyclic | 8 / 800 | Leg branch rotation and translation, plus nodes 0/1 translation |
| 2 | Cyclic | 10 / 1000 | Leg rotation; Tomahawk translates nodes 0/1/5/6/7/8, Achilles only 0/1 |
| 3, 4 | Cyclic | 2 / 200 each | All listed nodes held within each sequence |
| 5 | Cyclic | 8 / 800 | Node 11 rotation; node 0 held |
| 6 | Cyclic | 7 / 700 | Node 1 and leg rotation; selected leg/node 1 translations |
| 7 | Noncyclic | 7 / 700 | Node 1 and leg rotation/translation |

The existing game uses sequence 1 for gait, but that alone does not prove an
original clip name, rate or interpolation behavior. Reduced records each have
one two-frame, 200-tick noncyclic sequence. Complete frame indices, transforms,
object trees and per-sequence motion classifications are in the local reports.

## Unknowns and reference differences

- No original tick-to-seconds rate, physical translation scale or verified
  original rotation order was recovered. The native renderer's angular and
  world-scale conventions are not promoted to format facts here.
- MDK calls the two sequence header words `priority` and `groundMovement`.
  It calls the two frame words `firstTransition` and `numTransitions`. In these
  files, the latter pair resembles **count followed by start**, contrary to the
  Java names (for example, sequence 1 has `[3, 0]` then later `[1, 9]`). This is
  a hypothesis only. No transition ranges or behavior are inferred from it.
- Transition rows retain all four words. MDK's tick/destination/ground-movement
  names are reference hypotheses, not validated state-machine semantics.
- Shape maps are retained raw. Achilles record 0 has ten sequence-map words but
  eight actual sequences, so map entries are not validated as sequence indices.
- Known part containers are traversed; BSP and detail tails retain raw values.
  Mesh bodies are bounded opaque spans. Unsupported tags outside those spans
  fail explicitly. The successful inventory does **not** certify every polygon,
  material, embedded mesh record or arbitrary DTS variant.
- Toolkit's DTSExtract calls `writeRawJson(dts.getMeshes().iterator().next(), ...)`
  inside its export loop. This report reads each record independently; a fixture
  with different records checks against accidentally repeating the first one.
  Java exporters were not run and output equivalence is not claimed.

## Verification

20 synthetic tests passed; the optional local-original test also passed for both
HERCs (21 tests total when enabled). Fixtures cover signed/unsigned boundaries,
raw negative bit patterns, different top-level records, motion classification,
hierarchy errors, invalid references, unknown tags, count/depth limits, every
truncation of the small fixture, parent boundaries, frame timing, container
tails and read-only CLI behavior. CLI errors were also exercised with `python -O`.

Original checks independently unpack top-level headers and table spans using
`struct`, check count words, frame-major lengths and exact file consumption,
then compare known native metadata: 12 nodes/eight sequences in record 0.
No expected output is taken solely from the new inspector or scratch JSON.
Before/after SHA-256 hashes match:

```text
TOMAHAWK 907e682bdbfa6ed06ace15ee9758dab413edaa00b707be3e9751c8eb1f102115
ACHILLES b230f8f90497c2eb214e30d58d5fa5b2b049263b9eb23802d680231b8360f894
```

This verifies structural inventory, not original-game playback or visual motion.
No rendering session, animation playback, engine build or Java execution was
performed for this task. Existing source, assets and extraction tools are unchanged.

## Smallest next playback experiment (not implemented)

Add a separate, manually frame-stepped skeleton preview for **record 0, sequence
1 only**, covering both HERCs. Display both roots, use recorded defaults for
unlisted nodes, and apply the existing native transform convention explicitly
as an experimental assumption. Compare each frame's node positions with the
native decoder before adding interpolation or elapsed-time playback. This isolates
pose/hierarchy validation from the runtime's existing gait remapping and ground
correction. Leave transitions, clip naming and gameplay integration for later.

## Source attribution

The local scratch traversal is reused and narrowed; the existing native parser
provides bounds/reference and frame-duration checks. Related MIT reference
implementations consulted at fixed commits:

- [MDK `DTSModelTransformer.java`][mdk]: `readTSShape`, `readANSequenceFrame`,
  `readANSequence`, `readANCyclicSequence`, `readANAnimListTransition`,
  `readANAnimListTransform`, `readANAnimList`, `readANShape`. Their table order
  and layout inform the Python adaptation. Other context: `data/file/dts/anim/`,
  `core/data/ref/constants/DTSBoneId.java`, and TransferApi's `DTSServiceImpl`.
  Full **Copyright (c) 2023 Subject9x** MIT notice is retained in
  [licenses/herc-works-mdk-MIT.txt](licenses/herc-works-mdk-MIT.txt).
- [Toolkit `ExtractDTS.java`][toolkit], and `DTStoObj.java`: inspected for export
  limitations only, with no Toolkit code copied. Toolkit carries Copyright (c)
  2024 Subject9x, MIT. It delegates DTS parsing to MDK; it is not an independent
  corroborating parser.

No upstream repository, build system, dependency or original game data is vendored
by this change. These licenses do not grant rights to original game assets.

[mdk]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/io/transform/dbsim/DTSModelTransformer.java
[toolkit]: https://github.com/Subject9x/es2-toolkit/blob/f0f79a58bfac374c1114c796eef69918a742cb46/DTSExtract/src/main/java/org/es2tlk/dts/ExtractDTS.java
