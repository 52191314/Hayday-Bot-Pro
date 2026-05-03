from __future__ import annotations

import argparse
import csv
import json
from dataclasses import dataclass
from pathlib import Path
import struct
from typing import Any

from mb_sc_tools.codec import inspect_container


DEFAULT_TARGETS = {
    "pasture_cow": "CP",
    "building_mill_01": "FM",
    "bakery_01": "BK",
    "dairy_01": "DY",
    "sugarmill_01": "SM",
    "pasture_chicken": "CH",
    "pasture_pig": "PG",
    "pasture_sheep": "SH",
    "pasture_goat": "GT",
}


@dataclass(frozen=True)
class Rect:
    texture_index: int
    x: int
    y: int
    width: int
    height: int


class FlatReader:
    def __init__(self, data: bytes):
        self.data = data

    def u16(self, offset: int) -> int:
        return struct.unpack_from("<H", self.data, offset)[0]

    def u32(self, offset: int) -> int:
        return struct.unpack_from("<I", self.data, offset)[0]

    def i32(self, offset: int) -> int:
        return struct.unpack_from("<i", self.data, offset)[0]

    def f32(self, offset: int) -> float:
        return struct.unpack_from("<f", self.data, offset)[0]

    def table_fields(self, table_offset: int) -> dict[int, int]:
        vtable_offset = table_offset - self.i32(table_offset)
        vtable_size = self.u16(vtable_offset)
        fields = {}
        for field_index in range((vtable_size - 4) // 2):
            rel = self.u16(vtable_offset + 4 + field_index * 2)
            if rel:
                fields[field_index] = table_offset + rel
        return fields

    def size_prefixed_root(self, offset: int) -> int:
        return offset + 4 + self.u32(offset + 4)

    def vector(self, field_offset: int) -> tuple[int, int]:
        vector_offset = field_offset + self.u32(field_offset)
        return vector_offset + 4, self.u32(vector_offset)

    def table_vector(self, field_offset: int) -> list[int]:
        start, count = self.vector(field_offset)
        return [start + index * 4 + self.u32(start + index * 4) for index in range(count)]

    def string_vector(self, field_offset: int) -> list[str]:
        start, count = self.vector(field_offset)
        strings = []
        for index in range(count):
            item_offset = start + index * 4
            string_offset = item_offset + self.u32(item_offset)
            length = self.u32(string_offset)
            strings.append(
                self.data[string_offset + 4 : string_offset + 4 + length].decode(
                    "utf-8", "replace"
                )
            )
        return strings

    def ushort_vector(self, field_offset: int) -> list[int]:
        start, count = self.vector(field_offset)
        return [self.u16(start + index * 2) for index in range(count)]

    def uint_vector(self, field_offset: int) -> list[int]:
        start, count = self.vector(field_offset)
        return [self.u32(start + index * 4) for index in range(count)]

    def byte_vector(self, field_offset: int) -> bytes:
        start, count = self.vector(field_offset)
        return self.data[start : start + count]


class SC2AssetParser:
    def __init__(self, sc_path: Path, decoded_root: Path):
        self.sc_path = sc_path
        self.decoded_root = decoded_root
        self.info, payload = inspect_container(sc_path.read_bytes())
        self.reader = FlatReader(payload)
        self.metadata = FlatReader(bytes.fromhex(self.info["metadata_hex"]))

        self.storage_offset = 0
        descriptor_root = self.metadata.u32(0)
        descriptor_fields = self.metadata.table_fields(descriptor_root)
        self.resources_offset = self.metadata.u32(descriptor_fields[8])

        self.chunk_offsets = self._chunk_offsets()
        self.strings: list[str] = []
        self.frame_elements: list[int] = []
        self.vertex_buffer = b""
        self.exports: dict[str, int] = {}
        self.shapes: dict[int, list[dict[str, int]]] = {}
        self.movies: dict[int, dict[str, Any]] = {}
        self.texture_sizes = self._load_texture_sizes()

    def parse(self) -> None:
        self._parse_data_storage()
        self._parse_exports()
        self._parse_shapes()
        self._parse_movieclips()

    def _chunk_offsets(self) -> dict[str, int]:
        offset = self.resources_offset
        chunks: dict[str, int] = {}
        for name in ("exports", "textfields", "shapes", "movieclips", "modifiers", "textures"):
            chunks[name] = offset
            offset += 4 + self.reader.u32(offset)
        return chunks

    def _load_texture_sizes(self) -> dict[int, tuple[int, int]]:
        sizes = {}
        stem = self.sc_path.stem
        for index, data_path in enumerate(
            sorted(self.decoded_root.glob(f"{stem}_*/data.json"))
        ):
            data = json.loads(data_path.read_text(encoding="utf-8"))
            sizes[index] = (int(data["width"]), int(data["height"]))
        return sizes

    def _string_ref(self, ref_id: int) -> str:
        if ref_id == 0:
            return ""
        index = ref_id - 1
        if index < 0 or index >= len(self.strings):
            return ""
        return self.strings[index]

    def _parse_data_storage(self) -> None:
        root = self.reader.size_prefixed_root(self.storage_offset)
        fields = self.reader.table_fields(root)
        self.strings = self.reader.string_vector(fields[0])
        self.frame_elements = (
            self.reader.ushort_vector(fields[4]) if 4 in fields else []
        )
        self.vertex_buffer = self.reader.byte_vector(fields[5])

    def _parse_exports(self) -> None:
        root = self.reader.size_prefixed_root(self.chunk_offsets["exports"])
        fields = self.reader.table_fields(root)
        object_ids = self.reader.ushort_vector(fields[0])
        name_refs = self.reader.uint_vector(fields[1])
        for object_id, name_ref in zip(object_ids, name_refs):
            name = self._string_ref(name_ref)
            if name:
                self.exports[name] = object_id

    def _parse_shapes(self) -> None:
        root = self.reader.size_prefixed_root(self.chunk_offsets["shapes"])
        fields = self.reader.table_fields(root)
        for shape_table in self.reader.table_vector(fields[0]):
            shape_fields = self.reader.table_fields(shape_table)
            shape_id = self.reader.u16(shape_fields[0]) if 0 in shape_fields else 0
            commands = []
            if 1 in shape_fields:
                start, count = self.reader.vector(shape_fields[1])
                for index in range(count):
                    command_offset = start + index * 16
                    commands.append(
                        {
                            "texture_index": self.reader.u32(command_offset + 4),
                            "points_count": self.reader.u32(command_offset + 8),
                            "points_offset": self.reader.u32(command_offset + 12),
                        }
                    )
            self.shapes[shape_id] = commands

    def _parse_movieclips(self) -> None:
        root = self.reader.size_prefixed_root(self.chunk_offsets["movieclips"])
        fields = self.reader.table_fields(root)
        for movie_table in self.reader.table_vector(fields[0]):
            movie_fields = self.reader.table_fields(movie_table)
            movie_id = self.reader.u16(movie_fields[0])
            children = self.reader.ushort_vector(movie_fields[5]) if 5 in movie_fields else []
            visible_children = self._movie_first_frame_children(movie_fields, children)
            self.movies[movie_id] = {"children": visible_children or children}

    def _movie_first_frame_children(
        self, movie_fields: dict[int, int], children: list[int]
    ) -> list[int]:
        if not children:
            return []

        used_transform = 0
        if 12 in movie_fields:
            start, count = self.reader.vector(movie_fields[12])
            if count:
                used_transform = self.reader.u16(start)
        elif 8 in movie_fields:
            start, count = self.reader.vector(movie_fields[8])
            if count:
                used_transform = self.reader.u32(start)

        if not used_transform:
            return []

        frame_offset = (
            self.reader.u32(movie_fields[9])
            if 9 in movie_fields
            else 0xFFFFFFFF
        )
        if frame_offset == 0xFFFFFFFF:
            return []

        visible_children = []
        for index in range(used_transform):
            element_offset = frame_offset + index * 3
            if element_offset >= len(self.frame_elements):
                break
            instance_index = self.frame_elements[element_offset]
            if instance_index < len(children):
                visible_children.append(children[instance_index])
        return visible_children

    def object_rects(self, object_id: int) -> list[Rect]:
        visited: set[int] = set()
        rects: list[Rect] = []

        def visit(current_id: int) -> None:
            if current_id in visited:
                return
            visited.add(current_id)

            if current_id in self.shapes:
                rects.extend(self._shape_rects(current_id))
                return

            movie = self.movies.get(current_id)
            if movie is None:
                return
            for child_id in movie["children"]:
                visit(child_id)

        visit(object_id)
        return rects

    def _shape_rects(self, shape_id: int) -> list[Rect]:
        rects = []
        for command in self.shapes.get(shape_id, []):
            texture_index = command["texture_index"]
            texture_size = self.texture_sizes.get(texture_index)
            if texture_size is None:
                continue
            tex_width, tex_height = texture_size

            xs = []
            ys = []
            for vertex_index in range(command["points_count"]):
                offset = (command["points_offset"] + vertex_index) * 12
                u = self.reader.u16_from(self.vertex_buffer, offset + 8)
                v = self.reader.u16_from(self.vertex_buffer, offset + 10)
                xs.append(round(u * tex_width / 65535))
                ys.append(round(v * tex_height / 65535))
            if not xs or not ys:
                continue

            left = max(0, min(xs))
            top = max(0, min(ys))
            right = min(tex_width, max(xs))
            bottom = min(tex_height, max(ys))
            if right > left and bottom > top:
                rects.append(Rect(texture_index, left, top, right - left, bottom - top))
        return rects


def _u16_from(self: FlatReader, buffer: bytes, offset: int) -> int:
    return struct.unpack_from("<H", buffer, offset)[0]


FlatReader.u16_from = _u16_from  # type: ignore[attr-defined]


def merge_rects(rects: list[Rect]) -> list[Rect]:
    by_texture: dict[int, list[Rect]] = {}
    for rect in rects:
        by_texture.setdefault(rect.texture_index, []).append(rect)

    merged = []
    for texture_index, texture_rects in sorted(by_texture.items()):
        left = min(rect.x for rect in texture_rects)
        top = min(rect.y for rect in texture_rects)
        right = max(rect.x + rect.width for rect in texture_rects)
        bottom = max(rect.y + rect.height for rect in texture_rects)
        merged.append(Rect(texture_index, left, top, right - left, bottom - top))
    return merged


def load_targets(path: Path | None) -> dict[str, str]:
    if path is None:
        return DEFAULT_TARGETS
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, dict):
        return {str(name): str(tag) for name, tag in data.items()}
    return {str(item["name"]): str(item["tag"]) for item in data}


def atlas_path(decoded_root: Path, sc_stem: str, texture_index: int) -> Path:
    return decoded_root / f"{sc_stem}_{texture_index}" / f"{sc_stem}_{texture_index}.png"


def generate(
    sc_root: Path,
    decoded_root: Path,
    targets: dict[str, str],
    output_json: Path,
    output_csv: Path,
) -> None:
    parsers: dict[str, SC2AssetParser] = {}
    assets = []
    rows = []

    tags_seen: dict[str, str] = {}
    for name, tag in targets.items():
        if tag in tags_seen:
            raise ValueError(f"Duplicate tag {tag!r}: {tags_seen[tag]} and {name}")
        tags_seen[tag] = name

    for sc_path in sorted(sc_root.glob("*.sc")):
        parser = SC2AssetParser(sc_path, decoded_root)
        parser.parse()
        parsers[sc_path.stem] = parser

    for name, tag in targets.items():
        matches = []
        for parser in parsers.values():
            if name in parser.exports:
                rects = merge_rects(parser.object_rects(parser.exports[name]))
                if rects:
                    matches.append((parser, rects))

        if not matches:
            print(f"Missing target: {name}")
            continue

        parser, rects = matches[0]
        for rect_index, rect in enumerate(rects):
            image_path = atlas_path(decoded_root, parser.sc_path.stem, rect.texture_index)
            entry = {
                "name": name if len(rects) == 1 else f"{name}_{rect_index}",
                "asset_name": name,
                "tag": tag,
                "image": str(image_path).replace("\\", "/"),
                "bbox": [rect.x, rect.y, rect.width, rect.height],
            }
            assets.append(entry)
            rows.append(
                {
                    "asset_name": name,
                    "tag": tag,
                    "source_sc": parser.sc_path.name,
                    "texture_index": rect.texture_index,
                    "image": entry["image"],
                    "x": rect.x,
                    "y": rect.y,
                    "width": rect.width,
                    "height": rect.height,
                }
            )

    output_json.write_text(
        json.dumps({"assets": assets}, indent=2, ensure_ascii=False), encoding="utf-8"
    )
    with output_csv.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(
            file,
            fieldnames=[
                "asset_name",
                "tag",
                "source_sc",
                "texture_index",
                "image",
                "x",
                "y",
                "width",
                "height",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)
    print(f"Wrote {output_json}")
    print(f"Wrote {output_csv}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate black-tag asset config.")
    parser.add_argument(
        "--sc-root",
        type=Path,
        default=Path("Extracted/hayday_assets_sc/assets/sc"),
    )
    parser.add_argument(
        "--decoded-root",
        type=Path,
        default=Path("Decoded/hayday_assets_sctx"),
    )
    parser.add_argument("--targets", type=Path)
    parser.add_argument(
        "--output-json", type=Path, default=Path("asset_tags.generated.json")
    )
    parser.add_argument(
        "--output-csv", type=Path, default=Path("asset_name_rects.generated.csv")
    )
    args = parser.parse_args()

    generate(
        args.sc_root,
        args.decoded_root,
        load_targets(args.targets),
        args.output_json,
        args.output_csv,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
