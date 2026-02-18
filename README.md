# ATS Mini — Custom Fork

![](docs/source/_static/esp32-si4732-ui-theme.jpg)

Custom firmware fork for the SI4732 (ESP32-S3) Mini/Pocket Receiver, adding new UI layouts, a piggy mascot, performance improvements, and **Web control** on top of the [upstream ATS-Mini firmware](https://github.com/esp32-si4732/ats-mini).

## Branches

### `main` — Stable custom features

Tracks upstream with the following additions:

- **Signal Scale layout** — A third UI layout option (alongside Default and S-Meter) that replaces the bottom frequency scale with signal strength markers from scan data. Uses both SNR and RSSI-above-baseline for more accurate signal representation. Broadcast bands are marked in red, amateur bands in blue.
- **Piggy mascot** — A tiny piggy bitmap drawn on-screen next to the frequency display (loaded from PROGMEM, cached to RAM).

### `web-control` — Web control + performance improvements

Includes everything from `main`, plus:

- **Web control** — Remote control UI at **http://atsmini.local/control** (or device IP). Full band/frequency/mode control, VU-style meters, scan spectrum display, faders for volume/squelch/AGC/AVC, band selector, and RDS display. Uses Server-Sent Events for live status and a command queue so radio changes run on the main loop. See `docs/FORK_REFERENCE.md` for API and integration details.
- **Startup time improvements** — SI4732 power-on is parallelized with display initialization; splash screen and backlight appear immediately; WiFi/BLE connection is deferred to run in the background instead of blocking `setup()`.
- **Timer overflow safety** — All `millis()`-based timer variables changed from `long` to `uint32_t`.
- **Scan RSSI/SNR fix** — Removed off-by-one error in `scanGetRSSI`/`scanGetSNR` that prevented the strongest signal from reaching full scale; added bounds clamping to `[0.0, 1.0]` and flat-signal divide-by-zero guard.
- **Layout code deduplication** — Extracted `drawLayoutTop()` and `drawLayoutBottom()` shared helpers, eliminating repeated code across the three layout files.
- **Delay reduction** — `useBand()` settling delay reduced from 100 ms to 50 ms; main loop yield from 5 ms to 2 ms; WiFi status display delays from 2 s to 1 s.
- **Button semantics** — Three-way button behaviour (click / short-press / long-press) with correct handling for volume, squelch, and other modal commands; long-press in VFO mode toggles display sleep (once per hold).

## Building

Requires [arduino-cli](https://arduino.github.io/arduino-cli/) with ESP32-S3 board support. From the `ats-mini/` directory:

```bash
make
```

To embed the Web control page (and optional mascot image) into the firmware:

```bash
python3 tools/embed_html.py docs/source/control.html ats-mini/control_html.h
# Optional: add mascot image (resized if >3.5 KB to stay under CONTROL_PAGE_GZ_MAX)
python3 tools/embed_html.py docs/source/control.html ats-mini/control_html.h --embed-image steampigg.png
```

## Upstream

Based on the following sources:

* Upstream ATS-Mini:  https://github.com/esp32-si4732/ats-mini
* Volos Projects:     https://github.com/VolosR/TEmbedFMRadio
* PU2CLR, Ricardo:    https://github.com/pu2clr/SI4735
* Ralph Xavier:       https://github.com/ralphxavier/SI4735
* Goshante:           https://github.com/goshante/ats20_ats_ex
* G8PTN, Dave:        https://github.com/G8PTN/ATS_MINI

## Documentation

- **Fork reference (single source of truth):** `docs/FORK_REFERENCE.md`
- **Upstream hardware, software and flashing:** <https://esp32-si4732.github.io/ats-mini/>

## Discuss

* [GitHub Discussions](https://github.com/esp32-si4732/ats-mini/discussions) — feature requests, observations, sharing, etc.
* [TalkRadio Telegram Chat](https://t.me/talkradio/174172) — informal space to chat in Russian and English.
* [Si4732 Mini Receiver All Bands](https://www.facebook.com/share/g/18hjHo4HEe/) — Facebook group (unofficial).
