#!/usr/bin/env python3
"""Read-only comparison of one 216-byte HERC DAT with pinned MDK JSON.

Contract: docs/tomahawk-dat-audit.md (MDK 1f66699118d4c0032259e8db708b5bfed50f44c7).
This is a validator, not an exporter or a general DAT format detector.
"""

import argparse
from dataclasses import dataclass
import json
import math
from pathlib import Path
import re
import struct
import sys


RECORD_SIZE = 216


@dataclass(frozen=True)
class Field:
    offset: int
    kind: str
    name: str


# Exact DTO keys, including historical misspellings and unknown-field labels.
# All numeric DAT fields are signed in the pinned reader; see signed_value().
FIELDS = tuple(Field(int(offset), kind, name) for offset, kind, name in (
    line.split() for line in """
0 text16 speed.turn
2 text16 speed.reverse
4 text16 speed.forward
6 text16 speed.accel_and_decel
8 text16 speed.turning_decel
10 int16 camera_boneId
12 int16 input_throttle_hercFlag
14 int16 unk14_anim_leg1
16 int16 unk14_anim_leg2
18 int16 unk14_anim_leg3
20 int16 offset_y_adjust
22 int16 unk22_Val100Razor0
24 int16 ai_aim_targ_offset
26 text16 torso_config.razor_override
28 fixed16 torso_config.rotation.rotate_speed
30 fixed16 torso_config.rotation.accel_rate
32 fixed16 torso_config.rotation.angle_max
34 text16 torso_config.input_flags
36 fixed16 torso_config.pitch.rate_max
38 fixed16 torso_config.pitch.rate
40 fixed16 torso_config.pitch.angle_max
42 fixed16 torso_config.pitch.angle_min
44 int16 unk44_move_anim_rate
46 bytes20 model_lod_boneIds
66 int16 unk66_Val1000
68 int16 legsCritFlags1
70 int16 legsCritFlags2
72 int16 modelLegsTotal
74 int16 modelFlagNoDebris
76 int16 unk76_Val
78 int16 inputFlagFlyer
80 int16 maybe_hud_id
82 int16 unk82_val
84 int16 unk84_val
86 name12 name
98 int16 cam_adj_ofs_y
100 int16 cam_adj_ofs_x
104 int16 cameraExtOrgOffset
108 int16 unk108_camExtVal1
110 int16 unk110_camExtVal2
112 int16 modelFlagsShadow1
114 int16 modelFlagsShadow2
116 int16 unk116_val
118 int16 unk118_val
120 int16 unk120_val
122 int16 unk122_mdlFlagVal
124 shorts12 unk124_all500
148 int16 modelSkinId
150 int16 unk150_val
152 int16 unk152_val
154 int16 unk154_fixedVal
156 int16 unk156_400or800
170 int16 unk170_val
172 int16 unk172_val
174 int16 unk174_250or275
190 int16 shieldMaxTotal
192 int16 unk192_val
194 int16 physicsFrictionCoef
196 int16 physicsFrctionAccel
204 string12 debrisFile
""".strip().splitlines()
))
SIZES = {"int16": 2, "text16": 2, "fixed16": 2, "bytes20": 20,
         "shorts12": 24, "name12": 12, "string12": 12}
SKIPPED = ((102, 2), (106, 2), (158, 12), (176, 14), (198, 6))


@dataclass(frozen=True)
class Issue:
    offset: object
    field: str
    expected: object
    actual: object

    def __str__(self):
        offset = "n/a" if self.offset is None else str(self.offset)
        return (f"record 0 | byte offset {offset} | field {self.field} | "
                f"expected {self.expected!r} | actual {self.actual!r}")


def validate_layout(fields=FIELDS, skipped=SKIPPED):
    """Check the fixed schema before reading: bounds, alignment, overlap, gaps."""
    issues, occupied, names = [], set(), set()
    spans = []
    for field in fields:
        if field.kind not in SIZES or field.name in names:
            issues.append(Issue(field.offset, field.name, "known kind and unique field", field))
            continue
        names.add(field.name)
        if field.kind in ("int16", "text16", "fixed16", "shorts12") and field.offset % 2:
            issues.append(Issue(field.offset, field.name, "aligned 16-bit field", field.offset))
        spans.append((field.offset, SIZES[field.kind], field.name))
    spans.extend((offset, size, "<unrepresented>") for offset, size in skipped)
    for offset, size, name in spans:
        if offset < 0 or size <= 0 or offset + size > RECORD_SIZE:
            issues.append(Issue(offset, name, "span within bytes 0..215", (offset, size)))
            continue
        span = set(range(offset, offset + size))
        if occupied & span:
            issues.append(Issue(offset, name, "non-overlapping span", sorted(occupied & span)))
        occupied.update(span)
    missing = sorted(set(range(RECORD_SIZE)) - occupied)
    if missing:
        issues.append(Issue(missing[0], "<layout>", "all 216 bytes accounted for", missing))
    return issues


def signed_value(data, offset, width):
    """Read unsigned bits first, then apply the reader's two's-complement type."""
    raw = int.from_bytes(data[offset:offset + width], "little", signed=False)
    bits = width * 8
    return raw - (1 << bits) if raw & (1 << (bits - 1)) else raw


def float32(value):
    return struct.unpack("<f", struct.pack("<f", value))[0]


@dataclass(frozen=True)
class ScaledShort:
    raw: int

    def matches(self, actual):
        # Java's /100f and Float.toString: compare binary32 values, not spelling.
        if not isinstance(actual, str) or not re.fullmatch(
                r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?", actual, re.ASCII):
            return False
        try:
            value = float(actual)
            return math.isfinite(value) and float32(value) == float32(self.raw / 100)
        except (ValueError, OverflowError):
            return False

    def __repr__(self):
        return f"numeric string {str(self.raw / 100)!r} (binary32, raw={self.raw})"


def string_end(raw, kind):
    # Pinned service always includes name byte 0, even if it is NUL.
    start = 1 if kind == "name12" else 0
    end = raw.find(b"\0", start)
    return len(raw) if end < 0 else end


def unrepresented_bytes(data):
    """Expose omitted spans/padding verbatim; no inferred meanings or defaults."""
    if len(data) != RECORD_SIZE:
        return []
    spans = [(offset, "<unrepresented>", data[offset:offset + size])
             for offset, size in SKIPPED]
    for field in FIELDS:
        if field.kind in ("name12", "string12"):
            raw = data[field.offset:field.offset + 12]
            end = string_end(raw, field.kind)
            if end < 12:
                spans.append((field.offset + end, field.name + ".padding", raw[end:]))
    return spans


def validate(data, document, file_name):
    """Return discrepancies; input bytes and JSON are never changed."""
    issues = validate_layout()
    if len(data) != RECORD_SIZE:
        issues.append(Issue(min(len(data), RECORD_SIZE), "<record.length>", RECORD_SIZE, len(data)))
    if issues:
        return issues

    expected = {"classDef": "HercSimDat", "fileName": file_name}
    offsets = {}
    for field in FIELDS:
        offset, kind, name = field.offset, field.kind, field.name
        if kind in ("name12", "string12"):
            raw = data[offset:offset + 12]
            try:
                value = raw[:string_end(raw, kind)].decode("ascii")
            except UnicodeDecodeError as error:
                issues.append(Issue(offset + error.start, name, "audited ASCII string", raw.hex()))
                continue
        elif kind in ("bytes20", "shorts12"):
            width, count = (1, 20) if kind == "bytes20" else (2, 12)
            value = [signed_value(data, offset + i * width, width) for i in range(count)]
            offsets.update({f"{name}[{i}]": offset + i * width for i in range(count)})
        else:
            value = signed_value(data, offset, 2)
            if kind == "text16":
                value = str(value)
            elif kind == "fixed16":
                value = ScaledShort(value)
        node = expected
        parts = name.split(".")
        for i, key in enumerate(parts[:-1]):
            node = node.setdefault(key, {})
            offsets.setdefault(".".join(parts[:i + 1]), offset)
        node[parts[-1]] = value
        offsets[name] = offset
    if issues:
        return issues

    def compare(want, got, path):
        offset = offsets.get(path)
        if isinstance(want, dict):
            if type(got) is not dict:
                issues.append(Issue(offset, path or "<root>", "JSON object", got))
                return
            for key, child in want.items():
                child_path = f"{path}.{key}" if path else key
                if key not in got:
                    issues.append(Issue(offsets.get(child_path), child_path, child, "<missing>"))
                else:
                    compare(child, got[key], child_path)
            for key in sorted(got.keys() - want.keys()):
                child_path = f"{path}.{key}" if path else key
                issues.append(Issue(None, child_path, "<absent: not in audited schema>", got[key]))
        elif isinstance(want, list):
            if type(got) is not list:
                issues.append(Issue(offset, path, "JSON array", got))
                return
            if len(got) != len(want):
                issues.append(Issue(offset, path + ".length", len(want), len(got)))
            for i, (child, actual) in enumerate(zip(want, got)):
                compare(child, actual, f"{path}[{i}]")
        elif isinstance(want, ScaledShort):
            if not want.matches(got):
                issues.append(Issue(offset, path, want, got))
        elif type(want) is not type(got) or want != got:
            issues.append(Issue(offset, path, want, got))

    compare(expected, document, "")
    return issues


def load_json(path):
    def unique_keys(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate JSON field {key!r}")
            result[key] = value
        return result

    def invalid_constant(value):
        raise ValueError(f"non-finite JSON number {value}")

    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_keys,
                      parse_constant=invalid_constant)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dat", type=Path, help="one original 216-byte TOMAHAWK.DAT")
    parser.add_argument("json", type=Path, help="actual MDK DAT JSON export (not DTS JSON)")
    args = parser.parse_args(argv)
    try:
        data = args.dat.read_bytes()
        document = load_json(args.json)
    except (OSError, ValueError, RecursionError) as error:
        print(Issue(None, "<input>", "readable DAT and valid unambiguous JSON", str(error)),
              file=sys.stderr)
        return 2
    issues = validate(data, document, args.dat.stem)
    for issue in issues:
        print(issue, file=sys.stderr)
    for offset, name, raw in unrepresented_bytes(data):
        print(f"record 0 | byte offset {offset} | field {name} | "
              f"not exported; raw hex {raw.hex()}")
    if issues:
        print(f"FAIL: {len(issues)} discrepancy(s)", file=sys.stderr)
        return 1
    print("PASS: one 216-byte record matches the audited JSON mapping. "
          "Omitted bytes are not validated against JSON; exporter execution is not established.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
