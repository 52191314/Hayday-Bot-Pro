from __future__ import annotations

import json
import math
import shutil
import struct
from datetime import datetime
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from mb_sc_tools.codec import inspect_container
from mb_sc_tools.dispatch import encode_folder

from render_black_tag_assets import RenderParser


SC_ROOT = Path("Extracted/hayday_assets_sc/assets/sc")
DECODED_ROOT = Path("Decoded/hayday_assets_sctx")
WORK_ROOT = Path("Tagged_Corrected/cow_injection")
EXTRA_ROOT = Path("HD/x64/Release/injecthacks/extra_visuals")
INJECT_ROOT = Path("HD/x64/Release/injecthacks")

SC_NAME = "buildings_new.sc"
TEXTURE_INDEX = 6
TARGET_FOLDER = f"buildings_new_{TEXTURE_INDEX}"
TARGET_SCTX = f"{TARGET_FOLDER}.sctx"
PAYLOAD_NAME = "inject_extra_buildings_new_6.nxrth"
KEY = b"NXRTH_NATURE_KEY"

# Shape IDs confirmed from buildings_new.fla.
PASTURE_GROUND_SHAPES = [5034]
FENCE_WALL_SHAPES = [5025, 5026, 5054, 5055]
REMOVED_PASTURE_DETAIL_SHAPES = [5035, 5036, 5037, 5038, 5039, 5040, 5041, 5042, 5044, 5060]
BLACK_PASTURE_SHAPES = sorted(set(PASTURE_GROUND_SHAPES + FENCE_WALL_SHAPES))
ALL_COW_PASTURE_SHAPES = sorted(set(BLACK_PASTURE_SHAPES + REMOVED_PASTURE_DETAIL_SHAPES))


def load_font(size: int) -> ImageFont.ImageFont:
    for path in (Path("C:/Windows/Fonts/arialbd.ttf"), Path("C:/Windows/Fonts/arial.ttf")):
        if path.is_file():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def shape_mask(parser: RenderParser, shape_ids: list[int], size: tuple[int, int], scale: tuple[float, float] = (1.0, 1.0)) -> Image.Image:
    mask = Image.new("L", size, 0)
    draw = ImageDraw.Draw(mask)
    sx, sy = scale
    for shape_id in shape_ids:
        for command in parser.shapes.get(shape_id, []):
            if int(command["texture_index"]) != TEXTURE_INDEX:
                continue
            tex_width, tex_height = parser.texture_sizes[TEXTURE_INDEX]
            points = []
            for vertex_index in range(command["points_count"]):
                offset = (command["points_offset"] + vertex_index) * 12
                u = struct.unpack_from("<H", parser.vertex_buffer, offset + 8)[0]
                v = struct.unpack_from("<H", parser.vertex_buffer, offset + 10)[0]
                points.append((u * tex_width / 65535 * sx, v * tex_height / 65535 * sy))
            for index in range(len(points) - 2):
                triangle = (
                    [points[index], points[index + 2], points[index + 1]]
                    if index % 2
                    else points[index : index + 3]
                )
                draw.polygon(triangle, fill=255)
    return mask


def black_under_mask(image: Image.Image, mask: Image.Image) -> Image.Image:
    black = Image.new("RGBA", image.size, (0, 0, 0, 0))
    black.putalpha(Image.composite(image.getchannel("A"), Image.new("L", image.size, 0), mask))
    return Image.composite(black, image, mask)


def transparent_under_mask(image: Image.Image, mask: Image.Image) -> Image.Image:
    transparent = Image.new("RGBA", image.size, (0, 0, 0, 0))
    return Image.composite(transparent, image, mask)


def draw_center_tag(image: Image.Image, mask: Image.Image, tag: str) -> None:
    bbox = mask.getbbox()
    if bbox is None:
        return
    left, top, right, bottom = bbox
    width = right - left
    height = bottom - top
    draw = ImageDraw.Draw(image)
    max_size = max(18, int(min(width, height) * 0.40))
    font = load_font(max_size)
    for size in range(max_size, 15, -3):
        candidate = load_font(size)
        tb = draw.textbbox((0, 0), tag, font=candidate)
        if tb[2] - tb[0] <= width * 0.55 and tb[3] - tb[1] <= height * 0.45:
            font = candidate
            break
    tb = draw.textbbox((0, 0), tag, font=font)
    x = left + (width - (tb[2] - tb[0])) / 2 - tb[0]
    y = top + (height - (tb[3] - tb[1])) / 2 - tb[1]
    draw.text(
        (x, y),
        tag,
        fill=(255, 255, 255, 255),
        font=font,
        stroke_width=0,
    )


def make_review_crop(before: Image.Image, after: Image.Image, mask: Image.Image, out_path: Path) -> None:
    bbox = mask.getbbox()
    if bbox is None:
        return
    pad = 32
    left = max(0, bbox[0] - pad)
    top = max(0, bbox[1] - pad)
    right = min(before.width, bbox[2] + pad)
    bottom = min(before.height, bbox[3] + pad)
    before_crop = before.crop((left, top, right, bottom))
    after_crop = after.crop((left, top, right, bottom))
    w = before_crop.width + after_crop.width + 18
    h = max(before_crop.height, after_crop.height) + 30
    sheet = Image.new("RGBA", (w, h), (245, 245, 245, 255))
    sheet.alpha_composite(before_crop, (0, 0))
    sheet.alpha_composite(after_crop, (before_crop.width + 18, 0))
    draw = ImageDraw.Draw(sheet)
    font = load_font(14)
    draw.text((8, h - 24), "before", fill=(0, 0, 0, 255), font=font)
    draw.text((before_crop.width + 26, h - 24), "after", fill=(0, 0, 0, 255), font=font)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out_path)


def encrypt_xor_hex(data: bytes, key: bytes = KEY) -> bytes:
    encrypted = bytearray()
    for index, byte in enumerate(data):
        encrypted.append(byte ^ key[index % len(key)])
    return b"0123456789abcdef" + encrypted.hex().encode("ascii") + b"fedcba9876543210"


def patch_workspace() -> tuple[Path, int, dict[str, int]]:
    parser = RenderParser(SC_ROOT / SC_NAME, DECODED_ROOT)
    parser.parse()

    source_dir = DECODED_ROOT / TARGET_FOLDER
    work_dir = WORK_ROOT / "Decoded" / "hayday_assets_sctx" / TARGET_FOLDER
    if work_dir.exists():
        shutil.rmtree(work_dir)
    shutil.copytree(source_dir, work_dir)

    main_path = work_dir / f"{TARGET_FOLDER}.png"
    image = Image.open(main_path).convert("RGBA")
    black_mask = shape_mask(parser, BLACK_PASTURE_SHAPES, image.size)
    erase_mask = shape_mask(parser, REMOVED_PASTURE_DETAIL_SHAPES, image.size)
    full_review_mask = shape_mask(parser, ALL_COW_PASTURE_SHAPES, image.size)
    tag_mask = shape_mask(parser, PASTURE_GROUND_SHAPES, image.size)
    patched = transparent_under_mask(image, erase_mask)
    patched = black_under_mask(patched, black_mask)
    draw_center_tag(patched, tag_mask, "CP")
    patched.save(main_path)

    variant_path = work_dir / f"{TARGET_FOLDER}_variant_0.png"
    if variant_path.is_file():
        variant = Image.open(variant_path).convert("RGBA")
        sx = variant.width / image.width
        sy = variant.height / image.height
        black_variant_mask = shape_mask(parser, BLACK_PASTURE_SHAPES, variant.size, (sx, sy))
        erase_variant_mask = shape_mask(parser, REMOVED_PASTURE_DETAIL_SHAPES, variant.size, (sx, sy))
        tag_variant_mask = shape_mask(parser, PASTURE_GROUND_SHAPES, variant.size, (sx, sy))
        variant_patched = transparent_under_mask(variant, erase_variant_mask)
        variant_patched = black_under_mask(variant_patched, black_variant_mask)
        draw_center_tag(variant_patched, tag_variant_mask, "CP")
        variant_patched.save(variant_path)

    previews = WORK_ROOT / "previews"
    previews.mkdir(parents=True, exist_ok=True)
    black_mask.save(previews / "buildings_new_6_black_ground_fence_mask.png")
    erase_mask.save(previews / "buildings_new_6_removed_detail_mask.png")
    tag_mask.save(previews / "buildings_new_6_cp_tag_mask.png")
    make_review_crop(image, patched, full_review_mask, previews / "buildings_new_6_cow_before_after.png")

    changed_pixels = sum(black_mask.histogram()[1:])
    erased_pixels = sum(erase_mask.histogram()[1:])
    counts = {
        "black_ground_shapes": len(PASTURE_GROUND_SHAPES),
        "black_fence_wall_shapes": len(FENCE_WALL_SHAPES),
        "removed_detail_shapes": len(REMOVED_PASTURE_DETAIL_SHAPES),
        "blackened_pixels": changed_pixels,
        "erased_pixels": erased_pixels,
    }
    return work_dir, changed_pixels, counts


def main() -> int:
    work_dir, changed_pixels, counts = patch_workspace()
    raw_sctx = EXTRA_ROOT / "buildings_new_6_cow_black_cp.sctx"
    encoded_path = encode_folder(work_dir, output_path=raw_sctx)
    payload_path = INJECT_ROOT / PAYLOAD_NAME
    payload_path.write_bytes(encrypt_xor_hex(encoded_path.read_bytes()))

    _, decrypted = inspect_container(encoded_path.read_bytes())
    if not decrypted:
        raise RuntimeError("Encoded SCTX did not parse")

    manifest_path = EXTRA_ROOT / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.is_file() else {}
    manifest["updated_at"] = datetime.now().isoformat(timespec="seconds")
    active = manifest.setdefault("active_payloads", [])
    active = [item for item in active if item.get("target") != TARGET_SCTX]
    active.append(
        {
            "target": TARGET_SCTX,
            "source": "buildings_new.fla shape IDs",
            "black_shapes": BLACK_PASTURE_SHAPES,
            "transparent_shapes": REMOVED_PASTURE_DETAIL_SHAPES,
            "tag": "CP",
            "changed_pixels_blackened": changed_pixels,
            "raw_sctx_copy": str(raw_sctx).replace("\\", "/"),
            "encrypted_payload": str(payload_path).replace("\\", "/"),
            "preview": str((WORK_ROOT / "previews" / "buildings_new_6_cow_before_after.png")).replace("\\", "/"),
        }
    )
    manifest["active_payloads"] = active
    manifest["cow_fla_injection"] = {
        "work_dir": str(work_dir).replace("\\", "/"),
        "counts": counts,
        "note": "Ground and fence/wall shapes are black; trough/grass/detail shapes are transparent; CP is drawn last as a full white overlay on the ground shape.",
    }
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    print(f"Wrote {raw_sctx}")
    print(f"Wrote {payload_path}")
    print(f"Changed pixels: {changed_pixels}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
