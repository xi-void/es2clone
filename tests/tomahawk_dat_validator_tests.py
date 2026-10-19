#!/usr/bin/env python3
"""Synthetic fixtures only: no original assets or Java installation required."""

from copy import deepcopy
from dataclasses import replace
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import validate_tomahawk_dat as validator


def fixture():
    # Independent binary fixture, not generated from the validator's field table.
    # Distinct values catch shifted offsets; unknown labels impose no constants.
    bones = [0, 1, 127, -128, -1] + list(range(5, 20))
    unknown_array = list(range(-6, 6))
    data = struct.pack(
        "<23h20b10h12s2h2xh2x8h12h5h12x3h14x4h6x12s",
        10, -20, 30, -40, 50, 6, 7, 8, 9, 10, -11, 12, 13, -14,
        1234, -5678, 32767, -18, 1, -1, 120, -32768, 23,
        *bones, *range(31, 41), b"TestHerc", -41, 42, -43,
        51, 52, 53, -254, 55, 56, 57, 58, *unknown_array,
        71, 72, -73, 74, 75, 81, -82, 83, 91, 92, -93, 94, b"test_debris")
    document = {
        "classDef": "HercSimDat", "fileName": "TOMAHAWK",
        "speed": {"turn": "10", "reverse": "-20", "forward": "30",
                  "accel_and_decel": "-40", "turning_decel": "50"},
        "camera_boneId": 6, "input_throttle_hercFlag": 7,
        "unk14_anim_leg1": 8, "unk14_anim_leg2": 9, "unk14_anim_leg3": 10,
        "offset_y_adjust": -11, "unk22_Val100Razor0": 12, "ai_aim_targ_offset": 13,
        "torso_config": {
            "razor_override": "-14", "input_flags": "-18",
            "rotation": {"rotate_speed": "12.34", "accel_rate": "-56.78", "angle_max": "327.67"},
            "pitch": {"rate_max": "0.01", "rate": "-0.01", "angle_max": "1.2", "angle_min": "-327.68"}},
        "unk44_move_anim_rate": 23, "model_lod_boneIds": bones,
        "unk66_Val1000": 31, "legsCritFlags1": 32, "legsCritFlags2": 33,
        "modelLegsTotal": 34, "modelFlagNoDebris": 35, "unk76_Val": 36,
        "inputFlagFlyer": 37, "maybe_hud_id": 38, "unk82_val": 39, "unk84_val": 40,
        "name": "TestHerc", "cam_adj_ofs_y": -41, "cam_adj_ofs_x": 42,
        "cameraExtOrgOffset": -43, "unk108_camExtVal1": 51, "unk110_camExtVal2": 52,
        "modelFlagsShadow1": 53, "modelFlagsShadow2": -254,
        "unk116_val": 55, "unk118_val": 56, "unk120_val": 57, "unk122_mdlFlagVal": 58,
        "unk124_all500": unknown_array, "modelSkinId": 71, "unk150_val": 72,
        "unk152_val": -73, "unk154_fixedVal": 74, "unk156_400or800": 75,
        "unk170_val": 81, "unk172_val": -82, "unk174_250or275": 83,
        "shieldMaxTotal": 91, "unk192_val": 92, "physicsFrictionCoef": -93,
        "physicsFrctionAccel": 94, "debrisFile": "test_debris"}
    return bytearray(data), document


def leaves(node, path=()):
    if isinstance(node, dict):
        for key, child in node.items():
            yield from leaves(child, path + (key,))
    elif isinstance(node, list):
        for index, child in enumerate(node):
            yield from leaves(child, path + (index,))
    else:
        yield path, node


class ValidatorTests(unittest.TestCase):
    def setUp(self):
        self.data, self.document = fixture()

    def validate(self):
        return validator.validate(self.data, self.document, "TOMAHAWK")

    def test_complete_valid_record_and_read_only_inputs(self):
        before = bytes(self.data), deepcopy(self.document)
        self.assertEqual(len(self.data), 216)
        self.assertEqual(self.validate(), [])
        self.assertEqual(before, (bytes(self.data), self.document))

    def test_every_json_value_is_checked(self):
        for path, value in leaves(self.document):
            with self.subTest(path=path):
                changed = deepcopy(self.document)
                node = changed
                for key in path[:-1]:
                    node = node[key]
                node[path[-1]] = value + 1 if type(value) is int else value + "INVALID"
                self.assertTrue(validator.validate(self.data, changed, "TOMAHAWK"))

    def test_signed_short_boundaries(self):
        for value in (-32768, -1, 0, 1, 32767):
            with self.subTest(value=value):
                struct.pack_into("<h", self.data, 10, value)
                self.document["camera_boneId"] = value
                self.assertEqual(self.validate(), [])

    def test_unsigned_short_bits_require_signed_json(self):
        for raw, signed in ((0, 0), (32767, 32767), (32768, -32768), (65535, -1), (65282, -254)):
            with self.subTest(raw=raw):
                struct.pack_into("<H", self.data, 114, raw)
                self.document["modelFlagsShadow2"] = signed
                self.assertEqual(self.validate(), [])
                if raw != signed:
                    self.document["modelFlagsShadow2"] = raw
                    self.assertEqual(self.validate()[0].expected, signed)

    def test_unsigned_byte_bits_require_signed_json(self):
        for raw, signed in ((0, 0), (127, 127), (128, -128), (255, -1)):
            with self.subTest(raw=raw):
                self.data[46] = raw
                self.document["model_lod_boneIds"][0] = signed
                self.assertEqual(self.validate(), [])
                if raw != signed:
                    self.document["model_lod_boneIds"][0] = raw
                    self.assertEqual(self.validate()[0].offset, 46)

    def test_signed_short_array_boundaries(self):
        for value in (-32768, -1, 0, 32767):
            struct.pack_into("<h", self.data, 146, value)
            self.document["unk124_all500"][11] = value
            self.assertEqual(self.validate(), [])

    def test_negative_flags_in_string_maps(self):
        struct.pack_into("<H", self.data, 34, 65535)
        self.document["torso_config"]["input_flags"] = "-1"
        self.assertEqual(self.validate(), [])
        for wrong in ("65535", -1, True):
            self.document["torso_config"]["input_flags"] = wrong
            self.assertEqual(self.validate()[0].offset, 34)

    def test_scaled_short_boundaries(self):
        for raw, text in ((-32768, "-327.68"), (32767, "327.67"), (-1, "-0.01"),
                          (0, "0.0"), (1, "1e-2"), (1234, "12.340")):
            with self.subTest(raw=raw):
                struct.pack_into("<h", self.data, 28, raw)
                self.document["torso_config"]["rotation"]["rotate_speed"] = text
                self.assertEqual(self.validate(), [])

    def test_incorrect_scaled_conversions(self):
        for wrong in ("1234", "123.4", "12.35", "12", 12.34, "NaN", "Infinity",
                      "1e9999", "", "12_34", " 12.34", None):
            with self.subTest(wrong=wrong):
                self.document["torso_config"]["rotation"]["rotate_speed"] = wrong
                issue = self.validate()[0]
                self.assertEqual(issue.offset, 28)
                self.assertEqual(issue.field, "torso_config.rotation.rotate_speed")

    def test_no_integer_coercion(self):
        for wrong in (6.0, "6", True, None, 65542, -65530):
            self.document["camera_boneId"] = wrong
            self.assertTrue(self.validate())
        self.document["camera_boneId"] = 1
        struct.pack_into("<h", self.data, 10, 1)
        self.document["camera_boneId"] = True  # Python True == 1 must not pass.
        self.assertTrue(self.validate())

    def test_missing_fields_at_each_level(self):
        for path in (("camera_boneId",), ("speed",), ("speed", "turn"),
                     ("torso_config", "pitch", "angle_min"), ("classDef",)):
            with self.subTest(path=path):
                changed = deepcopy(self.document)
                node = changed
                for key in path[:-1]:
                    node = node[key]
                del node[path[-1]]
                issue = validator.validate(self.data, changed, "TOMAHAWK")[0]
                self.assertEqual(issue.field, ".".join(path))
                self.assertEqual(issue.actual, "<missing>")

    def test_extra_fields_at_each_level(self):
        for path in ((), ("speed",), ("torso_config", "pitch")):
            changed = deepcopy(self.document)
            node = changed
            for key in path:
                node = node[key]
            node["invented"] = 42
            issue = validator.validate(self.data, changed, "TOMAHAWK")[0]
            self.assertEqual(issue.field, ".".join(path + ("invented",)))
            self.assertIsNone(issue.offset)

    def test_wrong_shapes_and_array_lengths(self):
        for key, wrong in (("speed", []), ("model_lod_boneIds", {}),
                           ("model_lod_boneIds", [0] * 19), ("unk124_all500", [0] * 13)):
            changed = deepcopy(self.document)
            changed[key] = wrong
            self.assertTrue(validator.validate(self.data, changed, "TOMAHAWK"))
        for root in ([], None, {"simdata": self.document}):
            self.assertTrue(validator.validate(self.data, root, "TOMAHAWK"))

    def test_truncated_and_extra_records(self):
        for size in range(216):
            issue = validator.validate(self.data[:size], self.document, "TOMAHAWK")[0]
            self.assertEqual(issue.offset, size)
            self.assertEqual(issue.actual, size)
        for data in (self.data + b"\0", self.data * 2):
            issue = validator.validate(data, self.document, "TOMAHAWK")[0]
            self.assertEqual(issue.offset, 216)

    def test_invalid_schema_offsets(self):
        self.assertEqual(validator.validate_layout(), [])
        for offset in (-2, 1, 2, 216):  # bounds, alignment, overlap, end
            fields = (replace(validator.FIELDS[0], offset=offset),) + validator.FIELDS[1:]
            self.assertTrue(validator.validate_layout(fields))
        self.assertTrue(validator.validate_layout(validator.FIELDS[1:]))  # gap

    def test_unknown_values_and_omitted_bytes_are_not_reinterpreted(self):
        struct.pack_into("<h", self.data, 22, -12345)
        self.document["unk22_Val100Razor0"] = -12345
        self.data[158:170] = bytes(range(240, 252))
        self.assertEqual(self.validate(), [])
        raw = {offset: value for offset, _, value in validator.unrepresented_bytes(self.data)}
        self.assertEqual(raw[158], bytes(range(240, 252)))

    def test_strings_full_length_padding_and_empty_name_quirk(self):
        self.data[86:98] = b"ABCDEFGHIJKL"
        self.document["name"] = "ABCDEFGHIJKL"
        self.assertEqual(self.validate(), [])
        self.data[86:98] = b"A\0" + b"X" * 10
        self.document["name"] = "A"
        self.assertEqual(self.validate(), [])
        self.assertIn((87, "name.padding", b"\0" + b"X" * 10),
                      validator.unrepresented_bytes(self.data))
        self.data[86:98] = b"\0" * 12
        self.document["name"] = "\0"  # Verified service includes byte 0 unconditionally.
        self.data[204:216] = b"\0" * 12
        self.document["debrisFile"] = ""
        self.assertEqual(self.validate(), [])

    def test_unaudited_non_ascii_string_rejected(self):
        self.data[88] = 255
        issue = self.validate()[0]
        self.assertEqual(issue.offset, 88)
        self.assertEqual(issue.field, "name")

    def test_diagnostic_contains_record_offset_field_and_values(self):
        self.document["modelFlagsShadow2"] = 65282
        text = str(self.validate()[0])
        for required in ("record 0", "byte offset 114", "field modelFlagsShadow2",
                         "expected -254", "actual 65282"):
            self.assertIn(required, text)

    def run_cli(self, data, json_text):
        with tempfile.TemporaryDirectory() as directory:
            dat = Path(directory) / "TOMAHAWK.DAT"
            output = Path(directory) / "TOMAHAWK.json"
            dat.write_bytes(data)
            output.write_text(json_text, encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools/validate_tomahawk_dat.py"), str(dat), str(output)],
                text=True, capture_output=True, check=False)
            self.assertEqual(dat.read_bytes(), data)
            self.assertEqual(output.read_text(encoding="utf-8"), json_text)
            return result

    def test_cli_success_and_validation_failure(self):
        result = self.run_cli(self.data, json.dumps(self.document))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("PASS: one 216-byte record", result.stdout)
        self.assertIn("not exported; raw hex", result.stdout)
        del self.document["name"]
        result = self.run_cli(self.data, json.dumps(self.document))
        self.assertEqual(result.returncode, 1)
        self.assertIn("byte offset 86 | field name", result.stderr)
        result = self.run_cli(self.data[:-1], json.dumps(self.document))
        self.assertEqual(result.returncode, 1)
        self.assertIn("field <record.length>", result.stderr)

    def test_cli_malformed_duplicate_and_nonfinite_json(self):
        for text in ('{', '{"name":"a","name":"b"}', '{"speed":{"turn":1,"turn":2}}',
                     '{"value":NaN}', '{"value":Infinity}'):
            with self.subTest(text=text):
                result = self.run_cli(self.data, text)
                self.assertEqual(result.returncode, 2)
                self.assertIn("record 0 | byte offset n/a | field <input>", result.stderr)

    def test_cli_missing_input(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = str(Path(directory) / "missing")
            result = subprocess.run([sys.executable, str(ROOT / "tools/validate_tomahawk_dat.py"),
                                     missing, missing], text=True, capture_output=True, check=False)
            self.assertEqual(result.returncode, 2)
            self.assertIn("field <input>", result.stderr)


if __name__ == "__main__":
    unittest.main()
