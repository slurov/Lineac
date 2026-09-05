# LineAC

A small Windows autoclicker I wrote in C++ because every other one I tried
either couldn't hold the rate I set or wanted an installer, a runtime and a
folder full of config files. This one is a single portable `.exe`: no
dependencies, no registry keys, nothing left behind when you delete it.

<br>

## Download

Grab the latest build from **[Release/](Release/)** — or from the
[Releases page](https://github.com/slurov/Lineac/releases). Older versions stay
in the same folder, so you can always go back to one.

Windows 10/11, 64-bit. Just run the `.exe`.

<br>

## What it does

**Two independent click channels** for the left and right button, each with its
own CPS (0–100), its own keybind, and Hold or Toggle mode — click while the bind
is down, or press once to flip it on.

**Three patterns.** *Legit* spaces clicks irregularly so the rhythm isn't
machine-perfect. *Blatant* keeps jitter to a minimum and is the fastest and most
consistent, with an optional max-CPS cap that bursts on activation and then
holds the limit. *Custom* lets you set the hold duration per click plus how
often and how far the delay is allowed to drift.

**HighCPS** is a second left-click channel running in parallel with its own rate
and bind — the two rates add up. **BlockHit** fires the right button
automatically while you physically hold RMB, with its own rate and an optional
pause bind.

**Window targeting.** By default it clicks anywhere. Turn that off and use the
built-in picker to limit it to specific windows (up to 64).

**A console** you can open from the gear icon: an always-on-top panel that never
steals focus, showing the *measured* CPS per channel next to the numbers you
asked for, plus the things that explain a bad rate — timer resolution, process
priority, background-throttling state. It sits fine over a game in windowed or
borderless mode.

<br>

## Getting started

1. Run the `.exe`.
2. Set the CPS for the left and/or right channel.
3. Hit **Set bind** next to a channel and press the key you want.
4. Pick a mode (Hold / Toggle) and a pattern (Legit / Blatant / Custom).
5. Press **Apply**, then hold or toggle your bind.

If you want to see that the rate you set is the rate you're getting, open the
gear icon in the bottom right and turn on **Show Console**.

<br>

## Under the hood

The whole interface is owner-drawn — every card, toggle, badge and dropdown is
painted by hand in `WM_PAINT` rather than using standard Windows controls, which
is why it looks nothing like a default Win32 app.

Clicks go out through `SendInput` from dedicated threads that park on events
instead of polling, scheduled on an absolute timeline so the cost of the call
itself doesn't push the interval out. Down and up are emitted as one atomic
batch, so two channels sharing a button can't interleave into a swallowed click.
Waiting is done on high-resolution waitable timers, and the process runs at high
priority and opts out of background throttling — that's what keeps the rate up
when the window is minimised and a game has focus.

Settings live in memory only. Nothing is written to disk, which also means
nothing carries over between runs.

<br>

## Building it yourself

You need Visual Studio (or just the Build Tools) with the C++ desktop workload.
From an **x64 Native Tools Command Prompt**:

```bat
build.bat
```

That produces `LineAC.exe` and cleans up the `.obj` files. Static CRT, embedded
manifest, no external DLLs.

<br>

## Good to know

- Settings aren't saved between runs — reconfigure on launch.
- CPS is clamped to 0–100 per channel.
- Only one instance runs at a time.
- Windows SmartScreen may warn about an unsigned executable. It's unsigned
  because signing certificates cost money; build it from source if you'd rather
  not take my word for it.

[CHANGELOG.md](CHANGELOG.md) has the full list of what changed between versions.

<br>

---

Written by [@slurov](https://github.com/slurov). Provided as-is for personal
use — automating input is against the rules of plenty of games and
applications, so that part is on you.
