#!/usr/bin/env python3
"""Independent synthetic DTS fixtures; optional read-only original-file checks."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import inspect_dts_animation as dts


def pack(fmt, *values):
    return struct.pack("<" + fmt, *values)


def record(tag, payload):
    return pack("II", tag, len(payload)) + payload


def sequence(ticks=(100, 200), parts=(0, 1), indices=(0, 1, 0, 2), tag=0x1e0004):
    # Explicit independent layout: header, frame triples, part count/list,
    # frame-major transform indices. Unknown words deliberately use high bits.
    payload = pack("4H", sum(ticks), 32768, 65535, len(ticks))
    payload += b"".join(pack("3H", t, 65535, 32768) for t in ticks)
    payload += pack("H", len(parts)) + pack(f"{len(parts)}H", *parts)
    payload += pack(f"{len(indices)}H", *indices)
    return record(tag, payload)


def fixture(seqs=None, transforms=None, defaults=(0, 1), relations=((-1, 0), (0, 1)), children=None):
    seqs = [sequence()] if seqs is None else seqs
    transforms = [(0, 0, 0, 0, 0, 0), (0, 0, 0, 0, 100, 0),
                  (-32768, 32767, -1, -32768, 32767, -1)] if transforms is None else transforms
    anim = pack("H", len(seqs)) + b"".join(seqs)
    anim += pack("H4H", 1, 65535, 32768, 0, 1)
    anim += pack("H", len(transforms)) + b"".join(pack("6h", *t) for t in transforms)
    anim += pack("H", len(defaults)) + pack(f"{len(defaults)}H", *defaults)
    anim += pack("H", len(relations)) + b"".join(pack("2h", *r) for r in relations)
    children = [record(0x140005, pack("6h", 1, -32768, 0, 0, 0, 0))] if children is None else children
    shape = pack("6hH", -1, 0, 0, 0, 0, 0, len(children)) + b"".join(children)
    shape += pack("4H", 1, 1, 65535, 32768)  # maps retained as raw bits
    return record(0x1e0003, shape + record(0x1e0002, anim))


def animation(data):
    return dts.inspect_bytes(data)["records"][0]["animation"]


class SyntheticInventory(unittest.TestCase):
    def invalid(self, data, message):
        with self.assertRaisesRegex(dts.DTSError, r"record \d+, offset 0x[0-9a-f]+: .*" + message):
            dts.inspect_bytes(data)

    def test_valid_layout_offsets_and_signed_values(self):
        data = fixture()
        model = dts.inspect_bytes(data, "human.DTS")
        root = model["records"][0]
        a = root["animation"]
        # Calculated from fixture's independently specified byte layout.
        self.assertEqual(a["offset"], 50)
        self.assertEqual(a["sequences"][0]["offset"], 60)
        self.assertEqual(a["sequences"][0]["tables"]["frames"]["offset"], 76)
        self.assertEqual(a["tables"]["transforms"]["offset"], 114)
        self.assertEqual(a["transforms"][2], [-32768, 32767, -1, -32768, 32767, -1])
        self.assertEqual(a["relations"], [[-1, 0], [0, 1]])
        self.assertEqual(a["nodes"][0]["children"], [1])
        self.assertEqual(root["children"][0]["part_id"], -32768)
        self.assertEqual(model["sha256"], hashlib.sha256(data).hexdigest())

    def test_unsigned_unknown_words_preserved(self):
        root = dts.inspect_bytes(fixture())["records"][0]
        a = root["animation"]
        self.assertEqual(a["transitions_raw_u16"], [[65535, 32768, 0, 1]])
        self.assertEqual(a["sequences"][0]["header_unknown_u16"], [32768, 65535])
        self.assertEqual(a["sequences"][0]["frames"][0], [100, 65535, 32768])
        self.assertEqual(root["sequence_map_raw_u16"], [65535])
        self.assertEqual(root["transform_map_raw_u16"], [32768])

    def test_unsigned_tick_boundary(self):
        a = animation(fixture(seqs=[sequence(ticks=(65535,), indices=(0, 1))]))
        self.assertEqual(a["sequences"][0]["duration_ticks"], 65535)

    def test_different_top_level_records(self):
        second = fixture(transforms=[(0, 0, 0, 7, 8, 9)] * 3)
        report = dts.inspect_bytes(fixture() + second)
        first, last = report["records"]
        self.assertEqual(last["offset"], first["end"])
        self.assertEqual(last["end"], report["bytes"])
        self.assertNotEqual(first["animation"]["transforms"], last["animation"]["transforms"])
        self.assertEqual(last["record_index"], 1)

    def test_motion_compares_values_not_indices(self):
        a = animation(fixture())
        self.assertEqual(a["sequences"][0]["local_motion"], {
            "varying_rotation": [1], "varying_translation": [1], "held": [0], "unlisted": []})
        a = animation(fixture(transforms=[(0, 0, 0, 0, 0, 0)] * 3))
        self.assertEqual(a["sequences"][0]["local_motion"]["held"], [0, 1])
        a = animation(fixture(seqs=[sequence(parts=(1,), indices=(1, 2))]))
        self.assertEqual(a["sequences"][0]["local_motion"]["unlisted"], [0])

    def test_noncyclic_sequence_and_empty_sequence_table(self):
        self.assertEqual(animation(fixture(seqs=[sequence(tag=0x1e0001)]))["sequences"][0]["type"], "ANSequence")
        self.assertEqual(animation(fixture(seqs=[]))["sequences"], [])

    def test_missing_zero_relation_reported_without_guess(self):
        a = animation(fixture(relations=((-1, 1),)))
        self.assertEqual(a["missing_relation_nodes"], [0])
        self.assertIsNone(a["nodes"][0]["parent"])

    def test_incomplete_or_invalid_hierarchy(self):
        for relations, message in [(((-1, 0),), "incomplete hierarchy"),
                                   (((-1, 0), (-1, 0)), "duplicate relation"),
                                   (((1, 0), (0, 1)), "cyclic hierarchy"),
                                   (((-32768, 0), (0, 1)), "invalid relation"),
                                   (((32767, 0), (0, 1)), "invalid relation"),
                                   (((-1, -1), (0, 1)), "invalid relation"),
                                   (((-1, 0), (0, 2)), "invalid relation")]:
            with self.subTest(relations=relations):
                self.invalid(fixture(relations=relations), message)

    def test_default_part_and_keyframe_references(self):
        self.invalid(fixture(defaults=(0, 65535)), "invalid defaults")
        self.invalid(fixture(seqs=[sequence(parts=(0, 65535))]), "invalid parts")
        self.invalid(fixture(seqs=[sequence(indices=(0, 1, 0, 32768))]), "invalid indices")
        self.invalid(fixture(seqs=[sequence(parts=(1, 1))]), "duplicate sequence part")

    def test_invalid_object_bone(self):
        for value in (-32768, 32767):
            self.invalid(fixture(children=[record(0x140005, pack("6h", value, 0, 0, 0, 0, 0))]), "invalid part bone")

    def test_truncation_at_every_byte(self):
        data = fixture()
        for size in range(len(data)):
            with self.subTest(size=size):
                self.invalid(data[:size], "")

    def test_trailing_and_unconsumed_bytes(self):
        self.invalid(fixture() + b"\x00", "truncated")
        child = record(0x140005, pack("6h", 1, 0, 0, 0, 0, 0) + b"\x00\x00")
        self.invalid(fixture(children=[child]), "unconsumed")

    def test_child_cannot_consume_sibling_or_parent_tail(self):
        child = pack("II", 0x140005, 100000) + pack("6h", 1, 0, 0, 0, 0, 0)
        self.invalid(fixture(children=[child]), "crosses parent/file boundary")
        # A sequence frame table cannot consume following transition/transform data.
        child = record(0x1e0004, pack("4H", 300, 0, 0, 1024))
        self.invalid(fixture(seqs=[child]), "truncated frames")

    def test_oversized_counts_and_depth(self):
        for offset, message in [(20, "child"), (58, "sequence"), (74, "frame"), (88, "sequence part"), (112, "transform")]:
            data = bytearray(fixture())
            struct.pack_into("<H", data, offset, 65535)
            self.invalid(data, "invalid " + message + " count")
        child = record(0x140005, pack("6h", 1, 0, 0, 0, 0, 0))
        for _ in range(33):
            child = record(0x140007, pack("6hH", -1, 0, 0, 0, 0, 0, 1) + child)
        self.invalid(fixture(children=[child]), "excessive nesting")
        self.invalid(fixture() * 17, "excessive top-level")

    def test_zero_and_inconsistent_frame_duration(self):
        self.invalid(fixture(seqs=[sequence(ticks=(0, 100))]), "zero-length frame")
        data = bytearray(fixture())
        struct.pack_into("<H", data, 68, 301)
        self.invalid(data, "do not sum")
        self.invalid(fixture(seqs=[sequence(ticks=(), indices=())]), "invalid frame count")

    def test_unsupported_tags_with_context(self):
        self.invalid(record(0x12345678, b""), "unsupported record tag 0x12345678")
        self.invalid(fixture(children=[record(0x12345678, b"xx")]), "unsupported record tag")
        self.invalid(fixture(seqs=[record(0x140005, b"")]), "unsupported record tag")

    def test_opaque_mesh_is_explicit(self):
        child = record(0x140014, pack("6h", 1, 0, 0, 0, 0, 0) + b"opaque")
        root = dts.inspect_bytes(fixture(children=[child]))["records"][0]
        self.assertEqual(root["children"][0]["opaque_geometry"], {"offset": 42, "end": 48})

    def test_container_tails(self):
        child = record(0x140005, pack("6h", 1, 0, 0, 0, 0, 0))
        for tag, tail in [(0x140007, b""), (0x140008, pack("2H", 0, 0)),
                          (0x14000b, pack("H", 65535)), (0x14000c, pack("H", 32768)),
                          (0x140015, pack("H3hihhH", 1, -1, -2, -3, -2147483648, 32767, 1, 65535))]:
            with self.subTest(tag=tag):
                container = record(tag, pack("6hH", -1, 0, 0, 0, 0, 0, 1) + child + tail)
                root = dts.inspect_bytes(fixture(children=[container]))["records"][0]
                self.assertEqual(root["children"][0]["children"][0]["bone"], 1)
        self.invalid(fixture(children=[record(0x14000c, pack("6hH", -1, 0, 0, 0, 0, 0, 0) + b"x")]), "unconsumed")

    def test_text_contains_hierarchy_raw_data_and_limits(self):
        text = dts.text_inventory([dts.inspect_bytes(fixture(), "example.DTS")])
        for expected in ("MODEL example", "Object containment", "node 1 parent=0", "Sequence 0", "-32768", "unknown", "units"):
            self.assertIn(expected, text)

    def test_cli_read_only_outputs_and_errors_under_optimization(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source, output = root / "test.DTS", root / "inventory.json"
            source.write_bytes(fixture())
            cli = [sys.executable, "-O", str(ROOT / "tools/inspect_dts_animation.py")]
            result = subprocess.run(cli + [str(source), "--format", "json", "--output", str(output)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(output.read_text())[0]["bytes"], len(fixture()))
            result = subprocess.run(cli + [str(source), "--output", str(source)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(source.read_bytes(), fixture())
            link = root / "dangling"
            link.symlink_to(root / "must-not-create")
            result = subprocess.run(cli + [str(source), "--output", str(link)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((root / "must-not-create").exists())
            bad = root / "bad.DTS"
            bad.write_bytes(fixture()[:-1])
            absent = root / "absent.txt"
            result = subprocess.run(cli + [str(source), str(bad), "--output", str(absent)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("record 0, offset 0x", result.stderr)
            self.assertFalse(absent.exists())


@unittest.skipUnless(os.environ.get("ES2_DTS_ORIGINAL_DIR"), "optional local originals not requested")
class OriginalInventory(unittest.TestCase):
    def test_proof_pair_metadata_and_source_spans(self):
        inventories = []
        for name, transform_count, size in [("TOMAHAWK", 233, 70556), ("ACHILLES", 239, 75642)]:
            path = Path(os.environ["ES2_DTS_ORIGINAL_DIR"]) / (name + ".DTS")
            before = path.read_bytes()
            report = dts.inspect_file(path)
            inventories.append(report)
            self.assertEqual(report["bytes"], size)
            self.assertEqual(len(report["records"]), 7)
            at = 0
            for root in report["records"]:
                # Independent header traversal and table unpacking, without Reader.
                tag, payload = struct.unpack_from("<II", before, at)
                self.assertEqual(tag, 0x1e0003)
                self.assertEqual(root["offset"], at)
                at += 8 + payload
                self.assertEqual(root["end"], at)
                a = root["animation"]
                formats = {"transforms": "<6h", "defaults": "<H", "relations": "<2h",
                           "transitions": "<4H", "frames": "<3H", "parts": "<H", "indices": "<H"}
                for owner, names in [(a, ("transforms", "defaults", "relations", "transitions"))] + [
                        (s, ("frames", "parts", "indices")) for s in a["sequences"]]:
                    for field in names:
                        span = owner["tables"][field]
                        if field != "indices":  # indices have an implicit frame * part count
                            self.assertEqual(struct.unpack_from("<H", before, span["offset"] - 2)[0], span["count"])
                        self.assertEqual(span["format"], formats[field])
                        values = list(struct.iter_unpack(formats[field], before[span["offset"]:span["end"]]))
                        expected = [v[0] for v in values] if field in ("defaults", "parts", "indices") else [list(v) for v in values]
                        self.assertEqual(owner["transitions_raw_u16" if field == "transitions" else field], expected)
                for s in a["sequences"]:
                    self.assertEqual(s["tables"]["indices"]["end"], s["end"])
                    self.assertEqual(s["tables"]["indices"]["count"], len(s["frames"]) * len(s["parts"]))
                self.assertEqual(a["tables"]["relations"]["end"], a["end"])
                self.assertEqual(a["end"], root["end"])
            self.assertEqual(at, len(before))
            first = report["records"][0]["animation"]
            self.assertEqual(len(first["nodes"]), 12)
            self.assertEqual(len(first["sequences"]), 8)
            self.assertEqual(len(first["transforms"]), transform_count)
            self.assertEqual(len(first["transitions_raw_u16"]), 26)
            self.assertEqual(path.read_bytes(), before)
        self.assertEqual(inventories[0]["records"][0]["animation"]["relations"],
                         inventories[1]["records"][0]["animation"]["relations"])


if __name__ == "__main__":
    unittest.main()
