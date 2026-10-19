#!/usr/bin/env python3
"""Read-only inventory of the Tomahawk/Achilles DTS animation record family.

Narrows and hardens imported/herc-model-audit/inspect_dts.py; geometry stays
opaque. Layout references: src/original_models.cpp and Subject9x's pinned
DTSModelTransformer (see docs/dts-animation-inventory.md and
docs/licenses/herc-works-mdk-MIT.txt). No Java code is executed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


MAX_BYTES = 4 * 1024 * 1024
TAGS = {
    0x140005: "TSPart", 0x140007: "TSPartList", 0x140008: "TSShape",
    0x14000a: "TSBSPGroup", 0x14000b: "TSCellAnimPart",
    0x14000c: "TSDetailPart", 0x140014: "TSGroup", 0x140015: "TSBSPPart",
    0x1e0001: "ANSequence", 0x1e0002: "ANAnimList", 0x1e0003: "ANShape",
    0x1e0004: "ANCyclicSequence",
}
PARTS = set(TAGS) - {0x1e0001, 0x1e0002, 0x1e0003, 0x1e0004}
SEQUENCES = {0x1e0001, 0x1e0004}
MESHES = {0x140014, 0x14000a}
LIMITATIONS = [
    "Only the known Tomahawk/Achilles ANShape layouts are supported.",
    "Mesh bodies are bounded opaque spans; polygons/materials are not validated.",
    "Sequence IDs are table positions, not verified clip names.",
    "Ticks have no verified seconds conversion; rotation/translation units are raw.",
    "Frame/transition unknown words and shape maps have no assigned behavior.",
    "Local constant transforms do not imply world-static components (parents move).",
    "A missing relation for node 0 is reported, never synthesized as a root.",
]


class DTSError(ValueError):
    """Unsupported or malformed input, with source record and byte position."""


class Reader:
    def __init__(self, data, record=0, pos=0, end=None):
        self.data, self.record, self.pos = data, record, pos
        self.end = len(data) if end is None else end

    def fail(self, message, offset=None):
        raise DTSError(f"record {self.record}, offset 0x{self.pos if offset is None else offset:x}: {message}")

    def read(self, fmt):
        size = struct.calcsize("<" + fmt)
        if size > self.end - self.pos:
            self.fail(f"truncated field/table: need {size} bytes, have {self.end - self.pos}")
        values = list(struct.unpack_from("<" + fmt, self.data, self.pos))
        self.pos += size
        return values

    def count(self, label, maximum, minimum=0):
        at = self.pos
        n = self.read("H")[0]
        if not minimum <= n <= maximum:
            self.fail(f"invalid {label} count {n}; supported range {minimum}..{maximum}", at)
        return n

    def table(self, node, name, count, fmt):
        stride = struct.calcsize("<" + fmt)
        start = self.pos
        if count * stride > self.end - start:
            self.fail(f"truncated {name} table ({count} records of {stride} bytes)")
        node.setdefault("tables", {})[name] = {
            "offset": start, "end": start + count * stride, "count": count,
            "stride": stride, "format": "<" + fmt,
        }
        return [self.read(fmt) for _ in range(count)]

    def words(self, node, name, count):
        return [row[0] for row in self.table(node, name, count, "H")]


class Parser:
    def __init__(self):
        self.objects = 0

    def node(self, outer, allowed, depth=0):
        start = outer.pos
        self.objects += 1
        if depth >= 32 or self.objects > 50000:
            outer.fail("excessive nesting or object count", start)
        tag, size = outer.read("II")
        if tag not in allowed:
            outer.fail(f"unsupported record tag 0x{tag:08x} in this context", start)
        if size > outer.end - outer.pos:
            outer.fail(f"record 0x{tag:08x} crosses parent/file boundary", start + 4)
        r = Reader(outer.data, outer.record, outer.pos, outer.pos + size)
        outer.pos = r.end
        d = {"tag": f"0x{tag:08x}", "type": TAGS[tag], "offset": start,
             "payload_size": size, "end": r.end}
        if tag in SEQUENCES:
            d["duration_ticks"], *d["header_unknown_u16"] = r.read("3H")
            count = r.count("frame", 1024, 1)
            d["frames"] = r.table(d, "frames", count, "3H")
            if any(f[0] == 0 for f in d["frames"]):
                r.fail("zero-length frame", d["tables"]["frames"]["offset"])
            if sum(f[0] for f in d["frames"]) != d["duration_ticks"]:
                r.fail("frame ticks do not sum to sequence duration", start + 8)
            parts = r.count("sequence part", 64)
            d["parts"] = r.words(d, "parts", parts)
            d["indices"] = r.words(d, "indices", count * parts)
        elif tag == 0x1e0002:
            d["sequences"] = [self.node(r, SEQUENCES, depth + 1)
                              for _ in range(r.count("sequence", 64))]
            d["transitions_raw_u16"] = r.table(d, "transitions", r.count("transition", 65535), "4H")
            d["transforms"] = r.table(d, "transforms", r.count("transform", 8192, 1), "6h")
            d["defaults"] = r.words(d, "defaults", r.count("bone", 64, 1))
            d["relations"] = r.table(d, "relations", r.count("relation", len(d["defaults"])), "2h")
        else:
            d["bone"], d["part_id"], *d["bounds_raw_i16"] = r.read("6h")
            if tag in MESHES:
                d["opaque_geometry"] = {"offset": r.pos, "end": r.end}
                r.pos = r.end
            elif tag != 0x140005:
                d["children"] = [self.node(r, PARTS, depth + 1)
                                 for _ in range(r.count("child", 4096))]
                if tag in (0x140008, 0x1e0003):
                    nt, ns = r.read("2H")
                    d["sequence_map_raw_u16"] = r.words(d, "sequence_map", ns)
                    d["transform_map_raw_u16"] = r.words(d, "transform_map", nt)
                    if tag == 0x1e0003:
                        d["animation"] = self.node(r, {0x1e0002}, depth + 1)
                elif tag == 0x140015:
                    count = r.count("BSP node", 65535)
                    d["bsp_raw"] = r.table(d, "bsp", count, "3hihh")
                    d["bsp_transform_raw_u16"] = r.words(d, "bsp_transforms", count)
                elif tag == 0x14000b:
                    d["cell_raw_u16"] = r.read("H")[0]
                elif tag == 0x14000c:
                    d["detail_raw_u16"] = r.words(d, "details", (r.end - r.pos) // 2)
        if r.pos != r.end:
            r.fail(f"unconsumed bytes in {TAGS[tag]}: {r.end - r.pos}")
        return d


def parts(node):
    """Object containment is distinct from the animation parent table."""
    yield node
    for child in node.get("children", []):
        yield from parts(child)


def validate_animation(root, reader):
    a = root["animation"]
    n = len(a["defaults"])
    transforms = a["transforms"]

    def check_refs(owner, table, values, limit, minimum=0):
        for i, value in enumerate(values):
            if not minimum <= value < limit:
                reader.fail(f"invalid {table}[{i}] reference {value}; expected {minimum}..{limit - 1}",
                            owner["tables"][table]["offset"] + i * 2)

    check_refs(a, "defaults", a["defaults"], len(transforms))
    parents = [None] * n
    for i, (parent, child) in enumerate(a["relations"]):
        at = a["tables"]["relations"]["offset"] + i * 4
        if not 0 <= child < n or not -1 <= parent < n:
            reader.fail(f"invalid relation parent={parent}, child={child}", at)
        if parents[child] is not None:
            reader.fail(f"duplicate relation for node {child}", at)
        parents[child] = parent
    missing = [i for i, parent in enumerate(parents) if parent is None]
    if missing not in ([], [0]):
        reader.fail(f"incomplete hierarchy: missing relations for nodes {missing}", a["tables"]["relations"]["offset"])
    for i in range(n):
        seen, node = set(), i
        while node is not None and node != -1:
            if node in seen:
                reader.fail(f"cyclic hierarchy at node {node}", a["tables"]["relations"]["offset"])
            seen.add(node)
            node = parents[node]
    objects = list(parts(root))
    for p in objects:
        if not -1 <= p["bone"] < n or (p["type"] in ("TSGroup", "TSBSPGroup") and p["bone"] == -1):
            reader.fail(f"invalid part bone reference {p['bone']}", p["offset"] + 8)
    a["nodes"] = [{"id": i, "parent": parents[i],
                   "children": [j for j, parent in enumerate(parents) if parent == i],
                   "default_transform": a["defaults"][i],
                   "object_offsets": [p["offset"] for p in objects if p["bone"] == i]}
                  for i in range(n)]
    a["missing_relation_nodes"] = missing
    for seq_id, s in enumerate(a["sequences"]):
        s["id"] = seq_id
        check_refs(s, "parts", s["parts"], n)
        check_refs(s, "indices", s["indices"], len(transforms))
        if len(set(s["parts"])) != len(s["parts"]):
            reader.fail("duplicate sequence part", s["tables"]["parts"]["offset"])
        motion = {"varying_rotation": [], "varying_translation": [], "held": [],
                  "unlisted": [i for i in range(n) if i not in s["parts"]]}
        for column, bone in enumerate(s["parts"]):
            samples = [transforms[t] for t in s["indices"][column::len(s["parts"])]]
            rotation = len({tuple(t[:3]) for t in samples}) > 1
            translation = len({tuple(t[3:]) for t in samples}) > 1
            if rotation:
                motion["varying_rotation"].append(bone)
            if translation:
                motion["varying_translation"].append(bone)
            if not rotation and not translation:
                motion["held"].append(bone)
        s["local_motion"] = motion


def inspect_bytes(data, source="<memory>"):
    r = Reader(data)
    if not 0 < len(data) <= MAX_BYTES:
        r.fail(f"file size must be 1..{MAX_BYTES} bytes")
    records, parser = [], Parser()
    while r.pos < r.end:
        r.record = len(records)
        if r.record >= 16:
            r.fail("excessive top-level record count (maximum 16)")
        record = parser.node(r, {0x1e0003})
        validate_animation(record, r)
        record["record_index"] = r.record
        records.append(record)
    return {"schema_version": 1, "model": Path(source).stem, "source": str(source),
            "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data),
            "records": records, "unsupported_record_types": [], "limitations": LIMITATIONS}


def inspect_file(path):
    with Path(path).open("rb") as stream:
        return inspect_bytes(stream.read(MAX_BYTES + 1), str(path))


def text_inventory(models):
    lines = ["DTS skeletal animation inventory (raw source units; no playback)"]
    lines.extend("NOTE: " + note for note in LIMITATIONS)
    for model in models:
        lines += ["", f"MODEL {model['model']} | {model['source']}",
                  f"SHA256 {model['sha256']} | {model['bytes']} bytes | {len(model['records'])} records",
                  "Unsupported tags: none encountered outside opaque mesh bodies"]
        for root in model["records"]:
            a = root["animation"]
            lines += ["", f"RECORD {root['record_index']} {root['type']} {root['tag']} "
                      f"[0x{root['offset']:x}, 0x{root['end']:x}) payload={root['payload_size']}",
                      f"  nodes={len(a['nodes'])} sequences={len(a['sequences'])} "
                      f"transforms={len(a['transforms'])} transitions={len(a['transitions_raw_u16'])}",
                      f"  Shape maps (raw u16): sequence={root['sequence_map_raw_u16']} "
                      f"transform={root['transform_map_raw_u16']}", "  Object containment:"]

            def tree(p, depth=0):
                lines.append(f"    {'  ' * depth}{p['type']} @0x{p['offset']:x} "
                             f"bone={p['bone']} part_id={p['part_id']}" +
                             (" [opaque geometry]" if "opaque_geometry" in p else ""))
                for child in p.get("children", []):
                    tree(child, depth + 1)

            tree(root)
            lines.append("  Skeleton (numeric nodes; -1=root, None=relation absent):")
            for node in a["nodes"]:
                t = a["transforms"][node["default_transform"]]
                lines.append(f"    node {node['id']} parent={node['parent']} children={node['children']} "
                             f"default={node['default_transform']} rot={t[:3]} pos={t[3:]} "
                             f"objects={node['object_offsets']}")
            for s in a["sequences"]:
                lines += [f"  Sequence {s['id']} {s['type']} @0x{s['offset']:x}: "
                          f"{len(s['frames'])} frames, {s['duration_ticks']} ticks, "
                          f"header unknown u16={s['header_unknown_u16']}",
                          f"    parts={s['parts']}; local motion={s['local_motion']}"]
                width = len(s["parts"])
                for i, frame in enumerate(s["frames"]):
                    lines.append(f"    frame {i} @0x{s['tables']['frames']['offset'] + 6*i:x}: "
                                 f"ticks={frame[0]} unknown_u16={frame[1:]} "
                                 f"transform_indices={s['indices'][i*width:(i+1)*width]}")
            lines.append("  Transition table: four raw u16 words per record (semantics unverified)")
            for i, row in enumerate(a["transitions_raw_u16"]):
                lines.append(f"    {i} @0x{a['tables']['transitions']['offset'] + 8*i:x}: {row}")
            lines.append("  Transform table: signed i16 rotation XYZ / translation XYZ (units unverified)")
            for i, row in enumerate(a["transforms"]):
                lines.append(f"    {i} @0x{a['tables']['transforms']['offset'] + 12*i:x}: "
                             f"rot={row[:3]} pos={row[3:]}")
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sources", nargs="+", type=Path)
    parser.add_argument("--format", choices=("text", "json"), default="text")
    parser.add_argument("--output", type=Path, help="new file only; prefer ignored imported/; default stdout")
    args = parser.parse_args(argv)
    try:
        models = []
        for path in args.sources:
            try:
                models.append(inspect_file(path))
            except (DTSError, OSError) as error:
                raise DTSError(f"{path}: {error}") from error
        output = (json.dumps(models, indent=2) + "\n" if args.format == "json"
                  else text_inventory(models))
        if args.output:
            # Exclusive creation refuses originals, existing files, hardlinks and
            # symlinks, including dangling symlinks. Never overwrite any resource.
            with args.output.open("x", encoding="utf-8") as stream:
                stream.write(output)
        else:
            sys.stdout.write(output)
    except (DTSError, OSError) as error:
        print(f"DTS inventory: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
