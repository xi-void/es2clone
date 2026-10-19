# Tomahawk DAT → JSON exporter audit

Assessed 2026-10-18 against Herc Works MDK commit
`1f66699118d4c0032259e8db708b5bfed50f44c7`, following
[the tooling assessment](external-es2-tooling.md) and
[MDK investigation](herc-works-integration.md).

**Verdict:** suitable as a candidate offline inspection tool; not yet a validated
data-import contract. Java, javac and Maven remain unavailable on PATH. Nothing
was installed. This audit checked source and local DAT bytes, not executed JSON
output. No engine, asset, configuration or gameplay changes were made.

## Export path and supported inputs

There is no dedicated Tomahawk exporter. The existing path is:

`CommandLineMain` → `SimDatExportProcessor` → `GenericJsonProcessor.exportJson`
→ `HercSimDataTransformer.bytesToObject` → `HercSimDataDTOServiceImpl.convertToDTO`
→ Jackson `ObjectMapper.writeValue`.

Relevant source paths under the pinned repository:

| Module / Java package root | Files |
| --- | --- |
| `ES2Excavator/src/main/java/org/hercworks/extract/` | `exec/impl/SimDatExportProcessor.java`, `exec/GenericJsonProcessor.java`, `util/impl/SimFileMatcher.java`, `util/SimFileNames.java` |
| `ES2Core/src/main/java/org/hercworks/core/` | `io/transform/dbsim/HercSimDataTransformer.java`, `io/transform/ThreeSpaceByteTransformer.java`, `data/file/dat/sim/HercSimDat.java`, `data/struct/herc/HercLUT.java` |
| `ES2TransferApi/src/main/java/org/hercworks/transfer/` | `svc/impl/dbsim/HercSimDataDTOServiceImpl.java`, `dto/file/sim/HercSimDatDTO.java`, `dto/file/TransferObject.java` |

The simulation dispatcher recognizes the HERC names in `HercLUT` (including
Tomahawk and Achilles), explicitly skips Razor DAT, and sends the remaining 21
listed names through the same parser. This establishes dispatch coverage, not
correctness for all 21 layouts. Separate DAT exporters handle BEAM, PROJ,
BULLETS and ROCKETS. GL, DMG, PDG and FM use other transformers. Several recognized
simulation names, including MAT0, SHADES and simulation WEAPONS, have empty cases.

Shell DATs use a separate processor: HERCS, HERC_INF, ARM_*, ARM_WEAP, RPR_*,
INI_*, *_HOTS, CAREER, TRN_HERC and shell WEAPONS. Zone DATs are not HERC records.
Filename dispatch uses substring matching, not content signatures; arbitrary
`.DAT` files must not be passed as equivalent inputs. [Dispatch source][dispatch]

The offline dependency chain is Excavator → Core/TransferApi → ES2Vol, with
bytes 1.6.1, Jackson XML 2.16.0 and Commons CLI 1.6.0; Core also declares Kaitai
runtime 0.10 and Commons Math 3.6.1. See the existing
[license/dependency audit](external-es2-tooling.md#licensing-and-dependencies).
Keep these tools outside the game build/runtime and retain Subject9x's MIT notice
for any reused code or distributed utility.

## Local record validation

Input: `assets/original/models/TOMAHAWK.DAT`, 216 bytes, SHA-256
`1140c0dbeab971b72264767280162049178517914ab242cab0da66b3cf790d13`.

A read-only Python check independently accounted for the reader's entire layout:
**one fixed record**, 56 scalar signed little-endian 16-bit fields, one 20-byte
array, one 12-short array, two 12-byte strings, and 36 skipped bytes. There are no
record-count or version fields. All spans fit exactly without overlap or trailing
bytes. This verifies the layout against this input, not the Java implementation.

| Byte offsets (decimal, inclusive) | Exported content / checked examples |
| --- | --- |
| 0–9 | `speed` map: turn 600, reverse −102, forward 240, acceleration/deceleration 30, turning deceleration 150 |
| 10–25 | Camera bone 5; walk/run/stop/torso-pitch IDs 1/2/3/4; origin adjustment, unknown value and AI aim offset |
| 26–45 | `torso_config` flags/rotation/pitch and movement-animation value; rotation maximum raw 14000, pitch minimum raw −4000 |
| 46–65 | `model_lod_boneIds`: 20 signed integers; source byte 255 becomes −1 |
| 66–85 | Unknown values, leg/debris/flyer flags, proposed HUD value and related fields |
| 86–97 | `name`: source bytes contain `Tomahawk` followed by NUL padding |
| 98–123 | Camera/shadow/unknown fields, excluding skipped bytes 102–103 and 106–107; offset 114 is signed −254 / unsigned 65282 |
| 124–147 | `unk124_all500`: 12 shorts, each 500 in this input |
| 148–157 | `modelSkinId` = 1 and four unknown fields |
| 170–175, 190–197 | Further unknowns, `shieldMaxTotal` = 3500, friction and acceleration factor |
| 204–215 | `debrisFile`: source bytes contain `toma_deb` followed by NUL padding |

Skipped spans are 102–103, 106–107, 158–169, 176–189 and 198–203; all are zero
in this file. The Java reader skips them and its writer reconstructs zeros.
[Reader/writer source][reader]

## JSON contract and confirmed source limitations

The DTO annotations specify `classDef: "HercSimDat"`, `fileName`, maps, arrays,
strings and integer properties. `@JsonRootName("simdata")` is present, but the
CLI does not enable root wrapping; a surrounding `simdata` object should not be
assumed. Actual Jackson property discovery and output still require execution.
[DTO][dto], [base metadata][base], [mapping service][service]

- Speed-map values are **strings**, such as `"240"`; torso angle/rate values are
  strings after division by 100, such as `"140.0"`. Most other fields remain
  integers. Camera offsets are not uniformly converted. These transformations
  do not establish physical units for our simulation.
- Walk ID becomes `input_throttle_hercFlag`; run/stop/torso-pitch become
  `unk14_anim_leg1`, `unk14_anim_leg2`, `unk14_anim_leg3`, despite different
  offsets. These stale labels must not be interpreted literally. Several other
  names, such as `maybe_hud_id`, explicitly retain uncertain semantics.
- Byte/short reads are signed even where comments describe unsigned flags.
  Preserve the −1 sentinel and raw bit patterns rather than rejecting or
  clamping negative values. Name/debris strings are NUL-trimmed; padding and
  skipped bytes are not represented in JSON, so generic lossless round-trip
  conversion is not established.
- The reader has no explicit 216-byte length/version check or final-consumption
  check. Short inputs rely on lower-level failures; longer inputs can leave
  trailing data unused. Reverse conversion narrows numbers to byte/short and
  converts float strings by multiplication/casting without range validation.
- `GenericJsonProcessor` catches errors and returns; the top-level CLI can then
  exit successfully. Existing JSON may remain after a failed run. Use a fresh
  output directory and inspect output content, not just exit status.

Source-derived expected output for a bare relative `TOMAHAWK.DAT` argument is
`UNPACK/DAT/TOMAHAWK.json` beside the JAR, with `fileName: "TOMAHAWK"`.
Input paths are also based on the JAR location. Avoid `-s` source-output mode.
The upstream test lists `TOMAHAWK.DAT.json`, but that differs from the current
exporter's filename construction. Neither filename was observed in an executed
conversion during this audit. [Export implementation][export]

## Comparison and existing test evidence

Our `tools/import_original_models.py` checks length 216 and skin at offset 148;
`src/original_models.cpp` uses that skin binding and reads animation/hierarchy
from DTS. `HercShape` stores geometry/transforms/sequences, not MDK DTO metadata.
`Chassis` and `Player` in `src/simulation.hpp` contain prototype floating-point
speed/shield/mass values with no equivalent DAT schema. Raw forward value 240
and shield value 3500 cannot be assigned directly to those fields. No migration
or new JSON loader is justified by this audit.

`TestExports.simExportAll` includes Tomahawk but only invokes the CLI against
external files: it has no field/schema assertions, and the CLI calls
`System.exit`. `TestTransformers` covers a different BIN format.
`TestGamDatFiles` edits other resources; `UniversalJsonTest` exercises shell/DTS
examples, not a Tomahawk DAT correctness test. The previously inventoried tree
contains no committed Tomahawk JSON fixture. These tests establish intended
usage, not conversion correctness. [CLI test source][tests]

## Recommended next implementation task

Add **one standalone DAT/JSON comparison validator** accepting an input DAT and
an externally produced Tomahawk JSON file. It should check the 216-byte contract,
type discriminator, expected property names/types, array counts, signed sentinels,
selected values and torso scaling; reject truncated/extra input bytes and
missing/inconsistent JSON. Test it with synthetic fixtures, including misleading
animation names and unsigned-looking flag bits. Do not implement another exporter.

Once an appropriate Java/Maven environment or a pinned built JAR is available,
run the existing exporter in fresh scratch directories twice and apply that
validator, recording source/JAR hashes and checking semantic determinism. Keep
outputs under ignored `imported/`; test failed conversion with truncated input.
This execution gate remains outstanding. No JSON conversion, Java build,
round-trip correctness or gameplay-unit fidelity is claimed here.

[dispatch]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Excavator/src/main/java/org/hercworks/extract/exec/impl/SimDatExportProcessor.java
[reader]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/io/transform/dbsim/HercSimDataTransformer.java
[dto]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2TransferApi/src/main/java/org/hercworks/transfer/dto/file/sim/HercSimDatDTO.java
[base]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2TransferApi/src/main/java/org/hercworks/transfer/dto/file/TransferObject.java
[service]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2TransferApi/src/main/java/org/hercworks/transfer/svc/impl/dbsim/HercSimDataDTOServiceImpl.java
[export]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Excavator/src/main/java/org/hercworks/extract/exec/GenericJsonProcessor.java
[tests]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Excavator/src/test/java/org/hercworks/extract/cmd/TestExports.java
