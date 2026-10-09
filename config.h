/* the rate at which the mouse moves in Hz
 * does not change its speed */
static const unsigned int move_rate = 100;
// static const unsigned int move_rate = 50;

/* the default speed of the mouse pointer
 * in pixels per second */
static const unsigned int default_speed = 500;

/* changes the speed of the mouse pointer */
static SpeedBinding speed_bindings[] = {
  /* key             speed */
  // fast = Shift
  { XK_Return,      20 }
  // { XK_a,            100  },
  // { XK_Control_L,    10   },
};

/* moves the mouse pointer
 * you can also add any other direction (e.g. diagonals) */
static MoveBinding move_bindings[] = {
  /* key         x      y */
  { XK_Left,        -1,     0 },
  { XK_Right,         1,     0 },
  { XK_Up,         0,    -1 },
  { XK_Down,         0,     1 },
};

/* 1: left
 * 2: middle
 * 3: right */
static ClickBinding click_bindings[] = {
  /* key         button */
  { XK_8,        1 },
  { XK_9,        2 },
  { XK_0,        3 },
};

/* scrolls up, down, left and right
 * a higher value scrolls faster */
static ScrollBinding scroll_bindings[] = {
  //     /* key        x      y */
      { XK_Prior,        0 ,    25 },
      { XK_Next,         0 ,   -25 },
  //     { XK_plus,     0 ,    80 },
  //     { XK_minus,    0 ,   -80 },
  //     { XK_h,        25,    0  },
  //     { XK_g,       -25,    0  },
};

/* grid jump: works on any screen resolution
 * col = 0..8, row = 0..2; the position is computed from the
 * current screen size at runtime (9 cols x 3 rows) */
#define GRID(col, row) \
  "W=$(xdotool getdisplaygeometry | cut -d' ' -f1); " \
  "H=$(xdotool getdisplaygeometry | cut -d' ' -f2); " \
  "xdotool mousemove $((W*(2*" #col "+1)/18)) $((H*(2*" #row "+1)/6))"

/* executes shell commands */
static ShellBinding shell_bindings[] = {
  /* key         command (9 cols x 3 rows, resolution-independent) */
  { XK_q,        GRID(0, 0) },
  { XK_w,        GRID(1, 0) },
  { XK_f,        GRID(2, 0) },
  { XK_p,        GRID(3, 0) },
  { XK_g,        GRID(4, 0) },
  { XK_j,        GRID(5, 0) },
  { XK_l,        GRID(6, 0) },
  { XK_u,        GRID(7, 0) },
  { XK_y,        GRID(8, 0) },

  /* row 2 */
  { XK_a,        GRID(0, 1) },
  { XK_r,        GRID(1, 1) },
  { XK_s,        GRID(2, 1) },
  { XK_t,        GRID(3, 1) },
  { XK_d,        GRID(4, 1) },
  { XK_h,        GRID(5, 1) },
  { XK_n,        GRID(6, 1) },
  { XK_e,        GRID(7, 1) },
  { XK_i,        GRID(8, 1) },

  /* row 3 */
  { XK_z,        GRID(0, 2) },
  { XK_x,        GRID(1, 2) },
  { XK_c,        GRID(2, 2) },
  { XK_v,        GRID(3, 2) },
  { XK_b,        GRID(4, 2) },
  { XK_k,        GRID(5, 2) },
  { XK_m,        GRID(6, 2) },
  { XK_comma,    GRID(7, 2) },
  { XK_period,   GRID(8, 2) },
};

/* exits on key release which allows click and exit with one key */
static KeySym exit_keys[] = {
  // XK_Escape, XK_q, XK_space
  XK_Escape
};
