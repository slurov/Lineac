# Changelog

All notable changes to LineAC are listed here.
This project follows [Semantic Versioning](https://semver.org/).

---

## [2.0.0] — 2026-09-01

A rework focused on one thing above all: **the click rate you set is the click
rate you actually get**, including while the window is minimised and a game has
focus. Every number below was measured, not estimated.

### Added

- **Console window** — a separate always-on-top panel showing the *real*
  measured CPS for each channel (LMB, RMB, HighCPS, BlockHit), plus the
  diagnostics that explain a bad rate: current timer resolution, process
  priority class, and background-throttling state. It never takes focus, so it
  can sit over a game in windowed or borderless mode without interrupting input.
  Toggle it with **Show Console**.
- **Settings panel** on the gear icon (bottom right), replacing the old info
  icon. Holds `Show Console` and the credits in one place.
- **Backdrop blur** behind the settings panel, with a gentle dim, so the panel
  reads as the focused layer.
- **Animated toggles** — every switch eases its thumb and track colour over
  ~300 ms instead of snapping.

### Fixed

- **The configured CPS was never actually reached.** Each click was scheduled
  one delay *after the previous click finished*, so the cost of `SendInput` and
  the Custom hold duration were added on top of the interval. Clicks are now
  scheduled on an absolute timeline.

  | Setting | Before | After |
  |---|---|---|
  | 20 CPS, 5 ms hold | 18.17 cps | 20.00 cps |
  | 50 CPS, 5 ms hold | 39.99 cps | 50.00 cps |

- **Clicks were being swallowed when two channels shared a button.** The main
  left channel and HighCPS each sent button-down and button-up as two separate
  `SendInput` calls, so their sequences interleaved into down, down, up, up —
  which an application counts as a single click. The same applied to the right
  channel and BlockHit. Down/up pairs are now emitted as one atomic batch.

  Measured with two threads sending 3000 clicks each on the same button:

  | | Broken pairs |
  |---|---|
  | Before | 82.53% |
  | After | 0.03% |

- **The Legit pattern always ran about 5% below the configured CPS.** Its
  jitter range multiplied the interval by 0.90–1.20, which averages 1.05. The
  range is now centred on 1.0.
- **The rate collapsed when the window lost focus.** The process ran at normal
  priority with no protection against Windows demoting background processes and
  ignoring their timer-resolution request.
- **The green titlebar button did nothing.** Its click handler was empty. It has
  been removed and the remaining two buttons re-spaced.

### Changed

- Process runs at `HIGH_PRIORITY_CLASS`, click threads at
  `THREAD_PRIORITY_HIGHEST`, and the process explicitly opts out of Windows
  background throttling (EcoQoS) and of having its timer-resolution request
  ignored. The console reports whether that opt-out applied — on machines where
  throttling is already disabled system-wide it shows `off (system)`.
- Timing uses high-resolution waitable timers rather than `Sleep` plus a spin
  loop, so accuracy no longer depends on the system-wide timer resolution.
- Settings are read through an atomic generation counter. The old code entered a
  critical section and copied the whole ~600-byte settings struct on every loop
  iteration in five threads.
- Click threads park on events instead of polling with `Sleep(1)`.
  **Idle CPU: 3.37% → 0.52% of one core.**
- Default CPS for the left and right channels is now **20.00** (was 10.00).
- Author is **@slurov**; the status bar shows the handle.

---

## [1.0.0] — original release

The first public build, tagged `LineCord`.

- Left and right click channels with independent CPS and keybinds
- Hold and Toggle modes
- Legit, Blatant and Custom click patterns
- Limited CPS for the Blatant pattern
- HighCPS parallel left-click channel
- BlockHit auto right-click while RMB is physically held
- Window targeting with a built-in picker (up to 64 windows)
- Owner-drawn dark interface, single portable executable

[2.0.0]: https://github.com/slurov/Lineac/releases/tag/v2.0.0
[1.0.0]: https://github.com/slurov/Lineac/releases/tag/v1.0.0
