from __future__ import annotations

import csv
import json
import math
import struct
import zlib
from pathlib import Path
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageFont


NS = {"x": "http://ns.adobe.com/xfl/2008/"}
ROOT = Path("codex_tmp/buildings_new_fla_xfl")
OUT = Path("cow/from_buildings_new_fla")

ASSETS = ["pasture_cow", "cow_trough_01", "fence_cow01"]


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def matrix_from_element(element: ET.Element | None) -> tuple[float, float, float, float, float, float]:
    if element is None:
        return (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)
    matrix = element.find("x:matrix/x:Matrix", NS)
    if matrix is None:
        return (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)
    return tuple(float(matrix.attrib.get(k, default)) for k, default in [
        ("a", "1"),
        ("b", "0"),
        ("c", "0"),
        ("d", "1"),
        ("tx", "0"),
        ("ty", "0"),
    ])


def multiply(left: tuple[float, float, float, float, float, float], right: tuple[float, float, float, float, float, float]):
    la, lb, lc, ld, ltx, lty = left
    ra, rb, rc, rd, rtx, rty = right
    return (
        la * ra + lc * rb,
        lb * ra + ld * rb,
        la * rc + lc * rd,
        lb * rc + ld * rd,
        la * rtx + lc * rty + ltx,
        lb * rtx + ld * rty + lty,
    )


def transform_point(matrix, x: float, y: float) -> tuple[float, float]:
    a, b, c, d, tx, ty = matrix
    return (a * x + c * y + tx, b * x + d * y + ty)


def inverse(matrix):
    a, b, c, d, tx, ty = matrix
    det = a * d - b * c
    if abs(det) < 1e-9:
        raise ValueError(f"non-invertible matrix: {matrix}")
    ia = d / det
    ib = -b / det
    ic = -c / det
    id_ = a / det
    itx = (c * ty - d * tx) / det
    ity = (b * tx - a * ty) / det
    return (ia, ib, ic, id_, itx, ity)


def decode_dat(dat_path: Path) -> Image.Image:
    data = dat_path.read_bytes()
    if data[:2] != b"\x03\x05":
        raise ValueError(f"{dat_path} is not an XFL lossless bitmap")
    row_stride, width, height = struct.unpack_from("<HHH", data, 2)
    has_alpha = data[24]
    compressed = data[25]
    offset = 26
    chunks: list[bytes] = []
    while offset + 2 <= len(data):
        chunk_len = struct.unpack_from("<H", data, offset)[0]
        offset += 2
        if chunk_len == 0:
            break
        chunks.append(data[offset : offset + chunk_len])
        offset += chunk_len
    raw = zlib.decompress(b"".join(chunks)) if compressed else b"".join(chunks)
    if len(raw) != row_stride * height:
        raise ValueError(f"unexpected decoded size for {dat_path}: {len(raw)}")

    out = bytearray(width * height * 4)
    for y in range(height):
        src = y * row_stride
        dst = y * width * 4
        for x in range(width):
            i = src + x * 4
            o = dst + x * 4
            a, r, g, b = raw[i], raw[i + 1], raw[i + 2], raw[i + 3]
            if has_alpha and a not in (0, 255):
                r = min(255, round(r * 255 / a))
                g = min(255, round(g * 255 / a))
                b = min(255, round(b * 255 / a))
            out[o : o + 4] = bytes((r, g, b, a if has_alpha else 255))
    return Image.frombytes("RGBA", (width, height), bytes(out))


def media_map() -> dict[str, dict[str, str | int]]:
    doc = ET.parse(ROOT / "DOMDocument.xml").getroot()
    result = {}
    for item in doc.findall(".//x:DOMBitmapItem", NS):
        name = item.attrib["name"]
        result[name] = {
            "href": item.attrib["bitmapDataHRef"],
            "width": int(item.attrib["frameRight"]) // 20,
            "height": int(item.attrib["frameBottom"]) // 20,
        }
    return result


def shape_sprite(shape_id: int) -> tuple[str, tuple[float, float, float, float, float, float]]:
    path = ROOT / "LIBRARY" / "Shapes" / f"Shape {shape_id}.xml"
    shape = ET.parse(path).getroot()
    bitmap = shape.find(".//x:DOMBitmapInstance", NS)
    if bitmap is None:
        raise ValueError(f"Shape {shape_id} has no bitmap")
    return bitmap.attrib["libraryItemName"], matrix_from_element(bitmap)


def asset_instances(asset_name: str) -> list[dict]:
    path = ROOT / "LIBRARY" / "Assets" / f"{asset_name}.xml"
    root = ET.parse(path).getroot()
    layers = root.findall(".//x:DOMLayer", NS)
    rows = []
    draw_index = 0
    for layer in reversed(layers):
        layer_name = layer.attrib.get("name", "")
        for inst in layer.findall(".//x:DOMSymbolInstance", NS):
            lib = inst.attrib.get("libraryItemName", "")
            if not lib.startswith("Shapes/Shape "):
                continue
            shape_id = int(lib.rsplit(" ", 1)[-1])
            sprite, shape_matrix = shape_sprite(shape_id)
            rows.append(
                {
                    "asset": asset_name,
                    "draw_index": draw_index,
                    "layer": layer_name,
                    "shape_id": shape_id,
                    "sprite": sprite,
                    "asset_matrix": matrix_from_element(inst),
                    "shape_matrix": shape_matrix,
                    "world_matrix": multiply(matrix_from_element(inst), shape_matrix),
                }
            )
            draw_index += 1
    return rows


def alpha_multiplier(inst: dict) -> float:
    return 1.0


def paste_transformed(canvas: Image.Image, src: Image.Image, matrix, offset_x: float, offset_y: float):
    shifted = (matrix[0], matrix[1], matrix[2], matrix[3], matrix[4] - offset_x, matrix[5] - offset_y)
    corners = [
        transform_point(shifted, 0, 0),
        transform_point(shifted, src.width, 0),
        transform_point(shifted, 0, src.height),
        transform_point(shifted, src.width, src.height),
    ]
    min_x = math.floor(min(x for x, _ in corners))
    min_y = math.floor(min(y for _, y in corners))
    max_x = math.ceil(max(x for x, _ in corners))
    max_y = math.ceil(max(y for _, y in corners))
    if max_x <= min_x or max_y <= min_y:
        return
    inv = inverse(shifted)
    coeff = (
        inv[0],
        inv[2],
        inv[4] + inv[0] * min_x + inv[2] * min_y,
        inv[1],
        inv[3],
        inv[5] + inv[1] * min_x + inv[3] * min_y,
    )
    patch = src.transform((max_x - min_x, max_y - min_y), Image.Transform.AFFINE, coeff, Image.Resampling.BICUBIC)
    canvas.alpha_composite(patch, (min_x, min_y))


def render_asset(asset_name: str, instances: list[dict], sprites: dict[str, Image.Image]) -> Image.Image:
    points = []
    for inst in instances:
        img = sprites[inst["sprite"]]
        m = inst["world_matrix"]
        points.extend(
            [
                transform_point(m, 0, 0),
                transform_point(m, img.width, 0),
                transform_point(m, 0, img.height),
                transform_point(m, img.width, img.height),
            ]
        )
    min_x = math.floor(min(x for x, _ in points))
    min_y = math.floor(min(y for _, y in points))
    max_x = math.ceil(max(x for x, _ in points))
    max_y = math.ceil(max(y for _, y in points))
    canvas = Image.new("RGBA", (max_x - min_x + 8, max_y - min_y + 8), (0, 0, 0, 0))
    for inst in instances:
        paste_transformed(canvas, sprites[inst["sprite"]], inst["world_matrix"], min_x - 4, min_y - 4)
    return canvas


def trim(img: Image.Image) -> Image.Image:
    bbox = img.getbbox()
    return img.crop(bbox) if bbox else img


def contact_sheet(items: list[tuple[str, Image.Image]], path: Path):
    cell_w, cell_h = 240, 190
    cols = 3
    rows = math.ceil(len(items) / cols)
    sheet = Image.new("RGBA", (cols * cell_w, rows * cell_h), (248, 248, 248, 255))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for i, (label, img) in enumerate(items):
        x = (i % cols) * cell_w
        y = (i // cols) * cell_h
        draw.rectangle((x, y, x + cell_w - 1, y + cell_h - 1), outline=(210, 210, 210, 255))
        preview = trim(img)
        preview.thumbnail((cell_w - 20, cell_h - 42), Image.Resampling.LANCZOS)
        px = x + (cell_w - preview.width) // 2
        py = y + 8 + (cell_h - 42 - preview.height) // 2
        sheet.alpha_composite(preview, (px, py))
        draw.text((x + 8, y + cell_h - 28), label[:38], fill=(0, 0, 0, 255), font=font)
    sheet.save(path)


def make_silhouette(img: Image.Image, tag: str | None = None) -> Image.Image:
    source = trim(img).copy()
    alpha = source.getchannel("A")
    out = Image.new("RGBA", source.size, (0, 0, 0, 0))
    out.putalpha(alpha)
    if tag:
        draw = ImageDraw.Draw(out)
        font_size = max(18, min(source.width, source.height) // 3)
        font_path = Path("C:/Windows/Fonts/arialbd.ttf")
        font = ImageFont.truetype(str(font_path), font_size) if font_path.exists() else ImageFont.load_default()
        bbox = draw.textbbox((0, 0), tag, font=font)
        x = (source.width - (bbox[2] - bbox[0])) / 2
        y = (source.height - (bbox[3] - bbox[1])) / 2 - bbox[1]
        # A thin dark stroke keeps the white tag legible when it touches transparent edges.
        draw.text((x, y), tag, fill=(255, 255, 255, 255), font=font, stroke_width=max(1, font_size // 18), stroke_fill=(0, 0, 0, 255))
    return out


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "sprites").mkdir(exist_ok=True)
    (OUT / "rendered_assets").mkdir(exist_ok=True)
    (OUT / "tagged_preview").mkdir(exist_ok=True)

    media = media_map()
    all_instances: list[dict] = []
    for asset in ASSETS:
        all_instances.extend(asset_instances(asset))
    sprite_names = sorted({inst["sprite"] for inst in all_instances}, key=lambda s: int(s.rsplit(" ", 1)[-1]))

    sprites: dict[str, Image.Image] = {}
    sprite_rows = []
    for sprite_name in sprite_names:
        item = media[sprite_name]
        dat = ROOT / "bin" / str(item["href"])
        img = decode_dat(dat)
        sprites[sprite_name] = img
        sprite_id = int(sprite_name.rsplit(" ", 1)[-1])
        out_name = f"sprite_{sprite_id}.png"
        img.save(OUT / "sprites" / out_name)
        sprite_rows.append({"sprite": sprite_name, "file": out_name, "width": img.width, "height": img.height, "dat": dat.name})

    asset_items = []
    rendered_assets = {}
    for asset in ASSETS:
        inst = [row for row in all_instances if row["asset"] == asset]
        rendered = render_asset(asset, inst, sprites)
        rendered_assets[asset] = rendered
        rendered.save(OUT / "rendered_assets" / f"{asset}.png")
        asset_items.append((asset, rendered))

    shape_items = []
    for sprite_name in sprite_names:
        shape_ids = sorted({row["shape_id"] for row in all_instances if row["sprite"] == sprite_name})
        sprite_id = int(sprite_name.rsplit(" ", 1)[-1])
        shape_items.append((f"Shape {','.join(map(str, shape_ids))} / Sprite {sprite_id}", sprites[sprite_name]))
    contact_sheet(asset_items, OUT / "cow_fla_rendered_assets_sheet.png")
    contact_sheet(shape_items, OUT / "cow_fla_source_sprites_sheet.png")

    tag_items = [
        ("pasture_cow / CP tag", make_silhouette(rendered_assets["pasture_cow"], "CP")),
        ("cow_trough_01 / black only", make_silhouette(rendered_assets["cow_trough_01"])),
        ("fence_cow01 / black only", make_silhouette(rendered_assets["fence_cow01"])),
    ]
    for label, img in tag_items:
        safe = label.split(" / ", 1)[0]
        suffix = "__CP" if "CP tag" in label else "__black"
        img.save(OUT / "tagged_preview" / f"{safe}{suffix}.png")
    contact_sheet(tag_items, OUT / "cow_fla_tag_plan_sheet.png")

    with (OUT / "instances.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["asset", "draw_index", "layer", "shape_id", "sprite", "asset_matrix", "shape_matrix", "world_matrix"])
        writer.writeheader()
        for row in all_instances:
            writer.writerow(row)
    with (OUT / "sprites.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["sprite", "file", "width", "height", "dat"])
        writer.writeheader()
        writer.writerows(sprite_rows)
    (OUT / "manifest.json").write_text(json.dumps({"assets": ASSETS, "sprites": sprite_rows, "instances": all_instances}, indent=2), encoding="utf-8")
    print(f"Wrote {OUT.resolve()}")


if __name__ == "__main__":
    main()
