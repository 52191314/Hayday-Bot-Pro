from __future__ import annotations

import json
import shutil
from datetime import datetime
from pathlib import Path

from PIL import Image
from mb_sc_tools.codec import inspect_container
from mb_sc_tools.dispatch import encode_folder


DECODED_ROOT = Path("Decoded/hayday_assets_sctx")
WORK_ROOT = Path("Tagged_Corrected/all_animals_invisible_injection")
EXTRA_ROOT = Path("HD/x64/Release/injecthacks/extra_visuals")
INJECT_ROOT = Path("HD/x64/Release/injecthacks")
KEY = b"NXRTH_NATURE_KEY"

ANIMAL_STEMS = [
    "animals",
    "animals02",
    "animals03",
    "animals04",
    "animals05",
    "animal_accessories",
]


def encrypt_xor_hex(data: bytes, key: bytes = KEY) -> bytes:
    encoded = bytearray()
    for index, byte in enumerate(data):
        encoded.append(byte ^ key[index % len(key)])
    return b"0123456789abcdef" + encoded.hex().encode("ascii") + b"fedcba9876543210"


def encode_payload(work_dir: Path, payload_name: str, raw_name: str) -> tuple[Path, Path]:
    raw_sctx = EXTRA_ROOT / raw_name
    encoded = encode_folder(work_dir, output_path=raw_sctx)
    payload = INJECT_ROOT / payload_name
    payload.write_bytes(encrypt_xor_hex(encoded.read_bytes()))

    clean = payload.read_text(encoding="ascii")[16:-16]
    decrypted = bytes(
        int(clean[i : i + 2], 16) ^ KEY[(i // 2) % len(KEY)]
        for i in range(0, len(clean), 2)
    )
    if decrypted != encoded.read_bytes():
        raise RuntimeError(f"Encrypted payload mismatch for {payload}")
    inspect_container(decrypted)
    return encoded, payload


def transparent_atlas_folder(stem: str, index: int) -> tuple[Path, int]:
    source_dir = DECODED_ROOT / f"{stem}_{index}"
    work_dir = WORK_ROOT / "Decoded" / "hayday_assets_sctx" / f"{stem}_{index}"
    if work_dir.exists():
        shutil.rmtree(work_dir)
    shutil.copytree(source_dir, work_dir)

    changed_pixels = 0
    for image_path in work_dir.glob(f"{stem}_{index}*.png"):
        image = Image.open(image_path).convert("RGBA")
        alpha = image.getchannel("A")
        changed_pixels += sum(1 for value in alpha.tobytes() if value)
        transparent = Image.new("RGBA", image.size, (0, 0, 0, 0))
        transparent.save(image_path)

    return work_dir, changed_pixels


def discover_targets() -> list[tuple[str, int]]:
    targets: list[tuple[str, int]] = []
    for stem in ANIMAL_STEMS:
        for folder in sorted(DECODED_ROOT.glob(f"{stem}_*")):
            suffix = folder.name.rsplit("_", 1)[-1]
            if suffix.isdigit() and (folder / f"{stem}_{suffix}.png").is_file():
                targets.append((stem, int(suffix)))
    return targets


def main() -> int:
    EXTRA_ROOT.mkdir(parents=True, exist_ok=True)
    INJECT_ROOT.mkdir(parents=True, exist_ok=True)

    manifest_path = EXTRA_ROOT / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.is_file() else {}
    active = [
        item
        for item in manifest.get("active_payloads", [])
        if not any(str(item.get("target", "")).startswith(f"{stem}_") for stem in ANIMAL_STEMS)
    ]

    payloads = []
    for stem, index in discover_targets():
        work_dir, changed = transparent_atlas_folder(stem, index)
        target = f"{stem}_{index}.sctx"
        payload_name = f"inject_extra_{stem}_{index}.nxrth"
        raw_name = f"{stem}_{index}_all_invisible.sctx"
        encoded, payload = encode_payload(work_dir, payload_name, raw_name)
        item = {
            "target": target,
            "source": f"{stem}.sc full atlas transparency",
            "changed_pixels_erased": changed,
            "raw_sctx_copy": str(encoded).replace("\\", "/"),
            "encrypted_payload": str(payload).replace("\\", "/"),
            "note": "Entire animal/accessory atlas made transparent. Bot template cow_feed.png is not changed.",
        }
        active.append(item)
        payloads.append(item)

    manifest["updated_at"] = datetime.now().isoformat(timespec="seconds")
    manifest["active_payloads"] = active
    manifest["all_animals_invisible"] = {
        "targets": [item["target"] for item in payloads],
        "note": "All animals, animal seasonal sets, and animal accessories are transparent at SCTX atlas level. Cow-feed bot templates remain separate under templates/.",
    }
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    print(f"Wrote {len(payloads)} all-animal invisible payloads.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
