# Waterfall: Most Efficient Implementation + Colored Output (SDR-Style)

## 1. Most efficient and performance-friendly implementation

### Core idea: one frequency per loop tick (state machine)

- **Do not** run a blocking 10-minute loop. Use a **state machine** so each `loop()` does a small, bounded amount of work.
- **One frequency per tick:** In `waterfallTick()`, if we're waiting for tune → do nothing (or poll once). If tune is complete → read RSSI, store in current row at current column, advance to next frequency. If the row is full → advance to next row; if 10 min or stop requested → call `waterfallStop()`.
- **Effect:** Main loop keeps running every few ms; encoder button, WiFi, and display stay responsive. No long `delay()` blocks.

### Reuse existing scan timing

- Use the **same** sequence as `Scan.cpp`: `rx.setFrequency(freq)` (one frequency), then on next tick(s) poll `rx.getTuneCompleteTriggered()` until true, then `rx.getCurrentReceivedSignalQuality()` + `rx.getCurrentRSSI()` (or SNR). No extra delays.
- Reuse `rx.setMaxDelaySetFrequency()` and mute (MUTE_TEMP) for the whole run, like `scanRun()`.

### Single buffer, write once at end

- **RAM:** One 2D buffer, fixed size (e.g. `uint8_t wf[WF_ROWS][WF_BINS]`). No allocations during the run. Typical: 400×200 = 80 KB.
- **File:** Write **once** when recording stops: open LittleFS file, write header (if any), then one contiguous write of the buffer (or row-by-row from buffer). No writing during the sweep — avoids repeated LittleFS open/write and keeps timing consistent.

### Keep recording logic separate from UI

- `waterfallTick()` only: advance state, set frequency, read RSSI, store byte. No drawing inside the tick. Optional: set a “dirty” flag and let the **existing** screen refresh (e.g. every 500 ms) show “Waterfall 3:45” via `drawMessage()` or a minimal status — no need to redraw the full layout every tick.

### Summary (efficient shape)

| Aspect            | Choice                                      |
|-------------------|---------------------------------------------|
| Loop              | State machine, one frequency per `waterfallTick()` |
| Tune / RSSI       | Reuse scan pattern (setFrequency, poll, then read) |
| Buffer            | Single 2D `uint8_t`, fixed size             |
| File write        | Once at end, contiguous or simple row loop  |
| UI during record  | Minimal (message + progress), no full redraw per tick |

---

## 2. Colored waterfall (SDR-style) — how it can work

Modern SDRs show low signal as dark blue, mid as green/yellow, high as red/white (or similar). That is a **colormap** applied to a single intensity value per pixel.

### Recommended: store intensity only; color at export (best performance)

- **On device:** Store **one value per pixel** (e.g. `uint8_t` RSSI or normalized 0–255). No RGB, no colormap math on the ESP32.
- **Export format:** Raw binary (with tiny header) or PGM (grayscale). File size stays small (e.g. 80 KB for 400×200).
- **On PC (or phone):** A small script or tool reads the file and:
  1. Loads the 2D intensity array.
  2. Applies an SDR-style colormap (e.g. “jet”, “viridis”, or custom blue→cyan→green→yellow→red→white).
  3. Saves as PNG (or PPM). Optionally adds frequency/time axes.

**Why this is best:** Device does minimal work and minimal storage; all color and image formatting happen off-device. No PNG encoder or 3× file size on the radio.

**Example (Python) after download:**

```python
import numpy as np
from PIL import Image

# Read raw header (e.g. 4 int32: width, height, min_freq, max_freq) then raw bytes
# Or read PGM and parse comment for metadata
data = np.fromfile("waterfall.raw", dtype=np.uint8)
data = data[16:].reshape(height, width)  # skip header

# SDR-style colormap: low=dark blue, high=red/white (simplified jet-like)
def colormap_jet(v):
    v = np.clip(v, 0, 255) / 255.0
    r = np.clip(1.5 - 4*np.abs(v - 0.75), 0, 1)
    g = np.clip(1.5 - 4*np.abs(v - 0.5),  0, 1)
    b = np.clip(1.5 - 4*np.abs(v - 0.25), 0, 1)
    return np.stack([r,g,b], axis=-1)

rgb = (colormap_jet(data) * 255).astype(np.uint8)
Image.fromarray(rgb).save("waterfall.png")
```

### Alternative: device outputs a colored image (if you insist)

If you want the **downloaded file** to already be a colored image:

- **Option A — PPM (P6):** Easiest on device. Header: `P6\nwidth height\n255\n`, then RGB triplets (3 bytes per pixel). So 400×200 → 240 KB. On device you’d store intensity only in RAM; when writing the file, **convert each byte to RGB** using a small lookup table (e.g. 256 entries × 3 bytes = 768 bytes for a colormap). One pass over the buffer: for each byte, output 3 bytes from the table. CPU cost: one pass; no PNG compression.
- **Option B — PNG with colormap:** Use a minimal PNG encoder (e.g. “miniz” or “pngwriter”) and a 256-entry palette (colormap). File stays smaller than PPM thanks to compression, but you add library and CPU for compression on the ESP32.

**Recommendation:** Use **intensity-only storage and file** on the device; do **colormap and PNG on PC** (or in a browser/script after download). That is the most efficient and performance-friendly way and still gives you full SDR-style color in the final image.

---

## 3. End-to-end flow (efficient + colored result)

1. **Record:** Long-press Scan → state machine runs; one frequency per `loop()`; store uint8_t per bin; muted; stop after 10 min or button.
2. **Save:** On stop, write one file (e.g. raw with 16-byte header or PGM). Single write pass, no color on device.
3. **Download:** User opens http://atsmini.local/ → “Download waterfall” → gets `waterfall.raw` or `waterfall.pgm`.
4. **Color:** User runs a small script (or web tool): read file → apply SDR colormap → save as `waterfall.png`. Result looks like a modern SDR waterfall with colors.

This keeps the firmware efficient and performance-friendly while still giving you a colored, SDR-style waterfall after a trivial conversion step.
