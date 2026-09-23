#!/usr/bin/env python3
"""Draw the bongo cat and emit LVGL 4-bit indexed image data.

Run from the repo root:  python3 tools/bongo_cat_draw.py
Writes boards/shields/dongle_screen/src/widgets/bongo_cat_frames.{c,h} and
tools/bongo_cat_preview.png.

Base frames (idle/left/right) have no face; eyes, mouth, zzz and sweat are
small overlays placed on top at the offsets written to the header. Palette
indices 2/3 (body/outline) are recoloured at runtime to tint the cat per layer.
"""
from PIL import Image, ImageDraw

W, H, S = 48, 30, 2  # drawn at 48x30, scaled 2x -> 96x60
OUT_C = "boards/shields/dongle_screen/src/widgets/bongo_cat_frames.c"
OUT_H = "boards/shields/dongle_screen/src/widgets/bongo_cat_frames.h"
OUT_PNG = "tools/bongo_cat_preview.png"

# index: (r, g, b, a) -- index 0 is transparent background
PALETTE = [
    (0, 0, 0, 0),          # 0 transparent
    (255, 255, 255, 255),  # 1 white (paws)
    (242, 165, 65, 255),   # 2 ginger (body)       -- tinted at runtime
    (196, 106, 27, 255),   # 3 dark ginger (outline) -- tinted at runtime
    (255, 143, 171, 255),  # 4 pink (ears, cheeks, toe beans)
    (139, 90, 43, 255),    # 5 wood (desk)
    (20, 14, 10, 255),     # 6 near-black (eyes, mouth)
    (190, 140, 80, 255),   # 7 light wood (desk top edge)
    (120, 200, 255, 255),  # 8 light blue (sweat, zzz)
]
TRANSPARENT, WHITE, GINGER, DARK, PINK, WOOD, INK, WOOD_LIGHT, BLUE = range(9)


def canvas():
    im = Image.new("P", (W, H), TRANSPARENT)
    return im, ImageDraw.Draw(im)


def base():
    im, d = canvas()
    d.polygon([(12, 12), (13, 3), (19, 8)], fill=GINGER, outline=DARK)   # left ear
    d.polygon([(35, 12), (34, 3), (28, 8)], fill=GINGER, outline=DARK)   # right ear
    d.point([(14, 6), (14, 7), (15, 8)], fill=PINK)
    d.point([(33, 6), (33, 7), (32, 8)], fill=PINK)
    d.chord([6, 6, 41, 46], 180, 360, fill=GINGER, outline=DARK)        # body
    for x in (21, 23, 24, 26):                                            # tabby stripes
        d.line([(x, 7), (x, 9 if x in (21, 26) else 10)], fill=DARK)
    d.point([(15, 17), (16, 17), (31, 17), (32, 17)], fill=PINK)          # cheeks
    d.line([(0, 26), (47, 26)], fill=WOOD_LIGHT)                          # desk
    d.rectangle([0, 27, 47, 28], fill=WOOD)
    return im, d


def paw_up(d, left):
    d.ellipse([8, 14, 15, 20] if left else [32, 14, 39, 20], fill=WHITE)
    d.point([(10, 16), (13, 16)] if left else [(34, 16), (37, 16)], fill=PINK)


def paw_down(d, left):
    d.ellipse([8, 22, 17, 28] if left else [30, 22, 39, 28], fill=WHITE)
    d.point([(11, 24), (14, 24)] if left else [(33, 24), (36, 24)], fill=PINK)


def frame(left_down, right_down):
    im, d = base()
    (paw_down if left_down else paw_up)(d, True)
    (paw_down if right_down else paw_up)(d, False)
    return im


# --- overlays (drawn on the full 48x30 grid, then cropped) ---

def eyes_open():
    im, d = canvas()
    d.rectangle([18, 14, 18, 15], fill=INK)
    d.rectangle([29, 14, 29, 15], fill=INK)
    return im


def eyes_closed():
    im, d = canvas()
    d.line([(17, 15), (18, 15)], fill=INK)
    d.line([(29, 15), (30, 15)], fill=INK)
    return im


def mouth_smile():
    im, d = canvas()
    d.point([(22, 18), (23, 19), (24, 19), (25, 18)], fill=INK)
    return im


def mouth_frown():
    im, d = canvas()
    d.point([(22, 19), (23, 18), (24, 18), (25, 19)], fill=INK)
    return im


def sweat():
    im, d = canvas()
    d.point([(38, 8)], fill=BLUE)
    d.rectangle([37, 9, 39, 11], fill=BLUE)
    d.point([(38, 12)], fill=BLUE)
    d.point([(38, 10)], fill=WHITE)  # highlight
    return im


def zzz():
    im, d = canvas()
    d.line([(37, 4), (41, 4), (37, 8), (41, 8)], fill=BLUE)  # big Z
    d.line([(43, 0), (46, 0), (43, 3), (46, 3)], fill=BLUE)  # small z
    return im


FRAMES = {"idle": frame(False, False), "left": frame(True, False), "right": frame(False, True)}
OVERLAYS = {
    "eyes_open": eyes_open(), "eyes_closed": eyes_closed(),
    "mouth_smile": mouth_smile(), "mouth_frown": mouth_frown(),
    "sweat": sweat(), "zzz": zzz(),
}


def scaled(im):
    return im.resize((im.width * S, im.height * S), Image.NEAREST)


def crop(im):
    x0, y0, x1, y1 = im.getbbox()
    return im.crop((x0, y0, x1, y1)), x0, y0  # im is already scaled


def img_c(name, im):
    w, h = im.size
    px = im.load()
    data = []
    for r, g, b, a in PALETTE + [(0, 0, 0, 0)] * (16 - len(PALETTE)):
        data += [b, g, r, a]  # lv_color32_t byte order
    for y in range(h):
        for x in range(0, w, 2):
            data.append((px[x, y] << 4) | (px[x + 1, y] if x + 1 < w else 0))
    rows = [", ".join(f"0x{v:02x}" for v in data[i:i + 16]) for i in range(0, len(data), 16)]
    return (
        f"\nconst uint8_t bongo_{name}_map[] = {{\n    " + ",\n    ".join(rows) + ",\n};\n"
        f"\nconst lv_img_dsc_t bongo_{name} = {{\n"
        "    .header.cf = LV_IMG_CF_INDEXED_4BIT,\n"
        "    .header.always_zero = 0,\n"
        "    .header.reserved = 0,\n"
        f"    .header.w = {w},\n"
        f"    .header.h = {h},\n"
        f"    .data_size = sizeof(bongo_{name}_map),\n"
        f"    .data = bongo_{name}_map,\n}};\n"
    )


def emit():
    head = ("/*\n * Bongo cat images: 4-bit indexed (index 0 transparent).\n"
            " * GENERATED by tools/bongo_cat_draw.py -- edit that, not this file.\n"
            " *\n * SPDX-License-Identifier: MIT\n */\n")
    c = [head, '\n#include "bongo_cat_frames.h"\n']
    h = [head, "\n#pragma once\n\n#include <lvgl.h>\n\n",
         f"#define BONGO_W {W * S}\n#define BONGO_H {H * S}\n",
         f"#define BONGO_PALETTE_BODY {GINGER}\n#define BONGO_PALETTE_OUTLINE {DARK}\n",
         f"#define BONGO_FRAME_DATA_SIZE {16 * 4 + (W * S + 1) // 2 * H * S}\n\n"]
    for name, im in FRAMES.items():
        c.append(img_c(name, scaled(im)))
        h.append(f"extern const uint8_t bongo_{name}_map[];\nextern const lv_img_dsc_t bongo_{name};\n")
    for name, im in OVERLAYS.items():
        cropped, x, y = crop(scaled(im))
        c.append(img_c(name, cropped))
        h.append(f"extern const lv_img_dsc_t bongo_{name};\n"
                 f"#define BONGO_{name.upper()}_X {x}\n#define BONGO_{name.upper()}_Y {y}\n")
    open(OUT_C, "w").write("".join(c))
    open(OUT_H, "w").write("".join(h))


def preview():
    flat = [c for rgba in PALETTE for c in rgba[:3]]
    def rgb(im):
        im = im.copy(); im.putpalette(flat); return im.convert("RGB")
    def compose(fr, *ovs):
        out = fr.copy()
        for ov in ovs:
            mask = Image.new("L", ov.size)
            mask.putdata([255 if v else 0 for v in ov.tobytes()])
            out.paste(ov, (0, 0), mask)
        return scaled(out)
    shots = [
        compose(FRAMES["idle"], OVERLAYS["eyes_open"], OVERLAYS["mouth_smile"]),
        compose(FRAMES["left"], OVERLAYS["eyes_open"], OVERLAYS["mouth_smile"], OVERLAYS["sweat"]),
        compose(FRAMES["right"], OVERLAYS["eyes_open"], OVERLAYS["mouth_frown"]),
        compose(FRAMES["idle"], OVERLAYS["eyes_closed"], OVERLAYS["mouth_smile"], OVERLAYS["zzz"]),
    ]
    sheet = Image.new("RGB", (W * S * len(shots) + 10 * (len(shots) - 1), H * S))
    for i, im in enumerate(shots):
        sheet.paste(rgb(im), (i * (W * S + 10), 0))
    sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).save(OUT_PNG)


if __name__ == "__main__":
    emit()
    preview()
