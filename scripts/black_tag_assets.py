from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def _load_font(size: int) -> ImageFont.ImageFont:
    candidates = [
        Path("C:/Windows/Fonts/arialbd.ttf"),
        Path("C:/Windows/Fonts/arial.ttf"),
    ]
    for path in candidates:
        if path.is_file():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def _fit_font(draw: ImageDraw.ImageDraw, tag: str, width: int, height: int):
    max_size = max(8, int(min(width, height) * 0.45))
    min_size = 8
    for size in range(max_size, min_size - 1, -2):
        font = _load_font(size)
        left, top, right, bottom = draw.textbbox((0, 0), tag, font=font)
        if right - left <= width * 0.86 and bottom - top <= height * 0.72:
            return font
    return _load_font(min_size)


def _blacken_and_label(image: Image.Image, bbox: list[int], tag: str) -> None:
    x, y, width, height = bbox
    if width <= 0 or height <= 0:
        raise ValueError(f"Invalid bbox {bbox!r}")
    if x < 0 or y < 0 or x + width > image.width or y + height > image.height:
        raise ValueError(f"bbox {bbox!r} is outside {image.size}")

    region = image.crop((x, y, x + width, y + height)).convert("RGBA")
    alpha = region.getchannel("A")
    black = Image.new("RGBA", region.size, (0, 0, 0, 0))
    black.putalpha(alpha)
    image.paste(black, (x, y), alpha)

    draw = ImageDraw.Draw(image)
    font = _fit_font(draw, tag, width, height)
    left, top, right, bottom = draw.textbbox((0, 0), tag, font=font)
    text_width = right - left
    text_height = bottom - top
    text_x = x + (width - text_width) / 2 - left
    text_y = y + (height - text_height) / 2 - top
    draw.text((text_x, text_y), tag, fill=(255, 255, 255, 255), font=font)


def _validate_entries(entries: list[dict]) -> None:
    tag_owners: dict[str, set[str]] = defaultdict(set)
    for entry in entries:
        tag_owners[entry["tag"]].add(entry.get("asset_name", entry["name"]))

    dupes = sorted(tag for tag, owners in tag_owners.items() if len(owners) > 1)
    if dupes:
        raise ValueError(f"Duplicate tag(s): {', '.join(dupes)}")

    names = [entry["name"] for entry in entries]
    duplicate_names = sorted(name for name, count in Counter(names).items() if count > 1)
    if duplicate_names:
        raise ValueError(f"Duplicate asset name(s): {', '.join(duplicate_names)}")


def patch_assets(config_path: Path, output_root: Path) -> None:
    config = json.loads(config_path.read_text(encoding="utf-8"))
    entries = config["assets"]
    _validate_entries(entries)

    grouped: dict[Path, list[dict]] = defaultdict(list)
    for entry in entries:
        grouped[Path(entry["image"])].append(entry)

    for image_path, image_entries in grouped.items():
        image = Image.open(image_path).convert("RGBA")
        for entry in image_entries:
            _blacken_and_label(image, entry["bbox"], entry["tag"])

        output_path = output_root / image_path
        output_path.parent.mkdir(parents=True, exist_ok=True)
        image.save(output_path)
        print(f"Wrote {output_path}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Blacken selected atlas rectangles and draw unique white tags."
    )
    parser.add_argument("config", type=Path, help="JSON config with assets to patch")
    parser.add_argument(
        "-o",
        "--output-root",
        type=Path,
        default=Path("Tagged"),
        help="Output root for patched copies",
    )
    args = parser.parse_args()

    patch_assets(args.config, args.output_root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
