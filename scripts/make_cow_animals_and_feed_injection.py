from __future__ import annotations

import json
import math
import shutil
import struct
from datetime import datetime
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont
from mb_sc_tools.codec import inspect_container
from mb_sc_tools.dispatch import encode_folder

from render_black_tag_assets import RenderParser


SC_ROOT = Path("Extracted/hayday_assets_sc/assets/sc")
DECODED_ROOT = Path("Decoded/hayday_assets_sctx")
WORK_ROOT = Path("Tagged_Corrected/animal_feed_injection")
EXTRA_ROOT = Path("HD/x64/Release/injecthacks/extra_visuals")
INJECT_ROOT = Path("HD/x64/Release/injecthacks")
KEY = b"NXRTH_NATURE_KEY"

FEED_ASSET = "saucemixer"
FEED_TAG = "FM"
FEED_YELLOW_ANCHORS = [(627, 2732), (1442, 2683)]
DAIRY_ASSET = "dairy_01"
DAIRY_TAG = "DY"
DAIRY_TEXTURE_INDEX = 0
DAIRY_FLA_SHAPES = [
    343,
    344,
    345,
    348,
    349,
    350,
    351,
    352,
    353,
    354,
    355,
    356,
    357,
    358,
    359,
    360,
    361,
    362,
    363,
    364,
    365,
    367,
    368,
    369,
    370,
    380,
]
DAIRY_TAG_SHAPES = [348]


def is_cow_visual_asset(name: str) -> bool:
    return name.startswith("cow01_") or name.startswith("accessory_seasonal_cow_")


def load_font(size: int) -> ImageFont.ImageFont:
    for path in (Path("C:/Windows/Fonts/arialbd.ttf"), Path("C:/Windows/Fonts/arial.ttf")):
        if path.is_file():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def encrypt_xor_hex(data: bytes, key: bytes = KEY) -> bytes:
    encoded = bytearray()
    for index, byte in enumerate(data):
        encoded.append(byte ^ key[index % len(key)])
    return b"0123456789abcdef" + encoded.hex().encode("ascii") + b"fedcba9876543210"


def triangle_masks_for_assets(parser: RenderParser, asset_names: list[str]) -> dict[int, Image.Image]:
    masks: dict[int, Image.Image] = {}
    for asset_name in asset_names:
        object_id = parser.exports.get(asset_name)
        if object_id is None:
            continue
        for triangle in parser.object_triangles(object_id):
            texture_index = int(triangle["texture_index"])
            width, height = parser.texture_sizes[texture_index]
            mask = masks.setdefault(texture_index, Image.new("L", (width, height), 0))
            points = [
                (u * width / 65535, v * height / 65535)
                for u, v in triangle["uvs"]  # type: ignore[index]
            ]
            ImageDraw.Draw(mask).polygon(points, fill=255)
    return masks


def all_movie_children(parser: RenderParser) -> dict[int, list[int]]:
    root = parser.reader.size_prefixed_root(parser.chunk_offsets["movieclips"])
    fields = parser.reader.table_fields(root)
    children_by_movie: dict[int, list[int]] = {}
    for movie_table in parser.reader.table_vector(fields[0]):
        movie_fields = parser.reader.table_fields(movie_table)
        movie_id = parser.reader.u16(movie_fields[0]) if 0 in movie_fields else 0
        children_by_movie[movie_id] = parser.reader.ushort_vector(movie_fields[5]) if 5 in movie_fields else []
    return children_by_movie


def all_declared_shape_masks_for_assets(parser: RenderParser, asset_names: list[str]) -> dict[int, Image.Image]:
    children_by_movie = all_movie_children(parser)
    masks: dict[int, Image.Image] = {}

    def add_shape(shape_id: int) -> None:
        for command in parser.shapes.get(shape_id, []):
            texture_index = int(command["texture_index"])
            if texture_index not in parser.texture_sizes:
                continue
            width, height = parser.texture_sizes[texture_index]
            mask = masks.setdefault(texture_index, Image.new("L", (width, height), 0))
            points = [
                (
                    struct.unpack_from("<H", parser.vertex_buffer, (command["points_offset"] + vertex_index) * 12 + 8)[0]
                    * width
                    / 65535,
                    struct.unpack_from("<H", parser.vertex_buffer, (command["points_offset"] + vertex_index) * 12 + 10)[0]
                    * height
                    / 65535,
                )
                for vertex_index in range(command["points_count"])
            ]
            draw = ImageDraw.Draw(mask)
            for index in range(len(points) - 2):
                if index % 2:
                    triangle = [points[index], points[index + 2], points[index + 1]]
                else:
                    triangle = points[index : index + 3]
                draw.polygon(triangle, fill=255)

    def visit(object_id: int, visited: set[int]) -> None:
        if object_id in visited:
            return
        visited.add(object_id)
        if object_id in parser.shapes:
            add_shape(object_id)
            return
        for child_id in children_by_movie.get(object_id, []):
            visit(child_id, visited)

    for asset_name in asset_names:
        object_id = parser.exports.get(asset_name)
        if object_id is not None:
            visit(object_id, set())
    return masks


def shape_ids_mask(parser: RenderParser, shape_ids: list[int], texture_index: int, size: tuple[int, int]) -> Image.Image:
    mask = Image.new("L", size, 0)
    draw = ImageDraw.Draw(mask)
    tex_width, tex_height = size
    for shape_id in shape_ids:
        for command in parser.shapes.get(shape_id, []):
            if int(command["texture_index"]) != texture_index:
                continue
            points = []
            for vertex_index in range(command["points_count"]):
                offset = (command["points_offset"] + vertex_index) * 12
                u = struct.unpack_from("<H", parser.vertex_buffer, offset + 8)[0]
                v = struct.unpack_from("<H", parser.vertex_buffer, offset + 10)[0]
                points.append((u * tex_width / 65535, v * tex_height / 65535))
            for index in range(len(points) - 2):
                triangle = [points[index], points[index + 2], points[index + 1]] if index % 2 else points[index : index + 3]
                draw.polygon(triangle, fill=255)
    return mask


def set_explicit_texture_sizes(parser: RenderParser, stem: str) -> None:
    sizes: dict[int, tuple[int, int]] = {}
    for folder in DECODED_ROOT.glob(f"{stem}_*"):
        suffix = folder.name.rsplit("_", 1)[-1]
        if not suffix.isdigit():
            continue
        index = int(suffix)
        png = folder / f"{stem}_{index}.png"
        if png.is_file():
            with Image.open(png) as image:
                sizes[index] = image.size
    parser.texture_sizes = sizes


def black_under_mask(image: Image.Image, mask: Image.Image) -> Image.Image:
    black = Image.new("RGBA", image.size, (0, 0, 0, 0))
    black.putalpha(Image.composite(image.getchannel("A"), Image.new("L", image.size, 0), mask))
    return Image.composite(black, image, mask)


def transparent_under_mask(image: Image.Image, mask: Image.Image) -> Image.Image:
    transparent = Image.new("RGBA", image.size, (0, 0, 0, 0))
    return Image.composite(transparent, image, mask)


def feed_work_yellow_mask(image: Image.Image, object_mask: Image.Image) -> Image.Image:
    mask_bytes = object_mask.tobytes()
    pixels = image.convert("RGBA").tobytes()
    selected = bytearray(image.width * image.height)
    for index, mask_value in enumerate(mask_bytes):
        if not mask_value:
            continue
        red, green, blue, alpha = pixels[index * 4 : index * 4 + 4]
        if (
            alpha > 10
            and red >= 170
            and green >= 110
            and blue <= 135
            and red + green > 310
            and red >= green * 0.75
            and green >= blue * 1.05
        ):
            selected[index] = 255
    return Image.frombytes("L", image.size, bytes(selected))


def anchored_yellow_mask(image: Image.Image, anchors: list[tuple[int, int]]) -> Image.Image:
    width, height = image.size
    pixels = image.convert("RGBA").tobytes()
    selected = bytearray(width * height)
    visited = bytearray(width * height)

    def is_yellow(index: int) -> bool:
        red, green, blue, alpha = pixels[index * 4 : index * 4 + 4]
        return (
            alpha > 10
            and red >= 170
            and green >= 115
            and blue <= 145
            and red + green > 320
            and green >= blue * 1.05
        )

    for x, y in anchors:
        if not (0 <= x < width and 0 <= y < height):
            continue
        seed = y * width + x
        if visited[seed] or not is_yellow(seed):
            continue
        stack = [seed]
        visited[seed] = 1
        while stack:
            point = stack.pop()
            selected[point] = 255
            px = point % width
            py = point // width
            for nx, ny in (
                (px - 1, py),
                (px + 1, py),
                (px, py - 1),
                (px, py + 1),
                (px - 1, py - 1),
                (px + 1, py - 1),
                (px - 1, py + 1),
                (px + 1, py + 1),
            ):
                if not (0 <= nx < width and 0 <= ny < height):
                    continue
                neighbor = ny * width + nx
                if visited[neighbor] or not is_yellow(neighbor):
                    continue
                visited[neighbor] = 1
                stack.append(neighbor)
    return Image.frombytes("L", image.size, bytes(selected))


def expanded_mask(mask: Image.Image, radius: int = 2) -> Image.Image:
    if radius <= 0:
        return mask
    return mask.filter(ImageFilter.MaxFilter(radius * 2 + 1))


def mask_label_anchor(mask: Image.Image) -> tuple[float, float, int, tuple[int, int, int, int]] | None:
    bbox = mask.getbbox()
    if bbox is None:
        return None

    left, top, right, bottom = bbox
    crop = mask.crop(bbox)
    width, height = crop.size
    scale = min(1.0, 1024 / max(width, height))
    if scale < 1.0:
        sampled = crop.resize((max(1, int(width * scale)), max(1, int(height * scale))), Image.Resampling.NEAREST)
    else:
        sampled = crop

    sample_width, sample_height = sampled.size
    pixels = sampled.tobytes()
    visited = bytearray(sample_width * sample_height)
    best: tuple[int, int, int, int, int, int, int] | None = None

    for start, value in enumerate(pixels):
        if not value or visited[start]:
            continue

        stack = [start]
        visited[start] = 1
        count = 0
        sum_x = 0
        sum_y = 0
        min_x = sample_width
        min_y = sample_height
        max_x = 0
        max_y = 0

        while stack:
            point = stack.pop()
            x = point % sample_width
            y = point // sample_width
            count += 1
            sum_x += x
            sum_y += y
            min_x = min(min_x, x)
            min_y = min(min_y, y)
            max_x = max(max_x, x)
            max_y = max(max_y, y)

            if x > 0:
                neighbor = point - 1
                if pixels[neighbor] and not visited[neighbor]:
                    visited[neighbor] = 1
                    stack.append(neighbor)
            if x + 1 < sample_width:
                neighbor = point + 1
                if pixels[neighbor] and not visited[neighbor]:
                    visited[neighbor] = 1
                    stack.append(neighbor)
            if y > 0:
                neighbor = point - sample_width
                if pixels[neighbor] and not visited[neighbor]:
                    visited[neighbor] = 1
                    stack.append(neighbor)
            if y + 1 < sample_height:
                neighbor = point + sample_width
                if pixels[neighbor] and not visited[neighbor]:
                    visited[neighbor] = 1
                    stack.append(neighbor)

        if best is None or count > best[0]:
            best = (count, sum_x, sum_y, min_x, min_y, max_x, max_y)

    if best is None:
        return None

    count, sum_x, sum_y, min_x, min_y, max_x, max_y = best
    inv_scale = 1 / scale
    center_x = left + (sum_x / count + 0.5) * inv_scale
    center_y = top + (sum_y / count + 0.5) * inv_scale
    component_bbox = (
        left + int(min_x * inv_scale),
        top + int(min_y * inv_scale),
        left + int((max_x + 1) * inv_scale),
        top + int((max_y + 1) * inv_scale),
    )
    component_area = int(count * inv_scale * inv_scale)
    return center_x, center_y, component_area, component_bbox


def draw_tag(image: Image.Image, mask: Image.Image, tag: str) -> None:
    anchor = mask_label_anchor(mask)
    if anchor is None:
        return
    center_x, center_y, mask_area, bbox = anchor
    left, top, right, bottom = bbox
    width = right - left
    height = bottom - top
    draw = ImageDraw.Draw(image)
    area_span = int(math.sqrt(max(1, mask_area)))
    max_size = min(96, max(24, area_span // 3, min(width, height) // 7))
    font = load_font(max_size)
    for size in range(max_size, 16, -3):
        candidate = load_font(size)
        tb = draw.textbbox((0, 0), tag, font=candidate)
        if tb[2] - tb[0] <= max(42, area_span * 0.72) and tb[3] - tb[1] <= max(32, area_span * 0.55):
            font = candidate
            break
    tb = draw.textbbox((0, 0), tag, font=font)
    text_width = tb[2] - tb[0]
    text_height = tb[3] - tb[1]
    x = center_x - text_width / 2 - tb[0]
    y = center_y - text_height / 2 - tb[1]
    x = max(0, min(image.width - text_width, x))
    y = max(0, min(image.height - text_height, y))
    draw.text(
        (x, y),
        tag,
        fill=(255, 255, 255, 255),
        font=font,
        stroke_width=max(1, font.size // 18 if hasattr(font, "size") else 2),
        stroke_fill=(0, 0, 0, 255),
    )


def patch_sctx_folder(
    stem: str,
    texture_index: int,
    mask: Image.Image,
    tag: str | None,
    work_kind: str,
    mode: str = "black",
    post_erase_mask: Image.Image | None = None,
) -> tuple[Path, int]:
    source_dir = DECODED_ROOT / f"{stem}_{texture_index}"
    work_dir = WORK_ROOT / work_kind / "Decoded" / "hayday_assets_sctx" / f"{stem}_{texture_index}"
    if work_dir.exists():
        shutil.rmtree(work_dir)
    shutil.copytree(source_dir, work_dir)

    main_path = work_dir / f"{stem}_{texture_index}.png"
    image = Image.open(main_path).convert("RGBA")
    patch_mask = expanded_mask(mask) if mode == "transparent" else mask
    patched = transparent_under_mask(image, patch_mask) if mode == "transparent" else black_under_mask(image, patch_mask)
    if post_erase_mask is not None:
        patched = transparent_under_mask(patched, post_erase_mask)
    if tag:
        draw_tag(patched, patch_mask, tag)
    patched.save(main_path)

    variant_path = work_dir / f"{stem}_{texture_index}_variant_0.png"
    if variant_path.is_file():
        variant = Image.open(variant_path).convert("RGBA")
        sx = variant.width / image.width
        sy = variant.height / image.height
        small = patch_mask.resize(variant.size, Image.Resampling.NEAREST)
        variant_patched = transparent_under_mask(variant, small) if mode == "transparent" else black_under_mask(variant, small)
        if post_erase_mask is not None:
            small_erase = post_erase_mask.resize(variant.size, Image.Resampling.NEAREST)
            variant_patched = transparent_under_mask(variant_patched, small_erase)
        if tag:
            draw_tag(variant_patched, small, tag)
        variant_patched.save(variant_path)

    return work_dir, sum(patch_mask.histogram()[1:])


def patch_buildings_new_0_feed_and_dairy(
    feed_mask: Image.Image,
    feed_yellow_mask: Image.Image,
    dairy_mask: Image.Image,
    dairy_tag_mask: Image.Image,
) -> tuple[Path, int, int]:
    stem = "buildings_new"
    texture_index = 0
    source_dir = DECODED_ROOT / f"{stem}_{texture_index}"
    work_dir = WORK_ROOT / "feed_mill_and_dairy" / "Decoded" / "hayday_assets_sctx" / f"{stem}_{texture_index}"
    if work_dir.exists():
        shutil.rmtree(work_dir)
    shutil.copytree(source_dir, work_dir)

    main_path = work_dir / f"{stem}_{texture_index}.png"
    image = Image.open(main_path).convert("RGBA")
    patched = black_under_mask(image, feed_mask)
    patched = transparent_under_mask(patched, feed_yellow_mask)
    draw_tag(patched, feed_mask, FEED_TAG)
    patched = black_under_mask(patched, dairy_mask)
    draw_tag(patched, dairy_tag_mask, DAIRY_TAG)
    patched.save(main_path)

    variant_path = work_dir / f"{stem}_{texture_index}_variant_0.png"
    if variant_path.is_file():
        variant = Image.open(variant_path).convert("RGBA")
        feed_small = feed_mask.resize(variant.size, Image.Resampling.NEAREST)
        yellow_small = feed_yellow_mask.resize(variant.size, Image.Resampling.NEAREST)
        dairy_small = dairy_mask.resize(variant.size, Image.Resampling.NEAREST)
        variant_patched = black_under_mask(variant, feed_small)
        variant_patched = transparent_under_mask(variant_patched, yellow_small)
        draw_tag(variant_patched, feed_small, FEED_TAG)
        variant_patched = black_under_mask(variant_patched, dairy_small)
        dairy_tag_small = dairy_tag_mask.resize(variant.size, Image.Resampling.NEAREST)
        draw_tag(variant_patched, dairy_tag_small, DAIRY_TAG)
        variant_patched.save(variant_path)

    return work_dir, sum(feed_mask.histogram()[1:]), sum(dairy_mask.histogram()[1:])


def encode_payload(work_dir: Path, target_stem: str, payload_name: str, raw_name: str) -> tuple[Path, Path]:
    raw_sctx = EXTRA_ROOT / raw_name
    encoded = encode_folder(work_dir, output_path=raw_sctx)
    payload = INJECT_ROOT / payload_name
    payload.write_bytes(encrypt_xor_hex(encoded.read_bytes()))
    clean = payload.read_text(encoding="ascii")[16:-16]
    key = KEY
    decrypted = bytes(int(clean[i : i + 2], 16) ^ key[(i // 2) % len(key)] for i in range(0, len(clean), 2))
    if decrypted != encoded.read_bytes():
        raise RuntimeError(f"Encrypted payload mismatch for {payload}")
    inspect_container(decrypted)
    return encoded, payload


def make_preview(stem: str, index: int, mask: Image.Image, work_dir: Path, name: str) -> Path:
    source = Image.open(DECODED_ROOT / f"{stem}_{index}" / f"{stem}_{index}.png").convert("RGBA")
    patched = Image.open(work_dir / f"{stem}_{index}.png").convert("RGBA")
    bbox = mask.getbbox() or (0, 0, source.width, source.height)
    pad = 32
    left = max(0, bbox[0] - pad)
    top = max(0, bbox[1] - pad)
    right = min(source.width, bbox[2] + pad)
    bottom = min(source.height, bbox[3] + pad)
    before = source.crop((left, top, right, bottom))
    after = patched.crop((left, top, right, bottom))
    sheet = Image.new("RGBA", (before.width + after.width + 18, max(before.height, after.height) + 30), (245, 245, 245, 255))
    sheet.alpha_composite(before, (0, 0))
    sheet.alpha_composite(after, (before.width + 18, 0))
    draw = ImageDraw.Draw(sheet)
    font = load_font(14)
    draw.text((8, sheet.height - 24), "before", fill=(0, 0, 0, 255), font=font)
    draw.text((before.width + 26, sheet.height - 24), "after", fill=(0, 0, 0, 255), font=font)
    out = WORK_ROOT / "previews" / f"{name}_{stem}_{index}_before_after.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out)
    return out


def main() -> int:
    manifest_path = EXTRA_ROOT / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.is_file() else {}
    active = [
        item
        for item in manifest.get("active_payloads", [])
        if not str(item.get("target", "")).startswith("animals_")
        and item.get("target") not in {"buildings_new_0.sctx", "buildings_new_2.sctx"}
    ]

    animals = RenderParser(SC_ROOT / "animals.sc", DECODED_ROOT)
    animals.parse()
    set_explicit_texture_sizes(animals, "animals")
    cow_assets = [name for name in sorted(animals.exports) if is_cow_visual_asset(name)]
    cow_masks = all_declared_shape_masks_for_assets(animals, cow_assets)
    for texture_index, mask in sorted(cow_masks.items()):
        work_dir, changed = patch_sctx_folder("animals", texture_index, mask, None, "cows", mode="transparent")
        encoded, payload = encode_payload(
            work_dir,
            f"animals_{texture_index}",
            f"inject_extra_animals_{texture_index}.nxrth",
            f"animals_{texture_index}_cows_invisible.sctx",
        )
        preview = make_preview("animals", texture_index, expanded_mask(mask), work_dir, "cows_invisible")
        active.append(
            {
                "target": f"animals_{texture_index}.sctx",
                "source": "animals.sc cow01_* and accessory_seasonal_cow_* triangle masks",
                "changed_pixels_erased": changed,
                "raw_sctx_copy": str(encoded).replace("\\", "/"),
                "encrypted_payload": str(payload).replace("\\", "/"),
                "preview": str(preview).replace("\\", "/"),
            }
        )

    buildings = RenderParser(SC_ROOT / "buildings_new.sc", DECODED_ROOT)
    buildings.parse()
    set_explicit_texture_sizes(buildings, "buildings_new")
    feed_masks = triangle_masks_for_assets(buildings, [FEED_ASSET])
    if 0 not in feed_masks:
        raise RuntimeError(f"{FEED_ASSET} did not produce buildings_new_0 mask")
    feed_source = Image.open(DECODED_ROOT / "buildings_new_0" / "buildings_new_0.png").convert("RGBA")
    feed_yellow_mask = expanded_mask(anchored_yellow_mask(feed_source, FEED_YELLOW_ANCHORS), 1)
    dairy_mask = shape_ids_mask(buildings, DAIRY_FLA_SHAPES, DAIRY_TEXTURE_INDEX, feed_source.size)
    dairy_tag_mask = shape_ids_mask(buildings, DAIRY_TAG_SHAPES, DAIRY_TEXTURE_INDEX, feed_source.size)
    if dairy_mask.getbbox() is None:
        raise RuntimeError("dairy_01 FLA shape IDs did not produce buildings_new_0 mask")
    if dairy_tag_mask.getbbox() is None:
        raise RuntimeError("dairy_01 tag shape ID did not produce buildings_new_0 mask")
    work_dir, feed_changed, dairy_changed = patch_buildings_new_0_feed_and_dairy(
        feed_masks[0],
        feed_yellow_mask,
        dairy_mask,
        dairy_tag_mask,
    )
    encoded, payload = encode_payload(
        work_dir,
        "buildings_new_0",
        "inject_extra_buildings_new_0.nxrth",
        "buildings_new_0_feed_maker_fm_dairy_dy.sctx",
    )
    preview = make_preview("buildings_new", 0, feed_masks[0], work_dir, "feed_maker_fm")
    dairy_preview = make_preview("buildings_new", 0, dairy_mask, work_dir, "red_roof_dairy_dy")
    active.append(
        {
            "target": "buildings_new_0.sctx",
            "source": f"buildings_new.sc {FEED_ASSET} triangle mask + buildings_new.fla {DAIRY_ASSET} shape IDs",
            "tags": [FEED_TAG, DAIRY_TAG],
            "changed_pixels_blackened_feed_mill": feed_changed,
            "changed_pixels_blackened_dairy": dairy_changed,
            "changed_yellow_anchor_pixels_erased": sum(feed_yellow_mask.histogram()[1:]),
            "yellow_anchor_points": FEED_YELLOW_ANCHORS,
            "dairy_fla_shapes": DAIRY_FLA_SHAPES,
            "dairy_tag_shapes": DAIRY_TAG_SHAPES,
            "raw_sctx_copy": str(encoded).replace("\\", "/"),
            "encrypted_payload": str(payload).replace("\\", "/"),
            "preview": str(preview).replace("\\", "/"),
            "dairy_preview": str(dairy_preview).replace("\\", "/"),
            "note": "This targets the red pipe/barrel maker matched from buildings_new.fla as saucemixer and the red-roof white dairy building from buildings_new.fla.",
        }
    )

    # Keep the hard-coded injector map safe for older builds by making the previous
    # buildings_new_2 dairy payload a no-op copy of the original atlas.
    noop_work_dir = WORK_ROOT / "noop_buildings_new_2" / "Decoded" / "hayday_assets_sctx" / "buildings_new_2"
    if noop_work_dir.exists():
        shutil.rmtree(noop_work_dir)
    shutil.copytree(DECODED_ROOT / "buildings_new_2", noop_work_dir)
    encode_payload(
        noop_work_dir,
        "buildings_new_2",
        "inject_extra_buildings_new_2.nxrth",
        "buildings_new_2_noop_original.sctx",
    )

    manifest["updated_at"] = datetime.now().isoformat(timespec="seconds")
    manifest["active_payloads"] = active
    manifest["cow_animals_and_feed"] = {
        "cow_assets": cow_assets,
        "feed_asset": FEED_ASSET,
        "dairy_asset": DAIRY_ASSET,
        "dairy_fla_shapes": DAIRY_FLA_SHAPES,
        "dairy_tag_shapes": DAIRY_TAG_SHAPES,
        "note": "Cow animals and cow seasonal accessories are fully transparent; FM and the red-roof DY building are black/tagged on buildings_new_0.",
    }
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"Wrote {len(cow_masks)} invisible cow animal payloads and combined feed-maker/dairy payload.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
