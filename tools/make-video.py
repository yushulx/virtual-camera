# make-video.py - renders a folder of images into an MP4 slideshow.
# Natural file order, 2 seconds per image, 1280x720 letterboxed, H.264.

import re
import sys
from pathlib import Path

from PIL import Image
import imageio.v2 as imageio

FOLDER = Path(sys.argv[1] if len(sys.argv) > 1 else r"D:\images\all-images")
OUTPUT = Path(sys.argv[2] if len(sys.argv) > 2 else r"D:\images\all-images.mp4")
WIDTH, HEIGHT, FPS = 1280, 720, 30
SECONDS_PER_IMAGE = 2


def natural_key(name: str):
    return [int(part) if part.isdigit() else part.lower()
            for part in re.split(r"(\d+)", name)]


def load_images(folder: Path):
    exts = {".jpg", ".jpeg", ".png", ".webp", ".bmp", ".tif", ".tiff", ".gif"}
    files = sorted((p for p in folder.iterdir()
                    if p.suffix.lower() in exts and p.is_file()),
                   key=lambda p: natural_key(p.name))
    return files


def letterbox(img: Image.Image) -> Image.Image:
    img = img.convert("RGB")
    scale = min(WIDTH / img.width, HEIGHT / img.height)
    new_size = (max(1, round(img.width * scale)),
                max(1, round(img.height * scale)))
    img = img.resize(new_size, Image.LANCZOS)
    canvas = Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0))
    canvas.paste(img, ((WIDTH - img.width) // 2, (HEIGHT - img.height) // 2))
    return canvas


def main():
    files = load_images(FOLDER)
    if not files:
        print(f"no images in {FOLDER}")
        return 1
    print(f"{len(files)} images -> {OUTPUT} "
          f"({SECONDS_PER_IMAGE}s each, {WIDTH}x{HEIGHT}@{FPS}, H.264)")

    writer = imageio.get_writer(str(OUTPUT), fps=FPS, codec="libx264",
                               quality=8, macro_block_size=16)
    import numpy as np
    frames_per_image = FPS * SECONDS_PER_IMAGE
    for index, path in enumerate(files, 1):
        try:
            frame = np.asarray(letterbox(Image.open(path)))
        except Exception as exc:  # skip unreadable files
            print(f"  [{index}/{len(files)}] SKIP {path.name}: {exc}")
            continue
        for _ in range(frames_per_image):
            writer.append_data(frame)
        print(f"  [{index}/{len(files)}] {path.name}")
    writer.close()

    size_mb = OUTPUT.stat().st_size / 1024 / 1024
    duration = len(files) * SECONDS_PER_IMAGE
    print(f"done: {OUTPUT}  ~{duration}s, {size_mb:.1f} MB")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
