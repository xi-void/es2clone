#!/usr/bin/env python3
"""Synthetic regression vectors for the original interior triangle arithmetic."""
import copy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inventory_zone_cells import inventory
from reconstruct_zone_heightmap import reconstruct
from sample_zone_terrain import TerrainSampler, interpolate_cell


def fixture(samples, shift=2, multiplier=1):
    grid = reconstruct(struct.pack('<4i', 7, 7, shift, multiplier), {
        'frame_count': 1, 'width': len(samples[0]), 'height': len(samples),
        'indices': [v for row in reversed(samples) for v in row]})
    return grid, inventory(grid)


class TriangleTests(unittest.TestCase):
    def test_positive_negative_zero_full_cell_vectors(self):
        # Synthetic vectors cross-checked against the retained Ghidra instruction
        # evaluation; no original resource bytes or generated assembly in tests.
        for heights, mode, expected in [
            ([10, 30, 20, 50], 0, [[10, 15, 20, 25, 30], [12, 17, 22, 27, 35],
                                  [15, 20, 25, 32, 40], [17, 22, 30, 37, 45], [20, 27, 35, 42, 50]]),
            ([10, 30, 20, 35], 2, [[10, 15, 20, 25, 30], [12, 15, 21, 26, 31],
                                  [15, 18, 22, 27, 32], [17, 20, 24, 28, 33], [20, 23, 27, 31, 35]]),
            ([10, 30, 20, 40], 1, [[10, 15, 20, 25, 30], [12, 17, 22, 27, 32],
                                  [15, 20, 25, 30, 35], [17, 22, 27, 32, 37], [20, 25, 30, 35, 40]])]:
            with self.subTest(mode=mode):
                sampler = TerrainSampler(*fixture([heights[:2], heights[2:]]))
                self.assertEqual(sampler.modes, ((mode,),))
                actual = [[sampler.cell(0, 0, x, y)['height_i32'] for x in range(5)] for y in range(5)]
                self.assertEqual(actual, expected)

    def test_diagonal_equality_and_adjacent_branches(self):
        h = [10, 30, 20, 50]
        for x, y, branch in [(1, 2, '0x0046e30c'), (1, 3, '0x0046e30c'), (1, 4, '0x0046e332')]:
            self.assertEqual(interpolate_cell(h, 0, 2, x, y)[1], branch)
        for x, y, branch in [(1, 2, '0x0046e29a'), (2, 2, '0x0046e29a'), (3, 2, '0x0046e2cc')]:
            self.assertEqual(interpolate_cell(h, 2, 2, x, y)[1], branch)
        for x, y in [(0, 0), (4, 4), (3, 1)]:
            self.assertEqual(interpolate_cell(h, 1, 2, x, y)[1], '0x0046e366')

    def test_each_product_is_shifted_separately(self):
        # -1/4 floors to -1 twice: 5-1-1=3, not truncation (5) or combined floor (4).
        self.assertEqual(interpolate_cell([5, 4, 4, 4], 0, 2, 1, 1)[0], 3)
        self.assertEqual(interpolate_cell([0, 3, 3, 6], 1, 2, 1, 1)[0], 0)

    def test_anchor_choice_affects_rounding(self):
        # Rearranging into h00 + slope*fx + slope*(fy-fx) would give 0, not -1.
        # Separate rounding can put a result one unit below the smallest corner.
        self.assertEqual(interpolate_cell([0, 0, 2, 1], 2, 2, 1, 1)[0], -1)
        # The analogous h00 + slope*(fx-fy) + slope*fy would give 1, not 0.
        self.assertEqual(interpolate_cell([0, 2, 0, 1], 2, 2, 3, 1)[0], 0)
        # Mode 0 upper triangle: h11 + floor(-2/4) + floor(-3/4) = 3.
        self.assertEqual(interpolate_cell([1, 2, 3, 5], 0, 2, 3, 3)[0], 3)

    def test_low32_subtract_multiply_add(self):
        cases = [([2147483647, -2147483648, 2147483647, 0], 0, 2, 1, 1, 2147483647),
                 ([0, 2147483647, 0, 0], 0, 2, 2, 0, -1),
                 ([-2147483648, 2147483647, 2147483647, -2147483648], 2, 14, 8191, 8192, 2147483647),
                 ([2147483647, -2147483648, -2147483648, -2147483647], 1, 0, 1, 1, -2147483647)]
        for h, mode, shift, x, y, expected in cases:
            with self.subTest(h=h, mode=mode):
                self.assertEqual(interpolate_cell(h, mode, shift, x, y)[0], expected)

    def test_shift_limits_and_constant_heights(self):
        for shift in (0, 1, 14, 30):
            for h in (-2147483648, -1, 0, 2147483647):
                for mode in (0, 1, 2):
                    self.assertEqual(interpolate_cell([h] * 4, mode, shift, 1 << shift, 0)[0], h)

    def test_vertices_unsigned_samples_and_negative_multiplier(self):
        samples = [[128, 140, 150, 180], [145, 130, 200, 210],
                   [155, 155, 155, 155], [170, 200, 220, 255]]
        for multiplier in (200, -200):
            sampler = TerrainSampler(*fixture(samples, 14, multiplier))
            report = sampler.validation_report()
            self.assertEqual(report['cell_corner_checks'], 36)
            self.assertEqual(report['unique_vertices_covered_by_cell_corners'], 16)
            self.assertEqual(report['cell_corner_mismatches'], 0)
            self.assertEqual(report['global_vertex_checks'], 9)
            self.assertEqual(report['global_vertex_mismatches'], 0)

    def test_global_addressing_row_order_and_grid_lines(self):
        sampler = TerrainSampler(*fixture([[10, 30, 40, 60], [20, 35, 60, 80]]))
        self.assertEqual(sampler.sample(4, 0)['cell'], [1, 0])
        self.assertEqual(sampler.sample(4, 0)['offset_internal'], [0, 0])
        self.assertEqual(sampler.sample(4, 0)['height_i32'], 30)
        self.assertEqual(sampler.sample(5, 2)['height_i32'], 34)
        self.assertEqual(sampler.sample(11, 3)['cell'], [2, 0])

    def test_outer_strips_rejected_and_local_endpoints_explicit(self):
        sampler = TerrainSampler(*fixture([[10, 30], [20, 50]]))
        self.assertEqual(sampler.cell(0, 0, 4, 4)['height_i32'], 50)
        for x, y in [(-1, 0), (0, -1), (4, 0), (0, 4), (4, 4), (True, 0), (1.0, 0)]:
            with self.subTest(x=x, y=y), self.assertRaises(ValueError):
                sampler.sample(x, y)
        for x, y in [(1, 0), (0, 1), (-1, 0)]:
            with self.assertRaises(ValueError):
                sampler.cell(x, y, 0, 0)

    def test_reject_bad_interpolation_arguments(self):
        good = [[1, 2, 3, 4], 0, 2, 1, 1]
        for index, bad in [(0, [1, 2, 3]), (0, [1, 2, 3, True]), (0, [1, 2, 3, 2**31]),
                           (1, 3), (1, True), (2, -1), (2, 31), (3, -1), (4, 5), (3, 1.5)]:
            args = copy.deepcopy(good)
            args[index] = bad
            with self.subTest(index=index, bad=bad), self.assertRaises(ValueError):
                interpolate_cell(*args)

    def test_inventory_and_spacing_must_match(self):
        grid, modes = fixture([[10, 30], [20, 50]])
        for key, bad in [('cell_modes', [[2]]), ('cell_modes', [[False]]), ('cell_modes', [[0.0]]),
                         ('cell_modes', [[]]), ('cell_width', 2), ('schema_version', 2),
                         ('corner_offsets', {}), ('interior_cell_count', None)]:
            with self.subTest(key=key, bad=bad), self.assertRaises(ValueError):
                TerrainSampler(grid, dict(modes, **{key: bad}))
        for key, bad in [('status', 'unsupported_shift'), ('shift_i32', 31), ('spacing_internal', 5)]:
            broken = copy.deepcopy(grid)
            broken['horizontal_spacing'][key] = bad
            with self.assertRaises(ValueError):
                TerrainSampler(broken, modes)
        broken = copy.deepcopy(grid)
        broken['dat_words_i32'][2] = 3
        with self.assertRaisesRegex(ValueError, 'DAT word three'):
            TerrainSampler(broken, modes)
        for key in ('heights', 'horizontal_spacing'):
            broken = copy.deepcopy(grid)
            del broken[key]
            with self.assertRaises(ValueError):
                TerrainSampler(broken, modes)

    def test_signed_coordinate_span_limit(self):
        with self.assertRaisesRegex(ValueError, 'span exceeds'):
            TerrainSampler(*fixture([[1, 2, 3, 4], [2, 3, 4, 5]], 30))

    def test_inputs_remain_unchanged(self):
        grid, modes = fixture([[1, 2], [3, 4]])
        before = copy.deepcopy((grid, modes))
        sampler = TerrainSampler(grid, modes)
        sampler.validation_report()
        sampler.sample(1, 1)
        self.assertEqual((grid, modes), before)

    def test_cli_report_provenance_errors_and_non_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            grid, modes = fixture([[10, 30], [20, 35]])
            source, mode_file, report = root / 'heights.json', root / 'modes.json', root / 'report.json'
            source.write_text(json.dumps(grid))
            modes['source_grid'] = {'sha256': hashlib.sha256(source.read_bytes()).hexdigest()}
            mode_file.write_text(json.dumps(modes))
            original = (source.read_bytes(), mode_file.read_bytes())
            command = [sys.executable, str(ROOT / 'tools/sample_zone_terrain.py'), str(source), str(mode_file)]
            args = command + ['--point', '1', '2']
            run = subprocess.run(args, text=True, capture_output=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertFalse(report.exists())
            self.assertEqual(json.loads(run.stdout)['samples'][0]['height_i32'], 18)
            run = subprocess.run(args + ['--report', str(report)], text=True, capture_output=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            content = report.read_bytes()
            decoded = json.loads(content)
            self.assertEqual(decoded['validation']['cell_corner_mismatches'], 0)
            self.assertEqual(decoded['sources']['height_grid']['sha256'], modes['source_grid']['sha256'])
            self.assertNotEqual(subprocess.run(args + ['--report', str(report)], capture_output=True).returncode, 0)
            self.assertEqual(report.read_bytes(), content)
            self.assertNotEqual(subprocess.run(command + ['--point', '4', '0'], capture_output=True).returncode, 0)
            self.assertEqual((source.read_bytes(), mode_file.read_bytes()), original)
            source.write_bytes(original[0] + b' ')
            run = subprocess.run(command, text=True, capture_output=True)
            self.assertNotEqual(run.returncode, 0)
            self.assertIn('SHA-256', run.stderr)
            mode_file.write_text('{truncated')
            run = subprocess.run(command, text=True, capture_output=True)
            self.assertNotEqual(run.returncode, 0)
            self.assertIn('Terrain sampling failed:', run.stderr)


if __name__ == '__main__':
    unittest.main()
