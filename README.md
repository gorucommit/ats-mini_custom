# ATS Mini — Custom Fork

![](docs/source/_static/esp32-si4732-ui-theme.jpg)

This project is a custom firmware fork for the ATS-Mini radio.  
It keeps upstream ATS-Mini behavior and adds practical quality-of-life improvements for daily listening.

## What This Fork Adds

- **Signal Scale view**: an extra screen layout that gives a quick visual sense of activity across nearby frequencies.
- **Band-map hints**: color cues on the scale to help distinguish broadcast and amateur segments.
- **Web Control** (`web-control` branch): control the radio from a phone or computer at `http://atsmini.local/control`.
- **General usability/performance improvements**: smoother interaction and faster startup behavior.
- **Piggy mascot**: small visual element in the UI.

## Branch Guide

- **`main`**: stable custom UI/features without web control.
- **`web-control`**: includes everything from `main` plus browser-based remote control.
- **`waterfall-experiment`**: experimental/legacy branch, not actively maintained.

### `main`

Tracks upstream with the following additions:

- **Signal Scale layout** — A third UI layout option (alongside Default and S-Meter) that replaces the bottom frequency scale with signal strength markers from scan data.
- **Band-map line** — quick visual cues for broadcast and amateur segments.
- **Piggy mascot** — small branding element in the main UI.

### `web-control` — Web control + performance improvements

Includes everything from `main`, plus:

- **Browser-based remote control UI** at `http://atsmini.local/control` (or device IP).
- **Live status updates** in the page, so meters and radio state track changes in near real time.
- **Full day-to-day controls from the browser**, including:
  - tuning and seek
  - band and mode changes
  - volume, squelch, AGC/AVC, mute
  - memory tune/store/clear
  - scan start/stop and scan data view
  - RDS mode switching
- **Improved startup flow** so the radio feels ready faster and network tasks are less intrusive.
- **Smoother interaction under load**, with better responsiveness while using both hardware controls and web controls.
- **General stability/performance cleanup** focused on long-running everyday use.

## Getting Started

1. Flash firmware using the upstream ATS-Mini flashing guide.
2. Choose your branch (`main` or `web-control`) based on your use case.
3. If using `web-control`, configure Wi-Fi on the radio and open `http://atsmini.local/control`.
   If mDNS does not resolve, use the radio's IP address instead.

## Documentation

- **Upstream user docs (manual, flashing, hardware):** <https://esp32-si4732.github.io/ats-mini/>

## Upstream Credits

This fork builds on work from:

- ATS-Mini upstream: <https://github.com/esp32-si4732/ats-mini>
- Volos Projects: <https://github.com/VolosR/TEmbedFMRadio>
- PU2CLR (Ricardo): <https://github.com/pu2clr/SI4735>
- Ralph Xavier: <https://github.com/ralphxavier/SI4735>
- Goshante: <https://github.com/goshante/ats20_ats_ex>
- G8PTN (Dave): <https://github.com/G8PTN/ATS_MINI>

## Discuss

- [GitHub Discussions](https://github.com/esp32-si4732/ats-mini/discussions)
- [TalkRadio Telegram Chat](https://t.me/talkradio/174172)
- [Si4732 Mini Receiver All Bands (Facebook)](https://www.facebook.com/share/g/18hjHo4HEe/)
