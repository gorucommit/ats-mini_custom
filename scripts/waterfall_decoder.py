#!/usr/bin/env python3
"""
Decode ATS-Mini waterfall .raw file to a colored PNG image.
Usage: python waterfall_decoder.py [input.raw] [output.png]
  If no args: reads waterfall.raw, writes waterfall.png
  One arg: input file, writes <name>.png
  Two args: input and output paths.
"""

import argparse
import struct
import sys

try:
    import numpy as np
except ImportError:
    np = None
try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    Image = ImageDraw = ImageFont = None

MAGIC = 0x31465757  # "WF11" little-endian


def load_raw(path: str) -> dict:
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 4:
        raise ValueError("File too short")
    magic = struct.unpack("<I", data[0:4])[0]
    if magic != MAGIC:
        raise ValueError(f"Bad magic 0x{magic:08X}, expected 0x{MAGIC:08X}")
    if len(data) < 4 + 2 + 2 + 4 + 4 + 4 + 4:
        raise ValueError("Header too short")
    bins = struct.unpack("<H", data[4:6])[0]
    rows = struct.unpack("<H", data[6:8])[0]
    min_freq_khz = struct.unpack("<I", data[8:12])[0]
    max_freq_khz = struct.unpack("<I", data[12:16])[0]
    step_khz = struct.unpack("<I", data[16:20])[0]
    interval_ms = struct.unpack("<I", data[20:24])[0]
    payload = data[24:]
    expected = rows * bins
    if len(payload) < expected:
        raise ValueError(f"Payload short: got {len(payload)}, need {expected}")
    return {
        "bins": bins,
        "rows": rows,
        "min_freq_khz": min_freq_khz,
        "max_freq_khz": max_freq_khz,
        "step_khz": step_khz,
        "interval_ms": interval_ms,
        "pixels": payload[:expected],
    }


def colormap_jet(value: float) -> tuple:
    """Map 0..1 to (R,G,B) jet-like colormap."""
    if value <= 0:
        return (0, 0, 128)
    if value >= 1:
        return (128, 0, 0)
    v = value * 4
    if v <= 1:
        return (0, 0, 128 + int(127 * v))
    if v <= 2:
        return (0, int(255 * (v - 1)), 255)
    if v <= 3:
        return (int(255 * (v - 2)), 255, int(255 * (3 - v)))
    return (255, int(255 * (4 - v)), 0)


def decode_to_image(info: dict, use_numpy: bool = True) -> "Image.Image":
    bins, rows = info["bins"], info["rows"]
    raw = info["pixels"]

    if use_numpy and np is not None and Image is not None:
        arr = np.frombuffer(raw, dtype=np.uint8).reshape((rows, bins))
        # Normalize to 0..1 (optional: clip by percentiles for better contrast)
        lo, hi = arr.min(), arr.max()
        if hi > lo:
            norm = (arr.astype(np.float32) - lo) / (hi - lo)
        else:
            norm = np.zeros_like(arr, dtype=np.float32)
        # Jet-like colormap: simple RGB from norm
        r = np.clip(1.5 - 4 * np.abs(norm - 0.75), 0, 1)
        g = np.clip(1.5 - 4 * np.abs(norm - 0.5), 0, 1)
        b = np.clip(1.5 - 4 * np.abs(norm - 0.25), 0, 1)
        rgb = (np.stack([r, g, b], axis=-1) * 255).astype(np.uint8)
        return Image.fromarray(rgb)
    # Fallback without numpy: build RGB row by row
    if Image is None:
        raise RuntimeError("PIL is required for image output")
    rgb_rows = []
    lo = min(raw)
    hi = max(raw)
    span = hi - lo if hi > lo else 1
    for r in range(rows):
        row = []
        for c in range(bins):
            v = (raw[r * bins + c] - lo) / span
            row.append(colormap_jet(v))
        rgb_rows.append(row)
    # Build image: list of rows of (R,G,B)
    h, w = rows, bins
    img = Image.new("RGB", (w, h))
    pix = img.load()
    for y in range(h):
        for x in range(w):
            pix[x, y] = rgb_rows[y][x]
    return img


def format_freq_label(khz: int) -> str:
    """Format frequency for axis label."""
    if khz >= 10000:
        return f"{khz / 1000:.1f} MHz"
    return f"{khz} kHz"


def add_frequency_axis(img: "Image.Image", info: dict) -> "Image.Image":
    """Add a frequency axis with tick marks and labels below the waterfall."""
    if ImageDraw is None or Image is None:
        return img
    bins = info["bins"]
    min_f = info["min_freq_khz"]
    max_f = info["max_freq_khz"]
    step_f = info["step_khz"]
    w, h = img.size
    axis_h = 22
    new_h = h + axis_h
    out = Image.new("RGB", (w, new_h), (30, 30, 30))
    out.paste(img, (0, 0))
    draw = ImageDraw.Draw(out)
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 10)
    except (OSError, AttributeError):
        try:
            font = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 10)
        except (OSError, AttributeError):
            font = ImageFont.load_default()
    text_color = (200, 200, 200)
    tick_color = (160, 160, 160)
    n_ticks = min(10, max(3, (max_f - min_f) // max(step_f, 1) + 1))
    if max_f <= min_f:
        n_ticks = 1
    for i in range(n_ticks):
        if n_ticks == 1:
            freq_khz = min_f
        else:
            freq_khz = min_f + (max_f - min_f) * i // (n_ticks - 1)
        if step_f > 0:
            bin_idx = (freq_khz - min_f) / step_f
        else:
            bin_idx = 0
        x = int(bin_idx / max(bins - 1, 1) * (w - 1))
        x = max(0, min(x, w - 1))
        draw.line([(x, h), (x, new_h)], fill=tick_color, width=1)
        draw.line([(x, new_h - 6), (x, new_h)], fill=tick_color, width=1)
        label = format_freq_label(freq_khz)
        try:
            bbox = draw.textbbox((0, 0), label, font=font)
            tw = bbox[2] - bbox[0]
        except AttributeError:
            try:
                tw = draw.textsize(label, font=font)[0]
            except (TypeError, AttributeError):
                tw = len(label) * 6
        draw.text((x - tw // 2, h + 2), label, fill=text_color, font=font)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="Decode ATS-Mini waterfall .raw to PNG")
    ap.add_argument("input", nargs="?", default="waterfall.raw", help="Input .raw file")
    ap.add_argument("output", nargs="?", default=None, help="Output PNG (default: input with .png)")
    args = ap.parse_args()
    out = args.output
    if out is None:
        out = args.input.rsplit(".", 1)[0] + ".png" if "." in args.input else args.input + ".png"
    try:
        info = load_raw(args.input)
        print(f"bins={info['bins']} rows={info['rows']} "
              f"freq={info['min_freq_khz']}-{info['max_freq_khz']} kHz "
              f"step={info['step_khz']} kHz interval={info['interval_ms']} ms")
        img = decode_to_image(info)
        img = add_frequency_axis(img, info)
        img.save(out)
        print(f"Saved {out}")
        return 0
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
