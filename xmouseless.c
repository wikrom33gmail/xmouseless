/*
 * xmouseless - keyboard-driven mouse replacement for X11
 * Compile: gcc xmouseless.c -o xmouseless -lX11 -lXext -lXtst
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/shape.h>

#define LENGTH(X)   (sizeof X / sizeof X[0])

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
void move_tick(void);

void init_x(void);
void close_x(int exit_status);
void grid_overlay_create(void);
void grid_overlay_redraw(void);
void grid_overlay_destroy(void);

/* ---------- globals ---------- */
Display *dpy;
Window root;
static unsigned int speed;

struct { float x, y, speed_x, speed_y; } mouseinfo;
struct { float x, y, speed_x, speed_y; } scrollinfo;

/* grid overlay */
static Window       grid_win = None;
static GC           grid_gc = NULL;
static XFontStruct *grid_font = NULL;

static const char *grid_keys[3][9] = {
  { "q", "w", "f", "p", "g", "j", "l", "u", "y" },
  { "a", "r", "s", "t", "d", "h", "n", "e", "i" },
  { "z", "x", "c", "v", "b", "k", "m", ",", "." },
};

/* ---------- small helpers ---------- */

/* logs a message straight to the terminal */
static void term_log(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

static long now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* ---------- font metrics cache ---------- */
/* This X server takes ~12-50 ms to return a font's metrics via any API, and
 * that dominates startup. We record each resolved font's ascent/descent/char-width
 * once; later runs skip the slow query entirely and only call the free XLoadFont. */

static const char *font_cache_path(void) {
  static char buf[4096];
  const char *dir = getenv("XDG_CACHE_HOME");
  if (!dir || !*dir) dir = getenv("HOME");
  if (!dir || !*dir) return NULL;   /* no sane place to cache */
  snprintf(buf, sizeof buf, "%s/xmouseless/fonts", dir);
  return buf;
}

static void ensure_cache_dir(void) {
  const char *path = font_cache_path();
  if (!path) return;
  /* mkdir each component up to the file's directory, ignoring EEXIST */
  char tmp[4096];
  snprintf(tmp, sizeof tmp, "%s", path);
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
  }
}

/* returns 1 and fills metrics on a cache hit for `name` (last entry wins) */
static int cache_lookup(const char *name, int *ascent, int *descent, int *width) {
  const char *path = font_cache_path();
  if (!path) return 0;
  FILE *fp = fopen(path, "r");
  if (!fp) return 0;
  size_t nlen = strlen(name);
  char line[512];
  int found = 0;
  while (fgets(line, sizeof line, fp)) {
    /* format: <name>\t<ascent>\t<descent>\t<width> */
    if (strncmp(line, name, nlen) == 0 && line[nlen] == '\t') {
      char *p = line + nlen + 1, *end;
      int a = (int)strtol(p, &end, 10); p = end; while (*p == '\t') p++;
      int d = (int)strtol(p, &end, 10); p = end; while (*p == '\t') p++;
      int w = (int)strtol(p, NULL, 10);
      *ascent = a; *descent = d; *width = w;
      found = 1;
    }
  }
  fclose(fp);
  return found;
}

static void cache_store(const char *name, int ascent, int descent, int width) {
  const char *path = font_cache_path();
  if (!path) return;
  ensure_cache_dir();

  /* keep existing entries, dropping any stale line for this name */
  static char lines[64][512];
  int count = 0;
  size_t nlen = strlen(name);
  FILE *fp = fopen(path, "r");
  if (fp) {
    char line[512];
    while (count < 64 && fgets(line, sizeof line, fp)) {
      int dup = strncmp(line, name, nlen) == 0 && line[nlen] == '\t';
      if (!dup) snprintf(lines[count++], 512, "%s", line);
    }
    fclose(fp);
  }

  char entry[512];
  snprintf(entry, sizeof entry, "%s\t%d\t%d\t%d\n", name, ascent, descent, width);
  if (count < 64) snprintf(lines[count++], 512, "%s", entry);

  /* atomic write: temp file + rename; best-effort, never break startup */
  char tmp[4096];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE *out = fopen(tmp, "w");
  if (!out) return;
  for (int i = 0; i < count; i++) fputs(lines[i], out);
  fclose(out);
  rename(tmp, path);
}

/* uniform teardown: our structs are calloc'd and hold a live font ID */
static void free_font(XFontStruct *f) {
  if (!f) return;
  XUnloadFont(dpy, f->fid);
  free(f->per_char);   /* NULL-safe */
  free(f);
}

static XFontStruct *load_first_font(const char *const names[]) {
  for (int i = 0; names[i]; i++) {
    const char *name = names[i];
    int ascent, descent, width;

    if (!cache_lookup(name, &ascent, &descent, &width)) {
      /* first run: resolve the pattern to an exact name, fetch metrics once */
      int n = 0;
      char **list = XListFonts(dpy, name, 1, &n);
      if (list && n > 0) {
        XFontStruct *q = XLoadQueryFont(dpy, list[0]);
        if (!q) { XFreeFontNames(list); continue; }
        ascent = q->ascent; descent = q->descent; width = q->max_bounds.width;
        cache_store(name, ascent, descent, width);
        XFreeFont(dpy, q);
        XFreeFontNames(list);
      } else {
        if (list) XFreeFontNames(list);
        continue;   /* no font matches this pattern */
      }
    }

    /* uniform: load the font ID (free) and build our own struct from metrics */
    Font fid = XLoadFont(dpy, name);
    if (!fid) continue;   /* font vanished? try the next name */
    XFontStruct *f = calloc(1, sizeof(*f));
    f->fid = fid;
    f->ascent = ascent;
    f->descent = descent;
    f->max_bounds.width = width;
    /* XTextWidth returns 0 unless per_char is set; fixed-pitch => uniform width */
    f->per_char = malloc(256 * sizeof(XCharStruct));
    for (int c = 0; c < 256; c++) f->per_char[c].width = width;
    return f;
  }
  return NULL;
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

/* raises the grid overlay on top; returns 1 if it actually raised */
static int raise_overlays(void) {
  static long last = 0;
  long t = now_ms();
  if (t - last < RAISE_MIN_INTERVAL_MS) return 0;
  last = t;
  if (grid_win != None) XRaiseWindow(dpy, grid_win);
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

/* one movement/scroll step at move_rate Hz; called from the main loop */
void move_tick(void) {
 static long next = 0; /* ms timestamp of the next allowed tick */
 long t = now_ms();
 if (t < next) return;
 next = t + 1000 / move_rate;

 if (mouseinfo.speed_x != 0 || mouseinfo.speed_y != 0) {
   move_relative((float)mouseinfo.speed_x * speed / move_rate,
                 (float)mouseinfo.speed_y * speed / move_rate);
 }
 if (scrollinfo.speed_x != 0 || scrollinfo.speed_y != 0) {
   scroll((float)scrollinfo.speed_x / move_rate,
          (float)scrollinfo.speed_y / move_rate);
 }
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
  grid_overlay_destroy();
  XAutoRepeatOn(dpy);
  XUngrabKeyboard(dpy, CurrentTime);
  XCloseDisplay(dpy);
  exit(exit_status);
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
  /* hear about other windows mapping / restacking so we can stay on top */
  XSelectInput(dpy, root, SubstructureNotifyMask);

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
  if (grid_font) { free_font(grid_font); grid_font = NULL; }
  XFlush(dpy);
}

/* ---------- main ---------- */

int main(void) {
  char keys_return[32];
  int i, j;
  int need_raise = 0;

  signal(SIGCHLD, SIG_IGN);       /* auto-reap finished shell commands (no zombies) */

  init_x();                       /* grab first: failures are still visible in the terminal */
  grid_overlay_create();          /* create the control-grid overlay */

  get_pointer();
  mouseinfo.speed_x = 0;
  mouseinfo.speed_y = 0;
  speed = default_speed;
  scrollinfo.x = 0;
  scrollinfo.y = 0;
  scrollinfo.speed_x = 0;
  scrollinfo.speed_y = 0;

  XQueryKeymap(dpy, keys_return);
  for (i = 0; i < 32; i++) {
    for (j = 0; j < 8; j++) {
      if (keys_return[i] & (1 << j)) handle_key(8 * i + j, 1);
    }
  }

  while (1) {
    fd_set readfds;
    FD_ZERO(&readfds);
    int maxfd = ConnectionNumber(dpy);
    FD_SET(maxfd, &readfds);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 5000;
    select(maxfd + 1, &readfds, NULL, NULL, &tv);

    move_tick(); /* paced internally to move_rate Hz */

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
          if (event.xmap.window != grid_win)
            need_raise = 1;               /* a new window appeared above us */
          break;

        case ConfigureNotify:
          /* another window moved/restacked; ignore our own to avoid loops */
          if (event.xconfigure.window != grid_win)
            need_raise = 1;
          break;

        case Expose:
          if (event.xexpose.count > 0)
            break;                        /* more exposes coming, wait for the last */
          if (event.xexpose.window == grid_win)
            grid_overlay_redraw();
          break;
      }
    }

    /* rate-limited; if skipped now it is retried on the next loop (≤ 5 ms) */
    if (need_raise && raise_overlays()) need_raise = 0;

  }
}