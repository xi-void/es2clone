#!/usr/bin/env python3
"""Sample reconstructed interior terrain offline; see docs/terrain-triangle-sampler.md.

Inputs are the existing height-grid.json and cell-modes.json. Coordinates and
results are integers in internal units. No outer-strip or mission-state policy.
"""
import argparse
import hashlib
import json
from pathlib import Path

from inventory_zone_cells import inventory
from reconstruct_zone_heightmap import signed32


def integer(value, name, low, high):
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f'{name} must be an integer in {low}..{high}; got {value!r}')
    return value


def interpolate_cell(heights, mode, shift, fx, fy):
    """Return (height_i32, branch_address) for an explicitly selected cell.

    Corner order: 00,10,01,11. Inclusive local offsets 0..L allow diagnostic
    corner/edge evaluation, not reassignment of a global query to a boundary cell.
    Equations follow the saved REA/Ghidra instructions at 0x0046e28d..0x0046e388.
    """
    integer(mode, 'mode', 0, 2)
    integer(shift, 'shift', 0, 30)
    length = 1 << shift
    integer(fx, 'fx', 0, length)
    integer(fy, 'fy', 0, length)
    if not isinstance(heights, (list, tuple)) or len(heights) != 4:
        raise ValueError('Expected four heights in 00,10,01,11 order')
    h00, h10, h01, h11 = [integer(h, 'height', -2147483648, 2147483647) for h in heights]
    if mode == 2 and fx <= fy:
        anchor, a, u, b, v, branch = h01, h11, fx, h00, length - fy, '0x0046e29a'
    elif mode == 2:
        anchor, a, u, b, v, branch = h10, h00, length - fx, h11, fy, '0x0046e2cc'
    elif mode == 0 and length - fx < fy:
        anchor, a, u, b, v, branch = h11, h01, length - fx, h10, length - fy, '0x0046e332'
    else:
        anchor, a, u, b, v = h00, h10, fx, h01, fy
        branch = '0x0046e30c' if mode == 0 else '0x0046e366'
    # Wrap SUB and IMUL before SAR; floor negative products, shift each separately.
    first = signed32(signed32(a - anchor) * u) >> shift
    second = signed32(signed32(b - anchor) * v) >> shift
    return signed32(signed32(first + anchor) + second), branch


class TerrainSampler:
    def __init__(self, grid, modes):
        # Reuse unsigned-byte recovery and the verified selector to reject stale
        # or mismatched inventory; sample using the supplied, checked mode array.
        expected = inventory(grid)
        if not isinstance(modes, dict):
            raise ValueError('Expected a cell-mode inventory object')
        for key in ('schema_version', 'sample_width', 'sample_height', 'cell_width',
                    'cell_height', 'interior_cell_count', 'corner_offsets', 'cell_modes'):
            # JSON comparison also distinguishes bool/float from original integer fields.
            if json.dumps(modes.get(key), sort_keys=True) != json.dumps(expected[key], sort_keys=True):
                raise ValueError(f'Cell-mode inventory mismatch: {key}')
        spacing = grid.get('horizontal_spacing')
        if not isinstance(spacing, dict) or spacing.get('status') != 'decoded':
            raise ValueError('Decoded horizontal_spacing metadata is required')
        self.shift = integer(spacing.get('shift_i32'), 'shift_i32', 0, 30)
        self.length = 1 << self.shift
        if (type(spacing.get('spacing_internal')) is not int
                or spacing['spacing_internal'] != self.length):
            raise ValueError('spacing_internal must equal 1 << shift_i32')
        if 'dat_words_i32' in grid:
            words = grid['dat_words_i32']
            if (not isinstance(words, list) or len(words) != 4
                    or type(words[2]) is not int or words[2] != self.shift):
                raise ValueError('DAT word three disagrees with horizontal spacing')
        self.width, self.height = grid['width'], grid['height']
        for size in (self.width, self.height):
            if size & (size - 1):
                raise ValueError('Original grid dimensions must be powers of two')
            if (size - 1) * self.length > 2147483647:
                raise ValueError('Grid span exceeds supported signed 32-bit coordinates')
        self.heights = tuple(tuple(row) for row in grid['heights'])
        self.modes = tuple(tuple(row) for row in modes['cell_modes'])

    @classmethod
    def from_files(cls, grid_path, modes_path):
        grid_raw, modes_raw = grid_path.read_bytes(), modes_path.read_bytes()
        grid, modes = json.loads(grid_raw), json.loads(modes_raw)
        if not isinstance(modes, dict) or not isinstance(modes.get('source_grid'), dict):
            raise ValueError('Cell-mode inventory needs source_grid provenance')
        if modes['source_grid'].get('sha256') != hashlib.sha256(grid_raw).hexdigest():
            raise ValueError('Cell-mode inventory source_grid SHA-256 does not match height grid')
        sampler = cls(grid, modes)
        sampler.sources = {
            'height_grid': {'path': str(grid_path), 'sha256': hashlib.sha256(grid_raw).hexdigest()},
            'cell_modes': {'path': str(modes_path), 'sha256': hashlib.sha256(modes_raw).hexdigest()},
        }
        return sampler

    def cell(self, x, y, fx, fy):
        """Explicit interior cell, including its endpoints for diagnostic checks."""
        integer(x, 'cell x', 0, self.width - 2)
        integer(y, 'cell y', 0, self.height - 2)
        corners = (self.heights[y][x], self.heights[y][x + 1],
                   self.heights[y + 1][x], self.heights[y + 1][x + 1])
        mode = self.modes[y][x]
        value, branch = interpolate_cell(corners, mode, self.shift, fx, fy)
        return {'cell': [x, y], 'offset_internal': [fx, fy], 'mode': mode,
                'branch_address': branch, 'height_i32': value}

    def sample(self, x, y):
        """Grid-relative internal XY; reject outer strips instead of guessing."""
        integer(x, 'x', 0, (self.width - 1) * self.length - 1)
        integer(y, 'y', 0, (self.height - 1) * self.length - 1)
        i, j = x >> self.shift, y >> self.shift
        return dict(self.cell(i, j, x - (i << self.shift), y - (j << self.shift)),
                    position_internal=[x, y])

    def validation_report(self):
        """Check every explicit cell corner and every globally supported vertex."""
        checks, mismatches, examples = 0, 0, []
        vertices, vertex_mismatches = 0, 0
        for y in range(self.height - 1):
            for x in range(self.width - 1):
                for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
                    result = self.cell(x, y, dx * self.length, dy * self.length)
                    expected = self.heights[y + dy][x + dx]
                    checks += 1
                    if result['height_i32'] != expected:
                        mismatches += 1
                        if len(examples) < 16:
                            examples.append(dict(result, expected_height_i32=expected))
                vertices += 1
                if self.sample(x * self.length, y * self.length)['height_i32'] != self.heights[y][x]:
                    vertex_mismatches += 1
        return {'cell_corner_checks': checks, 'cell_corner_mismatches': mismatches,
                'unique_vertices_covered_by_cell_corners': self.width * self.height,
                'global_vertex_checks': vertices, 'global_vertex_mismatches': vertex_mismatches,
                'corner_mismatch_examples': examples,
                'note': 'Explicit cell corners include endpoint offsets L; global queries exclude '
                        'the outer strips. Overflow can break vertex equality on extreme synthetic grids.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('height_grid', type=Path)
    parser.add_argument('cell_modes', type=Path)
    parser.add_argument('--point', nargs=2, type=int, action='append', default=[], metavar=('X', 'Y'),
                        help='Repeat for grid-relative XY in internal integer units')
    parser.add_argument('--report', type=Path, help='Write validation/sample JSON to a new ignored file')
    args = parser.parse_args()
    try:
        sampler = TerrainSampler.from_files(args.height_grid, args.cell_modes)
        points = [sampler.sample(x, y) for x, y in args.point]
        report = {'schema_version': 1, 'sources': sampler.sources,
                  'scope': 'Interior only; bare reconstructed grid, no mission preprocessing or metre conversion',
                  'spacing_internal': sampler.length, 'shift_i32': sampler.shift,
                  'validation': sampler.validation_report(), 'samples': points}
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            with args.report.open('x') as output:
                output.write(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'Terrain sampling failed: {error}\n')


if __name__ == '__main__':
    main()
