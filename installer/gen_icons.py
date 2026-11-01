#!/usr/bin/env python3
"""生成导航之家（NavHome）应用图标。

输出到 installer/icons/：
  - app.ico           主程序图标（品牌蓝，多尺寸 16~256）
  - icon_0.ico ... icon_19.ico   20 个可选应用图标（不同配色）

ICO 采用 PNG 内嵌方式（Vista+ 标准），由各尺寸 PNG 手动组装而成。
"""
import colorsys
import io
import os
import struct

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "icons")
os.makedirs(OUT, exist_ok=True)

SIZES = [16, 32, 48, 64, 128, 256]
WHITE = (255, 255, 255, 255)


def make_icon(size, bg):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    m = max(1, size // 12)
    d.rounded_rectangle([m, m, size - m, size - m], radius=size // 5, fill=bg)
    cx, cy = size // 2, size // 2
    r = int(size * 0.30)
    w = max(1, size // 9)
    d.polygon([(cx, cy - r), (cx + w, cy), (cx, cy + r), (cx - w, cy)], fill=WHITE)
    dot = max(1, size // 14)
    d.ellipse([cx - dot, cy - dot, cx + dot, cy + dot], fill=WHITE)
    return img


def _png_bytes(img):
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    return buf.getvalue()


def save_ico(images, path):
    """images: list of PIL Image（按尺寸升序）。以 PNG 内嵌方式组装 ICO。"""
    pngs = [_png_bytes(img) for img in images]

    header = struct.pack("<HHH", 0, 1, len(pngs))
    entries = b""
    offset = 6 + 16 * len(pngs)
    for data in pngs:
        # PNG IHDR：width / height 位于偏移 16、20 处（大端 uint32）
        w, h = struct.unpack(">II", data[16:24])
        w = 0 if w >= 256 else w
        h = 0 if h >= 256 else h
        entries += struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(data), offset)
        offset += len(data)

    with open(path, "wb") as f:
        f.write(header + entries + b"".join(pngs))


def save_ico_from_bg(bg, path):
    save_ico([make_icon(s, bg) for s in SIZES], path)


def main():
    brand_blue = (91, 141, 239, 255)  # #5b8def
    save_ico_from_bg(brand_blue, os.path.join(OUT, "app.ico"))
    print("生成 app.ico")

    for i in range(20):
        hue = i / 20.0
        r, g, b = colorsys.hsv_to_rgb(hue, 0.55, 0.78)
        bg = (int(r * 255), int(g * 255), int(b * 255), 255)
        save_ico_from_bg(bg, os.path.join(OUT, f"icon_{i}.ico"))
        print(f"生成 icon_{i}.ico")


if __name__ == "__main__":
    main()
