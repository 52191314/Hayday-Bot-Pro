from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import struct

from PIL import Image, ImageDraw, ImageFont

from generate_asset_tag_config import DEFAULT_TARGETS, FlatReader, SC2AssetParser, load_targets


class Matrix:
    def __init__(
        self,
        a: float = 1.0,
        b: float = 0.0,
        c: float = 0.0,
        d: float = 1.0,
        tx: float = 0.0,
        ty: float = 0.0,
    ):
        self.a = a
        self.b = b
        self.c = c
        self.d = d
        self.tx = tx
        self.ty = ty

    def apply(self, x: float, y: float) -> tuple[float, float]:
        return (
            x * self.a + y * self.c + self.tx,
            x * self.b + y * self.d + self.ty,
        )

    def compose(self, child: "Matrix") -> "Matrix":
        return Matrix(
            self.a * child.a + self.c * child.b,
            self.b * child.a + self.d * child.b,
            self.a * child.c + self.c * child.d,
            self.b * child.c + self.d * child.d,
            self.a * child.tx + self.c * child.ty + self.tx,
            self.b * child.tx + self.d * child.ty + self.ty,
        )


def _load_font(size: int) -> ImageFont.ImageFont:
    for path in (Path("C:/Windows/Fonts/arialbd.ttf"), Path("C:/Windows/Fonts/arial.ttf")):
        if path.is_file():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def _draw_center_tag(image: Image.Image, tag: str) -> None:
    draw = ImageDraw.Draw(image)
    max_size = max(10, int(min(image.size) * 0.45))
    font = _load_font(max_size)
    for size in range(max_size, 7, -2):
        candidate = _load_font(size)
        left, top, right, bottom = draw.textbbox((0, 0), tag, font=candidate)
        if right - left <= image.width * 0.82 and bottom - top <= image.height * 0.7:
            font = candidate
            break
    left, top, right, bottom = draw.textbbox((0, 0), tag, font=font)
    x = (image.width - (right - left)) / 2 - left
    y = (image.height - (bottom - top)) / 2 - top
    draw.text((x, y), tag, fill=(255, 255, 255, 255), font=font)


class RenderParser(SC2AssetParser):
    def __init__(self, sc_path: Path, decoded_root: Path):
        super().__init__(sc_path, decoded_root)
        self.matrix_banks: list[list[Matrix]] = []
        self.movie_elements: dict[int, list[tuple[int, Matrix]]] = {}
        self.texture_cache: dict[int, Image.Image] = {}

    def parse(self) -> None:
        super().parse()
        self._parse_matrix_banks()
        self._parse_movie_elements()

    def _parse_matrix_banks(self) -> None:
        root = self.reader.size_prefixed_root(self.storage_offset)
        fields = self.reader.table_fields(root)
        if 6 not in fields:
            return
        banks = []
        for bank_table in self.reader.table_vector(fields[6]):
            bank_fields = self.reader.table_fields(bank_table)
            matrices = []
            if 0 in bank_fields:
                start, count = self.reader.vector(bank_fields[0])
                for index in range(count):
                    offset = start + index * 24
                    matrices.append(
                        Matrix(
                            self.reader.f32(offset),
                            self.reader.f32(offset + 4),
                            self.reader.f32(offset + 8),
                            self.reader.f32(offset + 12),
                            self.reader.f32(offset + 16),
                            self.reader.f32(offset + 20),
                        )
                    )
            elif 2 in bank_fields:
                start, count = self.reader.vector(bank_fields[2])
                for index in range(count):
                    offset = start + index * 12
                    values = struct.unpack_from("<hhhhhh", self.reader.data, offset)
                    matrices.append(
                        Matrix(
                            values[0] / 1024,
                            values[1] / 1024,
                            values[2] / 1024,
                            values[3] / 1024,
                            values[4] / 20,
                            values[5] / 20,
                        )
                    )
            banks.append(matrices)
        self.matrix_banks = banks

    def _parse_movie_elements(self) -> None:
        root = self.reader.size_prefixed_root(self.chunk_offsets["movieclips"])
        fields = self.reader.table_fields(root)
        for movie_table in self.reader.table_vector(fields[0]):
            movie_fields = self.reader.table_fields(movie_table)
            movie_id = self.reader.u16(movie_fields[0])
            children = self.reader.ushort_vector(movie_fields[5]) if 5 in movie_fields else []
            used_transform = self._first_frame_count(movie_fields)
            frame_offset = self.reader.u32(movie_fields[9]) if 9 in movie_fields else 0xFFFFFFFF
            bank_index = self.reader.u32(movie_fields[10]) if 10 in movie_fields else 0
            matrices = self.matrix_banks[bank_index] if bank_index < len(self.matrix_banks) else []

            elements = []
            if frame_offset != 0xFFFFFFFF and used_transform:
                for index in range(used_transform):
                    element_offset = frame_offset + index * 3
                    if element_offset + 1 >= len(self.frame_elements):
                        break
                    instance_index = self.frame_elements[element_offset]
                    matrix_index = self.frame_elements[element_offset + 1]
                    if instance_index < len(children):
                        matrix = matrices[matrix_index] if matrix_index < len(matrices) else Matrix()
                        elements.append((children[instance_index], matrix))
            else:
                elements = [(child_id, Matrix()) for child_id in children]
            self.movie_elements[movie_id] = elements

    def _first_frame_count(self, movie_fields: dict[int, int]) -> int:
        if 12 in movie_fields:
            start, count = self.reader.vector(movie_fields[12])
            return self.reader.u16(start) if count else 0
        if 8 in movie_fields:
            start, count = self.reader.vector(movie_fields[8])
            return self.reader.u32(start) if count else 0
        return 0

    def object_triangles(self, object_id: int) -> list[dict[str, object]]:
        triangles: list[dict[str, object]] = []

        def visit(current_id: int, matrix: Matrix, depth: int = 0) -> None:
            if depth > 50:
                return
            if current_id in self.shapes:
                for command in self.shapes[current_id]:
                    points = []
                    uvs = []
                    for vertex_index in range(command["points_count"]):
                        offset = (command["points_offset"] + vertex_index) * 12
                        x = struct.unpack_from("<f", self.vertex_buffer, offset)[0]
                        y = struct.unpack_from("<f", self.vertex_buffer, offset + 4)[0]
                        u = struct.unpack_from("<H", self.vertex_buffer, offset + 8)[0]
                        v = struct.unpack_from("<H", self.vertex_buffer, offset + 10)[0]
                        points.append(matrix.apply(x, y))
                        uvs.append((u, v))
                    for index in range(len(points) - 2):
                        if index % 2:
                            triangle = [points[index], points[index + 2], points[index + 1]]
                            triangle_uvs = [uvs[index], uvs[index + 2], uvs[index + 1]]
                        else:
                            triangle = points[index : index + 3]
                            triangle_uvs = uvs[index : index + 3]
                        triangles.append(
                            {
                                "texture_index": command["texture_index"],
                                "points": triangle,
                                "uvs": triangle_uvs,
                            }
                        )
                return
            for child_id, child_matrix in self.movie_elements.get(current_id, []):
                visit(child_id, matrix.compose(child_matrix), depth + 1)

        visit(object_id, Matrix())
        return triangles

    def texture_image(self, texture_index: int) -> Image.Image | None:
        if texture_index in self.texture_cache:
            return self.texture_cache[texture_index]
        path = (
            self.decoded_root
            / f"{self.sc_path.stem}_{texture_index}"
            / f"{self.sc_path.stem}_{texture_index}.png"
        )
        if not path.is_file():
            return None
        texture = Image.open(path).convert("RGBA")
        black = Image.new("RGBA", texture.size, (0, 0, 0, 255))
        black.putalpha(texture.getchannel("A"))
        self.texture_cache[texture_index] = black
        return black


def _affine_coeffs(
    dst: list[tuple[float, float]], src: list[tuple[float, float]]
) -> tuple[float, float, float, float, float, float] | None:
    (x0, y0), (x1, y1), (x2, y2) = dst
    (u0, v0), (u1, v1), (u2, v2) = src
    det = x0 * (y1 - y2) + x1 * (y2 - y0) + x2 * (y0 - y1)
    if abs(det) < 1e-6:
        return None

    a = (u0 * (y1 - y2) + u1 * (y2 - y0) + u2 * (y0 - y1)) / det
    b = (u0 * (x2 - x1) + u1 * (x0 - x2) + u2 * (x1 - x0)) / det
    c = (
        u0 * (x1 * y2 - x2 * y1)
        + u1 * (x2 * y0 - x0 * y2)
        + u2 * (x0 * y1 - x1 * y0)
    ) / det
    d = (v0 * (y1 - y2) + v1 * (y2 - y0) + v2 * (y0 - y1)) / det
    e = (v0 * (x2 - x1) + v1 * (x0 - x2) + v2 * (x1 - x0)) / det
    f = (
        v0 * (x1 * y2 - x2 * y1)
        + v1 * (x2 * y0 - x0 * y2)
        + v2 * (x0 * y1 - x1 * y0)
    ) / det
    return a, b, c, d, e, f


def _paste_textured_triangle(
    image: Image.Image,
    texture: Image.Image,
    dst_points: list[tuple[float, float]],
    src_points: list[tuple[float, float]],
) -> None:
    left = math.floor(min(x for x, _ in dst_points))
    top = math.floor(min(y for _, y in dst_points))
    right = math.ceil(max(x for x, _ in dst_points))
    bottom = math.ceil(max(y for _, y in dst_points))
    width = right - left
    height = bottom - top
    if width <= 0 or height <= 0:
        return

    local_dst = [(x - left, y - top) for x, y in dst_points]
    coeffs = _affine_coeffs(local_dst, src_points)
    if coeffs is None:
        return

    patch = texture.transform(
        (width, height),
        Image.Transform.AFFINE,
        coeffs,
        resample=Image.Resampling.BICUBIC,
    )
    mask = Image.new("L", (width, height), 0)
    ImageDraw.Draw(mask).polygon(local_dst, fill=255)
    alpha = patch.getchannel("A")
    alpha = Image.composite(alpha, Image.new("L", alpha.size, 0), mask)
    patch.putalpha(alpha)
    image.alpha_composite(patch, (left, top))


def render_asset(parser: RenderParser, asset_name: str, tag: str, output_path: Path) -> bool:
    object_id = parser.exports.get(asset_name)
    if object_id is None:
        return False
    triangles = parser.object_triangles(object_id)
    if not triangles:
        return False

    xs = [
        x
        for triangle in triangles
        for x, _ in triangle["points"]  # type: ignore[index]
    ]
    ys = [
        y
        for triangle in triangles
        for _, y in triangle["points"]  # type: ignore[index]
    ]
    left = math.floor(min(xs))
    top = math.floor(min(ys))
    right = math.ceil(max(xs))
    bottom = math.ceil(max(ys))
    padding = max(8, int(max(right - left, bottom - top) * 0.06))
    width = max(1, right - left + padding * 2)
    height = max(1, bottom - top + padding * 2)

    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    for triangle in triangles:
        texture_index = int(triangle["texture_index"])
        texture = parser.texture_image(texture_index)
        if texture is None:
            continue
        tex_width, tex_height = texture.size
        dst = [
            (x - left + padding, y - top + padding)
            for x, y in triangle["points"]  # type: ignore[index]
        ]
        src = [
            (u * tex_width / 65535, v * tex_height / 65535)
            for u, v in triangle["uvs"]  # type: ignore[index]
        ]
        _paste_textured_triangle(image, texture, dst, src)
    _draw_center_tag(image, tag)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path)
    return True


def validate_unique_tags(targets: dict[str, str]) -> None:
    owners: dict[str, str] = {}
    for asset_name, tag in targets.items():
        previous = owners.get(tag)
        if previous is not None and previous != asset_name:
            raise ValueError(f"Duplicate tag {tag!r}: {previous} and {asset_name}")
        owners[tag] = asset_name


def main() -> int:
    arg_parser = argparse.ArgumentParser(description="Render black tagged asset PNGs.")
    arg_parser.add_argument("--targets", type=Path)
    arg_parser.add_argument(
        "--sc-root", type=Path, default=Path("Extracted/hayday_assets_sc/assets/sc")
    )
    arg_parser.add_argument(
        "--decoded-root", type=Path, default=Path("Decoded/hayday_assets_sctx")
    )
    arg_parser.add_argument("--output-root", type=Path, default=Path("Tagged_Assets"))
    args = arg_parser.parse_args()

    targets = load_targets(args.targets)
    validate_unique_tags(targets)
    parsers: dict[str, RenderParser] = {}
    for sc_path in sorted(args.sc_root.glob("*.sc")):
        parser = RenderParser(sc_path, args.decoded_root)
        parser.parse()
        parsers[sc_path.stem] = parser

    written = 0
    missing = []
    for asset_name, tag in targets.items():
        done = False
        for parser in parsers.values():
            if asset_name not in parser.exports:
                continue
            output_path = args.output_root / f"{asset_name}__{tag}.png"
            if render_asset(parser, asset_name, tag, output_path):
                print(f"Wrote {output_path}")
                written += 1
                done = True
                break
        if not done:
            missing.append(asset_name)

    if missing:
        print("Missing: " + ", ".join(missing))
    print(f"Done. Wrote {written} tagged asset PNG(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
