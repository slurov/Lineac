# LineAC

A fast, lightweight **Windows autoclicker** with a hand-crafted dark interface — shipped as a single portable `.exe` with no installer, no dependencies, and nothing left on your system.

---

## Overview

LineAC is a native **Win32 autoclicker written in C++**. The whole interface is *owner-drawn* — every card, toggle, badge and dropdown is painted by hand to match the custom dark "Lineac" theme, so it doesn't rely on standard Windows controls. It runs as one self-contained executable and keeps all of its state in memory: it never touches the disk or the registry.

## Built with

| Area | Details |
|------|---------|
| **Language** | C++ (native Win32 API, no frameworks) |
| **UI** | Fully owner-drawn (`WM_PAINT`), borderless rounded window |
| **Engine** | Multi-threaded; clicks via `SendInput`; high-precision timing with `QueryPerformanceCounter` + `timeBeginPeriod(1)` |
| **Build** | MSVC (`cl`), static CRT (`/MT`), ComCtl32 v6 manifest embedded (`/MANIFEST:EMBED`) |
| **Footprint** | One `.exe`, no external DLLs, no config files — fully portable |

## Features

- **Left & Right click channels** — independent CPS (0–100) and a separate keybind for each.
- **Hold / Toggle modes** — click while the bind is held, or flip it on/off per press.
- **Click patterns**
  - **Legit** — irregular, human-like spacing.
  - **Blatant** — minimal jitter, fastest and most consistent.
  - **Custom** — tune hold duration plus delay-variation chance and strength.
- **Limited CPS** *(Blatant)* — a **Max CPS limit**: on each activation it bursts at the chosen CPS for a moment, then holds the cap. `0` disables it.
- **HighCPS Button** — a second, parallel left-click channel with its own CPS and bind; its rate stacks on top of the main left channel.
- **BlockHit** — auto-fires the right button while you physically hold RMB, with its own rate and an optional pause bind (Hold/Toggle).
- **Window targeting** — click anywhere, or restrict clicking to specific windows (up to 64) with the built-in picker.
- **Console** — an always-on-top, never-focus-stealing panel with the real measured CPS per channel plus timing diagnostics (timer resolution, priority, throttling state). Opened from the gear icon.
- **Settings panel** on the gear icon, single-instance guard, and a draggable borderless window.

## Usage

1. Run **`LineCord.exe`**.
2. Set the **CPS** for the Left and/or Right channel.
3. Click **Set bind** next to a channel and press the key you want to use.
4. Choose a **Mode** (Hold / Toggle) and a **Pattern** (Legit / Blatant / Custom).
5. *(optional)* configure the extra modules below.
6. Press **Apply**.
7. Hold (or toggle) your bind - clicking starts in the focused window.

To confirm the rate you set is the rate you get, open the gear icon (bottom
right) and turn on **Show Console**.

### Configuring the modules

**Pattern → Custom**
- **Click Duration (ms)** — how long the button stays held per click.
- **Difference Chance (%)** — how often the delay between clicks is varied.
- **Difference Strength (%)** — how far the delay can deviate.

**Pattern → Blatant → Limited CPS**
- **Max CPS limit** — the upper bound on the click rate. Each press starts with a short burst toward the cap. Leave at `0` to turn it off.

**HighCPS Button**
- Enable it, set its CPS, and bind a key (Hold). Its clicks run in parallel with the Left channel, so the rates add up.

**BlockHit**
- Enable it and set the BPS. It fires the right button automatically while you physically hold RMB. Optionally bind a **Pause** key and pick Hold/Toggle for it.

**Window targeting**
- Leave **Allow in all programs** on to click anywhere, or turn it off and use **Select window** to limit clicking to chosen windows.

## Building from source

Requires Visual Studio (or Build Tools) with the C++ desktop toolset.

```bat
:: run from "x64 Native Tools Command Prompt for VS"
build.bat
```

This produces a single `LineCord.exe`; intermediate `.obj` files are cleaned up automatically.

## Notes

- **Portable by design** — all settings live in memory. LineAC creates no config files and writes nothing to the registry; delete the `.exe` and nothing of it remains.
- Settings are **not saved** between runs — reconfigure on launch.
- CPS values are clamped to the **0-100** range.
- The process runs at high priority and opts out of Windows background throttling so the rate holds up while the window is minimised and a game has focus.

See [CHANGELOG.md](CHANGELOG.md) for what changed between releases.

> Provided as-is for personal use. Automation tools may be against the terms of service of some games or applications — use responsibly.
