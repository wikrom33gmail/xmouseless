/*
 * xmouseless - keyboard-driven mouse replacement for X11
 * Compile: gcc xmouseless.c -o xmouseless -lX11 -lXtst -lpthread -lXft -lfontconfig
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/shape.h>

#define LENGTH(X)   (sizeof X / sizeof X[0])

typedef struct { KeySym keysym; float x; float y; } MoveBinding;
typedef struct { KeySym keysym; unsigned int button; } ClickBinding;
typedef struct { KeySym keysym; float x; float y; } ScrollBinding;
typedef struct { KeySym keysym; unsigned int speed; } SpeedBinding;
typedef struct { KeySym keysym; char *command; } ShellBinding;

#include "config.h"

Display *dpy;
Window root;
pthread_t movethread;
static unsigned int speed;

struct { float x, y, speed_x, speed_y; } mouseinfo;
struct { float x, y, speed_x, speed_y; } scrollinfo;

/* Info window globals */
Window   info_win;
GC       info_gc;
XFontStruct *info_font;
int      capture_pipe[2];
char     outbuf[65536];
size_t   outlen;
static int info_x, info_y;
#define INFO_W 880
#define INFO_H 400

void get_pointer(void);
void move_relative(float x, float y);
void click(unsigned int button, Bool is_press);
void click_full(unsigned int button);
void scroll(float x, float y);
void handle_key(KeyCode keycode, Bool is_press);
void init_x(void);
void close_x(int exit_status);
void init_capture(void);
void info_window_read_pipe(void);
void info_window_create(void);
void info_window_redraw(void);
void info_window_destroy(void);

/* ---- Output capture ---- */

void init_capture(void) {
  if (pipe(capture_pipe)) return;
  dup2(capture_pipe[1], STDOUT_FILENO);
  dup2(capture_pipe[1], STDERR_FILENO);
  int flags = fcntl(capture_pipe[0], F_GETFL, 0);
  fcntl(capture_pipe[0], F_SETFL, flags | O_NONBLOCK);
}

void info_window_read_pipe(void) {
  if (outlen >= sizeof(outbuf) - 1) {
    memmove(outbuf, outbuf + 8192, outlen - 8192);
    outlen -= 8192;
  }
  ssize_t n = read(capture_pipe[0], outbuf + outlen, sizeof(outbuf) - 1 - outlen);
  if (n > 0) outlen += n;
  outbuf[outlen] = '\0';
}

/* ---- Info window ---- */

void info_window_create(void) {
  int sh = DisplayHeight(dpy, DefaultScreen(dpy));
  /* Start bottom-left */
  info_x = 20;
  info_y = sh - INFO_H - 20;

  XSetWindowAttributes wa;
  wa.override_redirect = True;
  wa.background_pixel = 0xCCFFCC;
  wa.border_pixel = 0x004400;
  wa.event_mask = StructureNotifyMask | ExposureMask;

  info_win = XCreateWindow(dpy, root, info_x, info_y, INFO_W, INFO_H, 3,
      CopyFromParent, InputOutput,
      CopyFromParent, CWOverrideRedirect | CWBackPixel |
      CWBorderPixel | CWEventMask, &wa);

  XSelectInput(dpy, info_win, ExposureMask | StructureNotifyMask);

  /* Load readable monospace font */
  info_font = XLoadQueryFont(dpy, "-misc-fixed-medium-r-normal--44-*-*-*-*-*-iso10646-1");
  if (!info_font) info_font = XLoadQueryFont(dpy, "-misc-fixed-medium-r-normal--36-300-75-75-C-200-iso10646-1");
  if (!info_font) info_font = XLoadQueryFont(dpy, "-misc-fixed-medium-r-normal--14-130-75-75-C-80-iso10646-1");

  // info_font = XLoadQueryFont(dpy, "-misc-fixed-medium-r-normal--44-300-75-75-C-200-iso10646-1");
  // if (!info_font) info_font = XLoadQueryFont(dpy, "fixed:size=36");
  // if (!info_font) info_font = XLoadQueryFont(dpy, "-misc-fixed-medium-r-normal--14-130-75-75-C-80-iso10646-1");

  info_gc = XCreateGC(dpy, info_win, 0, NULL);
  XSetForeground(dpy, info_gc, 0x222222);
  XSetBackground(dpy, info_gc, 0xCCFFCC);
  if (info_font) XSetFont(dpy, info_gc, info_font->fid);

  XMapWindow(dpy, info_win);
  XFlush(dpy);
}

static const char *help_lines[] = {
  "=== xmouseless active ===",
  "",
  "Arrows     move cursor",
  "8 / 9 / 0  left / middle / right click",
  "PgUp / PgDn  scroll down / up",
  "Hold Enter to lower speed for precision",
  "Esc        exit",
  NULL
};

static void draw_line(const char *text, int x, int y) {
  if (!info_font || !text) return;
  XDrawString(dpy, info_win, info_gc, x, y, text, strlen(text));
}

static int text_width(const char *text) {
  if (!info_font || !text) return 0;
  return XTextWidth(info_font, text, strlen(text));
}

void info_window_redraw(void) {
  XClearWindow(dpy, info_win);
  if (!info_font) return;

  int lh = info_font->ascent + info_font->descent + 4;
  int y = 30;
  const int margin = 24;
  const int maxw = INFO_W - margin * 2;

  for (int i = 0; help_lines[i]; i++) {
    draw_line(help_lines[i], margin, y);
    y += lh;
  }

  if (outlen > 0) {
    /* Only show last 3 lines */
    char *lines[3];
    int llen[3] = {0, 0, 0};
    int lcount = 0;
    for (int i = 0; i < 3; i++) lines[i] = malloc(1024);

    /* Walk backwards through outbuf to find last newline */
    char *pstart = outbuf + outlen - 1;
    while (*pstart != '\n' && pstart > outbuf) pstart--;
    if (*pstart == '\n') pstart++;

    int offset = (int)(pstart - outbuf);
    const char *p = outbuf + offset;
    size_t remaining = outlen - offset;

    while (lcount < 3 && remaining > 0) {
      const char *nl = memchr(p, '\n', remaining);
      int lmax = nl ? (int)(nl - p) : (int)remaining;
      if (lmax > 1023) lmax = 1023;
      memcpy(lines[lcount], p, lmax);
      lines[lcount][lmax] = '\0';
      llen[lcount] = lmax;

      int tw = text_width(lines[lcount]);
      if (tw > maxw) {
        /* Truncate to fit */
        while (llen[lcount] > 1 && text_width(lines[lcount]) > maxw) {
          llen[lcount]--;
          lines[lcount][llen[lcount]] = '\0';
        }
      }

      draw_line(lines[lcount], margin, y);
      y += lh;
      lcount++;

      if (nl) { p = nl + 1; remaining -= (size_t)(nl - p) + 1; }
      else break;
    }
    for (int i = 0; i < 3; i++) free(lines[i]);
  }
}

/* --- grid overlay: hollow boxes with key labels at jump positions --- */
static Window grid_win = None;
static GC grid_gc;
static XFontStruct *grid_font;    /* NEW: separate, bigger font for labels */

/* must match shell_bindings in config.h */
static const char *grid_keys[3][9] = {
  { "q", "w", "f", "p", "g", "j", "l", "u", "y" },
  { "a", "r", "s", "t", "d", "h", "n", "e", "i" },
  { "z", "x", "c", "v", "b", "k", "m", ",", "." },
};

#define GRID_BOX_W 200            /* was 96 — boxes grow with the font */
#define GRID_BOX_H 150            /* was 64 */

void grid_overlay_redraw(void) {
  if (grid_win == None || !grid_font) return;
  int sw = DisplayWidth(dpy, DefaultScreen(dpy));
  int sh = DisplayHeight(dpy, DefaultScreen(dpy));

  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 9; col++) {
      /* same center formula as GRID() in config.h */
      int cx = sw * (2 * col + 1) / 18;
      int cy = sh * (2 * row + 1) / 6;
      int x = cx - GRID_BOX_W / 2;
      int y = cy - GRID_BOX_H / 2;

      XSetForeground(dpy, grid_gc, 0x004400);
      XDrawRectangle(dpy, grid_win, grid_gc, x, y, GRID_BOX_W, GRID_BOX_H);
      XDrawRectangle(dpy, grid_win, grid_gc, x + 3, y + 3,
          GRID_BOX_W - 6, GRID_BOX_H - 6);

      const char *label = grid_keys[row][col];
      int len = strlen(label);
      int tw = XTextWidth(grid_font, label, len);
      XSetBackground(dpy, grid_gc, 0xCCFFCC);
      XSetForeground(dpy, grid_gc, 0x222222);
      XDrawImageString(dpy, grid_win, grid_gc,
          cx - tw / 2, cy + grid_font->ascent / 2,
          label, len);
    }
  }
  XFlush(dpy);
}

void grid_overlay_create(void) {
  int sw = DisplayWidth(dpy, DefaultScreen(dpy));
  int sh = DisplayHeight(dpy, DefaultScreen(dpy));

  XSetWindowAttributes wa;
  wa.override_redirect = True;
  wa.background_pixmap = None;   /* pseudo-transparent */
  wa.border_pixel = 0;
  wa.event_mask = ExposureMask;

  grid_win = XCreateWindow(dpy, root, 0, 0, sw, sh, 0,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixmap |
      CWBorderPixel | CWEventMask, &wa);

  /* empty input region: pointer events pass through */
  XShapeCombineRectangles(dpy, grid_win, ShapeInput, 0, 0, NULL, 0, ShapeSet, 0);

  grid_gc = XCreateGC(dpy, grid_win, 0, NULL);


  /* NEW: try progressively smaller fonts until one loads */
  grid_font = XLoadQueryFont(dpy, "-misc-fixed-medium-r-normal--64-*-*-*-*-*-iso10646-1");
  if (!grid_font) grid_font = XLoadQueryFont(dpy, "fixed:size=64");
  if (!grid_font) grid_font = XLoadQueryFont(dpy, "-misc-fixed-medium-r-normal--36-300-75-75-C-200-iso10646-1");
  if (!grid_font) grid_font = info_font;   /* last resort: reuse info font */
  if (grid_font) XSetFont(dpy, grid_gc, grid_font->fid);

  XMapWindow(dpy, grid_win);
  grid_overlay_redraw();

  /* NEW: help box stays visible above the overlay */
  if (info_win != None) {
    XRaiseWindow(dpy, info_win);
    info_window_redraw();
  }
}

void grid_overlay_destroy(void) {
  if (grid_win != None) {
    XDestroyWindow(dpy, grid_win);
    grid_win = None;
    XFlush(dpy);
  }
}

void info_window_destroy(void) {
  if (info_win) { XUnmapWindow(dpy, info_win); XDestroyWindow(dpy, info_win); info_win = 0; }
  if (info_gc) { XFreeGC(dpy, info_gc); info_gc = 0; }
  if (info_font) { XFreeFont(dpy, info_font); info_font = 0; }
}

void info_window_avoid_cursor(void) {
  int cx, cy, di;
  unsigned int dui;
  Window dummy;
  if (!XQueryPointer(dpy, root, &dummy, &dummy, &cx, &cy, &di, &di, &dui)) return;

  int margin = 80;
  int sw = DisplayWidth(dpy, DefaultScreen(dpy));
  int sh = DisplayHeight(dpy, DefaultScreen(dpy));

  /* Check if cursor is within margin of window edges */
  if (!(cx >= info_x - margin && cx <= info_x + INFO_W + margin &&
        cy >= info_y - margin && cy <= info_y + INFO_H + margin)) return;

  /* Calculate squared distance from cursor to each screen corner (clamped) */
  int gap = 20;
  int corners[4][2] = {{gap, gap}, {sw - INFO_W - gap, gap}, {gap, sh - INFO_H - gap}, {sw - INFO_W - gap, sh - INFO_H - gap}};
  int best = 0, best_dist = -1;
  for (int i = 0; i < 4; i++) {
    int dx = cx - corners[i][0];
    int dy = cy - corners[i][1];
    int d2 = dx * dx + dy * dy;
    if (d2 > best_dist) { best_dist = d2; best = i; }
  }

  info_x = corners[best][0];
  info_y = corners[best][1];
  XMoveWindow(dpy, info_win, info_x, info_y);
  info_window_redraw();        /* NEW: always repaint at the new spot */
}

/* ---- Mouse operations ---- */

void get_pointer(void) {
  int x, y, di;
  unsigned int dui;
  Window dummy;
  XQueryPointer(dpy, root, &dummy, &dummy, &x, &y, &di, &di, &dui);
  mouseinfo.x = x;
  mouseinfo.y = y;
}

void move_relative(float x, float y) {
  mouseinfo.x += x;
  mouseinfo.y += y;
  XWarpPointer(dpy, None, root, 0, 0, 0, 0, (int)mouseinfo.x, (int)mouseinfo.y);
  XFlush(dpy);
}

void click(unsigned int button, Bool is_press) {
  XTestFakeButtonEvent(dpy, button, is_press, CurrentTime);
  XFlush(dpy);
}

void click_full(unsigned int button) {
  XTestFakeButtonEvent(dpy, button, 1, CurrentTime);
  XTestFakeButtonEvent(dpy, button, 0, CurrentTime);
  XFlush(dpy);
}

void scroll(float x, float y) {
  scrollinfo.x += x;
  scrollinfo.y += y;
  while (scrollinfo.y <= -0.51) { scrollinfo.y += 1; click_full(4); }
  while (scrollinfo.y >= 0.51) { scrollinfo.y -= 1; click_full(5); }
  while (scrollinfo.x <= -0.51) { scrollinfo.x += 1; click_full(6); }
  while (scrollinfo.x >= 0.51) { scrollinfo.x -= 1; click_full(7); }
}

/* ---- X11 init / cleanup ---- */

void init_x(void) {
  int i, screen;
  XInitThreads();
  dpy = XOpenDisplay((char *) 0);
  if (!dpy) { fprintf(stderr, "xmouseless: cannot open display\n"); exit(EXIT_FAILURE); }
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  XAutoRepeatOff(dpy);
  for (i = 0; i < 100; i++) {
    if (XGrabKeyboard(dpy, root, False, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess) return;
    usleep(10000);
  }
  printf("grab keyboard failed\n");
  close_x(EXIT_FAILURE);
}

void close_x(int exit_status) {
  if (capture_pipe[1] > 2) { dup2(capture_pipe[1], STDOUT_FILENO); dup2(capture_pipe[1], STDERR_FILENO); }
  grid_overlay_destroy();
  info_window_destroy();
  XAutoRepeatOn(dpy);
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  XCloseDisplay(dpy);
  exit(exit_status);
}

/* ---- Background movement thread ---- */

void *move_forever(void *val) {
  (void)val;
  while (1) {
    if (mouseinfo.speed_x != 0 || mouseinfo.speed_y != 0) {
      move_relative((float)mouseinfo.speed_x * speed / move_rate,
          (float)mouseinfo.speed_y * speed / move_rate);
    }
    if (scrollinfo.speed_x != 0 || scrollinfo.speed_y != 0) {
      scroll((float)scrollinfo.speed_x / move_rate, (float)scrollinfo.speed_y / move_rate);
    }
    usleep(1000000 / move_rate);
  }
}

/* ---- Key handler ---- */

void handle_key(KeyCode keycode, Bool is_press) {
  unsigned int i;
  KeySym keysym = XkbKeycodeToKeysym(dpy, keycode, 0, 0);

  for (i = 0; i < LENGTH(move_bindings); i++) {
    if (move_bindings[i].keysym == keysym) {
      int sign = is_press ? 1 : -1;
      mouseinfo.speed_x += sign * move_bindings[i].x;
      mouseinfo.speed_y += sign * move_bindings[i].y;
    }
  }

  for (i = 0; i < LENGTH(click_bindings); i++) {
    if (click_bindings[i].keysym == keysym) {
      click(click_bindings[i].button, is_press);
      printf("click: %i %i\n", click_bindings[i].button, is_press);
    }
  }

  for (i = 0; i < LENGTH(speed_bindings); i++) {
    if (speed_bindings[i].keysym == keysym) {
      speed = is_press ? speed_bindings[i].speed : default_speed;
      printf("speed: %i\n", speed);
    }
  }

  for (i = 0; i < LENGTH(scroll_bindings); i++) {
    if (scroll_bindings[i].keysym == keysym) {
      int sign = is_press ? 1 : -1;
      scrollinfo.speed_x += sign * scroll_bindings[i].x;
      scrollinfo.speed_y += sign * scroll_bindings[i].y;
    }
  }

  if (!is_press) {
    for (i = 0; i < LENGTH(shell_bindings); i++) {
      if (shell_bindings[i].keysym == keysym) {
        printf("executing: %s\n", shell_bindings[i].command);
        if (fork() == 0) { system(shell_bindings[i].command); exit(EXIT_SUCCESS); }
      }
    }
    for (i = 0; i < LENGTH(exit_keys); i++) {
      if (exit_keys[i] == keysym) close_x(EXIT_SUCCESS);
    }
  }
}

/* ---- Main ---- */

int main(void) {
  XInitThreads();          /* must be the first Xlib call in the program */
  char keys_return[32];
  int rc, i, j;

  init_capture();
  init_x();
  info_window_create();
  grid_overlay_create();

  get_pointer();
  mouseinfo.speed_x = 0;
  mouseinfo.speed_y = 0;
  speed = default_speed;
  scrollinfo.x = 0;
  scrollinfo.y = 0;
  scrollinfo.speed_x = 0;
  scrollinfo.speed_y = 0;

  rc = pthread_create(&movethread, NULL, &move_forever, NULL);
  if (rc != 0) { printf("Unable to start thread.\n"); return EXIT_FAILURE; }

  XQueryKeymap(dpy, keys_return);
  for (i = 0; i < 32; i++) {
    for (j = 0; j < 8; j++) {
      if (keys_return[i] & (1<<j)) handle_key(8 * i + j, 1);
    }
  }

  info_window_redraw();

  while(1) {
    fd_set readfds;
    FD_ZERO(&readfds);
    int maxfd = ConnectionNumber(dpy);
    FD_SET(maxfd, &readfds);
    if (capture_pipe[0] >= 0 && capture_pipe[0] < FD_SETSIZE) {
      FD_SET(capture_pipe[0], &readfds);
      if (capture_pipe[0] > maxfd) maxfd = capture_pipe[0];
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 5000;
    select(maxfd + 1, &readfds, NULL, NULL, &tv);

    if (capture_pipe[0] >= 0 && FD_ISSET(capture_pipe[0], &readfds)) {
      size_t before = outlen;
      info_window_read_pipe();
      if (outlen > before) info_window_redraw();
    }

    while (XPending(dpy)) {
      XEvent event;
      XNextEvent(dpy, &event);
      switch (event.type) {
        case KeyPress:
        case KeyRelease:
          get_pointer();
          handle_key(event.xkey.keycode, event.xkey.type == KeyPress);
          break;
        case MapNotify:
          info_window_redraw();
          break;
        case Expose:
          if (event.xexpose.count > 0)
            break;   /* more exposes coming — wait for the last */
          if (event.xexpose.window == grid_win)
            grid_overlay_redraw();
          else if (event.xexpose.window == info_win)
            info_window_redraw();
          break;
      }

    }

    info_window_avoid_cursor();
  }
}
