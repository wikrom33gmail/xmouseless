# xmouseless

A keyboard-driven replacement for the physical mouse on X11. One C file, no runtime dependencies beyond the X libraries and `xdotool`.

This is a fork of [jbensmann/xmouseless](https://github.com/jbensmann/xmouseless) with a resolution-independent grid-jump overlay, faster startup, and click-and-exit keys (see [Changes since the fork](#changes-since-the-fork)).

## Features

- move the pointer at different speeds
- left / middle / right click — each click ends the session after exactly one guaranteed click
- scroll up and down
- jump the pointer to any of 27 grid cells (9 columns × 3 rows) on **any screen resolution**
- execute shell commands
- two-phase aiming: one coarse grid jump, then vim-style `h`/`e`/`l`/`k` fine-tuning; the grid disables itself after its single use

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

When started, xmouseless grabs the keyboard and shows a fullscreen **grid overlay**: hollow boxes with big key labels at each of the 27 cell centers. The overlay is click-through (pointer events pass through it) and stays on top of other windows. Grid-jump keys are active immediately; the vim-style fine-tune keys (`h`/`e`/`l`/`k`) activate only after your first jump, at which point the grid disables itself for good and its overlay is hidden. Press `Escape` to exit.

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

Typical workflow: press a grid key to land near the target, then fine-tune with `h`/`e`/`l`/`k` (see below), then click.
### Fine-tune (after one jump)

The workflow is two-phase. **Phase 1 — coarse:** any of the 27 grid keys jumps the pointer to that cell. The very first grid key you press performs its jump, then disables the entire grid and hides the overlay — the grid gets exactly one use per session. **Phase 2 — fine:** with the grid gone, these vim-style keys nudge the pointer (Colemak layout):

| Action | Key |
| --- | --- |
| Move up | `l` |
| Move down | `k` |
| Move left | `h` |
| Move right | `e` |

Hold a key to keep moving at the current speed; release to stop. The arrow keys still work throughout both phases. Because every click key exits the program, you usually bind a launcher (e.g. in your window manager) that starts xmouseless: jump once with a grid key, fine-tune with `h`/`e`/`l`/`k`, then click — each session ends with exactly one guaranteed click delivered via XTest after the overlay is unmapped.

## Configuration

Configuration lives in `config.h` — a C header file, but you don't need any programming knowledge to edit it. After editing, rebuild with `./buildonly.sh`.

- `move_rate`, `default_speed`: movement cadence and base speed
- `speed_bindings[]`: modifier keys that change speed while held
- `move_bindings[]`: direction keys (add diagonals if you like)
- `fine_move_bindings[]`: vim-style fine-tune keys (`h`=left, `e`=right, `l`=up, `k`=down), active only after the first grid jump
- `click_bindings[]`: click buttons; each one also exits the session
- `scroll_bindings[]`: scroll directions and rates
- `shell_bindings[]`: arbitrary shell commands, including the 27 `GRID(col,row)` jumps; these fire only before the first use (the grid disables itself after one jump)
- `exit_keys[]`: keys that exit on release

## Changes since the fork

1. **Resolution-independent grid** (`config.h`) — replaced hardcoded coordinates with a `GRID(col,row)` macro that reads the screen size at runtime via `xdotool getdisplaygeometry`. Same 9×3 layout, now works on any resolution.
2. **Grid overlay** (`xmouseless.c`) — fullscreen `override_redirect` window whose visible shape is clipped to just the box outlines and labels (everything else is truly see-through), drawing hollow double rectangles at each cell center with big key labels in a separate 64pt font (with fallbacks). Click-through via an empty input shape region; re-raises itself when other windows map or restack.
3. **Startup speed** (`xmouseless.c`) — this X server takes ~12–50 ms per font to return ascent/descent/char-width, which was ~97% of startup time. On first launch each resolved font's metrics are written to `~/.cache/xmouseless/fonts`; later launches skip the slow query entirely (only a free `XLoadFont`), dropping warm startup from ~60 ms to well under 1 ms for fonts.
4. **Click keys — unmap, click, exit** (`xmouseless.c`) — pressing a click key unmaps the grid overlay first, delivers one full press+release via XTest, then exits immediately (destroys the overlay, re-enables autorepeat, ungrabs the keyboard). Each click key ends the session after exactly one guaranteed click.
5. **Bugs fixed** (`xmouseless.c`) — two threads sharing one X connection unsafely caused boxes disappearing / missing text; fixed with `XInitThreads()` as the first line of `main()`, skipping partial Expose events (`event.xexpose.count > 0`), and an explicit redraw after re-raise.
6. **Two-phase aiming** (`config.h`, `xmouseless.c`) — vim-style fine-tune keys (`h`=left, `e`=right, `l`=up, `k`=down) that activate only after your first grid jump; the grid then disables itself and hides its overlay for the rest of the session. Escape exits in both phases.

## Tutorial

[`snail.xmouseless.md`](snail.xmouseless.md) is a slow, step-by-step walkthrough of how this program works — from the big picture down to individual functions.
