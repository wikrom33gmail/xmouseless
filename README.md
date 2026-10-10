# xmouseless

A keyboard-driven replacement for the physical mouse on X11. One C file, no runtime dependencies beyond the X libraries and `xdotool`.

This is a fork of [jbensmann/xmouseless](https://github.com/jbensmann/xmouseless) with a resolution-independent grid-jump overlay, faster startup, and click-and-exit keys (see [Changes since the fork](#changes-since-the-fork)).

## Features

- move the pointer at different speeds
- left / middle / right click — each click ends the session after exactly one guaranteed click
- scroll up and down
- jump the pointer to any of 27 grid cells (9 columns × 3 rows) on **any screen resolution**
- execute shell commands

## Installation

There is no Makefile; build directly:

```sh
./buildonly.sh
# or manually:
gcc -Wall -g -O2 -o xmouseless xmouseless.c -lX11 -lXtst -lpthread -lXext
```

On Debian-based distros you may need the headers first:

```sh
sudo apt-get install libx11-dev libxtst-dev libc6-dev
```

The grid-jump bindings also require `xdotool` at runtime.

## Usage

When started, xmouseless grabs the keyboard and shows a fullscreen **grid overlay**: hollow boxes with big key labels at each of the 27 cell centers. The overlay is click-through (pointer events pass through it) and stays on top of other windows. All bindings below are active immediately; press `Escape` to exit.

| Action | Key(s) |
| --- | --- |
| Move pointer (hold) | Arrow keys |
| Slow speed while held (20 px/s, default 500) | `Return` |
| Scroll up / down (hold) | `Page_Up` / `Page_Down` |
| Left click + exit | `8` |
| Middle click + exit | `9` |
| Right click + exit | `0` |
| Grid jump to a cell | see below |
| Exit without clicking | `Escape` (on release) |

### Grid jump

Each of the 27 keys jumps the pointer to the center of one grid cell, computed from the live screen size (`xdotool getdisplaygeometry`) — no hardcoded coordinates, so it works on any resolution. The overlay labels match these bindings:
PS: I use colemak layout

```
q   w   f   p   g   j   l   u   y
a   r   s   t   d   h   n   e   i
z   x   c   v   b   k   m   ,   .
```

Typical workflow: press a grid key to land near the target, fine-tune with the arrow keys, then click. Because every click key exits the program, you usually bind a launcher (e.g. in your window manager) that starts xmouseless, and each session ends with exactly one guaranteed click delivered via XTest after the overlay is unmapped.

## Configuration

Configuration lives in `config.h` — a C header file, but you don't need any programming knowledge to edit it. After editing, rebuild with `./buildonly.sh`.

- `move_rate`, `default_speed`: movement cadence and base speed
- `speed_bindings[]`: modifier keys that change speed while held
- `move_bindings[]`: direction keys (add diagonals if you like)
- `click_bindings[]`: click buttons; each one also exits the session
- `scroll_bindings[]`: scroll directions and rates
- `shell_bindings[]`: arbitrary shell commands, including the 27 `GRID(col,row)` jumps
- `exit_keys[]`: keys that exit on release

## Changes since the fork

1. **Resolution-independent grid** (`config.h`) — replaced hardcoded coordinates with a `GRID(col,row)` macro that reads the screen size at runtime via `xdotool getdisplaygeometry`. Same 9×3 layout, now works on any resolution.
2. **Grid overlay** (`xmouseless.c`) — fullscreen `override_redirect` window whose visible shape is clipped to just the box outlines and labels (everything else is truly see-through), drawing hollow double rectangles at each cell center with big key labels in a separate 64pt font (with fallbacks). Click-through via an empty input shape region; re-raises itself when other windows map or restack.
3. **Startup speed** (`xmouseless.c`) — this X server takes ~12–50 ms per font to return ascent/descent/char-width, which was ~97% of startup time. On first launch each resolved font's metrics are written to `~/.cache/xmouseless/fonts`; later launches skip the slow query entirely (only a free `XLoadFont`), dropping warm startup from ~60 ms to well under 1 ms for fonts.
4. **Click keys — unmap, click, exit** (`xmouseless.c`) — pressing a click key unmaps the grid overlay first, delivers one full press+release via XTest, then exits immediately (destroys the overlay, re-enables autorepeat, ungrabs the keyboard). Each click key ends the session after exactly one guaranteed click.
5. **Bugs fixed** (`xmouseless.c`) — two threads sharing one X connection unsafely caused boxes disappearing / missing text; fixed with `XInitThreads()` as the first line of `main()`, skipping partial Expose events (`event.xexpose.count > 0`), and an explicit redraw after re-raise.

## Tutorial

[`snail.xmouseless.md`](snail.xmouseless.md) is a slow, step-by-step walkthrough of how this program works — from the big picture down to individual functions.
