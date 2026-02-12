# ATS Mini — Custom Fork

![](docs/source/_static/esp32-si4732-ui-theme.jpg)

Custom firmware fork for the SI4732 (ESP32-S3) Mini/Pocket Receiver, adding new UI layouts, a piggy mascot, waterfall spectrum recording, and performance improvements on top of the [upstream ATS-Mini firmware](https://github.com/esp32-si4732/ats-mini).

## Branches

### `main` — Stable custom features

Tracks upstream with the following additions:

- **Signal Scale layout** — A third UI layout option (alongside Default and S-Meter) that replaces the bottom frequency scale with signal strength markers from scan data. Uses both SNR and RSSI-above-baseline for more accurate signal representation. Broadcast bands are marked in red, amateur bands in blue.
- **Piggy mascot** — A tiny piggy bitmap drawn on-screen next to the frequency display (loaded from PROGMEM, cached to RAM).

### `waterfall-experiment` — Experimental features + fixes

Includes everything from `main`, plus:

- **Waterfall spectrum recording** — Long-press SCAN to start a 10-minute muted recording that sweeps the current band, sampling RSSI at each frequency step. The raw data is saved to LittleFS as a `.raw` file (with frequency metadata in the header). Both the `.raw` file and a Python decoder script are downloadable from the ATS-Mini config portal (`/waterfall` and `/waterfall_decoder.py`). The decoder script generates a color-mapped PNG image with a frequency axis.
- **Startup time improvements** — SI4732 power-on is parallelized with display initialization; splash screen and backlight appear immediately; WiFi/BLE connection is deferred to run in the background instead of blocking `setup()`.
- **Dynamic waterfall buffer** — The 80KB recording buffer is `malloc`'d only during recording and freed afterwards, reducing idle RAM usage from ~43% to ~18%.
- **Timer overflow safety** — All `millis()`-based timer variables changed from `long` to `uint32_t`.
- **Scan RSSI/SNR fix** — Removed off-by-one error in `scanGetRSSI`/`scanGetSNR` that prevented the strongest signal from reaching full scale; added bounds clamping to `[0.0, 1.0]` and flat-signal divide-by-zero guard.
- **Layout code deduplication** — Extracted `drawLayoutTop()` and `drawLayoutBottom()` shared helpers, eliminating ~120 lines of repeated code across the three layout files.
- **Delay reduction** — `useBand()` settling delay reduced from 100ms to 50ms; main loop yield from 5ms to 2ms; WiFi status display delays from 2s to 1s.
- **Button fix** — Restored correct three-way button semantics (click/short-press/long-press) that were broken during the waterfall button integration, fixing click-to-exit for volume, squelch, and other modal commands.

## Building

Requires [arduino-cli](https://arduino.github.io/arduino-cli/) with ESP32-S3 board support. From the `ats-mini/` directory:

```bash
make
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

The upstream hardware, software and flashing documentation is available at <https://esp32-si4732.github.io/ats-mini/>

## Discuss

* [GitHub Discussions](https://github.com/esp32-si4732/ats-mini/discussions) — feature requests, observations, sharing, etc.
* [TalkRadio Telegram Chat](https://t.me/talkradio/174172) — informal space to chat in Russian and English.
* [Si4732 Mini Receiver All Bands](https://www.facebook.com/share/g/18hjHo4HEe/) — Facebook group (unofficial).
