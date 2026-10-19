#!/usr/bin/env python3
"""Synthetic terrain-mode fixtures; no game resources, decoder or Ghidra needed."""
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
import inventory_zone_cells as cells
import reconstruct_zone_heightmap as terrain
from zone_heightmap_tests import png_pixels


def grid(samples, multiplier=200, base=0):
    # Samples are in reconstructed grid order, not original bitmap file order.
    return {'schema_version': 1, 'width': len(samples[0]), 'height': len(samples),
            'row_order': cells.ROW_ORDER, 'multiplier_i32': multiplier, 'base_i32': base,
            'heights': [[terrain.signed32(s * multiplier + base) for s in row] for row in samples],
            'horizontal_spacing': {'sentinel': 'must stay unchanged'}}


class ZoneCellModes(unittest.TestCase):
    def test_three_branches_and_unsigned_corners(self):
        for samples, mode in [([[255, 0], [0, 255]], 0),
                              ([[0, 255], [255, 0]], 2),
                              ([[128, 140], [200, 212]], 1),
                              ([[159, 159], [159, 159]], 1)]:
            with self.subTest(samples=samples):
                result = cells.inventory(grid(samples))
                self.assertEqual(result['cell_modes'], [[mode]])
                counts = {e['mode']: (e['count'], e['percentage']) for e in result['modes']}
                self.assertEqual(counts[mode], (1, 100.0))
                for other in {0, 1, 2} - {mode}:
                    self.assertEqual(counts[other], (0, 0.0))

    def test_documented_diagonal_endpoints(self):
        result = cells.inventory(grid([[4, 8], [12, 16]]))
        definitions = {e['mode']: e for e in result['modes']}
        self.assertEqual(definitions[0]['diagonal_offsets'], [[1, 0], [0, 1]])
        self.assertEqual(definitions[2]['diagonal_offsets'], [[0, 0], [1, 1]])
        self.assertIsNone(definitions[1]['diagonal_offsets'])
        self.assertEqual(result['corner_offsets'],
                         {'s00': [0, 0], 's10': [1, 0], 's01': [0, 1], 's11': [1, 1]})

    def test_each_corner_contribution_and_reflection(self):
        for y, x, mode in [(0, 0, 0), (1, 1, 0), (0, 1, 2), (1, 0, 2)]:
            with self.subTest(corner=(x, y)):
                samples = [[20, 20], [20, 20]]
                samples[y][x] = 21
                self.assertEqual(cells.inventory(grid(samples))['cell_modes'], [[mode]])
                reflected = [row[::-1] for row in samples]
                self.assertEqual(cells.inventory(grid(reflected))['cell_modes'], [[2 - mode]])

    def test_grid_orientation_counts_and_no_boundary_cells(self):
        # Deltas, already in grid order: [2, 0, -2], [-4, 3, 1], [0, 0, 0].
        samples = [[10, 10, 10, 10], [10, 12, 12, 10],
                   [10, 8, 11, 10], [11, 9, 12, 11]]
        result = cells.inventory(grid(samples))
        self.assertEqual(result['cell_modes'], [[0, 1, 2], [2, 0, 0], [1, 1, 1]])
        self.assertEqual((result['cell_width'], result['cell_height']), (3, 3))
        self.assertEqual(result['interior_cell_count'], 9)
        counts = {e['mode']: e for e in result['modes']}
        for mode, count in [(0, 3), (1, 4), (2, 2)]:
            self.assertEqual(counts[mode]['count'], count)
            self.assertAlmostEqual(counts[mode]['percentage'], count * 100 / 9)
        self.assertAlmostEqual(sum(e['percentage'] for e in result['modes']), 100)

    def test_reconstruction_row_reversal_is_not_repeated(self):
        bitmap = {'frame_count': 1, 'width': 4, 'height': 2,
                  'indices': [10, 8, 9, 10, 10, 10, 10, 10]}
        reconstructed = terrain.reconstruct(struct.pack('<4i', 7, 7, 14, 200), bitmap)
        self.assertEqual(cells.inventory(reconstructed)['cell_modes'], [[2, 0, 0]])

    def test_recover_samples_before_sign_comparison(self):
        for multiplier, base in [(-200, 5000), (2147483647, 0), (200, 2147483640)]:
            with self.subTest(multiplier=multiplier, base=base):
                self.assertEqual(cells.inventory(grid([[255, 0], [0, 128]], multiplier, base))
                                 ['cell_modes'], [[0]])

    def test_ambiguous_sample_recovery_rejected(self):
        for multiplier in [0, -2147483648]:
            with self.subTest(multiplier=multiplier):
                with self.assertRaisesRegex(ValueError, 'Ambiguous sample recovery'):
                    cells.inventory(grid([[0, 1], [2, 3]], multiplier))

    def test_malformed_grids_fail_clearly(self):
        valid = grid([[1, 2], [3, 4]])
        for key, value in [('schema_version', 2), ('schema_version', True), ('row_order', 'source'),
                           ('width', 1), ('height', True), ('heights', [[200, 400]]),
                           ('heights', [[200], [600, 800]]), ('heights', [[201, 400], [600, 800]]),
                           ('heights', [[True, 400], [600, 800]]),
                           ('heights', [[200.0, 400], [600, 800]]),
                           ('multiplier_i32', None), ('base_i32', 2147483648)]:
            with self.subTest(key=key, value=value):
                with self.assertRaises(ValueError):
                    cells.inventory(dict(valid, **{key: value}))
        for key in valid:
            if key == 'horizontal_spacing':
                continue
            broken = dict(valid)
            del broken[key]
            with self.subTest(missing=key), self.assertRaises(ValueError):
                cells.inventory(broken)

    def test_inventory_preserves_entire_input_and_metadata(self):
        source = grid([[4, 1], [2, 5]])
        before = copy.deepcopy(source)
        cells.inventory(source)
        self.assertEqual(source, before)

    def test_cli_artifacts_preview_orientation_and_non_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'height-grid.json'
            source.write_text(json.dumps(grid([[10, 10, 10, 10], [10, 12, 12, 10]])))
            original = source.read_bytes()
            command = [sys.executable, str(ROOT / 'tools/inventory_zone_cells.py'), str(source)]
            for preview in [False, True]:
                output = root / str(preview)
                args = command + ['--output', str(output)] + (['--preview'] if preview else [])
                process = subprocess.run(args, text=True, capture_output=True)
                self.assertEqual(process.returncode, 0, process.stderr)
                result = json.loads((output / 'cell-modes.json').read_text())
                self.assertEqual(result['cell_modes'], [[0, 1, 2]])
                self.assertEqual(result['source_grid']['sha256'], hashlib.sha256(original).hexdigest())
                self.assertEqual((output / 'cell-modes.png').exists(), preview)
                if preview:
                    self.assertEqual(png_pixels((output / 'cell-modes.png').read_bytes()),
                                     ((3, 1), bytes([0, 0, 127, 255])))
                before = {p.name: p.read_bytes() for p in output.iterdir()}
                self.assertNotEqual(subprocess.run(args, capture_output=True).returncode, 0)
                self.assertEqual({p.name: p.read_bytes() for p in output.iterdir()}, before)
            self.assertEqual(source.read_bytes(), original)
            source.write_text('{truncated')
            bad_output = root / 'bad'
            process = subprocess.run(command + ['--output', str(bad_output)], text=True, capture_output=True)
            self.assertNotEqual(process.returncode, 0)
            self.assertIn('Cell-mode inventory failed:', process.stderr)
            self.assertFalse(bad_output.exists())

    def test_preview_legend_is_fixed_when_only_one_mode_is_present(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'modes'
            cells.write_inventory(cells.inventory(grid([[0, 255], [255, 0]])), output, preview=True)
            self.assertEqual(png_pixels((output / 'cell-modes.png').read_bytes()),
                             ((1, 1), bytes([0, 255])))


if __name__ == '__main__':
    unittest.main()
