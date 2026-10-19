# External EarthSiege 2 tooling assessment

Assessed 2026-10-18. Source references are pinned to:

- Herc Works MDK: `1f66699118d4c0032259e8db708b5bfed50f44c7`.
- ES2 Toolkit: `f0f79a58bfac374c1114c796eef69918a742cb46`, the revision
  already used by our [model investigation](original-models.md).

**Recommendation:** retain our current runtime and extraction pipeline; use
Subject9x's tools selectively for offline data inspection. The next useful
milestone is one existing HERC DAT-to-JSON conversion with byte-level checks.
Neither repository currently supplies a verified end-to-end original terrain
converter. Detailed MDK paths and terrain evidence are in
[herc-works-integration.md](herc-works-integration.md).

## Relationship and useful implementations

ES2 Toolkit depends on MDK ES2Core 0.5.0 and ES2TransferApi 0.6.0, matching the
pinned MDK module versions. It adds task-specific front ends and exporters around
those parsers; it is not a second independent decoding implementation. A version
number alone does not pin source: build the two explicit revisions together in
an isolated Maven repository for any future reproducible integration.
See the [Toolkit parent POM][toolpom] and [MDK parent POM][mdkpom].

Both pinned READMEs announce archival. The Toolkit refines overlapping bitmap
workflows but does not supersede MDK's parser libraries or ES2Vol.

| Function | MDK implementation | Toolkit implementation / practical value |
| --- | --- | --- |
| VOLN | ES2Vol `VolFileReader` / `VolFileWriter`; Excavator CLI | Uses the MDK libraries. No independent improved decompressor or ISO extraction path identified. Our existing importer already handles the verified disc. |
| DBA/DBM | ES2Core `DynamixBitmapArrayTransformer`, `DynamixBitmapTransformer`; Excavator splitting/previews | `DBAUnpack/src/main/java/org/es2tlk/dba/UnpackDBA.java`; `DBMUnpack/src/main/java/org/es2tlk/dbm/UnpackDBM.java`: script/batch exports to DBM and previews. |
| DPL | ES2Core `DynamixPaletteTransformer`, `DynFileWriter` | `es2palette/src/main/java/org/es2tlk/dpl/Paletter.java`: swatch PNG, text values, optional GIMP GPL palette. An offline authoring aid. |
| Early 3Space DTS | ES2Core `DTSModelTransformer`; TransferApi `DTSServiceImpl` | `DTSExtract/src/main/java/org/es2tlk/dts/ExtractDTS.java` and `DTStoObj.java`: OBJ/MTL plus raw JSON. Useful inspection export, with fidelity limits below. |
| HERC/equipment DAT, GL, DMG, FM | ES2Core transformers and TransferApi DTO services; Excavator selected JSON import/export | ES2ModCompiler assembles mod projects using the MDK services. Avoid running its installation/compilation flow on our original resources. |
| MSN/ENG | Partial mission parser and strings support | No additional general mission/terrain parser identified. Toolkit adds no replacement for MDK's hardcoded MSN sections. |
| Zone DAT / WLD / RMP | Zone/WLD field notes and unfinished classes; no RMP decoder found | DBA grayscale export only; no additional height formula, world parser or terrain material mapping found. |
| Bitmap authoring | Array/bitmap writers | CompileDBA and DBMconvert add scripts for DBM-to-DBA and indexed-PNG-to-DBM authoring. Useful for future original-content tools, unnecessary for lossless extraction. |

## DTSExtract: geometry, LODs and animation

The [README][dtsreadme] describes six HERC LOD meshes, one OBJ per mesh, with
animation records in JSON. The source needs to qualify that description:

- `DTStoObj.convertDTS_to_OBJ` iterates parsed top-level objects and emits those
  with mesh groups; it does not enforce a count of six. A read-only size/tag walk
  of our local Tomahawk and Achilles files found **seven top-level ANShape records
  in each**, consuming both files exactly. Our existing parser already checks
  all seven and renders the highest-detail record. Do not discard a record to
  match the README or assume what the seventh represents without verification.
- OBJ contains static geometry, normals, texture coordinates and material
  references, not a usable skeletal rig or animation clips. With
  `applyTransform=true`, `DTStoObj.calcOffset` accumulates default translations
  along the hierarchy; it does not evaluate animated rotations or produce
  runtime articulation. Raw DTS/DTO data retains hierarchy and animation records.
- `ExtractDTS.processDTSFile` passes
  `dts.getMeshes().iterator().next()` to `writeRawJson` inside the output-mesh
  loop. Thus the current code serializes the first top-level shape repeatedly
  under different mesh names. Its JSON sidecars cannot be assumed to represent
  every exported LOD. This is a source-verified defect, not a tested conversion.
- The exporter warns about overlapping/unnecessary faces. Our runtime already
  selects visible cell variants and suppresses camera/weapon locator geometry.
  Exported OBJ requires comparison before it could replace that handling.
- MTL texture references require separately exported DBA images. The tool
  requires supplied DBA/DPL inputs; it does not establish correct loadout,
  palette or weapon-attachment associations automatically.

Sources: [ExtractDTS.java][extractdts], [DTStoObj.java][obj],
[MDK DTS DTO service][dtsdto].

Our [original HERC pipeline](original-models.md) already retains source geometry,
indexed textures, transform hierarchy and walk keyframes for Tomahawk/Achilles,
with gait synchronization, torso articulation and terrain contact. It reads
original resources directly, not OBJ or Toolkit JSON. Switching to static OBJ
would lose working behavior without solving the remaining weapon/loadout,
animation-transition or full-roster gaps.

## Bitmap and terrain conversion limits

[DBAUnpack][unpack] delegates decoding to the same MDK transformers. Its
`heightmap` branch calls [DynFileWriter.writeDBMToHeightmap][writer], whose comment
explicitly treats heightmaps as an assumption. That method rescales each index
to `(byte)(index / 255.0 * 200)` before writing grayscale. This loses source
values and does not read Zone DAT's proposed height scalar. The caller also
passes the palette-file argument as the output path in that branch. Treat it
as an inspection aid needing fixes, not a terrain conversion contract.

The ordinary palette preview writer constructs an RGB image, so its
`index0Alpha` option does not by itself preserve a transparent PNG alpha channel.
Palette matching is partly heuristic. These outputs should not replace our
verified index-preserving artwork, palette flags or cockpit transparency.

The [MDK assessment](herc-works-integration.md#terrain-findings-compared-with-the-earlier-notes)
records the useful new evidence: the mission prefix yields zone candidates
matching resources for all 62 local missions. It also records the
`ZONE7000` dimension exception, unfinished WLD fields and absent RMP decoder.
World selection, height units/orientation, bounds, texture assignment and
original terrain behavior remain unverified. The single MDK Kaitai bitmap
definition is incomplete; Toolkit provides no additional `.ksy` files.

## What to reuse, and what is still missing

| Capability in OpenEarthSiege 2 | Decision |
| --- | --- |
| Safe VOL extraction, resource inventory, hashes, audio imports | Keep existing Python tooling; upstream archive writing offers no current benefit. |
| Direct DBM/DBA/DPL loading and original 4:3 cockpit | Keep existing C++ readers and rendering. Use external previews only as comparisons. |
| Animated original human/Cybrid proof pair | Keep direct DTS runtime path. Consider external OBJ/JSON only for inspecting remaining shapes after exporter defects are addressed. |
| Original HERC definitions and loadouts | Existing MDK JSON exports can expose additional metadata without writing another broad parser. Units and associations still need verification. |
| Remaining LOD/animation/weapon bindings | Raw MDK objects and DTOs are useful evidence; static OBJ does not restore these behaviors automatically. |
| Original mission terrain | Reuse the documented mission-prefix evidence for a later bounded inspector. Neither tool removes the need to establish height/material/world semantics. |

All tools can operate outside the game process. ISO handling remains with our
current importer. Java and Maven would be optional developer conversion tools;
they would not enter CMake, runtime packages or normal game launch.

## Licensing and dependencies

The pinned root licenses are MIT: [MDK, Copyright (c) 2023 Subject9x][mdklicense]
and [Toolkit, Copyright (c) 2024 Subject9x][toollicense]. No separate module license
files were present in the reviewed trees. Retain the full applicable copyright,
permission and warranty notices with any copied/adapted source or distributed
tool. Reference the exact source commit and document local patches. This
assessment imports no implementation, so it adds source attribution links rather
than vendoring license/source bundles.

Dependency versions come from the reviewed POMs; license links below point to
the corresponding release commits, not moving branches.

| Dependency | Use | Checked license |
| --- | --- | --- |
| `at.favre.lib:bytes:1.6.1` | MDK binary utilities | [Apache-2.0][byteslicense] |
| `io.kaitai:kaitai-struct-runtime:0.10` | MDK Core declared dependency | [MIT][kaitailicense] |
| `org.apache.commons:commons-math3:3.6.1` | MDK and mesh mathematics | [Apache-2.0 plus included third-party notices][mathlicense] |
| `commons-cli:commons-cli:1.6.0` | Excavator / DBMconvert CLI | [Apache-2.0][clilicense] |
| `jackson-dataformat-xml:2.16.0` | DTO serialization dependency; brings Jackson core/annotations/databind | [Apache-2.0][jacksonlicense] |
| `woodstox-core:6.5.1` | Jackson XML transitive dependency | [Apache-2.0][woodstoxlicense] |
| `stax2-api:4.2.1` | Jackson XML transitive dependency | [BSD-2-Clause][staxlicense] |
| `commons-io:commons-io:2.17.0` | Toolkit ES2ModCompiler | [Apache-2.0][iolicense] |
| `junit:junit:4.9` | Tests, not needed for runtime conversion | [CPL-1.0 at this old release][junitlicense] |

The [Jackson XML POM][jacksonpom] establishes the listed XML transitive
dependencies. This is a source/declaration audit, not a resolved Maven dependency
tree or a license audit of a built shaded JAR. Before redistributing a tool,
record the resolved dependencies and checksums, inspect bundled LICENSE/NOTICE
files, and retain required notices, including third-party notices within Commons
Math. Test libraries need not ship. JDK AWT/ImageIO is also used for previews.

The POMs mix release 9 with Java 8 properties, while Toolkit READMEs request
Java 19+. Test a recorded suitable JDK before promising a reproducible build.
Some additional authoring modules have apparent packaging defects (DBMUnpack's
manifest names the DBA entry point, DBMconvert's entry point is misspelled, and
CompileDBA requests a DBMconvert version different from that module's declared
version). Build only needed modules initially; do not treat every shaded JAR as
verified.

Original ISO files, extracted resources, converted meshes/images/JSON containing
game data and soundtrack remain proprietary asset material for repository
exclusion purposes. Keep them under existing ignored `original/`, `imported/`
or `assets/original/` paths and out of public packages. MIT source licenses do
not grant rights to those game assets. No copyrighted assets were downloaded or
added during this assessment.

## Recommended next milestone

**Audit one Tomahawk DAT export through the existing ES2Excavator CLI.**
This provides new metadata for future HERC compatibility work with no new
binary parser and no engine changes.

1. Build MDK at the pinned commit outside the engine tree using a recorded JDK
   and isolated Maven cache. Preserve notices; record tool/JAR hashes. Use its
   existing ES2UnVol/Excavator entry point.
2. Put a copy of the already extracted `TOMAHAWK.DAT` beside the JAR in a fresh
   ignored scratch directory. Invoke the CLI with that relative filename; check
   the expected `UNPACK/DAT/TOMAHAWK.json` output. Avoid source-output mode.
3. Check the JSON fields against the original 216-byte input: signed
   little-endian walk ID at offset 12 = 1, run ID at 14 = 2, skin ID at 148 = 1.
   Compare field values, not JSON key ordering or mere command exit status.
   These byte values were confirmed locally during this assessment.
4. Require deterministic semantic output on a second run, unchanged input
   hashes, and failure of the audit when output is absent/incomplete or the
   input is truncated. Synthetic fixtures can cover those failure paths without
   publishing original game data.
5. Store provenance and the report under `imported/`. Stop there: JSON export
   does not validate original movement units and must not alter gameplay values.

Later small candidates, after this gate: audit one GL hardpoint export; compare
DTS LOD/animation inventories after correcting the repeated-first-shape export;
or build a mission-prefix inspector from the bounded layout evidence. Do not
combine them into the first milestone.

## Verification performed

Reviewed both complete source-tree listings, relevant implementations, POMs,
Kaitai/docs and pinned licenses. Read-only local checks covered 62 mission
prefixes, 39 zone DAT/DBA dimension pairs, both proof-pair DTS record counts and
the HERC DAT fields above. These checks did not execute the Java parsers.

Java, javac, Maven and the Kaitai compiler were absent from PATH. No external
tool build, Java conversion, new runtime test or visual comparison was performed.
The game was not changed; existing presentation, models, audio and gameplay
remain as before. The investigation stops with these two assessment documents.

[toolpom]: https://github.com/Subject9x/es2-toolkit/blob/f0f79a58bfac374c1114c796eef69918a742cb46/pom.xml
[mdkpom]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/pom.xml
[dtsreadme]: https://github.com/Subject9x/es2-toolkit/blob/f0f79a58bfac374c1114c796eef69918a742cb46/DTSExtract/README.md
[extractdts]: https://github.com/Subject9x/es2-toolkit/blob/f0f79a58bfac374c1114c796eef69918a742cb46/DTSExtract/src/main/java/org/es2tlk/dts/ExtractDTS.java
[obj]: https://github.com/Subject9x/es2-toolkit/blob/f0f79a58bfac374c1114c796eef69918a742cb46/DTSExtract/src/main/java/org/es2tlk/dts/DTStoObj.java
[dtsdto]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2TransferApi/src/main/java/org/hercworks/transfer/svc/impl/dbsim/DTSServiceImpl.java
[unpack]: https://github.com/Subject9x/es2-toolkit/blob/f0f79a58bfac374c1114c796eef69918a742cb46/DBAUnpack/src/main/java/org/es2tlk/dba/UnpackDBA.java
[writer]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/io/write/DynFileWriter.java
[mdklicense]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/LICENSE
[toollicense]: https://github.com/Subject9x/es2-toolkit/blob/f0f79a58bfac374c1114c796eef69918a742cb46/LICENSE
[jacksonpom]: https://github.com/FasterXML/jackson-dataformat-xml/blob/fe6ffce81ea500387905813f56a0f3be500c0762/pom.xml
[byteslicense]: https://github.com/patrickfav/bytes-java/blob/9644a8d432bb1ed5aa074dea4a0ad47bc042048d/LICENSE
[kaitailicense]: https://github.com/kaitai-io/kaitai_struct_java_runtime/blob/cf78a29c11c438136f0a09cee0a7fea1f797b0a3/LICENSE
[mathlicense]: https://github.com/apache/commons-math/blob/16abfe5de688cc52fb0396e0609cb33044b15653/LICENSE.txt
[clilicense]: https://github.com/apache/commons-cli/blob/0d29a55998aa52be8832497527e266630f97c371/LICENSE.txt
[jacksonlicense]: https://github.com/FasterXML/jackson-dataformat-xml/blob/fe6ffce81ea500387905813f56a0f3be500c0762/LICENSE
[woodstoxlicense]: https://github.com/FasterXML/woodstox/blob/725694cd20aa6853229f737c667fcd3dc9f3b8ca/LICENSE
[staxlicense]: https://github.com/FasterXML/stax2-api/blob/8f10bb50124cb70048a3ec8dd2dca4421a04733c/src/main/resources/META-INF/LICENSE
[iolicense]: https://github.com/apache/commons-io/blob/7b5c3ec3ddcedeb764fa79f29e9baefdd3444163/LICENSE.txt
[junitlicense]: https://github.com/junit-team/junit4/blob/61f06547599bb6b98bca99d5bc457eb20bc17cab/LICENSE
