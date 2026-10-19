# Tomahawk DAT/JSON validation

This standalone, read-only Python 3 tool checks the contract established by
[the DAT audit](tomahawk-dat-audit.md). It uses only the standard library and has
no connection to the game engine, Java, Maven or the exporter build.

## Usage

From the project root, after obtaining an actual DAT JSON export:

```sh
python3 tools/validate_tomahawk_dat.py \
  assets/original/models/TOMAHAWK.DAT \
  imported/tomahawk-validation/UNPACK/DAT/TOMAHAWK.json

python3 tests/tomahawk_dat_validator_tests.py
```

The JSON path is an example location for an externally produced export, not an
output created by this validator. Keep original resources and exports in the
existing ignored directories. The validator never writes to either input.

Exit status is **0** for a matching record, **1** for comparison/layout errors,
and **2** for unreadable inputs, malformed JSON or command-line usage errors.
Discrepancies go to stderr; omitted raw bytes and the success summary go to
stdout. For example:

```text
record 0 | byte offset 114 | field modelFlagsShadow2 | expected -254 | actual 65282
```

Offsets are decimal, relative to the beginning of the DAT. Metadata, unexpected
JSON properties and JSON syntax errors have offset `n/a`, because they have no
corresponding DAT field. Record index is always **0**: the verified file contains
one record, with no count/header or multi-record container. Concatenated records
and all other lengths are rejected.

## Contract and scope

- Checks all 216 bytes are accounted for by non-overlapping, in-bounds spans;
  16-bit fields must be aligned. Offsets belong to the fixed validator schema,
  not pointers stored in the DAT. There is no format signature or version to
  validate: do not treat arbitrary 216-byte files as identified HERC DATs.
- Compares all 56 scalar words, 20 byte-array entries, 12 short-array entries,
  and both 12-byte strings. Exact existing DTO keys are retained, including
  `unk22_Val100Razor0`, `unk14_anim_leg2` and `physicsFrctionAccel`.
  Unknown fields are compared as raw values without imposing guessed meanings
  or constants, even for a label such as `unk124_all500`.
- Reads unsigned bit patterns, then explicitly interprets them as signed
  two's-complement bytes/shorts according to the pinned Java reader. No exported
  numeric field in this audited layout has an unsigned DTO contract. Thus
  `0xff` maps to `-1`, and `0xff02` maps to `-254`; JSON `255`/`65282` are errors
  for those respective fields. No clamping, wrapping or numeric type coercion
  is accepted. JSON booleans, floats and numeric strings cannot replace integer
  fields.
- Speed and torso flag values must be decimal integer **strings**, matching
  Java `String.valueOf(short)`. Torso rate/angle strings are checked against
  signed raw value divided by 100 with Java binary32 precision. Numerically
  equivalent spellings such as `"1.20"` and `"1.2"` are accepted when they
  resolve to the same binary32 value; literal `Float.toString` formatting is
  not tested. No arbitrary numerical tolerance is used.
- Expects the unwrapped JSON object, `classDef: "HercSimDat"`, and `fileName`
  equal to the DAT basename without extension, case preserved. Missing/extra
  keys, wrong nesting/types, wrong array counts, duplicate keys, non-finite
  tokens and malformed JSON are rejected.
- String validation covers audited ASCII content. Bytes after the terminator
  are reported as raw hex. Full 12-byte strings are accepted; non-ASCII content
  before the terminator is explicitly unsupported. The pinned service's
  asymmetric empty-name behavior is preserved: it includes name byte 0 even
  when NUL, so an all-zero `name` exports `"\u0000"`; an all-zero `debrisFile`
  exports `""`. This is a source-observed edge case, not a newly inferred rule.

The 36 skipped bytes at offsets 102–103, 106–107, 158–169, 176–189 and 198–203
have no JSON counterparts. They and omitted string padding are printed verbatim
in hex, including nonzero values, without inventing JSON fields. They cannot be
compared against this exporter. A pass establishes equality of **represented
fields**, not lossless round-trip conversion or proof that Java generated the
provided JSON.

## Validation report — 2026-10-18

Follow-up: Java and Maven are now installed. The [Java build attempt report](tomahawk-java-validation.md)
records the remaining source/dependency download blocker. The results below
describe the earlier synthetic-only validation.

**Confirmed:** all **22 automated synthetic tests pass**. Fixtures are built
independently with `struct.pack`, not generated from the validator's layout.
They contain no original game data. Checks cover:

- A complete record with distinct values, unchanged inputs, and mutation of
  every JSON leaf to ensure it is compared.
- Signed byte/short bounds, unsigned boundary bit patterns, negative flags,
  signed arrays, scaled values and incorrect numeric/type conversions.
- Missing/extra fields at multiple levels, wrong root/nested types and array
  counts, every truncated length 0–215, trailing data and concatenated records.
- Invalid schema offsets, overlap, alignment and coverage gaps; opaque bytes,
  string padding, full-width strings, the empty-name edge case and unsupported
  non-ASCII strings.
- CLI exit codes, required diagnostic details, malformed/duplicate/non-finite
  JSON and missing files.

The local `assets/original/models/TOMAHAWK.DAT` is still 216 bytes with the
audit's SHA-256
`1140c0dbeab971b72264767280162049178517914ab242cab0da66b3cf790d13`.
The only Tomahawk JSON found in the inspected local resource/tool directories,
`imported/herc-model-audit/TOMAHAWK.json`, is a list of DTS model records, **not a
DAT export**. It was not used as conversion evidence.

**At the initial validation:** Java, javac and Maven were unavailable on PATH, and no actual
Tomahawk DAT JSON export was found. The real Java exporter remains untested;
there is **no successful end-to-end conversion claim**. Actual Jackson property
emission, filename metadata and string encoding must still be checked by
execution. The exporter limitations recorded in the audit remain: incomplete
length checks, ignored source spans, unsafe reverse narrowing, and swallowed
export errors that can leave stale files. This validator does not alter them.

**Next small milestone:** in a separately provisioned Java environment, run the
pinned exporter twice in fresh scratch output directories, retain source/JAR
hashes, and validate each output against the original DAT. Confirm both outputs
pass independently and match semantically. Avoid stale JSON and source-output
mode. Do not migrate engine data based on this synthetic validation alone.

Only the validator, its synthetic test file and this document were added. No
engine/exporter changes, dependency installation, original-asset writes or build
integration were required. The existing game tests/build were not rerun because
the tool is independent of the runtime.

Format provenance: Subject9x's MIT-licensed Herc Works MDK at commit
`1f66699118d4c0032259e8db708b5bfed50f44c7`; relevant pinned reader, DTO and service
links are in [the audit](tomahawk-dat-audit.md). This is an independently written
validator using those documented mappings; no Java implementation, library or
copyrighted game resources are bundled.
