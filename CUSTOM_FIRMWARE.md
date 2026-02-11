# ATS Mini Custom Firmware – Changes & Notes for AI Agents

This document describes all custom modifications made to the [esp32-si4732/ats-mini](https://github.com/esp32-si4732/ats-mini) firmware and key technical context. Use it to maintain or extend this fork.

---

## Repository & Build

- **Upstream:** https://github.com/esp32-si4732/ats-mini  
- **This fork (published):** https://github.com/gorucommit/ats-mini_custom  
- **Local clone (this tree):** `ats-mini-signalscale` – original + Signal scale + band-map + later tweaks.  
- **Build:** From repo root: `cd ats-mini && make build`  
- **Target:** ESP32-S3 (profile `esp32s3-ospi`). Merged image: `ats-mini/build/esp32.esp32.esp32s3/ats-mini.ino.merged.bin` (flash at 0x0).  
- **Display:** 320×170 sprite (`spr`), TFT_eSPI; bottom row of scale at y=169.

---

## 1. Signal Scale UI Layout

- **What:** New UI layout option **“Signal scale”** (Berndt-style): same as Default layout but the bottom tuning scale can show **signal strength bars** when scan data exists.
- **Where:**  
  - `Menu.h`: `#define UI_SIGNAL_SCALE 2`  
  - `Menu.cpp`: `"Signal scale"` added to `uiLayoutDesc[]`  
  - `Draw.cpp`: `drawScaleWithSignals()`, `drawLayoutSignalScale()` case in `drawScreen()`  
  - `Draw.h`: Declarations for `drawScaleWithSignals`, `drawLayoutSignalScale`  
  - `Layout-SignalScale.cpp`: New file – layout routine that calls `drawScaleWithSignals()` when showing the scale  
  - `Makefile`: `Layout-SignalScale.cpp` in `SRC`  
- **Scan dependency:** Bars only appear when `scanHasData()` is true (scan has been run and completed). `scanHasData()` and scan data access live in `Scan.cpp` / `Common.h`.

---

## 2. Signal Scale Bars – Data and Logic

- **Current behaviour (do not assume “RSSI only” or “SNR only”):**  
  - **Baseline:** Average RSSI over the **visible scale range** (same frequency range as the scale) is computed each frame and used as “band baseline”.  
  - **Per bin:** For each scale bin: `rssi = scanGetRSSI(freq)`, `snr = scanGetSNR(freq)`, `rssiAboveBaseline = max(0, rssi - baseline)`.  
  - **Level:** `level = max(rssiAboveBaseline, snr)`. If `level <= 0`, nothing is drawn.  
  - **Height:** Bar height ∝ `level` (capped to 1.0). Bar color: `TH.scan_snr`.  
- **Important:** Bars use **both** SNR and RSSI above the band-average baseline so that weak-but-readable stations and strong-but-noisy ones both show. Do not change back to “SNR only” or “RSSI only” without user request.

---

## 3. Band-Map Line (1-pixel at Bottom of Scale)

- **What:** A 1-pixel-high line at **y = 169** (bottom row of display) that moves with the tuner scale.  
  - **Red:** Broadcast bands (MW 520–1602 kHz, 120 m, 90 m, 75 m, 60 m, 49 m, 41 m, 31 m, 25 m, 22 m, 19 m – exact ranges in `Draw.cpp` `getBandMapType()`).  
  - **Blue:** Amateur bands (160 m, 80 m, 60 m, 40 m, 30 m, 20 m – exact ranges in `getBandMapType()`).  
  - **No pixel:** Frequencies outside those ranges.  
- **Where:** `Draw.cpp`: `getBandMapType()`, `drawBandMapLine()`, called from `drawScaleWithSignals()`.  
- **FM:** Band-map line is **skipped in FM mode** (`if (currentMode != FM) drawBandMapLine(freq)`). In FM, the scale uses 10 kHz units; the band-map logic is in kHz and would false-match (e.g. 94–99 MHz as 9.4–9.9 MHz). So we do not draw the band-map for FM at all.  
- **Coordinate mapping:** Same as `drawScale()`: `scaleFreq = centerFreqKhz/10 - 20 - slack`, per-pixel `freqKhz = (scaleFreq + (x+offset)/8) * 10`. All in kHz for band-map lookup.

---

## 4. Scale and Scan Grid – Why 10 kHz (7220, 7230, 7240…)

- The **scale** is built in fixed **10 kHz** steps: loop variable is “tens of kHz”, incremented by 1 each column (`freq++` → +10 kHz). So tick marks and columns land on 7220, 7230, 7240 kHz.  
- The **scan** is started with `scanRun(currentFrequency, 10)` → step **10 kHz**. So scan data and signal-scale bars also sit on 10 kHz bins.  
- The **tuning step** (e.g. 5 kHz for shortwave) from `getCurrentStep()` is **not** used for drawing the scale or for the scan; it only affects encoder tuning. So stations on 7225 kHz don’t get their own scale line or bar – they fall between bins.

---

## 5. Band-Change “Thump” (Diagnosis Only – No Code Changes)

- **Two hushes (before SSB load):** Caused by the **mute** sequence in `Utils.cpp` `muteOn(MUTE_TEMP, true)`: amp is turned off first, then hardware mute, then 50 ms, then `rx.setAudioMute(true)`. Two separate analog events → two hushes.  
- **Big thump (after SSB load):** Caused at **unmute** in `Menu.cpp` `selectBand()`. The SI4732 is reconfigured (SSB load, `useBand()`, `setBandwidth()`, etc.) and there is no extra delay after the last register write before unmuting. When the amp and chip are unmuted, a DC/transient is amplified → thump.  
- **Possible fixes (not implemented):** (1) Mute chip first, then amp (reorder in `muteOn`). (2) Delay before unmute (e.g. 150 ms) in `selectBand()`. (3) Unmute chip first, short delay, then amp. User asked for diagnosis only.

---

## 6. EIBI Download Over WiFi

- **Trigger:** Settings → “Load EiBi” → `eibiLoadSchedule()` in `EIBI.cpp`.  
- **URL:** `EIBI_URL` → default `http://eibispace.de/dx/eibi.txt` (HTTP).  
- **Flow:** `http.begin()` + `http.GET()` (blocking). Body read byte-by-byte; lines parsed with `eibiParseLine()`; valid entries written as binary `StationSchedule` structs to LittleFS temp file. On success: remove old `/schedules.bin`, rename temp to `/schedules.bin`. Cancel: encoder button during download.  
- **Storage:** LittleFS. Lookups (`eibiLookup`, `eibiNext`, `eibiPrev`, `eibiAtSameFreq`) **open the file each time** – no persistent handle. Good target for optimisation (caching, or keeping file open for a batch of lookups).

---

## 7. Scan Screen (During/After Scan)

- **“Scanning…”** is shown once; then `scanRun()` **blocks** until the scan finishes. No live-updating chart during the scan.  
- **After the scan,** when the Scan menu item is selected (`currentCmd == CMD_SCAN`), the layout draws the **line chart** via `drawScanGraphs()`.  
- **Green line:** `TH.scan_rssi` – data from `scanGetRSSI()` → `scanData[].rssi` (filled by `rx.getCurrentRSSI()` during scan). **RSSI = total received power (signal + noise).**  
- **Blue line:** `TH.scan_snr` – data from `scanGetSNR()` → `scanData[].snr` (filled by `rx.getCurrentSNR()`). **SNR = signal-to-noise ratio.**

---

## 8. Key Files and Symbols

| File / area        | Relevance |
|--------------------|-----------|
| `ats-mini/Draw.cpp` | Scale, band-map, signal-scale bars, `drawScreen()` layout switch |
| `ats-mini/Layout-SignalScale.cpp` | Signal scale layout; calls `drawScaleWithSignals()` when not WiFi/scan/RDS |
| `ats-mini/Menu.cpp` | `selectBand()`, `doBand()`, menu items, `uiLayoutDesc` |
| `ats-mini/Menu.h`   | `UI_SIGNAL_SCALE`, `CMD_*` |
| `ats-mini/Scan.cpp` | `scanHasData()`, `scanGetRSSI()`, `scanGetSNR()`, scan data array |
| `ats-mini/Common.h` | `scanHasData()` declaration, `currentMode`, FM/AM/SSB |
| `ats-mini/Utils.cpp`| `muteOn()` (MUTE_TEMP, MUTE_MAIN, etc.) |
| `ats-mini/EIBI.cpp` | `eibiLoadSchedule()`, `eibiLookup()`, file open/read |
| `ats-mini/ats-mini.ino` | `loop()`, `processRssiSnr()`, `needRedraw`, `drawScreen()` |

---

## 9. Performance and Efficiency (Summary for Agents)

- **Main loop:** `delay(5)` every iteration; RSSI/SNR every 200 ms; RDS every 250 ms (FM); EIBI `identifyFrequency` every 2 s; background full redraw every 5 s.  
- **Display:** Full redraw every time (`spr.fillSprite` + full layout + `pushSprite`). No dirty regions.  
- **Signal scale:** Baseline RSSI is recomputed every frame (sum over ~47 bins); could be cached when scan completes.  
- **Band-map:** 320 × `getBandMapType()` per frame when active; could be optimised with a precomputed segment.  
- **EIBI:** File opened on every lookup; caching last (freq, minute, result) or keeping file open briefly would reduce I2C/LittleFS load.  
- A fuller list of **options** (loop delay, throttling, caching, EIBI, NVS, etc.) was produced in a prior analysis; refer to that for concrete optimisation ideas.

---

## 10. What Was *Not* Changed (Preserved from Upstream)

- **Band definitions** (menu bands, frequency limits): unchanged. The band-map line uses its **own** hardcoded broadcast/amateur tables in `Draw.cpp`; it does not alter the radio’s band list or tuning limits.  
- **RDS behaviour** in this fork: no “centered RT” or “PS+RT” compact layout; no change to RDS options or scrolling in this codebase.  
- **Mute/unmute sequence:** Diagnosis only; no reorder or delay changes in `Utils.cpp` or `Menu.cpp` for thump.

---

*Document generated for maintainers and AI agents working on this fork. Update this file when making further custom changes.*
