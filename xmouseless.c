/*
 * xmouseless - keyboard-driven mouse replacement for X11
 * Compile: gcc xmouseless.c -o xmouseless -lX11 -lXext -lXtst -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/select.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/shape.h>

#define LENGTH(X)   (sizeof X / sizeof X[0])

/* help box */
#define INFO_W          880       /* minimum width  (grows to fit the font) */
#define INFO_H          400       /* minimum height (grows to fit the font) */
#define INFO_PAD        24        /* left/right text margin */
#define INFO_TOP        16        /* top/bottom text margin */
#define INFO_OUT_LINES  3         /* captured output lines shown at the bottom */
#define INFO_BG         0xCCFFCC
#define INFO_BORDER     0x004400
#define INFO_TEXT       0x222222
#define INFO_ROW_GAP    8         /* clearance below a grid row's bottom edge */

/* grid overlay */
#define GRID_BOX_W  200
#define GRID_BOX_H  150
#define GRID_FG     0x004400
#define GRID_BG     0xCCFFCC
#define GRID_TEXT   0x222222

#define GRAB_TRIES             300   /* x 10 ms = up to 3 s */
#define RAISE_MIN_INTERVAL_MS  50    /* at most 20 re-raises per second */

typedef struct { KeySym keysym; float x; float y; } MoveBinding;
typedef struct { KeySym keysym; unsigned int button; } ClickBinding;
typedef struct { KeySym keysym; float x; float y; } ScrollBinding;
typedef struct { KeySym keysym; unsigned int speed; } SpeedBinding;
typedef struct { KeySym keysym; char *command; } ShellBinding;

#include "config.h"

/* ---------- prototypes ---------- */
void get_pointer(void);
void move_relative(float x, float y);
void click(unsigned int button, Bool is_press);
void click_full(unsigned int button);
void scroll(float x, float y);
void handle_key(KeyCode keycode, Bool is_press);
void *move_forever(void *val);
void init_x(void);
void close_x(int exit_status);
void init_capture(void);
void info_window_read_pipe(void);
void info_window_create(void);
void info_window_redraw(void);
void info_window_avoid_cursor(void);
void info_window_set_alt(void);
void info_window_set_default(void);
void info_window_destroy(void);
void grid_overlay_create(void);
void grid_overlay_redraw(void);
void grid_overlay_destroy(void);

/* ---------- globals ---------- */
Display *dpy;
Window root;
pthread_t movethread;
static int movethread_started = 0;
static volatile sig_atomic_t running = 1;
static unsigned int speed;

struct { float x, y, speed_x, speed_y; } mouseinfo;
struct { float x, y, speed_x, speed_y; } scrollinfo;

/* info window */
Window       info_win = None;
GC           info_gc = NULL;
XFontStruct *info_font = NULL;
int          capture_pipe[2] = { -1, -1 };
static int   saved_stdout = -1, saved_stderr = -1;
char         outbuf[65536];
size_t       outlen;
static int   info_x, info_y;
static int   info_w = INFO_W, info_h = INFO_H;
static int   info_def_x = 20, info_def_y = 0;

/* grid overlay */
static Window       grid_win = None;
static GC           grid_gc = NULL;
static XFontStruct *grid_font = NULL;

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

static const char *grid_keys[3][9] = {
  { "q", "w", "f", "p", "g", "j", "l", "u", "y" },
  { "a", "r", "s", "t", "d", "h", "n", "e", "i" },
  { "z", "x", "c", "v", "b", "k", "m", ",", "." },
};

/* ---------- small helpers ---------- */

/* always writes to the real terminal, even after stdout is captured */
static void term_log(const char *fmt, ...) {
  int fd = saved_stderr >= 0 ? saved_stderr : STDERR_FILENO;
  va_list ap;
  va_start(ap, fmt);
  vdprintf(fd, fmt, ap);
  va_end(ap);
}

static long now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static XFontStruct *load_first_font(const char *const names[]) {
  for (int i = 0; names[i]; i++) {
    XFontStruct *f = XLoadQueryFont(dpy, names[i]);
    if (f) return f;
  }
  return NULL;
}

static int text_width(const char *text) {
  if (!info_font || !text) return 0;
  return XTextWidth(info_font, text, (int)strlen(text));
}

static void draw_line(const char *text, int x, int y) {
  if (!info_font || !text) return;
  XDrawString(dpy, info_win, info_gc, x, y, text, (int)strlen(text));
}

/* adds the 1-px outline that XDrawRectangle(x, y, w, h) would draw */
static void region_add_outline(Region r, int x, int y, int w, int h) {
  XRectangle e[4] = {
    { (short)x,       (short)y,       (unsigned short)(w + 1), 1 },
    { (short)x,       (short)(y + h), (unsigned short)(w + 1), 1 },
    { (short)x,       (short)y,       1, (unsigned short)(h + 1) },
    { (short)(x + w), (short)y,       1, (unsigned short)(h + 1) },
  };
  for (int i = 0; i < 4; i++) XUnionRectWithRegion(&e[i], r, r);
}

/* raises grid, then help box on top; returns 1 if it actually raised */
static int raise_overlays(void) {
  static long last = 0;
  long t = now_ms();
  if (t - last < RAISE_MIN_INTERVAL_MS) return 0;
  last = t;
  if (grid_win != None) XRaiseWindow(dpy, grid_win);
  if (info_win != None) XRaiseWindow(dpy, info_win);
  XFlush(dpy);
  return 1;
}

/* ---------- pointer / buttons ---------- */

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
  while (scrollinfo.y >=  0.51) { scrollinfo.y -= 1; click_full(5); }
  while (scrollinfo.x <= -0.51) { scrollinfo.x += 1; click_full(6); }
  while (scrollinfo.x >=  0.51) { scrollinfo.x -= 1; click_full(7); }
}

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
      printf("click: %u %i\n", click_bindings[i].button, is_press);
    }
  }

  for (i = 0; i < LENGTH(speed_bindings); i++) {
    if (speed_bindings[i].keysym == keysym) {
      speed = is_press ? speed_bindings[i].speed : default_speed;
      printf("speed: %u\n", speed);
    }
  }

  for (i = 0; i < LENGTH(scroll_bindings); i++) {
    if (scroll_bindings[i].keysym == keysym) {
      int sign = is_press ? 1 : -1;
      scrollinfo.speed_x += sign * scroll_bindings[i].x;
      scrollinfo.speed_y += sign * scroll_bindings[i].y;

      /* scroll once, workaround for scrolling not working the first time */
      int scroll_x = 0, scroll_y = 0;
      if (scrollinfo.speed_x < 0) scroll_x = -1;
      if (scrollinfo.speed_x > 0) scroll_x = 1;
      if (scrollinfo.speed_y < 0) scroll_y = -1;
      if (scrollinfo.speed_y > 0) scroll_y = 1;
      scroll(scroll_x, scroll_y);
    }
  }

  /* help box repositioning: left-side grid keys push it aside, right-side bring it back */
  if (is_press) {
    for (i = 0; i < LENGTH(info_alt_keys); i++)
      if (info_alt_keys[i] == keysym) info_window_set_alt();
    for (i = 0; i < LENGTH(info_default_keys); i++)
      if (info_default_keys[i] == keysym) info_window_set_default();
  }

  if (!is_press) {
    for (i = 0; i < LENGTH(shell_bindings); i++) {
      if (shell_bindings[i].keysym == keysym) {
        printf("executing: %s\n", shell_bindings[i].command);
        fflush(stdout);                       /* nothing left for the child to duplicate */
        pid_t pid = fork();
        if (pid == 0) {
          signal(SIGCHLD, SIG_DFL);           /* sh must be able to wait() for xdotool */
          execl("/bin/sh", "sh", "-c", shell_bindings[i].command, (char *)NULL);
          _exit(127);                         /* never flush the parent's buffers */
        } else if (pid < 0) {
          perror("xmouseless: fork");
        }
      }
    }
    for (i = 0; i < LENGTH(exit_keys); i++) {
      if (exit_keys[i] == keysym) close_x(EXIT_SUCCESS);
    }
  }
}

void *move_forever(void *val) {
  (void)val;
  while (running) {
    if (mouseinfo.speed_x != 0 || mouseinfo.speed_y != 0) {
      move_relative((float)mouseinfo.speed_x * speed / move_rate,
                    (float)mouseinfo.speed_y * speed / move_rate);
    }
    if (scrollinfo.speed_x != 0 || scrollinfo.speed_y != 0) {
      scroll((float)scrollinfo.speed_x / move_rate,
             (float)scrollinfo.speed_y / move_rate);
    }
    usleep(1000000 / move_rate);
  }
  return NULL;
}

/* ---------- X setup / teardown ---------- */

void init_x(void) {
  int i, screen;

  dpy = XOpenDisplay(NULL);
  if (!dpy) { fprintf(stderr, "xmouseless: cannot open display\n"); exit(EXIT_FAILURE); }
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);

  /* don't leak the X connection into child processes (xdotool etc.) */
  fcntl(ConnectionNumber(dpy), F_SETFD, FD_CLOEXEC);

  for (i = 0; i < GRAB_TRIES; i++) {
    if (XGrabKeyboard(dpy, root, False, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess) {
      XAutoRepeatOff(dpy);               /* only after we really own the keyboard */
      return;
    }
    usleep(10000);                       /* 10 ms */
  }

  /* stdout is NOT captured yet, so this reaches the terminal */
  fprintf(stderr, "xmouseless: grab keyboard failed (another program holds the keyboard)\n");
  XCloseDisplay(dpy);
  exit(EXIT_FAILURE);
}

void close_x(int exit_status) {
  /* stop the move thread before tearing down the display */
  running = 0;
  if (movethread_started) pthread_join(movethread, NULL);

  fflush(stdout);
  fflush(stderr);
  if (saved_stdout >= 0) dup2(saved_stdout, STDOUT_FILENO);
  if (saved_stderr >= 0) dup2(saved_stderr, STDERR_FILENO);

  grid_overlay_destroy();
  info_window_destroy();
  XAutoRepeatOn(dpy);
  XUngrabKeyboard(dpy, CurrentTime);
  XCloseDisplay(dpy);
  exit(exit_status);
}

/* ---------- stdout capture ---------- */

void init_capture(void) {
  if (pipe(capture_pipe)) { capture_pipe[0] = capture_pipe[1] = -1; return; }

  /* keep the real terminal so errors and the final exit still reach it */
  saved_stdout = dup(STDOUT_FILENO);
  saved_stderr = dup(STDERR_FILENO);
  if (saved_stdout >= 0) fcntl(saved_stdout, F_SETFD, FD_CLOEXEC);
  if (saved_stderr >= 0) fcntl(saved_stderr, F_SETFD, FD_CLOEXEC);
  fcntl(capture_pipe[0], F_SETFD, FD_CLOEXEC);
  fcntl(capture_pipe[1], F_SETFD, FD_CLOEXEC);  /* children still get it as fd 1/2 */

  dup2(capture_pipe[1], STDOUT_FILENO);
  dup2(capture_pipe[1], STDERR_FILENO);
  setvbuf(stdout, NULL, _IOLBF, 0);             /* line-buffered: each line shows up at once */

  int flags = fcntl(capture_pipe[0], F_GETFL, 0);
  fcntl(capture_pipe[0], F_SETFL, flags | O_NONBLOCK);
}

void info_window_read_pipe(void) {
  for (;;) {                                    /* drain everything available */
    if (outlen >= sizeof(outbuf) - 1) {
      memmove(outbuf, outbuf + 8192, outlen - 8192);
      outlen -= 8192;
    }
    ssize_t n = read(capture_pipe[0], outbuf + outlen, sizeof(outbuf) - 1 - outlen);
    if (n <= 0) break;
    outlen += (size_t)n;
  }
  outbuf[outlen] = '\0';
}

/* ---------- help box ---------- */

void info_window_create(void) {
  static const char *const fonts[] = {
    "-misc-fixed-medium-r-normal--44-*-*-*-*-*-iso10646-1",
    "-misc-fixed-medium-r-normal--36-300-75-75-C-200-iso10646-1",
    "-misc-fixed-medium-r-normal--24-*-*-*-*-*-iso10646-1",
    "-misc-fixed-medium-r-normal--20-*-*-*-*-*-iso10646-1",
    "-misc-fixed-medium-r-normal--14-130-75-75-C-80-iso10646-1",
    "fixed",                                    /* exists on practically every X server */
    NULL
  };
  int sw = DisplayWidth(dpy, DefaultScreen(dpy));
  int sh = DisplayHeight(dpy, DefaultScreen(dpy));

  info_font = load_first_font(fonts);
  if (!info_font) term_log("xmouseless: no usable font for the help box\n");

  /* size the box from the font that actually loaded */
  if (info_font) {
    int lh = info_font->ascent + info_font->descent + 4;
    int nhelp = 0, widest = 0;
    for (; help_lines[nhelp]; nhelp++) {
      int w = text_width(help_lines[nhelp]);
      if (w > widest) widest = w;
    }
    info_w = widest + 2 * INFO_PAD;
    info_h = 2 * INFO_TOP + (nhelp + INFO_OUT_LINES) * lh;
    if (info_w < INFO_W) info_w = INFO_W;
    if (info_h < INFO_H) info_h = INFO_H;
  }
  if (info_w > sw - 40) info_w = sw - 40;
  if (info_h > sh - 40) info_h = sh - 40;

  /* default spot: left side, top edge just below the middle grid row ("a") */
  info_def_x = 20;
  info_def_y = sh / 2 + GRID_BOX_H / 2 + INFO_ROW_GAP;
  info_x = info_def_x;
  info_y = info_def_y;

  XSetWindowAttributes wa;
  wa.override_redirect = True;
  wa.background_pixel = INFO_BG;
  wa.border_pixel = INFO_BORDER;
  wa.event_mask = ExposureMask | StructureNotifyMask | VisibilityChangeMask;

  info_win = XCreateWindow(dpy, root, info_x, info_y, info_w, info_h, 3,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &wa);
  XStoreName(dpy, info_win, "xmouseless-info");

  /* hear about other windows being mapped / restacked so we can stay on top */
  XSelectInput(dpy, root, SubstructureNotifyMask);

  info_gc = XCreateGC(dpy, info_win, 0, NULL);
  XSetForeground(dpy, info_gc, INFO_TEXT);
  XSetBackground(dpy, info_gc, INFO_BG);
  if (info_font) XSetFont(dpy, info_gc, info_font->fid);

  XMapRaised(dpy, info_win);                    /* created after the grid => already on top */
  info_window_redraw();
}

void info_window_redraw(void) {
  if (info_win == None) return;
  XClearWindow(dpy, info_win);
  if (!info_font) { XFlush(dpy); return; }

  int lh = info_font->ascent + info_font->descent + 4;
  int y = INFO_TOP + info_font->ascent;         /* first baseline: letters no longer clipped */
  const int maxw = info_w - INFO_PAD * 2;

  for (int i = 0; help_lines[i]; i++) {
    draw_line(help_lines[i], INFO_PAD, y);
    y += lh;
  }

  if (outlen > 0) {
    /* find the start of the last INFO_OUT_LINES lines (ignore a trailing newline) */
    ssize_t k = (ssize_t)outlen - 1;
    if (outbuf[k] == '\n') k--;
    int nl_seen = 0;
    size_t offset = 0;
    for (; k >= 0; k--) {
      if (outbuf[k] == '\n' && ++nl_seen == INFO_OUT_LINES) { offset = (size_t)k + 1; break; }
    }

    const char *p = outbuf + offset;
    size_t remaining = outlen - offset;
    char line[1024];

    for (int n = 0; n < INFO_OUT_LINES && remaining > 0; n++) {
      const char *nl = memchr(p, '\n', remaining);
      size_t len = nl ? (size_t)(nl - p) : remaining;
      if (len > sizeof(line) - 1) len = sizeof(line) - 1;
      memcpy(line, p, len);
      line[len] = '\0';

      while (len > 1 && text_width(line) > maxw) line[--len] = '\0';   /* truncate to fit */

      draw_line(line, INFO_PAD, y);
      y += lh;

      if (!nl) break;
      remaining -= (size_t)(nl - p) + 1;
      p = nl + 1;
    }
  }
  XFlush(dpy);
}

void info_window_avoid_cursor(void) {
  if (info_win == None) return;

  int cx, cy, di;
  unsigned int dui;
  Window dummy;
  if (!XQueryPointer(dpy, root, &dummy, &dummy, &cx, &cy, &di, &di, &dui)) return;

  int margin = 80;

  /* only react when the cursor is near the box */
  if (!(cx >= info_x - margin && cx <= info_x + info_w + margin &&
        cy >= info_y - margin && cy <= info_y + info_h + margin)) return;

  info_window_set_alt();   /* no-op if already at the alternative spot */
}

void info_window_set_alt(void) {
  if (info_win == None) return;

  int sw = DisplayWidth(dpy, DefaultScreen(dpy));
  int sh = DisplayHeight(dpy, DefaultScreen(dpy));

  /* alternative spot: right side, top edge just below the top grid row ("y") */
  int alt_x = sw - info_w - 20;
  int alt_y = sh / 6 + GRID_BOX_H / 2 + INFO_ROW_GAP;
  if (alt_x == info_x && alt_y == info_y) return;   /* already at the alternative spot */

  info_x = alt_x;
  info_y = alt_y;
  XMoveWindow(dpy, info_win, info_x, info_y);
  info_window_redraw();
}

void info_window_set_default(void) {
  if (info_win == None) return;
  if (info_def_x == info_x && info_def_y == info_y) return;   /* already at the default spot */

  info_x = info_def_x;
  info_y = info_def_y;
  XMoveWindow(dpy, info_win, info_x, info_y);
  info_window_redraw();
}

void info_window_destroy(void) {
  if (info_win != None) { XUnmapWindow(dpy, info_win); XDestroyWindow(dpy, info_win); info_win = None; }
  if (info_gc)   { XFreeGC(dpy, info_gc); info_gc = NULL; }
  if (info_font) { XFreeFont(dpy, info_font); info_font = NULL; }
}

/* ---------- grid overlay ---------- */

/* label background rect + text origin for one grid cell */
static void grid_label_rect(int row, int col, int sw, int sh, XRectangle *r, int *tx, int *ty) {
  const char *label = grid_keys[row][col];
  int cx = sw * (2 * col + 1) / 18;             /* same center formula as GRID() in config.h */
  int cy = sh * (2 * row + 1) / 6;
  int tw = XTextWidth(grid_font, label, (int)strlen(label));
  *tx = cx - tw / 2;
  *ty = cy + grid_font->ascent / 2;
  r->x      = (short)(*tx - 6);
  r->y      = (short)(*ty - grid_font->ascent - 3);
  r->width  = (unsigned short)(tw + 12);
  r->height = (unsigned short)(grid_font->ascent + grid_font->descent + 6);
}

void grid_overlay_create(void) {
  static const char *const fonts[] = {
    "-misc-fixed-medium-r-normal--64-*-*-*-*-*-iso10646-1",
    "-misc-fixed-medium-r-normal--36-300-75-75-C-200-iso10646-1",
    "-misc-fixed-medium-r-normal--24-*-*-*-*-*-iso10646-1",
    "fixed",
    NULL
  };
  int ev_base, err_base;
  int sw = DisplayWidth(dpy, DefaultScreen(dpy));
  int sh = DisplayHeight(dpy, DefaultScreen(dpy));

  if (!XShapeQueryExtension(dpy, &ev_base, &err_base)) {
    term_log("xmouseless: X Shape extension missing, grid overlay disabled\n");
    return;
  }

  grid_font = load_first_font(fonts);
  if (!grid_font) {
    term_log("xmouseless: no usable font for the grid, overlay disabled\n");
    return;
  }

  XSetWindowAttributes wa;
  wa.override_redirect = True;
  wa.background_pixel = GRID_FG;                /* real background: no more ghosting */
  wa.border_pixel = 0;
  wa.event_mask = ExposureMask;

  grid_win = XCreateWindow(dpy, root, 0, 0, sw, sh, 0,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &wa);
  XStoreName(dpy, grid_win, "xmouseless-grid");

  /* visible shape = only the box outlines and labels; everything else is truly see-through */
  Region vis = XCreateRegion();
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 9; col++) {
      int cx = sw * (2 * col + 1) / 18;
      int cy = sh * (2 * row + 1) / 6;
      int x = cx - GRID_BOX_W / 2;
      int y = cy - GRID_BOX_H / 2;
      XRectangle lr;
      int tx, ty;

      region_add_outline(vis, x, y, GRID_BOX_W, GRID_BOX_H);
      region_add_outline(vis, x + 3, y + 3, GRID_BOX_W - 6, GRID_BOX_H - 6);
      grid_label_rect(row, col, sw, sh, &lr, &tx, &ty);
      XUnionRectWithRegion(&lr, vis, vis);
    }
  }
  XShapeCombineRegion(dpy, grid_win, ShapeBounding, 0, 0, vis, ShapeSet);
  XDestroyRegion(vis);

  /* empty input region: pointer events pass through */
  XShapeCombineRectangles(dpy, grid_win, ShapeInput, 0, 0, NULL, 0, ShapeSet, 0);

  grid_gc = XCreateGC(dpy, grid_win, 0, NULL);
  XSetFont(dpy, grid_gc, grid_font->fid);

  XMapWindow(dpy, grid_win);
  grid_overlay_redraw();
}

void grid_overlay_redraw(void) {
  if (grid_win == None || !grid_font) return;
  int sw = DisplayWidth(dpy, DefaultScreen(dpy));
  int sh = DisplayHeight(dpy, DefaultScreen(dpy));

  /* clipped to the shape, so this paints only the outlines */
  XSetForeground(dpy, grid_gc, GRID_FG);
  XFillRectangle(dpy, grid_win, grid_gc, 0, 0, sw, sh);

  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 9; col++) {
      const char *label = grid_keys[row][col];
      XRectangle lr;
      int tx, ty;
      grid_label_rect(row, col, sw, sh, &lr, &tx, &ty);

      XSetForeground(dpy, grid_gc, GRID_BG);
      XFillRectangle(dpy, grid_win, grid_gc, lr.x, lr.y, lr.width, lr.height);
      XSetForeground(dpy, grid_gc, GRID_TEXT);
      XDrawString(dpy, grid_win, grid_gc, tx, ty, label, (int)strlen(label));
    }
  }
  XFlush(dpy);
}

void grid_overlay_destroy(void) {
  if (grid_win != None) { XDestroyWindow(dpy, grid_win); grid_win = None; }
  if (grid_gc)   { XFreeGC(dpy, grid_gc); grid_gc = NULL; }
  if (grid_font) { XFreeFont(dpy, grid_font); grid_font = NULL; }
  XFlush(dpy);
}

/* ---------- main ---------- */

int main(void) {
  char keys_return[32];
  int rc, i, j;
  int need_raise = 0;

  XInitThreads();                 /* must be the first Xlib call in the program */
  signal(SIGCHLD, SIG_IGN);       /* auto-reap finished shell commands (no zombies) */

  init_x();                       /* grab first: failures are still visible in the terminal */
  init_capture();                 /* from here on stdout/stderr go into the help box */
  grid_overlay_create();          /* grid first ...                                    */
  info_window_create();           /* ... help box last, so it starts above the grid     */

  get_pointer();
  mouseinfo.speed_x = 0;
  mouseinfo.speed_y = 0;
  speed = default_speed;
  scrollinfo.x = 0;
  scrollinfo.y = 0;
  scrollinfo.speed_x = 0;
  scrollinfo.speed_y = 0;

  rc = pthread_create(&movethread, NULL, &move_forever, NULL);
  if (rc != 0) {
    term_log("xmouseless: unable to start thread\n");
    close_x(EXIT_FAILURE);
  }
  movethread_started = 1;

  XQueryKeymap(dpy, keys_return);
  for (i = 0; i < 32; i++) {
    for (j = 0; j < 8; j++) {
      if (keys_return[i] & (1 << j)) handle_key(8 * i + j, 1);
    }
  }

  info_window_redraw();

  while (1) {
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
      if (outlen != before) info_window_redraw();
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
          if (event.xmap.window == info_win)
            info_window_redraw();
          else if (event.xmap.window != grid_win)
            need_raise = 1;               /* a new window appeared above us */
          break;

        case ConfigureNotify:
          /* another window moved/restacked; ignore our own to avoid loops */
          if (event.xconfigure.window != info_win &&
              event.xconfigure.window != grid_win)
            need_raise = 1;
          break;

        case VisibilityNotify:
          if (event.xvisibility.window == info_win &&
              event.xvisibility.state != VisibilityUnobscured)
            need_raise = 1;               /* something is covering the help box */
          break;

        case Expose:
          if (event.xexpose.count > 0)
            break;                        /* more exposes coming, wait for the last */
          if (event.xexpose.window == grid_win)
            grid_overlay_redraw();
          else if (event.xexpose.window == info_win)
            info_window_redraw();
          break;
      }
    }

    /* rate-limited; if skipped now it is retried on the next loop (≤ 5 ms) */
    if (need_raise && raise_overlays()) need_raise = 0;

    info_window_avoid_cursor();
  }
}