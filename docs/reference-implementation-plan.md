# Reference implementation plan

Decision recorded 2026-10-18: Herc Works MDK and ES2 Toolkit are **optional
historical source references only**. Their build success is not a prerequisite
for OpenEarthSiege 2 development. Do not invoke Maven, require a JVM, add Java
build hooks, vendor the repositories, or ship their binaries with the game.

This decision supersedes the Java build/export milestones in the earlier
[tooling comparison](external-es2-tooling.md), [MDK assessment](herc-works-integration.md),
[Tomahawk audit](tomahawk-dat-audit.md) and [validator report](tomahawk-dat-validation.md).
Their byte-level findings and recorded limitations remain useful. The
[terrain investigation](original-terrain.md) remains a compatibility gate, with
the later mission-prefix evidence and corrections noted below.

Reference revisions remain pinned:

- Herc Works MDK: [`1f66699118d4c0032259e8db708b5bfed50f44c7`][mdk].
- ES2 Toolkit: [`f0f79a58bfac374c1114c796eef69918a742cb46`][toolkit].

## Existing native tools: retain and reuse

| Existing implementation | Verified scope / boundary |
| --- | --- |
| `tools/import_original_audio.py` | Bounded VOLN directory/payload reader, safe paths, non-overwriting extraction, hashes, inventories and PCM WAV checks. Existing ISO path uses `7z`. Only compression flag `0x02` is supported; other records are preserved/reported, not guessed. |
| `tools/import_original_graphics.py`; `src/original_graphics.cpp`; `tools/graphics_probe.cpp` | Verified cockpit importer; bounded uncompressed 8-bit DBM/DBA decoding, packed rows and object alignment; DPL six-bit RGB expansion, retained pixel indices and explicit cockpit transparency. A preview tool already exists. Flags are not generic alpha. |
| `tools/import_original_models.py`; `src/original_models.cpp` | Importer copies the seven Tomahawk/Achilles resources with hashes and verifies headers/skin bindings. The C++ reader decodes geometry, hierarchy, sequences, model palette ramps and texture references. It validates all seven top-level records, retains the highest-detail shape for rendering, and supports the existing gait/torso articulation. No new general DTS loader is needed. |
| `imported/herc-model-audit/inspect_dts.py` | Existing Python investigation script already walks DTS records and exposes animation tables. It is an unchecked scratch tool with hardcoded output paths and assertions, not a reusable validated converter. Review/reuse it before writing another inspection parser. Its local JSON files are DTS research outputs, not Java DAT exports. |
| `tools/validate_tomahawk_dat.py` | Standalone 216-byte DAT/JSON comparison with exact historical field names, signed byte/short handling, scaled strings and raw omitted-byte reporting. Its 22 synthetic tests establish validator coverage, not Java exporter equivalence. The game currently consumes only the verified skin binding from these DATs. |
| `tools/stage_public_assets.py`, `tools/check_public_files.py`, existing ignore rules | Existing public-package exclusions and checks for original assets/media. Preserve them and all original extraction locations. |

Existing focused coverage includes `tests/import_audio_tests.py`,
`tests/import_graphics_tests.py`, `tests/original_graphics_tests.cpp`,
`tests/import_models_tests.py`, `tests/model_tests.cpp` and
`tests/tomahawk_dat_validator_tests.py`. Use these as regression coverage rather
than replacing working readers. CMake, packaging and native tools currently
contain no Java/Maven invocation; this plan requires no dependency change.

## Useful reference knowledge and implementation order

The order below follows the agreed priorities. Each step needs a demonstrable
gap and a small independent validation before implementation. It does not
authorize changes to rendering, models, gameplay, missions or audio.

| Order | Reference knowledge | Missing capability and limits |
| --- | --- | --- |
| **1. DTS structure and animation** | MDK `DTSModelTransformer`, `data/file/dts/anim/`, and TransferApi `DTSServiceImpl`; Toolkit `DTSExtract/ExtractDTS.java` and `DTStoObj.java`. See [the existing DTS assessment](external-es2-tooling.md#dtsextract-geometry-lods-and-animation). | A reusable report of per-record hierarchy/animation data, including raw transition records currently skipped by C++. Later: identify run/stop/fall transitions and attachment bindings. Keep the proof pair and current animation unchanged. Toolkit's OBJ is static; its JSON loop reuses the first shape. Seven observed top-level records must not be relabeled as six LODs or assigned guessed roles. |
| **2. DBA/DBM/DPL** | MDK `DynamixBitmapTransformer`, `DynamixBitmapArrayTransformer`, `DynamixPaletteTransformer`; Toolkit `DBAUnpack`, `DBMUnpack`, `es2palette`. | Investigate only a locally encountered unsupported encoding or remap/material record. Keep current index-preserving decoding, model ramps and transparency. No second bitmap decoder or palette converter is justified now. Toolkit grayscale export alters index values and does not validate terrain heights. |
| **3. HERC definitions/equipment** | MDK `HercSimDataTransformer`, `GunLayoutTransformer`, related GL/DMG/FM data classes, and shell DAT transformers/DTOs. | A bounded Python inspection of one verified hardpoint/equipment record family could resolve model attachment metadata. Reuse the existing 216-byte DAT contract where applicable. Do not generalize it to all DATs, import statistics into gameplay, or infer equipment/shape associations from filenames alone. |
| **4. Mission structures** | MDK `MissionFileTransformer` and mission string reader; recorded count-dependent MSN prefix offsets in [the assessment](herc-works-integration.md#terrain-findings-compared-with-the-earlier-notes). | Turn the already investigated prefix into a reusable read-only inventory only when needed. All 62 local zone candidates match resources, but that correlation does not establish world selection or complete mission semantics. The remaining Java parser uses mission-specific fixed skips; do not port those as universal rules. |
| **5. WLD/RMP/terrain** | MDK `ZoneDat` and `WorldData` field notes; no verified RMP decoder in either repository. | Preserve unknown data and report boundaries only until original height units, orientation, bounds, texture assignment and sky behavior have evidence. `WorldData` is an unfinished data class, not a reader, correcting wording in the older terrain report. The dimension relationship holds for 38/39 zone pairs, with `ZONE7000` an exception. No heightmap implementation is justified by DBA decoding alone. |

Toolkit largely wraps MDK's parsers; agreement between their wrappers is not
independent corroboration. The existing Kaitai bitmap draft is incomplete and
must not become a new parser/dependency without a separate demonstrated need.
Archive decompression is also not supplied by either reviewed implementation.

## First coding task: bounded DTS animation inspection

**Proposed, not implemented here:** make a standalone Python inspection CLI,
for example `tools/inspect_dts_animation.py`, by narrowing and hardening the
existing local DTS investigation script. Its value is reproducible inspection
of all seven records and currently unused animation tables; it does not replace
the model importer or native decoder.

Limit the first version to the known Tomahawk/Achilles record layouts. Reuse
the scratch script's documented traversal, cross-check with the native reader,
and skip geometry payloads using verified tagged-record boundaries instead of
adding mesh conversion. Unsupported layouts must produce an offset-bearing
error; do not scan arbitrary bytes for apparent animation tags.

The read-only output should contain source hash, top-level record index/tag,
byte offsets and lengths, hierarchy/default-transform references, sequence and
frame counts, raw tick values, transform indices and raw transition-table
words. Preserve unknown fields and their bytes without guessing clip names,
transition behavior, physical units or the role of the seventh record. Support
stdout or an explicitly chosen non-overwriting output under ignored `imported/`;
remove the scratch tool's hardcoded output behavior from the new utility only.

Acceptance criteria:

1. Synthetic fixtures cover two different top-level animation records, signed
   transform/parent boundaries, unsigned counts/indices, unknown transition
   words, truncation, oversized counts, child spans crossing parent boundaries,
   bad references and unsupported tags. Errors identify record and byte offset,
   return nonzero, and work under `python -O` without relying on assertions.
2. A fixture with different animation data in each record proves the report
   does not repeat the first record, addressing the known Toolkit export defect.
3. Read the existing local Tomahawk and Achilles DTS files without writing to
   them. Independently check exact file consumption, seven top-level ANShapes,
   and the known highest-detail metadata: 12 transform nodes and eight sequences.
   Confirm that bounded frame/part/index arrays account for their source spans.
4. Compare raw offsets/types with hand-constructed fixtures, source bytes and
   documented native metadata. Reusing the scratch JSON or generating one's own
   expected JSON is insufficient as the sole correctness oracle.
5. Leave the importer allowlist, native model decoder, animation playback,
   cockpit, audio mappings and gameplay untouched. No Java execution or new
   dependency is part of this milestone. Do not attach runtime behavior to the
   newly reported transition words.

This is one inspection task, not a whole DTS port, roster expansion, animation
reimplementation or equipment conversion project.

## Independent validation and attribution

For every future parser increment, specify the supported record family, field
widths, signedness, endianness and maximum lengths before coding. Use synthetic
fixtures independent of the implementation plus read-only checks against local
original resources. Verify complete record consumption, counts, references and
unknown spans. Keep raw values alongside any evidence-backed interpretation;
unsupported data must fail clearly rather than silently invent defaults.

Distinguish structural checks, original-file observations, and original-game
behavior. Synthetic success or Java source agreement alone establishes none of
the latter. Do not claim Java equivalence without actual historical outputs or
sufficiently complete independent reference data. Running the Java tools is no
longer an implementation or release gate. Keep the existing DAT validator as an
independent historical comparison tool; do not manufacture exporter output to
claim a successful conversion.

For adapted source, record repository, exact commit, source paths, original
symbol and the local adaptation. Preserve the full MIT copyright, permission
and warranty notice with distributed adaptations: MDK names **Copyright (c)
2023 Subject9x**, Toolkit **Copyright (c) 2024 Subject9x**. Put applicable notices
in a small attribution/license file when an adaptation is actually introduced;
review provenance of the scratch inspector before promoting its code. Check
separate third-party notices if copying their code. No Java library dependency
is inherited merely by consulting a format reference.

Original game data has separate rights: keep ISO, extracted resources, soundtrack,
inspection dumps and converted assets under the existing ignored paths, outside
Git and public packages. Repository MIT notices do not license game assets.

## Temporary artifacts: inventory only, no deletion

These are optional historical references, not build inputs. No deletion or
directory move is authorized by this plan.

| Location | Keep as evidence / possible later cleanup |
| --- | --- |
| `imported/tooling-validation/java-20261018/` | Historical partial source copies, Maven cache, logs, release probe and module `target/` outputs. Keep pinned source/license excerpts and logs if useful; caches, compiled outputs and probes are no longer required. |
| `imported/tooling-validation/es2vol-compilation-20261018/` | Verified restored-file hashes and compilation evidence; duplicate Maven cache and `no-processors/` class output are cleanup candidates. |
| `imported/tooling-validation/mdk-complete-20261018/` | Source inventory, baseline hashes and fetched source batches from the interrupted restoration work. Those batches were not applied to the source tree and no Maven build was started for that restoration. Full-tree restoration is no longer a prerequisite. |
| `/tmp/herc-works-assessment/`, `/tmp/es2-toolkit-assessment/`, `/tmp/tomahawk-dat-audit/` | Optional cached reference excerpts. They can be retired after useful pinned citations/evidence are retained, with explicit deletion approval. |
| `imported/herc-model-audit/` | Retain the scratch Python inspector and existing resource/animation observations for the proposed task; these are useful evidence, not disposable Java build output. |

Do not remove the user's system Java/Maven installation, shared Maven cache,
original resources or existing game build directories as part of this cleanup.
No cleanup commands are proposed for automatic execution.

## Scope of this deliverable

Documentation only: reviewed the five existing research documents and inspected
the relevant native tools, scratch inspector, test coverage and build/package
entry points. No parser, engine, dependency, asset or extraction-path change is
made. No new fidelity or Java-conversion result is claimed; the earlier evidence
is cited rather than rerun. Implementation stops at this plan.

[mdk]: https://github.com/Subject9x/herc-works-mdk/tree/1f66699118d4c0032259e8db708b5bfed50f44c7
[toolkit]: https://github.com/Subject9x/es2-toolkit/tree/f0f79a58bfac374c1114c796eef69918a742cb46
