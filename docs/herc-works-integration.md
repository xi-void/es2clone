# Herc Works MDK assessment

Assessed 2026-10-18 at commit
`1f66699118d4c0032259e8db708b5bfed50f44c7`.
See [the combined tooling assessment](external-es2-tooling.md) for the comparison
with ES2 Toolkit, shared licensing details, and the single recommended milestone.
This is source inspection and limited read-only checks of already extracted local
resources. No Java tools were built or run, and no engine integration was made.

## Recommendation

Use the MDK as an optional offline inspection/conversion dependency where it
adds supported data exports. Retain our working VOL, bitmap, palette and HERC
readers. Its most immediate additional value is HERC/equipment metadata exported
to JSON. It also supplies useful mission-header leads, but does not provide a
complete terrain decoder.

The pinned README describes a prototype and announces archival. Plan around a
fixed revision, with explicit validation rather than an expectation of ongoing
upstream maintenance. Source declarations, working converters, and verified game
semantics are different levels of support.

## Modules and source inventory

Paths below are relative to the pinned repository. In ES2Core, the Java package
root is `ES2Core/src/main/java/org/hercworks/core/`.

| Module | Relevant implementation | Useful capability and limit |
| --- | --- | --- |
| ES2Vol 0.0.5 | `ES2Vol/src/main/java/org/hercworks/voln/io/VolFileReader.java`, `VolFileWriter.java` | ES2 VOLN archive objects, extraction and writing. The reader records a compression byte and copies payload bytes; it does not implement general decompression. No ISO reader. |
| ES2Core 0.5.0 | `io/transform/common/DynamixBitmapTransformer.java`, `DynamixBitmapArrayTransformer.java`, `DynamixPaletteTransformer.java` | DBM, DBA-family arrays, DPL read/write. Useful for offline inspection; not evidence of terrain semantics or correct palette selection for every resource. |
| ES2Core | `io/transform/dbsim/DTSModelTransformer.java`; `data/file/dts/`, including `anim/` | Early 3Space object trees, polygons, groups, detail parts, transforms and animation records. Supports more object classes than our deliberately limited HERC pair; loading another class does not certify its appearance. |
| ES2Core | `io/transform/dbsim/HercSimDataTransformer.java`, `GunLayoutTransformer.java`, `HercDamageFileTransformer.java`, `FlightModelTransformer.java` | HERC DAT, GL hardpoints, DMG and FM. Movement/animation/skin/camera fields, attachment data and damage metadata are useful leads. Numerous fields remain unknown; CLI explicitly skips Razor DAT. |
| ES2Core | `io/transform/dbsim/BeamDatFileTransformer.java`, `ProjectileDataTransformer.java`, `MissileDatFileTransformer.java`; `io/transform/shell/` | Weapon and shell/hangar definitions. DAT is a family of filename-specific layouts, not one interchangeable format. Several names in the CLI dispatch have empty implementations. |
| ES2Core | `io/transform/common/MissionFileTransformer.java`, `MissionStringFileTransformer.java`; `data/file/msn/MissionFile.java` | Partial MSN entities, coordinates, world/zone candidates and accompanying mission strings. The MSN reader contains mission-specific fixed-size skips, so it is not a general mission parser. |
| ES2Core | `data/file/dat/sim/ZoneDat.java`, `data/file/dbsim/WorldData.java` | Field notes/data-class skeletons, not working terrain converters. No corresponding Zone/WLD transformer or RMP decoder was found in the pinned tree. |
| ES2TransferApi 0.6.0 | `ES2TransferApi/src/main/java/org/hercworks/transfer/svc/impl/dbsim/`; `MissionFileDTOServiceImpl.java` under `svc/impl/` | DTO conversion for HERC definitions, weapons and DTS, usable with Jackson JSON. Mission DTO export exists, but its reverse conversion returns null. DTO coverage must be checked per format. |
| ES2Excavator / artifact ES2UnVol 0.3.0 | `ES2Excavator/src/main/java/org/hercworks/extract/CommandLineMain.java`; `exec/impl/DynamixFileProcessor.java`, `SimDatExportProcessor.java`, `ShellDatJsonExportProcessor.java` | Standalone CLI for VOL extraction, DBA splitting, bitmap/palette previews and selected DAT/GL/DMG/FM JSON conversions. DTS/MSN/WLD/RMP conversion is not wired into this CLI. |

Sources: [VOL reader][vol], [bitmap array reader][dba], [DTS reader][dts],
[HERC DAT reader][hercdat], [simulation CLI dispatch][simcli],
[CLI entry point][cli], [mission reader][mission].

## Comparison with OpenEarthSiege 2

| Existing local implementation | MDK benefit / decision |
| --- | --- |
| `tools/import_original_audio.py`: bounded VOLN reader, safe output paths, uncompressed-entry validation, hashes, inventory and no-overwrite imports | Keep it. The MDK adds archive writing, which extraction does not need; no demonstrated extra compression coverage. |
| `tools/import_original_graphics.py`, `src/original_graphics.cpp`, `tools/graphics_probe.cpp` | Keep the verified DBM/DBA/DPL path. MDK/Toolkit offer broader batch previews and authoring, not an established fidelity improvement. |
| `tools/import_original_models.py`, `src/original_models.cpp`, `src/original_models_render.cpp` | Keep direct DTS/DBA/DPL rendering, hierarchy, gait-driven animation and ground contact. MDK was already a cited format reference in [original-models.md](original-models.md). |
| HERC DAT currently checked for the proof pair's skin binding; original equipment/statistics not integrated | Existing MDK JSON exporters are a useful incremental addition for offline audits. Do not feed guessed units into gameplay. |
| `docs/original-terrain.md`; procedural fallback world | Partial mission-header evidence improves the investigation. Neither a bitmap decoder nor a class named `WorldData` supplies the missing environment renderer. |

Raw extracted supported DTS/DBA/DBM/DPL and PCM WAV resources are compatible with
the current loaders subject to their existing allowlists and validation. MDK
metadata JSON, palette swatches and converted PNGs are inspection artifacts;
the engine has no loader for the MDK DTO schema or for converted original terrain.
There is no reason to add such loaders for this assessment.

## Terrain findings compared with the earlier notes

The earlier [terrain report](original-terrain.md) remains correct about the
runtime blocker, but the following details refine its description:

- **MSN linkage:** `MissionFileTransformer` reads a little-endian 16-bit count
  at offset 2, then that many 14-byte header entries beginning at offset 4.
  After two additional 16-bit values it reads fields named `worldId` and
  `zoneNumber`: byte offsets `8 + 14*n` and `10 + 14*n`. A temporary read-only
  Python check applied only this prefix layout to all 62 local MSN resources.
  All 62 zone candidates match existing zone DBAs. `TRAIN1`: count 2, world
  candidate 1, zone 111; `TRAIN5`: count 12, 1, 555; `C1_01`: count 65, 1, 123.
  Every world candidate was 1, so this check does **not** establish world/day/night
  selection or prove that field's interpretation. It does provide a substantially
  stronger zone-linkage lead than filename resemblance.
- **Full MSN parsing:** the same reader subsequently skips 189 shorts with a
  C1_01 comment, 20 shorts, and 393 shorts; commented alternatives target TRAIN5.
  Do not run it over arbitrary missions and accept all resulting entities.
- **Zone DAT:** `ZoneDat.java` labels offset 12 a nonzero `heightScalar`; offsets
  0 and 4 are described as 7 or 8, and offset 8 as 13/14/15. It has no decoding
  method or scalar application formula. Local checks found offset-8 value 12
  as well. Treat the names as upstream hypotheses pending original-game checks.
- **Dimensions:** powers of the first two DAT values match paired DBA width and
  height in 38 of 39 local pairs. `ZONE7000.DAT` is `(7,7,13,256)`, but its DBA
  is 256×256. This exception prevents promoting the correlation to a validated
  universal rule. The square grids also cannot establish axis orientation.
- **WLD:** `WorldData.java` documents sky-related fields and trailing resource
  names, but is an unfinished class with a TODO, not a byte reader. The earlier
  report's description of it as a reader was too strong.
- **RMP / ground textures:** no RMP decoder, TEX ground-material pipeline or
  terrain triangulation/UV implementation was found. Original height units,
  interpolation, orientation, world bounds and surface assignment remain open.

References: [mission prefix and fixed skips][mission], [ZoneDat][zone],
[WorldData][world]. See the combined assessment for why Toolkit's grayscale
heightmap export does not resolve these gaps.

## Kaitai and conversion cautions

The pinned tree contains one `.ksy`: [docs/kaitai/DynamixBitmap.ksy][ksy].
It references undefined type `chunk` while declaring `bin`; its proposed header
also differs from the implemented Java and our verified 21-byte DBM header.
It is a draft, not a ready-to-generate terrain specification. No Kaitai definitions
for WLD, RMP, zones or missions were found. The declared Kaitai Java runtime
dependency does not imply those parsers exist.

[docs/voln_file_format.txt][volnotes] is similarly exploratory: prefer the current
reader and actual files over its older offset/prefix descriptions.
[PaletteBinding.java][binding] explicitly uses heuristic filename associations.
The palette transformer interprets a flag byte as alpha, whereas our decoder
preserves palette flags and applies cockpit transparency explicitly. Preview
output must not silently replace the verified cockpit or model resources.

## Integration approaches

| Approach | Assessment |
| --- | --- |
| Invoke existing CLI offline | Preferred when the command already exports the needed data, initially one HERC DAT to JSON. No copied parser, no game-runtime dependency. Validate output rather than trusting exit status. |
| Adapt individual parsers | Reserve for a small, validated gap without a usable command. Porting broad Java parser families would duplicate maintenance. Preserve MIT notices for copied/adapted implementation. |
| Reuse documented layouts | Appropriate for bounded inspection such as the MSN prefix. Check actual files and malformed inputs; keep unverified semantics explicitly named as candidates. Do not regenerate parsers from the draft KSY. |

Any future wrapper should use an explicitly pinned external checkout/build,
operate on copies in ignored `imported/`, and record source/tool hashes and
conversion parameters. Keep originals read-only and validate before publishing
anything into `assets/original/`. No build-system/runtime hook is needed.

The Excavator expects inputs relative to the JAR location, with archives under
`VOL/` and output under `UNPACK/`; it has no general output-directory option.
Some processors catch errors and return normally. Run in an isolated scratch
directory, avoid source-output mode, and verify files, schema and source hashes.
See [FileProcessor][paths] and [GenericJsonProcessor][jsoncli].

## Licensing, build status and next milestone

The root [MIT license][license] names Copyright (c) 2023 Subject9x and covers
the repository's source/documentation absent a separate notice. It does not
license original game resources. The [combined assessment](external-es2-tooling.md#licensing-and-dependencies)
records checked dependency licenses and notice requirements. This assessment
copies no upstream implementation into the project.

The POMs declare Java release 9 while retaining Java 8 source/target properties;
the CLI explicitly requires Java 9+. A build must resolve that configuration
with an appropriate JDK. `java`, `javac`, `mvn` and the Kaitai compiler were not
on PATH during this assessment. Upstream tests also refer to unavailable local
game fixtures and Windows paths. No build or converter success is claimed.

The next milestone is the **single-file Tomahawk DAT JSON audit** described in
[external-es2-tooling.md](external-es2-tooling.md#recommended-next-milestone).
Stop at that offline acceptance gate before considering engine consumption.

[vol]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Vol/src/main/java/org/hercworks/voln/io/VolFileReader.java
[dba]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/io/transform/common/DynamixBitmapArrayTransformer.java
[dts]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/io/transform/dbsim/DTSModelTransformer.java
[hercdat]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/io/transform/dbsim/HercSimDataTransformer.java
[simcli]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Excavator/src/main/java/org/hercworks/extract/exec/impl/SimDatExportProcessor.java
[cli]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Excavator/src/main/java/org/hercworks/extract/CommandLineMain.java
[mission]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/io/transform/common/MissionFileTransformer.java
[zone]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/data/file/dat/sim/ZoneDat.java
[world]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/data/file/dbsim/WorldData.java
[ksy]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/docs/kaitai/DynamixBitmap.ksy
[volnotes]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/docs/voln_file_format.txt
[binding]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/util/PaletteBinding.java
[paths]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Excavator/src/main/java/org/hercworks/extract/exec/FileProcessor.java
[jsoncli]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Excavator/src/main/java/org/hercworks/extract/exec/GenericJsonProcessor.java
[license]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/LICENSE
