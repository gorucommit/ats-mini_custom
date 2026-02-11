#!/usr/bin/env python3
"""
Convert piggy.PNG to a tiny RGB565 C header for ATS-Mini.
Usage: python piggy_to_header.py [piggy.PNG] [output piggy.h]
  Resizes to PIGGY_W x PIGGY_H (default 20x20), outputs PROGMEM array.
"""
import argparse
import sys

try:
    from PIL import Image
except ImportError:
    Image = None

PIGGY_W = 20
PIGGY_H = 20


def rgb_to_rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input", nargs="?", default="piggy.PNG", help="Input PNG")
    ap.add_argument("output", nargs="?", default=None, help="Output .h file (default: ats-mini/piggy.h)")
    args = ap.parse_args()
    out = args.output
    if out is None:
        out = "ats-mini/piggy.h" if args.input == "piggy.PNG" else args.input.rsplit(".", 1)[0] + ".h"

    if not Image:
        print("PIL/Pillow required: pip install Pillow", file=sys.stderr)
        return 1

    img = Image.open(args.input).convert("RGBA")
    img = img.resize((PIGGY_W, PIGGY_H), Image.Resampling.LANCZOS)
    w, h = img.size
    pixels = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            if a < 128:
                pixels.append(0x0000)  # transparent -> black (blends with dark theme)
            else:
                pixels.append(rgb_to_rgb565(r, g, b))

    with open(out, "w") as f:
        f.write("#ifndef PIGGY_BITMAP_H\n#define PIGGY_BITMAP_H\n\n")
        f.write(f"#define PIGGY_W {w}\n#define PIGGY_H {h}\n\n")
        f.write("static const uint16_t piggyBitmap[] PROGMEM = {\n  ")
        for i, p in enumerate(pixels):
            f.write(f"0x{p:04X}")
            if i < len(pixels) - 1:
                f.write(",")
            if (i + 1) % 12 == 0 and i < len(pixels) - 1:
                f.write("\n  ")
        f.write("\n};\n\n#endif /* PIGGY_BITMAP_H */\n")
    print(f"Wrote {out} ({w}x{h})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
