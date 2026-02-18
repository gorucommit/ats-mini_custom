# Fork Reference

This file is the single source of truth for this fork's custom behavior.

It replaces:
- `CUSTOM_FIRMWARE.md`
- `CHANGES_WEB_CONTROL_BRANCH.md`
- `docs/WEB_CONTROL_REFERENCE.md`

## Scope

Repository: `ats-mini-signalscale`

Upstream base: [esp32-si4732/ats-mini](https://github.com/esp32-si4732/ats-mini)

This fork has two active branches:
- `main`: stable custom UI features (Signal Scale layout + piggy mascot)
- `web-control`: everything from `main` plus web control and performance updates

## Build

Build firmware:

```bash
cd ats-mini
make
```

Regenerate embedded web control page:

```bash
python3 tools/embed_html.py docs/source/control.html ats-mini/control_html.h
python3 tools/embed_html.py docs/source/control.html ats-mini/control_html.h --embed-image steampigg.png
```

## Current Custom Features

### Signal Scale layout
- Added as a third UI layout (`UI_SIGNAL_SCALE`).
- Implemented in `ats-mini/Layout-SignalScale.cpp`.
- Uses `drawScaleWithSignals()` in `ats-mini/Draw.cpp`.

Signal bars use:
- `scanGetRSSI(freq)` and `scanGetSNR(freq)`
- visible-window RSSI baseline
- `level = max(rssiAboveBaseline, snr)`

### Band-map line
- Drawn at `y=169` in `drawBandMapLine()`.
- Shown only when `currentMode != FM`.
- Broadcast ranges (kHz): `520-1602, 2300-2500, 3200-3400, 3900-4000, 4750-5060, 5800-6325, 7200-7450, 9400-9900, 11600-12100, 13570-13870, 15100-15800, 17500-17900, 18900-19020, 21500-21850, 25600-26100`.
- Amateur ranges (kHz): `1810-2000, 3500-3800, 5250-5450, 7000-7200, 10100-10150, 14000-14530, 18070-18170, 21000-21500, 28000-29700`.

### Web control
- UI at `/control` (hosted by firmware).
- REST API + SSE stream.
- Async handlers enqueue commands only.
- Radio/I2C operations run in main loop (`webControlProcessCommands()`).

### Startup and loop behavior (`web-control`)
- Deferred WiFi connect via `netRequestConnect()` + `netTickTime()`.
- Main loop yield: `delay(2)`.
- `MIN_ELAPSED_RSSI_TIME = 80 ms`.
- `RDS_CHECK_TIME = 250 ms`.
- `SCHEDULE_CHECK_TIME = 2000 ms`.
- `BACKGROUND_REFRESH_TIME = 5000 ms`.

### RDS status in this fork
- Current `web-control` now matches upstream/main RDS parsing behavior in `Station.cpp`.
- The short-lived RT persistence experiment was reverted.

## Web Control Architecture

Primary files:
- `ats-mini/WebControl.h`
- `ats-mini/WebControl.cpp`
- `ats-mini/control_html.h` (generated)
- `docs/source/control.html`
- `tools/embed_html.py`

Important implementation facts:
- Command queue size: `24` entries (`CMD_QUEUE_SIZE`).
- Queue behavior when full: drops oldest command, keeps latest intent.
- Batched commands within `75 ms`: volume, squelch, agc, avc, tune-freq, mute, mode-set.
- SSE broadcast throttle: `50 ms` (`20 Hz` max).
- SSE max clients: `3`.
- Gzip page RAM cap: `CONTROL_PAGE_GZ_MAX = 13300`.

`/api/status` behavior:
- Built in main loop and cached in double buffer.
- Async handlers never build status directly.
- Returns `503` until first cached snapshot is ready.

## REST + SSE API (current)

Base URL: `http://atsmini.local` (or device IP)

GET:
- `/control`
- `/api/options`
- `/api/status`
- `/api/memories`
- `/api/scan` (404 if no scan data)
- `/events` (SSE event name: `status`)

POST:
- `/api/tune` body: `{"direction":1|-1}` or `{"freq":N,"bfo":N}`
- `/api/seek` body: `{"direction":1|-1}` or `{"stop":true}`
- `/api/scan` body: `{"action":"start"|"stop"|"exit"}`
- `/api/volume` body: `{"value":0..63}`
- `/api/squelch` body: `{"value":0..127}`
- `/api/agc` body: `{"auto":true}` or `{"value":N}`
- `/api/avc` body: `{"value":12..90}` (AM/SSB only)
- `/api/band` body: `{"index":N}`
- `/api/mode` body: `{"direction":1|-1}` or `{"mode":0..3}`
- `/api/step` body: `{"direction":1|-1}`
- `/api/bandwidth` body: `{"direction":1|-1}`
- `/api/mute` body: `{"mute":true|false}`
- `/api/rds` body: `{"mode":0..7}`
- `/api/memory/store?slot=N`
- `/api/memory/tune?slot=N`
- `/api/memory/clear?slot=N`

## JSON Payload Notes

`/api/status` includes:
- tuning/state: `freq, bfo, band, bandName, mode, modeName`
- signal/audio: `rssi, snr, vol, sql, agc, agcMax, agcAuto, avc, avcAvail`
- UI state: `stepIdx, stepDesc, bwIdx, bwDesc, stereo`
- station/RDS: `station, radioText, rdsModeIdx`
- control state: `inScanMode, scanHasData, mute, seeking, seekDirection`

`/api/options` includes:
- `version`
- `bands[]` with `idx,name,mode,min,max`
- `bandMap.broadcast[]` and `bandMap.amateur[]`
- `modes[]`
- limits: `agcMax`, `volumeMax`, `squelchMax`, `avcMin`, `avcMax`, `memorySlots`

`/api/scan` includes:
- `startFreq, step, count, band`
- `points[]` with `{rssi,snr}`

## Scan Behavior Notes

- `scanRun()` is blocking while scan is active.
- Audio is muted during scan via `muteOn(MUTE_TEMP, true)` and restored after.
- Web scan start/stop is coordinated through queue plus `webScanStartPending/webScanCancelPending`.

## Integration Points

- `Network.cpp`: calls `webControlInit(server)` before `onNotFound`.
- `ats-mini.ino`: calls `webControlTick()` in `loop()`.
- `Menu.cpp`: calls `webControlNotify()` after state changes (volume, avc, step, agc, mode, band, squelch, bandwidth).
- `Makefile`: includes `WebControl.cpp` and `Layout-SignalScale.cpp`.

## Maintenance Rule

When behavior changes, update this file first. Keep legacy docs as thin pointers only.
