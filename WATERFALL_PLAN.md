# Waterfall Recording – Analysis and Implementation Plan

## Goal

- **Trigger:** Long-press on **SCAN** (when Scan menu item is selected). Short-press keeps current behaviour (single band scan).
- **Behaviour:** Continuously sweep the **entire selected band** in a loop for **10 minutes** (or until user stops with button), **muted** for the whole duration.
- **Output:** A **waterfall** (frequency vs time, signal strength as intensity) stored in a **file** that can easily be converted to an image on a PC.
- **Download:** Use the existing web portal at **http://atsmini.local/** – add a **“Download waterfall”** button that serves the file.

---

## Feasibility: Yes

- The SI4732 is a **single-tuner** receiver: only one frequency is measured at a time. So we do **not** get an instant FFT spectrum like an SDR; we get a **sequential sweep**.
- A waterfall is then: **X = frequency**, **Y = time** (or vice versa), **colour/intensity = signal strength**. Each **sweep** across the band gives one **row** (or column) of the waterfall; repeating sweeps for 10 minutes stacks the **time** axis.
- Existing **scan** logic already does: set frequency → wait for tune → read RSSI/SNR. We reuse that pattern; we only add: repeat for full band, repeat sweeps for 10 min, store in a 2D buffer or stream to file, mute throughout, and stop on button or timeout.

---

## Constraints and Numbers

- **Band width:** Bands in `Menu.cpp` have `minimumFreq` and `maximumFreq` (in 10 kHz units, e.g. 7200–7600 for 41 m). So e.g. 400 “points” = 4 MHz. The “ALL” band is 150–30000 (2985 “points”) – too many for a dense sweep in 10 min.
- **Time per point:** Current scan uses `rx.setFrequency()` (has internal tuning delay, ~30–80 ms per step) plus `getTuneCompleteTriggered()` polling. So roughly **50–100 ms per frequency**. For 200 points → **10–20 s per sweep**. For 10 minutes → **30–60 sweeps**.
- **Memory:** 400 freq bins × 200 time rows = 80 KB (uint8_t). ESP32-S3 has enough RAM; if we cap at 400×200 we are safe. Larger bands can use **coarser step** (e.g. step 20 or 50 kHz) so that “number of freq bins” is capped (e.g. 400).
- **File size:** 400×200 = 80 KB raw. LittleFS and HTTP download can handle this.
- **Button:** Long-press is **2 s** (`LONG_PRESS_INTERVAL` in `Button.h`). When user is on Scan and **releases** after long-press, `clickHandler(CMD_SCAN, false)` runs (shortPress = false). Today that just does `currentCmd = CMD_NONE`. We change it to **start waterfall recording** instead.

---

## Implementation Options

### Option A – Fixed matrix in RAM, write file at end (simplest)

- **Dimensions:** e.g. `WF_FREQ_BINS = 400`, `WF_TIME_ROWS = 200`. So max 80 KB buffer.
- **Sweep:** For the selected band, compute `step_khz = max(10, (maxFreq - minFreq) * 10 / WF_FREQ_BINS)` so we use at most 400 bins and cover the full band. Each sweep: for each bin, `setFrequency()`, wait tune, `getCurrentReceivedSignalQuality()` + `getCurrentRSSI()` (or SNR), store in current row. Then next row; total rows = min(200, sweeps completed in 10 min).
- **Storage:** One 2D array `uint8_t waterfall[WF_TIME_ROWS][WF_FREQ_BINS]`. After 10 min or stop, write to LittleFS (see “File format” below), then clear state.
- **Pros:** Simple, predictable RAM, single write at end.  
- **Cons:** Fixed max time (e.g. 200 rows); if sweep is slow, we might get fewer rows.

### Option B – Stream to file row-by-row (no full 2D in RAM)

- **Header:** Write a small header (magic, width, height, min_freq, max_freq, step_khz, interval_sec). Height can be a placeholder (e.g. 0) and we seek back to fix it at the end.
- **Loop:** Each sweep writes one row of raw uint8_t to the open file. No 2D buffer; only one row in RAM.
- **Pros:** Works for very long runs (e.g. 30 min) without large RAM; file grows incrementally.  
- **Cons:** Slightly more code (header fix at end); need to keep file open and avoid conflicts with other LittleFS use (e.g. EIBI).

### Option C – Hybrid: cap time rows, stream to file

- Same as B but we **cap** the number of rows (e.g. 200). After 200 rows we stop and close the file (or overwrite from start for “rolling” – not required for 10 min). Simplifies “max file size” and “height” in header.

**Recommendation:** Start with **Option A** (fixed matrix, write at end) for simplicity. If you later need longer than 10 min or very wide bands, switch to B/C.

---

## File Format (easy conversion to image)

Two practical choices:

### Format 1: PGM (grayscale image)

- **Structure:** `P5\n` + `width height\n` + `255\n` + raw bytes (row-major). Optional comment line: `# min_freq_khz max_freq_khz step_khz interval_sec`.
- **Pros:** Any image tool (ImageMagick, Python PIL/Pillow) can open it; trivial to convert to PNG.  
- **Cons:** No binary metadata (you put freq/time in a comment or separate sidecar).

**Example header:**  
`P5\n400 200\n255\n# 7200 7600 10 15\n`  
Then 400×200 bytes. A small Python script can read the comment, then `numpy.fromfile` + `reshape` and save as PNG with correct axis labels.

### Format 2: Custom binary + small header

- **Header (e.g. 28 bytes):** magic "WF01" (4), width (4), height (4), min_freq_khz (4), max_freq_khz (4), step_khz (4), interval_sec (4). Then raw `width * height` bytes.
- **Pros:** Single file, all metadata for axes and scaling.  
- **Cons:** You need a tiny converter script (Python) to read header + buffer and export PNG.

**Recommendation:** Use **PGM with comment line** so the file is viewable as an image immediately; optional small script to add frequency/time axes when converting to PNG.

---

## Web Download (http://atsmini.local/)

- **Route:** Add e.g. `server.on("/waterfall", HTTP_GET, ...)`.
- **Behaviour:** If waterfall file exists (e.g. `LittleFS.exists("/waterfall.pgm")`), serve it with:
  - `Content-Type: application/octet-stream` (or `image/x-portable-graymap` for PGM).
  - `Content-Disposition: attachment; filename="waterfall.pgm"` so the browser offers “Save as” instead of displaying.
- **API:** AsyncWebServer allows something like `request->send(LittleFS, "/waterfall.pgm", "application/octet-stream", true)` and there may be a way to set the download filename; otherwise send the file in chunks and set headers manually.
- **Button on portal:** In `webRadioPage()` (in `Network.cpp`), add a line such as:  
  `"<P><A HREF='/waterfall' DOWNLOAD='waterfall.pgm'>Download waterfall</A></P>"`  
  (or a button that links to `/waterfall`). Show only when `LittleFS.exists("/waterfall.pgm")` (or your chosen path) so the button appears when a recording exists.

---

## Code Layout (suggested)

1. **Waterfall.h / Waterfall.cpp (new)**  
   - `waterfallStart()`: check band valid, mute, init dimensions and buffer (or open file), set state RECORDING.  
   - `waterfallTick()`: called from main loop when state == RECORDING; runs one frequency step or one sweep step; when a sweep finishes, advance row; when 10 min elapsed or button stop, call `waterfallStop()`.  
   - `waterfallStop()`: unmute, write file to LittleFS (PGM or custom), clear buffer/close file, set state IDLE.  
   - `waterfallIsRecording()`: so main loop and UI can show “Recording…” and avoid starting a second run.  
   - Use **existing** `checkStopSeeking()` or a dedicated `waterfallStopRequested()` (set by encoder button in loop) to stop early.

2. **Menu.cpp**  
   - In `clickScan(bool shortPress)`: if `!shortPress` (long-press), call `waterfallStart()` instead of doing `currentCmd = CMD_NONE`. Optionally keep `currentCmd = CMD_SCAN` and show “Waterfall recording…” in the layout when `waterfallIsRecording()`.

3. **Main loop (ats-mini.ino)**  
   - When `waterfallIsRecording()`: call `waterfallTick()` each iteration; optionally reduce or skip other heavy work (e.g. skip RDS/EIBI) to keep sweep timing stable. Check encoder button (or reuse seek stop flag) to request stop.

4. **Network.cpp**  
   - Add `server.on("/waterfall", ...)` to serve the file with attachment headers.  
   - In `webRadioPage()`, add “Download waterfall” link/button when the waterfall file exists.

5. **UI during recording**  
   - When `waterfallIsRecording()` is true and `currentCmd == CMD_SCAN`, the layout can show a simple message (e.g. “Waterfall 3:45” and “Press to stop”) instead of the normal scan graph. Reuse `drawMessage()` or a minimal screen so the user sees progress and knows they can press to stop.

---

## Sweep and Timing Details

- **Full band:** `minFreq = getCurrentBand()->minimumFreq`, `maxFreq = getCurrentBand()->maximumFreq`. These are in 10 kHz units (e.g. 7200 = 72 MHz). So in kHz: `minFreq*10` to `maxFreq*10`. Number of 10 kHz steps: `(maxFreq - minFreq)`. Cap to `WF_FREQ_BINS` (e.g. 400) by using step = `(maxFreq - minFreq) * 10 / WF_FREQ_BINS` in kHz, or use 10 kHz step and cap the number of points so we don’t exceed 400.
- **Tune delay:** Reuse the same delay as scan (e.g. `rx.setMaxDelaySetFrequency(...)` before starting, restore after). Per-point timing will be similar to the existing scan (tune complete poll + RSSI read).
- **Stop conditions:** (1) `millis() - startTime >= 10*60*1000`. (2) Button pressed: set a flag in the main loop when encoder button is pressed (or reuse `seekStop`), and check it in `waterfallTick()` after each point or each row.

---

## Summary Table

| Item              | Recommendation / value                          |
|-------------------|--------------------------------------------------|
| Trigger           | Long-press on SCAN menu item                    |
| Duration          | 10 minutes or until button stop                 |
| Audio             | Muted for entire recording (MUTE_TEMP)          |
| Freq axis         | Full selected band; step 10 kHz or adaptive cap 400 bins |
| Time axis         | One row per sweep; cap e.g. 200 rows            |
| Value stored      | RSSI (or SNR) per bin, uint8_t                  |
| File format       | PGM with comment line (min/max freq, step, interval) |
| File path         | e.g. `/waterfall.pgm` on LittleFS               |
| Download          | GET `/waterfall` → send file, attachment        |
| Portal            | “Download waterfall” link when file exists      |
| New code          | Waterfall.cpp/.h; small changes in Menu, loop, Network |

---

## Risks and Mitigations

- **Long blocking:** The 10-minute loop will block the main loop unless we make each “tick” non-blocking (one frequency per call to `waterfallTick()`). So we **must** use a state machine: one or a few frequencies per `loop()` iteration, then return; next iteration continues. That way encoder button and WiFi keep getting CPU.
- **LittleFS during EIBI:** If the user has EIBI and the web server, avoid writing the waterfall file while EIBI is being read (or vice versa). Prefer writing the file only at the end of the 10 min (Option A); then conflict is minimal.
- **Memory:** 80 KB for 400×200 is acceptable on ESP32-S3. If you hit low-memory issues, reduce to 300×150 or stream to file (Option B/C).

---

## Can We Achieve It?

**Yes.** The firmware already has: band limits, scan-style stepping and RSSI/SNR read, mute, button handling (long vs short), and a web server with file serving capability. The new parts are: a state machine for “waterfall recording” (one sweep step per loop tick), a 2D buffer or row-by-row file write, PGM (or custom) export, and one new HTTP route + one link on the portal. This document is the plan; implementation can follow the steps above and Option A + PGM unless you prefer streaming or custom binary.
