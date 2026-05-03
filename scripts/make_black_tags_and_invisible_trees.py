from __future__ import annotations

from collections import deque
from pathlib import Path
import argparse

from PIL import Image, ImageDraw, ImageFont

from render_black_tag_assets import RenderParser


REFERENCE_TAGS = {
    "cow_pasture": ("Reference_Assets/cow_pasture_ref.png", "CP", "green_background"),
    "feed_mill": ("Reference_Assets/feed_mill_ref.png", "FM", "alpha"),
}

TREE_ASSETS = [
    "fir_big_01",
    "fir_big_01_autumn",
    "fir_big_01_halloween",
    "fir_big_01_winter",
    "fir_medium_01",
    "fir_medium_01_winter",
    "fir_medium_02",
    "fir_medium_02_halloween",
    "fir_medium_02_winter",
    "fir_small_01",
    "fir_small_01_winter",
    "leafy_big_01",
    "leafy_big_01_autumn",
    "leafy_big_01_halloween",
    "leafy_big_01_winter",
    "leafy_big_02",
    "leafy_big_02_autumn",
    "leafy_big_02_halloween",
    "leafy_big_02_winter",
    "leafy_small_01",
    "leafy_small_01_autumn",
    "leafy_small_01_halloween",
]


def _load_font(size: int) -> ImageFont.ImageFont:
    for path in (Path("C:/Windows/Fonts/arialbd.ttf"), Path("C:/Windows/Fonts/arial.ttf")):
        if path.is_file():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def _largest_component(mask: Image.Image) -> Image.Image:
    width, height = mask.size
    data = mask.load()
    seen: set[tuple[int, int]] = set()
    largest: list[tuple[int, int]] = []

    for y in range(height):
        for x in range(width):
            if (x, y) in seen or not data[x, y]:
                continue
            component = []
            queue = deque([(x, y)])
            seen.add((x, y))
            while queue:
                px, py = queue.popleft()
                component.append((px, py))
                for ny in range(py - 1, py + 2):
                    for nx in range(px - 1, px + 2):
                        if (
                            nx < 0
                            or ny < 0
                            or nx >= width
                            or ny >= height
                            or (nx, ny) in seen
                            or not data[nx, ny]
                        ):
                            continue
                        seen.add((nx, ny))
                        queue.append((nx, ny))
            if len(component) > len(largest):
                largest = component

    output = Image.new("L", mask.size, 0)
    output_data = output.load()
    for x, y in largest:
        output_data[x, y] = 255
    return output


def _green_background_mask(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    mask = Image.new("L", rgba.size, 0)
    pixels = rgba.load()
    mask_pixels = mask.load()
    for y in range(rgba.height):
        for x in range(rgba.width):
            r, g, b, a = pixels[x, y]
            if a < 16:
                continue
            green_background = g > r * 1.12 and g > b * 1.08 and g > 80
            near_white = r > 245 and g > 245 and b > 245
            if not green_background and not near_white:
                mask_pixels[x, y] = 255
    return _largest_component(mask)


def _alpha_mask(image: Image.Image) -> Image.Image:
    mask = image.convert("RGBA").getchannel("A").point(lambda value: 255 if value > 32 else 0)
    return _largest_component(mask)


def _draw_center_tag(image: Image.Image, mask: Image.Image, tag: str) -> None:
    bbox = mask.getbbox()
    if bbox is None:
        return
    left, top, right, bottom = bbox
    width = right - left
    height = bottom - top
    draw = ImageDraw.Draw(image)
    max_size = max(12, int(min(width, height) * 0.46))
    font = _load_font(max_size)
    for size in range(max_size, 9, -2):
        candidate = _load_font(size)
        tl, tt, tr, tb = draw.textbbox((0, 0), tag, font=candidate)
        if tr - tl <= width * 0.72 and tb - tt <= height * 0.56:
            font = candidate
            break
    tl, tt, tr, tb = draw.textbbox((0, 0), tag, font=font)
    x = left + (width - (tr - tl)) / 2 - tl
    y = top + (height - (tb - tt)) / 2 - tt
    draw.text((x, y), tag, fill=(255, 255, 255, 255), font=font)


def make_black_tag(image_path: Path, tag: str, mode: str, output_path: Path) -> None:
    source = Image.open(image_path).convert("RGBA")
    mask = _alpha_mask(source) if mode == "alpha" else _green_background_mask(source)
    bbox = mask.getbbox()
    if bbox is None:
        raise ValueError(f"No object mask found in {image_path}")

    black = Image.new("RGBA", source.size, (0, 0, 0, 0))
    black.putalpha(mask)
    _draw_center_tag(black, mask, tag)
    cropped = black.crop(bbox)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    cropped.save(output_path)


def patch_invisible_trees(sc_root: Path, decoded_root: Path, output_root: Path) -> None:
    parser = RenderParser(sc_root / "nature_new.sc", decoded_root)
    parser.parse()

    masks: dict[int, Image.Image] = {}
    for asset_name in TREE_ASSETS:
        object_id = parser.exports.get(asset_name)
        if object_id is None:
            print(f"Missing tree asset: {asset_name}")
            continue
        for triangle in parser.object_triangles(object_id):
            texture_index = int(triangle["texture_index"])
            if texture_index not in parser.texture_sizes:
                continue
            width, height = parser.texture_sizes[texture_index]
            mask = masks.setdefault(texture_index, Image.new("L", (width, height), 0))
            points = [
                (u * width / 65535, v * height / 65535)
                for u, v in triangle["uvs"]  # type: ignore[index]
            ]
            ImageDraw.Draw(mask).polygon(points, fill=255)

    for texture_index, mask in sorted(masks.items()):
        source = decoded_root / f"nature_new_{texture_index}" / f"nature_new_{texture_index}.png"
        image = Image.open(source).convert("RGBA")
        clear = Image.new("RGBA", image.size, (0, 0, 0, 0))
        image = Image.composite(clear, image, mask)
        output_path = output_root / source
        output_path.parent.mkdir(parents=True, exist_ok=True)
        image.save(output_path)
        print(f"Wrote {output_path}")


def make_contact_sheet(output_root: Path) -> None:
    paths = sorted((output_root / "black_tags").glob("*.png"))
    if not paths:
        return
    cellw, cellh = 300, 250
    sheet = Image.new("RGBA", (cellw * len(paths), cellh), (245, 245, 245, 255))
    draw = ImageDraw.Draw(sheet)
    font = _load_font(14)
    for index, path in enumerate(paths):
        image = Image.open(path).convert("RGBA")
        image.thumbnail((cellw - 24, cellh - 44), Image.Resampling.LANCZOS)
        x = index * cellw + (cellw - image.width) // 2
        y = 14 + (cellh - 54 - image.height) // 2
        sheet.alpha_composite(image, (x, y))
        draw.text((index * cellw + 12, cellh - 30), path.name, fill=(0, 0, 0, 255), font=font)
    sheet.save(output_root / "black_tags_contact_sheet.png")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create corrected black/tag references and transparent tree atlas copies."
    )
    parser.add_argument("--sc-root", type=Path, default=Path("Extracted/hayday_assets_sc/assets/sc"))
    parser.add_argument("--decoded-root", type=Path, default=Path("Decoded/hayday_assets_sctx"))
    parser.add_argument("--output-root", type=Path, default=Path("Tagged_Corrected"))
    args = parser.parse_args()

    tags_seen: dict[str, str] = {}
    for name, (image, tag, mode) in REFERENCE_TAGS.items():
        if tag in tags_seen:
            raise ValueError(f"Duplicate tag {tag!r}: {tags_seen[tag]} and {name}")
        tags_seen[tag] = name
        make_black_tag(
            Path(image),
            tag,
            mode,
            args.output_root / "black_tags" / f"{name}__{tag}.png",
        )
    patch_invisible_trees(args.sc_root, args.decoded_root, args.output_root / "transparent_trees")
    make_contact_sheet(args.output_root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
