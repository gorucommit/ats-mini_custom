# Web Control – Firmware & UI Reference

This document describes the **ats-mini-signalscale** firmware, the **web control** feature, and how they work together. Use it as the single source of truth for the codebase.

---

## 1. Base Firmware

**Repo:** `ats-mini-signalscale`

### 1.1 What the radio is

- **Hardware:** ESP32-S3, SI4735 (I2C), TFT display, rotary encoder + button, WiFi.
- **Modes:** FM (0), LSB (1), USB (2), AM (3). FM is 64–108 MHz (frequency in 100ths: 6400–10800). Other modes use kHz (150–30000).
- **Bands:** Defined in `Menu.cpp` as `Band bands[]`: VHF (FM), ALL, 11M–90M (broadcast), MW3/MW2/MW1, 160M–10M (amateur), CB. Each band has min/max freq, default mode, step index, bandwidth index.
- **Global state (Common.h, ats-mini.ino, Menu.cpp):** `currentFrequency`, `currentBFO`, `currentMode`, `bandIdx`, `volume`, `currentSquelch`, `rssi`, `snr`, AGC/AVC indices, etc. Preferences (Storage) persist bands, memories, settings.

### 1.2 Main loop (ats-mini.ino)

- **loop()** runs: encoder/button → `doTune` / `doSeek` / menu; **RSSI/SNR** polling (`processRssiSnr()`); **webControlTick()**; RDS/schedule/NTP; prefs tick; **netTickTime()** (WiFi connect); redraw.

### 1.3 Key modules

- **Menu.cpp:** Bands, steps, bandwidths, modes; `doVolume`, `doTune`, `doSeek`, `doMode`, `doBand`, `doStep`, `doBandwidth`, `doAgc`, AVC; `selectBand()`, `getCurrentBand()`, `getCurrentStep()`, `getCurrentBandwidth()`.
- **Scan.cpp:** `scanRun(centerFreq, step)` runs a blocking sweep (200 points), fills `scanData[]` with RSSI/SNR; `scanHasData()`, `scanGetRSSI(freq)`, `scanGetSNR(freq)`.
- **Network.cpp:** WiFi (AP/STA), mDNS (atsmini.local), AsyncWebServer: `/`, `/memory`, `/config`, then **webControlInit(server)**, then `onNotFound`, `/setconfig`.

---

## 2. Web Control – Architecture

**Purpose:** Remote control and status over WiFi via a browser at `http://atsmini.local/control`.

**Design:**

- **REST** for commands (POST with JSON body).
- **SSE** for live status (GET `/events`, event type `status`, JSON payload).
- **Thread safety:** ESPAsyncWebServer runs handlers in a **different FreeRTOS task** than `loop()`. The SI4735 is driven over **I2C**, which is not thread-safe. So:
  - Handlers **never** call `rx.*` or other I2C/radio code.
  - Handlers only **enqueue commands** in a small ring buffer.
  - **webControlTick()** runs in **loop()** and **dequeues** commands and runs the real radio code (command queue pattern).

### 2.1 Files added for web control

| File | Role |
|------|------|
| **ats-mini/WebControl.h** | Public API: `webControlInit()`, `webControlTick()`, `webControlNotify()`, `webControlClientCount()`. Lists all endpoints. |
| **ats-mini/WebControl.cpp** | Command queue, JSON builders, GET/POST handlers, SSE, command processors. |
| **ats-mini/control_html.h** | Generated: gzipped control page in PROGMEM (`control_html_gz[]`, `control_html_gz_len`). |
| **docs/source/control.html** | Source of the steampunk UI (single file: HTML + CSS + JS). |
| **tools/embed_html.py** | Minifies HTML/CSS/JS, gzips, and emits `control_html.h`. |

### 2.2 Integration points (changes to existing files)

- **Network.cpp**
  - `#include "WebControl.h"`.
  - Hardcoded WiFi for testing: `WIFI_TEST_SSID` / `WIFI_TEST_PASS`; `wifiMulti.addAP(WIFI_TEST_SSID, WIFI_TEST_PASS)` in `wifiConnect()` before loading prefs.
  - **Before** `server.onNotFound()`: `webControlInit(server)`.
- **ats-mini.ino**
  - `#include "WebControl.h"`.
  - In `loop()`: `webControlTick()`.
- **Menu.cpp**
  - `#include "WebControl.h"`.
  - After any change that should be reflected on the web: `webControlNotify()`. Called from: `doVolume`, AVC setter, `doStep`, `doAgc`, `doMode`, `doBand`, `doSquelch`, `doBandwidth`.
- **Makefile**
  - `WebControl.cpp` in SRC, `WebControl.h` in HEADERS.

---

## 3. Command Queue (WebControl.cpp)

- **Types:** `WebCmdType`: TUNE_DIR, TUNE_FREQ, SEEK, VOLUME, SQUELCH, AGC, AVC, BAND, MODE, MODE_SET, STEP, BANDWIDTH, MUTE, MEMORY_TUNE.
- **Queue:** `cmdQueue[8]`, head (write from async task), tail (read in loop). `cmdEnqueue()` / `cmdDequeue()`.
- **Flow:** POST handler parses JSON → `cmdEnqueue(cmd, p1, p2)` → returns 200 or 4xx. In `loop()`, `webControlTick()` calls `cmdDequeue()` and `processCommand(cmd)` which runs `doTune`, `processSetFrequency`, `processSetVolume`, `selectBand`, etc., and sets `statusDirty = true`.

---

## 4. REST API

**Base URL:** `http://atsmini.local` (or device IP).

| Method | Path | Body / params | Effect |
|--------|------|----------------|--------|
| GET | /control | — | Serves gzipped HTML from RAM (copied from PROGMEM at init). |
| GET | /api/options | — | Static config: version, bands[], modes[], agcMax, volumeMax, squelchMax, avcMin/Max, memorySlots. |
| GET | /api/status | — | Current state: freq, bfo, band, bandName, mode, modeName, rssi, snr, vol, sql, agc, agcMax, agcAuto, avc, avcAvail, stepIdx, stepDesc, bwIdx, bwDesc, stereo, station, radioText. |
| GET | /events | — | SSE stream; event `status` sends same JSON as /api/status (throttled ~100 ms, only when state changed). |
| POST | /api/tune | `{"direction": ±1}` or `{"freq": int, "bfo": int}` | Tune step or set frequency. |
| POST | /api/seek | `{"direction": ±1}` | Seek up/down. |
| POST | /api/volume | `{"value": 0–63}` | Set volume. |
| POST | /api/squelch | `{"value": 0–127}` | Set squelch. |
| POST | /api/agc | `{"auto": true}` or `{"value": n}` | AGC auto or manual (mode-dependent max). |
| POST | /api/avc | `{"value": 12–90}` | AVC (step 2); no-op in FM. |
| POST | /api/band | `{"index": 0..N-1}` | Switch band. |
| POST | /api/mode | `{"direction": ±1}` or `{"mode": 0|1|2|3}` | Cycle mode or set mode by index (VHF: only 0; others: 1=LSB, 2=USB, 3=AM). |
| POST | /api/step | `{"direction": ±1}` | Cycle tuning step. |
| POST | /api/bandwidth | `{"direction": ±1}` | Cycle bandwidth. |
| POST | /api/mute | `{"mute": true|false}` | Mute/unmute. |
| GET | /api/memories | — | List memories (slot, freq, band, mode, name). |
| POST | /api/memory/store | query `slot=N` | Store current freq/band/mode to slot N. |
| POST | /api/memory/tune | query `slot=N` | Tune to memory slot N. |
| POST | /api/memory/clear | query `slot=N` | Clear memory slot N. |

**Frequency units:** FM: firmware uses 100ths of MHz (e.g. 9850 = 98.50 MHz). AM/SSB: kHz (e.g. 7100 = 7100 kHz). Same in status and in POST `/api/tune` body.

**Stereo:** Not read from I2C in the async task. `cachedStereo` is updated in `webControlTick()` from `rx.getCurrentPilot()` and used in `buildStatusJSON()`.

---

## 5. Control Page (control.html)

**Served as:** Gzipped, from RAM, at GET `/control`. Generated from `docs/source/control.html` via `tools/embed_html.py` → `ats-mini/control_html.h` (PROGMEM) → copied to `s_controlPageGz[]` at init.

### 5.1 Layout and style

- **Theme:** Steampunk (browns, brass #c9a227, copper, steel; Georgia/Courier/Monospace).
- **Structure:** Header (title, connection status, RSSI/SNR text); main grid: **left** VU meters (RSSI/SNR needles), **center** frequency + BFO + band/mode, station name, radio text, signal spectrum canvas; **right** mixer faders (VOL, SQL, AGC with AUTO toggle, AVC) and Info (STEP / BANDWIDTH with arrows). Bottom: control row (seek, tune, step arrows, BW arrows, MUTE); band selector in three columns (Amateur, General, Broadcast).

### 5.2 Behaviour

- **Connection:** On load, fetches `/api/options` (for bands/modes), opens `EventSource('/events')`. On `status` events, parses JSON and calls `updateUI(s)`.
- **Frequency:** Click on frequency → inline text input. Submit: parse (64–108 as FM MHz, else kHz), find band that contains freq (prefer non-ALL), POST `/api/band` if band changed, then POST `/api/tune` with `{freq, bfo}`.
- **Mode:** FM shows static “STEREO”/“MONO”. Non-FM shows a `<select>` (LSB, USB, AM); change → POST `/api/mode` with `{mode: index}`.
- **STEP / BW:** Left/right arrows in control row and in Info panel; POST `/api/step` or `/api/bandwidth` with `{direction: ±1}`.
- **Faders:** VOL, SQL, AGC, AVC: drag knob or use range; on release send POST with `{value}`. AGC AUTO toggle sends `{auto: true}` or `{value: 1}`.
- **Bands:** Rendered from `opts.bands` into three groups (ham: LSB/USB or CB; general: FM/VHF, ALL, MW*; broadcast: rest). Click → POST `/api/band` with `{index}`.
- **VU meters:** Animated from current `rssi`/`snr` (smoothed). Signal canvas is decorative (RSSI-based).
- **Visibility:** Animations (VU, canvas) pause when tab is hidden (`visibilitychange`).

### 5.3 Rebuilding the embedded page

After editing `docs/source/control.html`:

```bash
python3 tools/embed_html.py docs/source/control.html ats-mini/control_html.h
```

Then rebuild firmware. Max gzip size in code is 16 KiB (`CONTROL_PAGE_GZ_MAX`).

---

## 6. Summary of “what we changed”

1. **New module:** WebControl (queue, REST, SSE, JSON, command execution in loop).
2. **New assets:** control.html, embed script, generated control_html.h.
3. **Network:** WebControl init before 404; hardcoded WiFi for testing.
4. **Loop:** One `webControlTick()` per iteration (command drain + SSE broadcast when state changed).
5. **Menu:** `webControlNotify()` after volume, AVC, step, AGC, mode, band, squelch, bandwidth so the web UI updates quickly after encoder/button changes.

This is the full picture of the firmware, the changes for web control, and how the web control works end to end.
