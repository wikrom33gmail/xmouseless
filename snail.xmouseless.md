# A Snail-Paced Journey into xmouseless

Welcome, dear learner. You are about to understand something wonderfully complete: a program called **xmouseless** that lets you drive your mouse — move it, click it, scroll with it — using *only the keyboard*. No physical mouse needed at all. We will go slowly. Very slowly. Like a snail exploring a garden for the first time, one leaf at a time.

There is no rush here. Every single word matters, every symbol has a reason, and nothing will be assumed about what you already know. If a term feels strange when it appears, that's because we haven't met it yet — and by the end of this journey, it will feel as ordinary as "chair" or "door".

Let us begin at the very top, with the biggest picture there is.

---

## Step 1: The Big Picture — What Is xmouseless?

**xmouseless is a program that turns your keyboard into a mouse.**

That's the whole big idea in one sentence. Let me unpack every word of it, because each word carries meaning.

First, what does **turns your keyboard into a mouse** mean? A normal mouse is a little device you slide across a desk; moving it slides an arrow on your screen, and pressing its buttons makes the computer "click". xmouseless replaces that physical device with *keys*. Instead of sliding a plastic box, you press keys — and those keys move the arrow, click, and scroll exactly as if a real mouse had done it. Your hands never leave the keyboard.

Second, what does **program** mean? A program is just a list of instructions written in a language computers can read (here, a language called **C**), saved into a file, then *compiled* — translated by a tool called `gcc` — into something your computer runs directly. xmouseless lives in one main file: `xmouseless.c`.

Third, what does the name itself tell us? Break it apart:

| Piece      | Meaning                                                        |
| ---        | ---                                                            |
| `x`        | Short for **X11**, the drawing system most Linux desktops use  |
| `mouse`    | It controls the mouse                                          |
| `less`     | …but with *no physical mouse* — "mouse-less"                   |

So put it together: **xmouseless = a small C program that makes your keyboard act like an X11 mouse.**

**Key idea:** xmouseless = "a keyboard-driven stand-in for the physical mouse."

Here is our concept map for today's journey. Right now it holds just one single branch — everything else will grow out of this as we go:

```
xmouseless ── a program that turns your keyboard into a mouse (on X11)
```

That's all there is to know at this level. You don't need anything more yet. Each next step adds exactly one new idea, and after each one I'll show you where it fits in this map.

---

## Step 2: Why Does xmouseless Exist? (Why Should We Care?)

You might wonder: "My computer already has a mouse — why would anyone build another way to use it?"

There are three gentle reasons, and each one is worth feeling before we move on.

**1. A physical mouse can fail at the worst moment.** Imagine you're working from a laptop over a shaky table, or your mouse battery dies mid-task, or you simply don't have a mouse plugged in. xmouseless means the keyboard alone is always enough to point and click. It's like keeping a flashlight in your desk drawer — you hope never to need it, but you're glad it's there.

**2. Keeping both hands on the keyboard is faster for some people.** Many keyboard-centric users (people who use tiling window managers or text-heavy workflows) find that moving their hands between keyboard and mouse all day adds up to lost time. If your fingers already live on the keys, driving the pointer from those same keys feels natural — like a pianist never reaching off the keys for a pedal.

**3. It's a perfect teaching subject.** Because xmouseless is small (one C file of about 760 lines plus one config file), you can read *all* of it and truly understand how a program talks to your screen, grabs your keyboard, moves a pointer, fakes clicks, runs shell commands, and even draws its own on-screen help. By the end, "how does software control my computer?" will stop being magic.

**Key idea:** xmouseless trades a physical device for keys — so you can always point & click with nothing but your keyboard.

Our map hasn't grown yet — this step was about motivation, not structure. It still looks like this:

```
xmouseless ── a program that turns your keyboard into a mouse (on X11)
               (works when the mouse is gone · keeps hands on keys · small enough to fully understand)
```

---

## Step 3: How It Works at the Highest Level — The Flow

Before we ever type anything, let's learn how xmouseless *works* in one picture. This is the "how it works" layer of our journey, and once you see this diagram, everything else in this tutorial just hangs on it like ornaments on a tree.

Here is the whole architecture of xmouseless — all of it:

```
 ┌──────────────┐      ┌─────────────────────┐      ┌──────────────────────────────┐
 │   YOUR       │      │     XMOUSELESS      │      │        THE COMPUTER          │
 │  KEYBOARD    │ ───► │   (the brain)       │ ───► │                             │
 │              │ keys │                    │ acts │  • moves the pointer         │
 │ q w e r t …  │ in   │ decides what each  │      │  • fakes a click             │
 │ arrows, 8/9/0│      │ key should do      │      │  • scrolls                   │
 └──────────────┘      └─────────────────────┘      │  • runs shell commands       │
                                                   └──────────────────────────────┘
   THE INPUT              THE DECIDER                    THE EFFECTS
```

Three boxes. That is the entire universe of how xmouseless works. Let me name each one, because we'll use these names forever after:

**1. The input (left box).** Your keyboard. But here's a crucial detail we'll meet in Step 15: when xmouseless starts, it *grabs* the whole keyboard — meaning every key you press goes to xmouseless first, not to whatever app is open. For as long as xmouseless runs, your keys are its keys.

**2. The decider (middle box).** This is `xmouseless.c` itself. Its job is a simple lookup: "which key did I just get? What should that key do?" It keeps tables of bindings — like a cheat sheet mapping each key to an action ("arrow right → move pointer right", "8 → left click"). We'll meet these tables in Step 13.

**3. The effects (right box).** Whatever the decider chose, it makes happen on your computer: moving the mouse pointer, faking a mouse-button press, scrolling, or running a shell command (like jumping the pointer to a spot on screen). These are the *outputs* — the visible results you see.

Now — and this is the single most important rule of xmouseless, so let's say it three times because we'll rely on it constantly:

> **Rule #1:** Every key press goes through one place — `handle_key()` — which looks up what that key means.
> **Rule #2:** The meaning comes from a *binding table* (a list of "key → action" pairs) you can edit in `config.h`.
> **Rule #3:** The actual effect is done by calling an X11 function (move, click, scroll) or running a shell command.

Why does this matter? Because it means xmouseless is like a receptionist at a hotel: every guest (key press) arrives at one desk (`handle_key`), the receptionist checks the guest list (the binding table), and then sends them to their room (an effect). Change the guest list, and you change where everyone goes — without touching the building.

**Key idea:** key in → `handle_key()` looks up the binding → an X11 call or shell command does the effect.

Our map grows by exactly one branch:

```
xmouseless ── a program that turns your keyboard into a mouse (on X11)
               (works when the mouse is gone · keeps hands on keys · small enough to fully understand)
  │
  └── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
                        ▲            │              ├── move pointer
                     grabs all   looks up a        ├── fake click
                     the keys    binding table     ├── scroll
                                                  └── run shell command
```

Notice we added only *one* new idea this step — the three-box flow. Everything else waits patiently for its own step. That's the snail way: never two new things at once.

---

## Step 4: Your Environment — What Is Installed and Verified

Before we run anything, let's look around our kitchen and confirm all the ingredients are on the shelf. Here is what this tutorial was built against, checked for real on a Debian-based Linux machine:

**Your environment:**
- **gcc (Debian 14.2.0-19) 14.2.0** — the C compiler that turns `xmouseless.c` into a runnable program
- **xdotool version 3.20160805.1** — a helper tool xmouseless calls to jump the pointer and read screen size
- **libX11-dev 2:1.8.12-1** and **libxtst-dev 2:1.2.5-1** — the X11 development libraries (headers + code) that let a C program draw on screen and fake mouse events
- An **X11 desktop session** — xmouseless draws directly on X11 screens; if you're on a Wayland-only setup, see Troubleshooting at the end

You can check your own machine any time with these tiny commands:

```sh
gcc --version        # asks: "which compiler is this?" → prints its version
xdotool --version    # asks: "which xdotool is this?" → prints its version
```

Let's decode those, one word at a time — because even `--version` deserves an explanation.

- **`gcc`** stands for *GNU Compiler Collection*. It reads C source and produces a runnable binary. Think of it as a translator between human-readable instructions (C) and machine-executable ones.
- **`--version`** is an *option* (also called a *flag*) — a word starting with dashes that asks the program for information instead of doing its normal job. Here it just reports "who am I, and how new am I?"

> **Where are we?** This step added no new concept to the map — it just confirmed our tools exist. The map is unchanged:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   └── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
> ```

---

## Step 5: Meet the Bare Shell — `main()` and Every Character on It

Now we begin building. We start at the absolute root: a single, empty structural shell that does almost nothing yet. This is the smallest possible C program. Here it is, in its entirety:

```c
int main(void) {
    return 0;
}
```

That's *all* of it — four lines. And yet this is a complete, runnable C program. Let me explain every single character on screen, because each one has a job.

**`int`.** This is the *return type*. It tells the computer: "when `main` finishes, it will hand back a whole number (an integer)." Think of it as a label on an envelope saying what kind of thing comes out when you're done.

**`main`.** The name of this particular function — and it's a *special* name. Every C program starts running at the function called `main`. It's the front door: no matter how many other functions exist, the computer always walks in through `main` first.

**`(void)`.** These are the *parameters* — things you could hand into the function. `(void)` means "hand me nothing." The parentheses are required even when there's nothing to pass; they're like an empty tray that must still be present on the table.

**`{ ... }`.** The curly braces are the *body* of the function — everything between them is what `main` does. They're a pair of hands holding together all the instructions inside.

**`return 0;`.** This says "finish, and hand back the number zero." By convention, returning `0` means "I succeeded without any problems." The semicolon `;` at the end is required in C — it's the period at the end of a sentence. Without it, the compiler doesn't know where one instruction ends.

**A function**, by the way, is just a named bundle of instructions you can reuse. Like a recipe card: "to make tea" lists steps; anyone can follow that same card whenever they want tea. `main` is our first recipe card.

Let's put this tiny program in a file and actually run it — because seeing it work will anchor everything. Create the project folder and the file:

```sh
mkdir -p ~/xmouseless && cd ~/xmouseless
printf 'int main(void) {\n    return 0;\n}\n' > xmouseless.c
cat xmouseless.c          # show us what we just wrote
gcc xmouseless.c -o xmouseless   # compile it into a runnable program named "xmouseless"
./xmouseless              # run it (the ./ means "run the file right here")
echo $?                   # print the exit code → should be 0
```

Run those. You'll see `cat` show your four lines, then `gcc` produces a new file called `xmouseless`, and running `./xmouseless` does nothing visible (correct — it just returns) and `echo $?` prints `0`. **It worked.** You now have a real, compiled C program that does nothing yet. That empty shell is our foundation; every later step adds exactly one more brick to it.

> **Where are we?** We've met the front door of any C program — `main()`. The map gains its first structural branch:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │
>   └── the code itself starts as an empty shell:
>        int main(void) { return 0; }     ← front door, does nothing yet ✓ (this step)
> ```

---

## Step 6: The Project's Files — Where Everything Will Live

Before we add more code, let's orient ourselves in the *files*, not just the ideas. A real project is several files working together. Here is the complete file tree of xmouseless as it will be when finished — and I'll annotate what each one does:

```
xmouseless/
├── xmouseless.c      (the main program — all the logic lives here)
├── config.h          (your settings — every key binding, in plain tables you can edit)
├── buildonly.sh      (a tiny script that runs the gcc command for us)
├── README.md         (notes about what was changed)
└── LICENSE           (the legal terms under which this code is shared)
```

Let's meet each file with a gentle analogy:

- **`xmouseless.c`** — the *kitchen*. All the cooking happens here. This is where we'll spend most of our time, adding one ingredient at a time.
- **`config.h`** — the *recipe card on the fridge*. It holds the settings (which key does what) in simple tables. You can change it without touching the kitchen. We'll meet it properly in Step 13.
- **`buildonly.sh`** — the *oven timer button*. Instead of typing the long `gcc ...` command every time, this little script runs it for you with one click. Here's its whole content:

```sh
#!/usr/bin/env bash

/usr/bin/gcc -Wall -g -o xmouseless  xmouseless.c -lX11 -lXtst -lpthread -lXext
```

Let's decode that build line, because it tells us a lot about what the program needs:

| Piece            | Meaning                                                                 |
| ---              | ---                                                                     |
| `#!/usr/bin/env bash` | A *shebang* — tells the system "run this file with the bash shell"     |
| `-Wall`          | Turn on many helpful warnings (the compiler will nudge you about bugs)  |
| `-g`             | Add debugging info so errors point to exact lines                       |
| `-o xmouseless`  | Name the finished program `xmouseless`                                  |
| `xmouseless.c`   | The source file we're compiling                                         |
| `-lX11 -lXtst -lpthread -lXext` | Link in extra libraries: X11 drawing, fake mouse events (XTest), threads, and extensions |

The `-l...` flags are the important new idea here. **Linking** means "attach these ready-made helper libraries to my program so I can use their functions." We'll meet each library as we need it — for now just know: xmouseless borrows four helpers (drawing, fake mouse events, threads, extensions).

> **Where are we?** No new code yet — this step was about *where* things live. The map is unchanged, but you now know the three files that matter (`xmouseless.c`, `config.h`, `buildonly.sh`):
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │
>   └── the code itself starts as an empty shell:
>        int main(void) { return 0; }     ← front door, does nothing yet ✓ (this step)
> ```

---

## Step 7: Comments — Words That Are Never Run

Programs need notes. Sometimes you want to write a sentence *inside* the code that explains what's happening — but the computer must ignore it and only run the real instructions. Those ignored sentences are called **comments**.

C has two kinds of comments, and both appear throughout xmouseless:

**1. The block comment — `/* ... */`.** Everything between `/*` and `*/` is a note to humans. It can span many lines. Look at the very top of the real `xmouseless.c`:

```c
/*
 * xmouseless - keyboard-driven mouse replacement for X11
 * Compile: gcc xmouseless.c -o xmouseless -lX11 -lXext -lXtst -lpthread
 */
```

The computer reads this and does nothing with it — it's purely a memo. The `*` at the start of each line is just decoration to make the note look tidy; only `/*` opens and `*/` closes.

**2. The line comment — `// ...`.** Everything from `//` to the end of that one line is ignored. It's shorter, for quick notes:

```c
static const unsigned int move_rate = 100;   // how many times per second we act
```

Why do comments matter so much? Because code without them is like a recipe with no instructions — you can see the ingredients but not what to *do*. Comments are the "now stir gently" notes. In xmouseless, they explain tricky spots (like why a certain line must come first).

Let's add one comment to our shell so you feel it:

```c
/* This is my keyboard-driven mouse program */
int main(void) {
    return 0;   // zero means "success"
}
```

Compile and run again — nothing changes, because comments are invisible to the compiler. That's exactly the point: they're for *you*.

> **Where are we?** Comments added no behavior — only understanding. The map is unchanged:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │
>   └── the code itself starts as an empty shell:
>        int main(void) { return 0; }     ← front door, does nothing yet ✓ (this step)
> ```

---

## Step 8: `#include` — Borrowing Someone Else's Instructions

Our program needs to do things it doesn't know how to do on its own — like print text or talk to the screen. Rather than writing all that from scratch, C lets us *borrow* ready-made instructions with a directive called **`#include`**.

Here is our shell now borrowing one thing:

```c
#include <stdio.h>

int main(void) {
    return 0;
}
```

Let's decode every character:

- **`#`** — In C, any line starting with `#` is a *preprocessor directive*. It's an instruction to the compiler *before* it even starts compiling. Think of it as a note pinned above the recipe card saying "first, gather these extra ingredients."
- **`include`** — The specific directive: "paste in the contents of this other file right here."
- **`<stdio.h>`** — The file to paste in. `stdio` means *standard input/output* (printing text, reading input). The `.h` extension means it's a *header file*.

What is a **header file**? It's like a menu of functions you're allowed to use. `<stdio.h>` lists all the standard print/read functions and their exact spelling. By including it, we tell the compiler "I'm going to use these — please let me." Without the `#include`, using those functions would be like ordering a dish that isn't on the menu: the kitchen (compiler) refuses.

xmouseless includes many headers because it does many things. Here's the real list at the top of `xmouseless.c` — notice how each one matches a capability we'll meet later:

```c
#include <stdio.h>      /* printing text */
#include <stdlib.h>     /* general utilities, like exit() */
#include <string.h>     /* working with strings of characters */
#include <unistd.h>     /* low-level system calls (sleep, pipes) */
#include <fcntl.h>      /* file-descriptor flags */
#include <signal.h>     /* handling signals (like "a child finished") */
#include <time.h>       /* getting the current time */
#include <pthread.h>    /* threads — running things in parallel */

#include <X11/Xlib.h>   /* the core X11 drawing library */
#include <X11/Xutil.h>  /* extra X11 helpers (fonts, colors) */
#include <X11/XKBlib.h> /* keyboard handling in X11 */
#include <X11/extensions/XTest.h> /* faking mouse clicks */
#include <X11/extensions/shape.h> /* making windows see-through */
```

You don't need to memorize these. Just feel the pattern: **each `#include` unlocks a new ability**, and xmouseless has unlocked drawing, keyboard control, fake clicks, threads, and transparency — exactly matching its features from Step 3.

> **Where are we?** We've met how C borrows capabilities. The map gains one small detail under the code branch:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │
>   └── the code itself starts as an empty shell:
>        #include <...h>   ← each one unlocks a new ability ✓ (this step)
>        int main(void) { return 0; }     ← front door, does nothing yet
> ```

---

## Step 9: A Function That Does Something — Printing a Word

Our `main` still only returns. Let's make it *do* something visible for the first time: print a word to your terminal. This teaches us how functions can call other functions and send out text.

```c
#include <stdio.h>

int main(void) {
    printf("xmouseless is running\n");
    return 0;
}
```

The new line is `printf("xmouseless is running\n");`. Let's take it apart:

- **`printf`** — A function borrowed from `<stdio.h>` (Step 8). It *prints formatted text*. "Printf" = print + format.
- **`( ... )`** — The parentheses hold the *arguments* — what you hand to `printf`. Here, one argument: a string of characters in quotes.
- **`"xmouseless is running"`** — A *string literal*: exact text, wrapped in double quotes so C knows it's words and not code.
- **`\n`** — An *escape sequence* meaning "newline." It moves the cursor to the next line after printing. (The backslash `\` introduces a special character.)
- **`;`** — The required period ending this instruction.

Compile and run:

```sh
gcc xmouseless.c -o xmouseless && ./xmouseless
```

You'll see `xmouseless is running` printed, then return to your prompt. **The program now does something you can see.** That's the difference between a shell that merely exists and one that acts.

> **Where are we?** Our front door now performs its first action — printing. The map grows:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │
>   └── the code itself starts as an empty shell:
>        #include <stdio.h>          ← unlocks printing ✓ (this step)
>        int main(void) {
>            printf("...");         ← first visible action ✓ (this step)
>            return 0;              ← front door, done
>        }
> ```

---

## Step 10: Variables — Named Boxes for Values

Programs need to *remember* things while they run. A **variable** is a named box that holds a value you can read and change later. Let's add one:

```c
#include <stdio.h>

int main(void) {
    int speed = 500;                 /* remember the mouse speed */
    printf("speed is %d\n", speed);
    return 0;
}
```

Let's decode `int speed = 500;`:

- **`int`** — The *type* of the box. `int` means "whole number." Types matter because a box for numbers isn't the same as a box for text or for precise decimals. (We'll meet `float`, a type for decimal numbers, soon.)
- **`speed`** — The *name* of the box. We chose it because it holds the mouse's speed. Good names make code readable; bad names (`x1`) don't.
- **`= 500`** — Put the value `500` inside the box right now. This is called *initializing*.
- **`;`** — End of instruction.

Then `%d` in the `printf`: that's a *placeholder* meaning "put an integer here." So `printf("speed is %d\n", speed)` prints `speed is 500`. The second argument to `printf` (the value after the comma) fills the `%d` slot.

Why does xmouseless need variables? Because it must remember things like: where the mouse currently is, how fast to move it, whether a key is still being held. Every one of those "remember this" needs becomes a variable. In real `xmouseless.c`, you'll find globals like `unsigned int speed;` and structs holding the pointer's position — all just named boxes.

> **Where are we?** We've met how programs remember values. The map gains the idea of state:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │
>   └── the code itself starts as an empty shell:
>        #include <stdio.h>          ← unlocks printing ✓ (this step)
>        int main(void) {
>            int speed = 500;       ← a named box holding a value ✓ (this step)
>            printf("speed is %d\n", speed);   ← first visible action
>            return 0;              ← front door, done
>        }
> ```

---

## Step 11: Arrays — A Row of Boxes

One variable holds one value. But xmouseless needs to remember *many* values at once — like "which keys move the mouse, and in which direction?" For that we use an **array**: a row of boxes of the same type, numbered from `0`.

```c
#include <stdio.h>

int main(void) {
    int steps[3] = { 1, -1, 2 };   /* three boxes: steps[0]=1, steps[1]=-1, steps[2]=2 */
    printf("first step is %d\n", steps[0]);
    return 0;
}
```

Let's decode `int steps[3] = { 1, -1, 2 };`:

- **`int`** — Each box holds a whole number.
- **`steps`** — The name of the *whole row*.
- **`[3]`** — There are three boxes in this row.
- **`= { 1, -1, 2 }`** — Fill them, left to right: `steps[0]=1`, `steps[1]=-1`, `steps[2]=2`.
- **`steps[0]`** — To read one box, use its *index* (its position). Counting starts at `0`, not `1`! So the first box is `[0]`, the second is `[1]`, and so on.

Why does xmouseless need arrays? Because a single key can't describe "move right by 1." It needs a *list* of many such facts — one per direction, one per click button, one per scroll direction. Arrays are how it stores those lists. In real `xmouseless.c`, you'll see arrays like `static KeySym exit_keys[] = { XK_Escape };` — a row holding the keys that quit the program.

> **Where are we?** We've met rows of boxes. The map gains collections:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │
>   └── the code itself starts as an empty shell:
>        #include <stdio.h>          ← unlocks printing ✓ (this step)
>        int main(void) {
>            int speed = 500;       ← one named box ✓ (this step)
>            int steps[3] = {...};  ← a row of boxes, indexed from 0 ✓ (this step)
>            printf(...);           ← first visible action
>            return 0;              ← front door, done
>        }
> ```

---

## Step 12: Structs — Grouping Related Boxes Together

An array holds many values of the *same* type. But sometimes one fact needs several *different* pieces together — like "this key moves the mouse by x=+1 and y=0." For that we use a **struct** (short for structure): a custom box that groups several fields under one name.

```c
#include <stdio.h>

typedef struct {
    int keysym;   /* which key */
    float x;      /* how much to move horizontally */
    float y;      /* how much to move vertically */
} MoveBinding;

int main(void) {
    MoveBinding b = { 65, 1.0f, 0.0f };   /* one binding: key 65 → move right */
    printf("key %d moves x by %g\n", b.keysym, b.x);
    return 0;
}
```

Let's decode the new pieces:

- **`typedef struct { ... } MoveBinding;`** — We're *defining a new type* called `MoveBinding`. A `struct` is like designing your own kind of box with labeled compartments. Here each compartment holds one field (`keysym`, `x`, `y`).
- **`float x; float y;`** — Now we meet the `float` type: numbers with a decimal point (like `1.0f`). We use floats for movement amounts because they can be fractional and precise. The trailing `f` in `1.0f` tells C "this is a float."
- **`MoveBinding b = { 65, 1.0f, 0.0f };`** — Create one box of our new type and fill its compartments left to right: `keysym=65`, `x=1.0`, `y=0.0`.
- **`b.keysym`** — To read a field inside the struct, use *dot notation*: `box.field`. So `b.x` reads the horizontal amount from box `b`.

Why does xmouseless need structs? Because every binding is really three facts glued together: *which key*, and *what it does*. A struct keeps those glued so they travel as one unit. In real `xmouseless.c`, you'll find exactly these types defined near the top:

```c
typedef struct { KeySym keysym; float x; float y; } MoveBinding;
typedef struct { KeySym keysym; unsigned int button; } ClickBinding;
typedef struct { KeySym keysym; float x; float y; } ScrollBinding;
typedef struct { KeySym keysym; unsigned int speed; } SpeedBinding;
typedef struct { KeySym keysym; char *command; } ShellBinding;
```

Each one is "a key plus whatever that kind of action needs." That's the whole idea.

> **Where are we?** We've met grouping related values. The map gains composite data:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │
>   └── the code itself starts as an empty shell:
>        #include <stdio.h>          ← unlocks printing ✓ (this step)
>        int main(void) {
>            int speed = 500;       ← one named box ✓ (this step)
>            int steps[3] = {...};  ← a row of boxes ✓ (this step)
>            MoveBinding b = {...};← a grouped box: key + action ✓ (this step)
>            printf(...);           ← first visible action
>            return 0;              ← front door, done
>        }
> ```

---

## Step 13: `config.h` — The Editable Binding Tables

Now we meet the file you'll most want to touch: **`config.h`**. Remember from Step 6 it's "the recipe card on the fridge" — all your settings in plain tables. And remember Rule #2 from Step 3: *the meaning of each key comes from a binding table.* This is where those tables live.

Here is a real slice of `config.h`, with comments that explain each table:

```c
/* moves the mouse pointer */
static MoveBinding move_bindings[] = {
  /* key         x      y */
  { XK_Left,        -1,     0 },   /* press Left  → move left  */
  { XK_Right,         1,     0 },  /* press Right → move right */
  { XK_Up,          0,    -1 },    /* press Up    → move up    */
  { XK_Down,        0,     1 },    /* press Down  → move down  */
};

/* 1: left   2: middle   3: right */
static ClickBinding click_bindings[] = {
  /* key         button */
  { XK_8,        1 },             /* press 8 → left click   */
  { XK_9,        2 },             /* press 9 → middle click */
  { XK_0,        3 },             /* press 0 → right click  */
};

/* changes the speed of the mouse pointer */
static SpeedBinding speed_bindings[] = {
  /* key             speed */
  { XK_Return,      20 }         /* hold Enter → slow down for precision */
};
```

Let's decode what we're seeing:

- **`XK_Left`, `XK_Right`, …** — These are *keysyms*: symbolic names for keys. Instead of writing a cryptic number, C lets us say "the Left arrow key" by name. (They come from the X11 keyboard header we included in Step 8.)
- **`move_bindings[] = { ... }`** — An *array* (Step 11) of `MoveBinding` structs (Step 12). Each row is one "key → direction" fact. The `-1 / +1` values are the x/y amounts we'll feed to the mouse later.
- **`click_bindings[]`** — Same pattern, but each row maps a key to a *button number* (`1`=left, `2`=middle, `3`=right).
- **`speed_bindings[]`** — Maps a key (Enter) to a slower speed value, so holding it makes the pointer creep for fine control.

Now here's the beautiful part that ties back to Rule #2: **to change what a key does, you only edit this file.** Want `A` instead of arrow keys? Swap `XK_Left` for `XK_a`. No need to touch the logic in `xmouseless.c`. That's why xmouseless keeps its settings separate — the kitchen (logic) stays clean while the fridge card (settings) is easy to rewrite.

How does `config.h` get into the program? With one line near the top of `xmouseless.c`:

```c
#include "config.h"
```

Notice the *quotes* around `"config.h"` instead of angle brackets `<...>`. That's a small but real difference: **angle brackets** `<stdio.h>` mean "look in the system's standard library folders," while **quotes** `"config.h"` mean "look right here, next to my source file." Both are `#include` (Step 8) — they just search different places.

> **Where are we?** We've met the editable heart of xmouseless. The map gains the settings branch:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │
>   ├── config.h — the editable binding tables ✓ (this step):
>   │     move_bindings[]    key → direction
>   │     click_bindings[]   key → button number
>   │     speed_bindings[]   key → slower speed
>   │     scroll_bindings[]  key → scroll amount
>   │     shell_bindings[]   key → command to run
>   │
>   └── the code itself starts as an empty shell:
>        #include "config.h"    ← pulls these tables in ✓ (this step)
>        int main(void) { ... } ← front door, does nothing yet
> ```

---

## Step 14: X11 Basics — Display, Window, and Graphics Context

Now we reach the part that makes xmouseless *talk to your screen*. This is where C meets **X11**, the drawing system. Three concepts appear everywhere in `xmouseless.c`, so let's meet them once, clearly, with analogies:

**1. The Display (`Display *dpy`).** Your whole X11 session — all screens, windows, and input devices together — is one big object called a **display**. Opening it gives you a handle (a pointer) named `dpy`. Think of the display as *the entire theater*; everything else is a seat or actor inside it.

```c
Display *dpy;                 /* our handle to the whole X11 session */
dpy = XOpenDisplay(NULL);     /* open it — NULL means "my default screen" */
if (!dpy) { ... }             /* if opening failed, bail out gracefully */
```

- **`*`** after `Display` means this is a *pointer* — not the thing itself, but an address pointing to it. Pointers are how C refers to big objects without copying them. (We'll keep using pointers throughout; just read `X *name` as "a handle to an X.")
- **`XOpenDisplay(NULL)`** — Ask X11 for a connection to the screen. If it returns nothing (`!dpy` is true), we can't draw, so we stop with an error message.

**2. The Window (`Window root`).** A *window* in X11 is any rectangular region on screen you can draw into — even the whole desktop background counts as one (called the **root window**). xmouseless draws its overlays onto windows it creates, and reads the pointer position relative to `root`.

```c
int    screen = DefaultScreen(dpy);   /* which screen number? */
Window root   = RootWindow(dpy, screen);  /* the whole-desktop window */
```

**3. The Graphics Context (`GC`).** A **graphics context** (often written `gc`) is like a *paintbrush with settings already chosen* — its color, font, line style. You don't draw directly; you pick up a brush (`XCreateGC`), set its properties (`XSetForeground`, `XSetFont`), and then paint with it (`XDrawString`, `XFillRectangle`).

```c
GC info_gc = XCreateGC(dpy, info_win, 0, NULL);  /* make a brush for this window */
XSetForeground(dpy, info_gc, INFO_TEXT);         /* choose the ink color */
```

Why do we need all three? Because drawing in X11 always follows the same shape: **open the display → pick a window to draw on → use a graphics context (brush) to paint.** Every visual thing xmouseless does — the grid, the help box — is built from exactly these three pieces.

> **Where are we?** We've met the vocabulary of drawing. The map gains the X11 branch:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   └── drawing vocabulary (X11):
>        Display *dpy   ← the whole session (the theater)      ✓ (this step)
>        Window root    ← a rectangle to draw on (a seat)       ✓ (this step)
>        GC gc          ← a brush with chosen color/font       ✓ (this step)
> ```

---

## Step 15: Grabbing the Keyboard — Why Every Key Reaches xmouseless

Remember from Step 3 that when xmouseless runs, *every* key goes to it first. That's not automatic — xmouseless must actively **grab** the keyboard. Here is how, straight from `init_x()` in `xmouseless.c`:

```c
for (i = 0; i < GRAB_TRIES; i++) {
    if (XGrabKeyboard(dpy, root, False, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess) {
        XAutoRepeatOff(dpy);               /* only after we really own the keyboard */
        return;                            /* success — stop trying */
    }
    usleep(10000);                          /* wait 10 ms and try again */
}
```

Let's decode it gently:

- **`XGrabKeyboard(...)`** — Ask X11 to hand over exclusive control of the keyboard. While grabbed, key events go to xmouseless instead of other apps. It's like putting your hand on a shared steering wheel so only you can turn it right now.
- **`== GrabSuccess`** — The grab might fail (another program may already hold the keyboard). So we check: did it succeed? If yes, proceed; if no, retry.
- **The `for` loop with `GRAB_TRIES` (300)** — Try up to 300 times, waiting 10 ms between tries (about 3 seconds total), in case the keyboard is briefly busy and frees up. A *loop* repeats a block of code while a condition holds; here it's "keep trying until success or out of attempts."
- **`XAutoRepeatOff(dpy)`** — Turn off key auto-repeat. Normally, holding a key makes it repeat (like holding `a` types `aaaa`). For mouse control we want *one* deliberate action per press, so we switch repeats off — and turn them back on when we exit.

Why does this matter? Because without the grab, pressing arrow keys would type into your open app instead of moving the pointer. The grab is what makes "keyboard drives the mouse" possible at all. It's the single most important setup step in xmouseless.

> **Where are we?** We've met how xmouseless takes over the keyboard. The map gains input capture:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲
>   │              XGrabKeyboard takes exclusive control ✓ (this step)
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   └── drawing vocabulary: Display / Window / GC  ✓ (this step)
> ```

---

## Step 16: The Event Loop — Waiting for Keys, Then Reacting

A program that does nothing until something happens needs a way to *wait* and then *react*. That's an **event loop**. Here is the core of xmouseless's main loop (simplified from `main()`):

```c
while (1) {                                   /* keep going forever */
    while (XPending(dpy)) {                   /* while there are waiting events… */
        XEvent event;                         /* grab one event */
        XNextEvent(dpy, &event);              /* read it into 'event' */

        switch (event.type) {                 /* decide what kind it is */
            case KeyPress:                    /* a key went down   */
            case KeyRelease:                  /* a key came up     */
                handle_key(event.xkey.keycode, event.xkey.type == KeyPress);
                break;                        /* done with this one */

            default:                          /* anything else: ignore for now */
                break;
        }
    }
}
```

Let's decode the new pieces, one at a time:

- **`while (1)`** — A loop that runs forever. In C, `1` means "true," so `while(1)` is the classic "loop until told to stop." xmouseless stops only when you press an exit key (Step 28).
- **`XPending(dpy)`** — Asks X11: "are there any waiting events?" An *event* is a message like "a key was pressed" or "this window needs redrawing." If yes, we process them.
- **`XNextEvent(dpy, &event)`** — Pulls the next event out of the queue and stores it in `event`. The `&` means "give me the address of this variable so you can fill it in" (a pointer again, from Step 14).
- **`switch (event.type)`** — A *branch* that picks one block to run based on a value. It's like a receptionist checking which room a guest wants and sending them there. Each `case` is a possible room; `break` means "stop after this case."
- **`KeyPress` / `KeyRelease`** — Two kinds of key events: the moment a key goes *down*, and the moment it comes back *up*. xmouseless cares about both, because some actions (like moving) happen while held, others (like clicking) happen on release.

This loop is the heartbeat of xmouseless: **wait for an event → figure out what kind → react.** Every key you press eventually lands in this `switch`, and from there goes to `handle_key` — exactly Rule #1 from Step 3.

> **Where are we?** We've met the heartbeat. The map gains the loop:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            ▲
>   │              grabs keys    called from the event loop ✓ (this step)
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   └── drawing vocabulary: Display / Window / GC  ✓ (this step)
> ```

---

## Step 17: `handle_key()` — Looking Up What a Key Means

Now we reach Rule #1's home: the function that decides what a key does. Here is its essential shape from `xmouseless.c`:

```c
void handle_key(KeyCode keycode, Bool is_press) {
    unsigned int i;
    KeySym keysym = XkbKeycodeToKeysym(dpy, keycode, 0, 0);   /* name of the key */

    for (i = 0; i < LENGTH(move_bindings); i++) {            /* check each move binding */
        if (move_bindings[i].keysym == keysym) {             /* does it match? */
            int sign = is_press ? 1 : -1;                    /* down=+1, up=-1 */
            mouseinfo.speed_x += sign * move_bindings[i].x;  /* add to movement speed */
            mouseinfo.speed_y += sign * move_bindings[i].y;
        }
    }

    for (i = 0; i < LENGTH(click_bindings); i++) {           /* check each click binding */
        if (click_bindings[i].keysym == keysym) {
            click(click_bindings[i].button, is_press);       /* do the click */
        }
    }
}
```

Let's decode it gently:

- **`void handle_key(KeyCode keycode, Bool is_press)`** — A function (Step 9) that takes two things: *which key* (`keycode`) and *whether it went down or up* (`is_press`). `void` at the front means "it doesn't hand back a value."
- **`XkbKeycodeToKeysym(...)`** — Translates a raw key number into a friendly keysym name (like `XK_Left`). This lets us compare against the names in our binding tables.
- **The two `for` loops** — Remember arrays (Step 11)? Each loop walks down one binding table, comparing each row's key (`move_bindings[i].keysym`) to the key we just got. The dot notation `[i].keysym` means "row i, its keysym field" (Step 12).
- **`if (... == keysym)`** — If this row matches our key, do that row's action. This is the lookup: *find my key in the table, then act.*
- **`is_press ? 1 : -1`** — A *ternary* (three-part) expression meaning "if it's a press use +1, otherwise use −1." Why? Because while you *hold* an arrow key we want to keep moving (`+1` adds speed); when you *release* it we subtract (`-1`) so the pointer stops. Hold = go, release = stop.

This is the receptionist from Step 3 doing its job: check the guest list (binding tables), find your name, send you to your room (an effect). And notice — **we only added one concept here**: matching a key to a table row and acting on it. Everything else waits for its own step.

> **Where are we?** We've met the lookup. The map's middle box is now filled in:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            │              ▲
>   │              grabs keys    looks up binding ✓ (this step)   does the effect
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   └── drawing vocabulary: Display / Window / GC  ✓ (this step)
> ```

---

## Step 18: Moving the Pointer — `XWarpPointer` and Friends

Let's make one effect real: moving the mouse pointer. Here are the three small functions from `xmouseless.c` that do it:

```c
void get_pointer(void) {
    int x, y;
    XQueryPointer(dpy, root, &dummy, &dummy, &x, &y, ...);  /* where is the pointer now? */
    mouseinfo.x = x;                                          /* remember it */
    mouseinfo.y = y;
}

void move_relative(float x, float y) {
    mouseinfo.x += x;                                         /* add to remembered position */
    mouseinfo.y += y;
    XWarpPointer(dpy, None, root, 0, 0, 0, 0, (int)mouseinfo.x, (int)mouseinfo.y);
    XFlush(dpy);                                              /* actually send the request */
}
```

Let's decode:

- **`get_pointer()`** — Asks X11 "where is the pointer right now?" and stores that position in `mouseinfo`. We need to *know* where we are before we can move relative to it. (The `&dummy, &dummy` are extra values this function fills but doesn't use here.)
- **`move_relative(x, y)`** — Moves the pointer by an *amount* (`x`, `y`) from its current spot, rather than jumping to a fixed place. "Relative" = "by this much," not "to that exact coordinate."
- **`XWarpPointer(...)`** — The actual X11 call that teleports the pointer to `(mouseinfo.x, mouseinfo.y)`. *Warp* means "jump instantly."
- **`XFlush(dpy)`** — This is important and easy to miss. Many X11 calls are just *queued* (put in a waiting line) rather than sent immediately. `XFlush` pushes the whole queue out so it actually happens now. Think of it as pressing "send" on an email you've been drafting.

Why does xmouseless track position itself (`mouseinfo`) instead of asking X11 every time? Because while holding a key, we move many tiny steps in quick succession; remembering our own running total is faster and smoother than re-querying each step.

> **Where are we?** We've met one effect — moving. The map's right box gains its first action:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            │              ├── move pointer ✓ (this step)
>   │              grabs keys    looks up binding      ├── fake click
>   │                                                 ├── scroll
>   │                                                 └── run shell command
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   └── drawing vocabulary: Display / Window / GC  ✓ (this step)
> ```

---

## Step 19: The Background Thread — Moving While You Hold a Key

Here's a subtle but powerful idea. When you *hold* an arrow key, the pointer should keep gliding smoothly — not just jump once per press. To do that, xmouseless runs a second piece of code in parallel with the event loop. That parallel worker is called a **thread**.

```c
void *move_forever(void *val) {
    (void)val;                                   /* we don't use the argument */
    while (running) {                            /* keep going until told to stop */
        if (mouseinfo.speed_x != 0 || mouseinfo.speed_y != 0) {
            move_relative((float)mouseinfo.speed_x * speed / move_rate,
                          (float)mouseinfo.speed_y * speed / move_rate);
        }
        usleep(1000000 / move_rate);             /* pause a tiny bit */
    }
    return NULL;
}
```

Let's decode it gently:

- **A thread** is like hiring a second pair of hands that works at the same time as your first pair. The main loop keeps listening for keys (Step 16) while this thread keeps nudging the pointer — both happening together. Threads come from `<pthread.h>` (Step 8).
- **`void *move_forever(void *val)`** — A function that returns a `void *` (a generic handle; threads always return this shape). The `(void)val;` line is a quiet way of saying "I know there's an argument, I just don't use it" — it silences a compiler warning.
- **`while (running)`** — Loop as long as the global flag `running` is true. When we exit (Step 28), we set `running = 0`, and this loop stops on its own.
- **The `if (... != 0 ...)` check** — Only move if there's actually a direction to go. `speed_x`/`speed_y` are the "how fast in each direction" values that `handle_key` set when you pressed an arrow (Step 17). If both are zero, do nothing this tick.
- **`* speed / move_rate`** — The math: take the direction amount, scale it by your current `speed`, and divide by `move_rate`. This converts "how fast overall" into "how far to nudge *this one tick*."
- **`usleep(1000000 / move_rate)`** — Sleep for a fraction of a second. `move_rate` is `100`, so this sleeps 1/100th of a second (10 ms). That means the thread nudges the pointer about **100 times per second** — fast enough to look like smooth, continuous motion.

Why do we need a thread at all? Because the event loop (Step 16) is busy *waiting* for keys; it can't also be nudging the pointer 100 times a second. So xmouseless splits the job: one part listens, another moves. That's the whole reason threads exist here — **two jobs that must happen at once.**

> **Where are we?** We've met parallel work. The map gains concurrency under "move":
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            │              ├── move pointer ✓ (this step)
>   │              grabs keys    looks up binding      │     └─ a thread nudges it 100×/sec while held ✓ (this step)
>   │                                                 ├── fake click
>   │                                                 ├── scroll
>   │                                                 └── run shell command
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   └── drawing vocabulary: Display / Window / GC  ✓ (this step)
> ```

---

## Step 20: Clicking — Faking a Mouse Button Press

Now let's make another effect real: clicking. A "click" is really two events in a row — the button going *down*, then coming back *up*. xmouseless fakes both using X11's test extension (from `<X11/extensions/XTest.h>`, Step 8):

```c
void click(unsigned int button, Bool is_press) {
    XTestFakeButtonEvent(dpy, button, is_press, CurrentTime);
    XFlush(dpy);
}

void click_full(unsigned int button) {
    XTestFakeButtonEvent(dpy, button, 1, CurrentTime);   /* press down */
    XTestFakeButtonEvent(dpy, button, 0, CurrentTime);   /* release up */
    XFlush(dpy);
}
```

Let's decode:

- **`XTestFakeButtonEvent(...)`** — Asks X11 to *pretend* a mouse button was pressed or released. The arguments are: which `button`, whether it's going down (`is_press`) or up, and the time (`CurrentTime`). It's like pressing a real button on behalf of the user.
- **`click(button, is_press)`** — Sends *one half* of a click (either down or up). We use this when we want to track press/release separately.
- **`click_full(button)`** — Sends both halves back-to-back: down then up. That's a complete click. Remember `XFlush` from Step 18? It's here again, pushing the request out so it happens now.

Why two functions? Because some actions want to *hold* a button (press and release at different times), while others just need a quick tap (`click_full`). Having both lets xmouseless express either behavior cleanly. And recall from Step 13: your `config.h` maps keys like `8 → button 1`, so pressing `8` calls this with button `1` (left click).

> **Where are we?** We've met clicking. The map's right box gains its second action:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            │              ├── move pointer ✓ (this step)
>   │              grabs keys    looks up binding      ├── fake click ✓ (this step): down then up
>   │                                                 ├── scroll
>   │                                                 └── run shell command
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   └── drawing vocabulary: Display / Window / GC  ✓ (this step)
> ```

---

## Step 21: Scrolling — Accumulating Small Amounts into Whole Wheel Turns

Scrolling is a bit trickier, so let's take it slowly. A mouse wheel clicks in *whole* steps, but you might want to scroll at varying speeds. xmouseless solves this by **accumulating** small fractional amounts until they add up to one whole step — like saving coins until you have enough for a candy bar.

```c
void scroll(float x, float y) {
    scrollinfo.x += x;                 /* add today's little amount */
    scrollinfo.y += y;

    while (scrollinfo.y <= -0.51) { scrollinfo.y += 1; click_full(4); }  /* one wheel-up   */
    while (scrollinfo.y >=  0.51) { scrollinfo.y -= 1; click_full(5); }  /* one wheel-down */
}
```

Let's decode:

- **`scrollinfo.x += x`** — Add this tick's small amount to a running total. We don't act yet; we just keep saving up.
- **The `while (... <= -0.51)` loop** — Once the saved-up amount crosses about half a step in one direction, fire *one* whole wheel event and subtract that step from the total. The `while` (not `if`) handles the case where you've saved up more than one step at once.
- **Buttons 4 and 5** — In X11, mouse buttons aren't just left/middle/right. Buttons `4` and `5` are the *wheel* going up and down; `6` and `7` are horizontal scrolling. So "scrolling" is really just faking these extra button events (Step 20's mechanism).

Why accumulate instead of scrolling directly? Because your scroll speed might be, say, 0.3 steps per tick — not a whole step. If we only fired on exact integers, slow speeds would never scroll at all. Accumulating means even tiny amounts eventually add up to real wheel clicks. It's the same "hold = keep going" idea as movement (Step 19), just applied to scrolling.

> **Where are we?** We've met scrolling. The map's right box gains its third action:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            │              ├── move pointer ✓ (this step)
>   │              grabs keys    looks up binding      ├── fake click ✓ (this step)
>   │                                                 ├── scroll ✓ (this step): save up to whole wheel steps
>   │                                                 └── run shell command
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   └── drawing vocabulary: Display / Window / GC  ✓ (this step)
> ```

---

## Step 22: Speed Control — Slowing Down for Precision

Remember from Step 13 that holding Enter lowers your speed? Let's see how. It's a simple variable swap in `handle_key`:

```c
for (i = 0; i < LENGTH(speed_bindings); i++) {
    if (speed_bindings[i].keysym == keysym) {
        speed = is_press ? speed_bindings[i].speed : default_speed;
    }
}
```

Let's decode:

- **The loop** — Walks the `speed_bindings` table (Step 13), comparing each row's key to ours. Same lookup pattern as Step 17, repeated here because we'll use this "find my key in a table" idea many times — and that repetition is exactly how it becomes second nature.
- **`is_press ? speed_bindings[i].speed : default_speed`** — The ternary again (Step 17): if you're *pressing* the speed key, switch to the slower value from the table; when you *release*, snap back to `default_speed`. So: hold Enter → creep slowly for fine aiming; let go → return to normal cruising speed.

Why is this useful? Because moving fast gets you close to a target quickly, but landing exactly on a tiny button needs slow, careful motion. One key toggles between "fast travel" and "precise placement." It's like shifting from highway speed to parking-lot crawl.

> **Where are we?** We've met precision control — it reuses the lookup pattern (Step 17) and the `speed` variable (Step 10). The map is unchanged in shape; this just deepens "move pointer":
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            │              ├── move pointer ✓ (this step)
>   │              grabs keys    looks up binding      │     ├─ thread nudges 100×/sec while held ✓ (this step)
>   │                                                 │     └─ hold Enter → slow down for precision ✓ (this step)
>   │                                                 ├── fake click ✓ (this step)
>   │                                                 ├── scroll ✓ (this step)
>   │                                                 └── run shell command
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   └── drawing vocabulary: Display / Window / GC  ✓ (this step)
> ```

---

## Step 23: Shell Commands and the Grid Jump — `fork`, `exec`, and Resolution Independence

Now for one of xmouseless's most clever features: pressing a grid key (like `q`) makes the pointer *jump* to that spot on screen. It does this by running a shell command. Let's meet two new ideas together, because they work as a pair: **running a shell command** and **making it work at any screen size**.

First, here is how xmouseless runs a command (from `handle_key`):

```c
pid_t pid = fork();                          /* split into parent + child */
if (pid == 0) {                              /* we are the CHILD now */
    execl("/bin/sh", "sh", "-c", shell_bindings[i].command, (char *)NULL);
    _exit(127);                              /* only reached if exec failed */
} else if (pid < 0) {                        /* fork itself failed */
    perror("xmouseless: fork");
}
```

Let's decode the pair of ideas:

- **`fork()`** — Splits your program into two copies running at once: the *parent* (keeps doing xmouseless's job) and a *child* (will run the command). `fork` returns different values to each copy, so they can tell who they are. Here, if `pid == 0`, we're the child.
- **`execl("/bin/sh", "sh", "-c", command, ...)`** — The child replaces itself with a shell (`/bin/sh`) that runs our command string. `-c` means "run this text as a command." After `exec`, the child *is* the command now.
- **Why fork+exec?** Because xmouseless is one program; to run an external tool like `xdotool` without stopping itself, it spawns a short-lived child to do that job and returns to its own work.

Now the clever part — making the jump work at *any* screen resolution. Here's the `GRID` macro from `config.h`:

```c
#define GRID(col, row) \
  "W=$(xdotool getdisplaygeometry | cut -d' ' -f1); " \
  "H=$(xdotool getdisplaygeometry | cut -d' ' -f2); " \
  "xdotool mousemove $((W*(2*" #col "+1)/18)) $((H*(2*" #row "+1)/6))"
```

Let's decode it gently:

- **`#define GRID(col, row)`** — A *macro*: a named recipe the preprocessor substitutes everywhere `GRID(...)` appears. It takes two numbers — which column and which row of the grid.
- **`xdotool getdisplaygeometry`** — Asks xdotool "how big is my screen?" and returns width and height. So instead of hardcoding "1600×900," we *measure* the real size at runtime. That's what makes it resolution-independent.
- **`$((W*(2*col+1)/18))`** — Shell math that computes the center x-coordinate of column `col`. With 9 columns, dividing by 18 and using `(2*col+1)` lands you in the middle of each column. Similarly `/6` for the 3 rows.
- **`xdotool mousemove X Y`** — Finally moves the pointer to that computed center point.

So pressing `q` (column 0, row 0) measures your screen, computes the center of the top-left cell, and jumps there — no matter whether you're on a laptop or a huge monitor. That's the whole trick: **measure first, then compute the spot.**

> **Where are we?** We've met the last effect plus resolution independence. The map's right box is now complete:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            │              ├── move pointer ✓ (this step)
>   │              grabs keys    looks up binding      ├── fake click ✓ (this step)
>   │                                                 ├── scroll ✓ (this step)
>   │                                                 └── run shell command ✓ (this step): fork+exec, GRID macro measures screen size
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   └── drawing vocabulary: Display / Window / GC  ✓ (this step)
> ```

---

## Step 24: The Grid Overlay — A See-Through Layer of Labeled Boxes

Now let's build something you can *see*: the fullscreen grid overlay. When xmouseless starts, it draws a transparent layer across your whole screen with nine-by-three boxes, each labeled with its key (`q w f p g j l u y` / `a r s t d h n e i` / `z x c v b k m , .`). Pressing any label jumps the pointer there (Step 23).

Here is the heart of creating it (from `grid_overlay_create()`):

```c
XSetWindowAttributes wa;
wa.override_redirect = True;          /* don't let the window manager move us */
wa.background_pixel = GRID_FG;        /* a real background color */
wa.event_mask = ExposureMask;         /* tell me when I need redrawing */

grid_win = XCreateWindow(dpy, root, 0, 0, sw, sh, 0, ...);   /* fullscreen window */

/* visible shape = only the box outlines and labels; everything else is see-through */
Region vis = XCreateRegion();
for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 9; col++) {
        int cx = sw * (2 * col + 1) / 18;   /* center of this cell — same math as GRID() */
        int cy = sh * (2 * row + 1) / 6;
        region_add_outline(vis, cx - GRID_BOX_W/2, cy - GRID_BOX_H/2, GRID_BOX_W, GRID_BOX_H);
    }
}
XShapeCombineRegion(dpy, grid_win, ShapeBounding, 0, 0, vis, ShapeSet);

/* empty input region: pointer events pass through */
XShapeCombineRectangles(dpy, grid_win, ShapeInput, 0, 0, NULL, 0, ShapeSet, 0);
```

Let's decode the new pieces:

- **`XCreateWindow(...)`** — Creates a window covering the whole screen (`sw` × `sh`, starting at `0,0`). This is our overlay canvas. Remember windows from Step 14? This is one we draw on.
- **`override_redirect = True`** — Tells the window manager "don't manage this window" (no title bar, no moving it around). We want a raw layer pinned exactly where we put it.
- **The nested `for` loops (`row`, then `col`)** — Visit all 3×9 = 27 cells and add each box's outline to a *region*. A **region** is just "a set of rectangles." We're building up the shape of everything that should be visible.
- **`cx = sw * (2*col+1)/18`** — The exact same center-of-cell math as the `GRID` macro in Step 23! This keeps the drawn boxes and the jump targets perfectly aligned. Seeing it twice is deliberate reinforcement: one place draws the box, the other jumps to it, and they must agree.
- **`XShapeCombineRegion(... ShapeBounding ...)`** — Sets which parts of the window are *visible*. Only the outlines and labels show; everything else is transparent (see-through). This uses the shape extension from `<X11/extensions/shape.h>` (Step 8).
- **`XShapeCombineRectangles(... ShapeInput ..., NULL, 0 ...)`** — Sets which parts accept mouse input. Passing `NULL, 0` means *none* — so clicks pass straight through the overlay to whatever is behind it. That's why you can still click things even with the grid drawn on top.

Why do we need a shape at all? Because a normal window would be an opaque rectangle blocking your view. By shaping it down to just thin outlines and labels, xmouseless shows you the grid *without* hiding your desktop. It's like drawing only the frame of a picture, leaving the middle clear.

> **Where are we?** We've met one visual layer. The map gains overlays:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            │              ├── move pointer ✓ (this step)
>   │              grabs keys    looks up binding      ├── fake click ✓ (this step)
>   │                                                 ├── scroll ✓ (this step)
>   │                                                 └── run shell command ✓ (this step)
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   ├── drawing vocabulary: Display / Window / GC  ✓ (this step)
>   │
>   └── on-screen overlays ✓ (this step):
>        grid overlay — fullscreen, see-through, 9×3 labeled boxes; clicks pass through
> ```

---

## Step 25: The Help Box — A Window That Shows Text and Output

The second visual layer is the **help box**: a small window that lists your key bindings *and* shows the output of any command you run. Let's meet how it's created (from `info_window_create()`):

```c
static const char *const fonts[] = {
    "-misc-fixed-medium-r-normal--44-*-*-*-*-*-iso10646-1",
    "fixed",                                    /* exists on practically every X server */
    NULL
};

info_font = load_first_font(fonts);             /* try each font until one loads */

XSetWindowAttributes wa;
wa.override_redirect = True;
wa.background_pixel = INFO_BG;                  /* the box's background color */
wa.border_pixel     = INFO_BORDER;              /* its border color */

info_win = XCreateWindow(dpy, root, info_x, info_y, info_w, info_h, 3, ...);
XStoreName(dpy, info_win, "xmouseless-info");   /* give it a name */

info_gc = XCreateGC(dpy, info_win, 0, NULL);    /* make a brush for this window */
if (info_font) XSetFont(dpy, info_gc, info_font->fid);  /* choose the font on the brush */
```

Let's decode:

- **`fonts[] = { ... }`** — An array (Step 11) of font names, tried in order. `load_first_font` walks down this list and returns the first one that actually exists on your system. This is a *fallback* strategy: try the big fancy font; if it's missing, fall back to something simpler; finally `"fixed"`, which almost every X server has.
- **`NULL` at the end** — A special marker meaning "the list ends here." C arrays don't know their own length by themselves, so we put `NULL` as a sentinel (a stop sign) and loop until we hit it.
- **`XCreateWindow(...)`** — Creates our help-box window at position `(info_x, info_y)` with size `(info_w, info_h)`. Same idea as the grid overlay (Step 24), but smaller and opaque (it has a real background color).
- **`XStoreName(...)`** — Gives the window an identity label so tools can recognize it.
- **`XCreateGC` + `XSetFont`** — Recall the graphics context from Step 14: we make a brush for this window and set its font, so later when we draw text it uses our chosen typeface.

Why does xmouseless show a help box at all? Because once your keyboard is grabbed (Step 15), you can't easily look up "what key do I press?" So xmouseless draws the answer right on screen — a cheat sheet that stays visible while you work. It's like taping the instructions to the machine itself.

> **Where are we?** We've met the second visual layer. The map's overlays branch grows:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            │              ├── move pointer ✓ (this step)
>   │              grabs keys    looks up binding      ├── fake click ✓ (this step)
>   │                                                 ├── scroll ✓ (this step)
>   │                                                 └── run shell command ✓ (this step)
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   ├── drawing vocabulary: Display / Window / GC  ✓ (this step)
>   │
>   └── on-screen overlays ✓ (this step):
>        grid overlay — fullscreen, see-through, 9×3 labeled boxes; clicks pass through
>        help box     — small opaque window showing bindings + command output ✓ (this step)
> ```

---

## Step 26: Capturing Output — A Pipe That Feeds the Help Box

Here's a delightful trick. When you run a shell command (Step 23), its printed text normally goes to your terminal. But xmouseless wants that text shown *in the help box* instead. To do that, it **captures** the program's output using a mechanism called a **pipe**.

Let's meet the idea with an analogy first: a pipe is like a garden hose connecting two things — whatever flows out of one end flows into the other. In computing, a pipe connects a program's *output* to somewhere else (here, our help box).

Here is how xmouseless sets it up (from `init_capture()`):

```c
if (pipe(capture_pipe)) { capture_pipe[0] = capture_pipe[1] = -1; return; }

saved_stdout = dup(STDOUT_FILENO);   /* keep a copy of the real terminal */
dup2(capture_pipe[1], STDOUT_FILENO);/* now "stdout" points into our pipe  */
dup2(capture_pipe[1], STDERR_FILENO);/* and so does "stderr"             */
```

Let's decode:

- **`pipe(capture_pipe)`** — Creates a pipe with two ends, stored in `capture_pipe`. One end is for *writing* (the program's output goes here), the other for *reading* (we read it out and show it). If creation fails (`if (pipe(...))` true means failure), we set both to `-1` ("none") and continue without capture.
- **`dup(STDOUT_FILENO)`** — Makes a spare copy of the real terminal connection, so we can still send errors and the final exit message there later. `STDOUT_FILENO` is just the standard name for "the default place printed text goes."
- **`dup2(capture_pipe[1], STDOUT_FILENO)`** — This is the key move: it *redirects* where "stdout" points, so now anything the program prints flows into our pipe instead of the terminal. It's like rerouting a hose from the drain to a bucket you can inspect.

Then, in the main loop (Step 16), xmouseless periodically reads whatever accumulated in the pipe and redraws the help box with it. So: **command runs → its text flows into the pipe → xmouseless reads the pipe → help box shows it.** The whole chain is just "capture output, then display it."

Why bother? Because seeing a command's result right next to your bindings means you never have to hunt for another terminal window — everything about xmouseless stays in one place on screen.

> **Where are we?** We've met capturing output. The map gains the capture chain under the help box:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            │              ├── move pointer ✓ (this step)
>   │              grabs keys    looks up binding      ├── fake click ✓ (this step)
>   │                                                 ├── scroll ✓ (this step)
>   │                                                 └── run shell command ✓ (this step)
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   ├── drawing vocabulary: Display / Window / GC  ✓ (this step)
>   │
>   └── on-screen overlays ✓ (this step):
>        grid overlay — fullscreen, see-through, 9×3 labeled boxes; clicks pass through
>        help box     — small opaque window showing bindings + command output ✓ (this step)
>                          ▲ fed by a pipe: command → pipe → read → redraw ✓ (this step)
> ```

---

## Step 27: Staying on Top and Avoiding the Cursor

Two final polish behaviors keep xmouseless pleasant to use. Let's meet them together since they're small and related.

**1. Staying on top.** Other windows might open *above* your overlays and hide them. xmouseless watches for that and re-raises its windows back to the front:

```c
static int raise_overlays(void) {
    static long last = 0;
    long t = now_ms();
    if (t - last < RAISE_MIN_INTERVAL_MS) return 0;   /* don't raise too often */
    last = t;
    if (grid_win != None) XRaiseWindow(dpy, grid_win);   /* bring the grid forward */
    if (info_win != None) XRaiseWindow(dpy, info_win);   /* then the help box on top */
    XFlush(dpy);
    return 1;
}
```

- **`XRaiseWindow(...)`** — Tells X11 to move a window up in the stacking order so it's drawn above others. We raise the grid first, then the help box, so the help box ends up on top of everything.
- **The `if (t - last < RAISE_MIN_INTERVAL_MS)` check** — A *rate limit*: don't re-raise more than about 20 times per second. Raising too often would waste effort and could fight with your window manager. This is a gentle "wait a moment before doing it again" guard.

**2. Avoiding the cursor.** If you move the pointer right over the help box, it would hide what's underneath. So xmouseless nudges the box aside when the cursor gets close:

```c
void info_window_avoid_cursor(void) {
    /* ... find where the cursor is (cx, cy) ... */
    int margin = 80;   /* "close" means within this many pixels */
    if (!(cx >= info_x - margin && cx <= info_x + info_w + margin &&
          cy >= info_y - margin && cy <= info_y + info_h + margin)) return;

    info_window_set_alt();   /* move the box to its other spot */
}
```

- **The big `if (!(...))`** — Checks whether the cursor is *not* near the box. If it's far away, do nothing (`return`). Only when the cursor is close does it act.
- **`info_window_set_alt()`** — Moves the help box to its alternative position (the other side of the screen), so it gets out of your way.

Why these two? Because a tool that hides itself or blocks your view would be frustrating. Staying on top keeps it *visible*; avoiding the cursor keeps it *out of the way*. Together they make the overlays helpful rather than obstructive.

> **Where are we?** We've met the polish behaviors. The map's overlay branch is now complete:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            │              ├── move pointer ✓ (this step)
>   │              grabs keys    looks up binding      ├── fake click ✓ (this step)
>   │                                                 ├── scroll ✓ (this step)
>   │                                                 └── run shell command ✓ (this step)
>   │
>   ├── config.h — the editable binding tables ✓ (this step)
>   │
>   ├── drawing vocabulary: Display / Window / GC  ✓ (this step)
>   │
>   └── on-screen overlays ✓ (this step):
>        grid overlay — fullscreen, see-through, 9×3 labeled boxes; clicks pass through
>        help box     — small opaque window showing bindings + command output ✓ (this step)
>                          ▲ fed by a pipe: command → pipe → read → redraw ✓ (this step)
>                          │ stays on top (rate-limited raise) ✓ (this step)
>                          └ avoids the cursor when it gets close ✓ (this step)
> ```

---

## Step 28: Exiting Cleanly — `close_x()` and the Exit Key

Every program needs a graceful way to stop. xmouseless exits when you press an exit key (by default, **Escape**) *on release*. Here is how it tears everything down neatly (from `close_x()`):

```c
void close_x(int exit_status) {
    running = 0;                                  /* tell the move thread to stop */
    if (movethread_started) pthread_join(movethread, NULL);   /* wait for it to finish */

    fflush(stdout);                               /* flush any pending text */
    fflush(stderr);
    if (saved_stdout >= 0) dup2(saved_stdout, STDOUT_FILENO); /* restore the real terminal */
    if (saved_stderr >= 0) dup2(saved_stderr, STDERR_FILENO);

    grid_overlay_destroy();                       /* remove the grid overlay */
    info_window_destroy();                        /* remove the help box */
    XAutoRepeatOn(dpy);                           /* turn key repeat back on */
    XUngrabKeyboard(dpy, CurrentTime);            /* give the keyboard back */
    XCloseDisplay(dpy);                           /* close the X11 connection */
    exit(exit_status);                            /* actually quit */
}
```

Let's decode it gently — notice how each line *undoes* something we set up earlier:

- **`running = 0; pthread_join(...)`** — Recall the move thread (Step 19) loops while `running` is true. Setting it to `0` tells that thread "stop," and `pthread_join` waits until it's truly finished before we tear anything down. Never quit while a worker is still moving your pointer!
- **`dup2(saved_stdout, STDOUT_FILENO)`** — Remember the capture pipe (Step 26) rerouted stdout? Now we point it back to the saved real terminal, so any final messages reach you normally.
- **`grid_overlay_destroy()` / `info_window_destroy()`** — Remove both overlays from screen (the reverse of Steps 24 and 25).
- **`XAutoRepeatOn(dpy)`** — Turn key auto-repeat back on (we turned it off in Step 15). Your keyboard behaves normally again.
- **`XUngrabKeyboard(...)`** — Release the keyboard grab (Step 15), so keys go back to your normal apps. This is the crucial "give control back" step.
- **`XCloseDisplay(dpy)`** — Close the X11 connection entirely (Step 14).

Why does order matter here? Because quitting in the wrong order could leave a thread moving your pointer, or leave your keyboard grabbed with no way to type. `close_x` carefully reverses each setup step: stop workers → restore output → remove windows → re-enable repeat → release keyboard → close display. It's like shutting down a kitchen: turn off burners before you clean the counters.

And recall from Step 13, your exit key lives in `config.h`:

```c
static KeySym exit_keys[] = { XK_Escape };   /* press Escape (on release) to quit */
```

So the whole exit path is: **press Escape → event loop catches it (Step 16) → `handle_key` finds it in `exit_keys` (Step 17) → calls `close_x` → everything unwinds gracefully.**

> **Where are we?** We've met graceful shutdown. The map gains the exit path:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            │              ├── move pointer ✓ (this step)
>   │              grabs keys    looks up binding      ├── fake click ✓ (this step)
>   │                                                 ├── scroll ✓ (this step)
>   │                                                 └── run shell command ✓ (this step)
>   │
>   ├── config.h — the editable binding tables ✓ (this step)  [exit_keys[] = Escape]
>   │
>   ├── drawing vocabulary: Display / Window / GC  ✓ (this step)
>   │
>   ├── on-screen overlays ✓ (this step): grid + help box (+ pipe, stay-on-top, avoid cursor)
>   │
>   └── exit path ✓ (this step): Escape → close_x() → stop thread, restore output,
>        remove windows, re-enable repeat, release keyboard, close display
> ```

---

## Step 29: Putting It All Together — The Real `main()` and a First Run

We've met every piece. Now let's see how they assemble in the real `main()`, because seeing the whole thing once will make it click into place. Here is the actual startup sequence from `xmouseless.c`:

```c
int main(void) {
    XInitThreads();                 /* must be the first Xlib call in the program */
    signal(SIGCHLD, SIG_IGN);       /* auto-reap finished shell commands (no zombies) */

    init_x();                       /* grab the keyboard first: failures still show in terminal */
    init_capture();                 /* from here on stdout/stderr go into the help box */
    grid_overlay_create();          /* grid first ...                                    */
    info_window_create();           /* ... help box last, so it starts above the grid     */

    get_pointer();                  /* learn where the pointer currently is */
    speed = default_speed;          /* start at normal cruising speed */

    rc = pthread_create(&movethread, NULL, &move_forever, NULL);  /* start the move thread */
    movethread_started = 1;

    info_window_redraw();           /* paint the help box once */

    while (1) {                     /* the event loop from Step 16 — runs forever until exit */
        /* ... wait for events, handle keys, read the pipe, redraw, stay on top ... */
    }
}
```

Let's decode the order, because *why this sequence* is the lesson:

- **`XInitThreads()` first** — This must be the very first X11 call. It tells X11 "this program will use threads," so it can make its drawing safe for multiple workers (Step 19). If you forget it, things can misbehave when two parts touch X11 at once.
- **`init_x()` before `init_capture()`** — Grab the keyboard *first*, because if that fails we still want the error message to reach your real terminal. Once capture starts (Step 26), output goes into the help box instead — so any setup errors must happen *before* capture begins.
- **`grid_overlay_create()` then `info_window_create()`** — Create the grid first, the help box second. Since later-created windows stack on top, this makes the help box start above the grid (Step 27's "stay on top" keeps it that way).
- **`pthread_create(...)`** — Launches the move thread (Step 19) so pointer movement can run in parallel with the event loop.
- **The `while(1)` loop** — The heartbeat from Step 16, now running forever until you press Escape and `close_x` unwinds everything (Step 28).

Now let's actually build and run the real program, so you see it all working together:

```sh
cd ~/xmouseless        # or wherever your xmouseless.c and config.h live
./buildonly.sh         # runs gcc with all the right libraries (Step 6)
./xmouseless           # start it — the grid + help box appear, keyboard is grabbed
# ... now try: arrow keys move · hold Enter to slow · 8/9/0 click · PgUp/PgDn scroll
# press Escape to exit cleanly and give your keyboard back
```

When you run `./xmouseless`, you should see the fullscreen grid of labeled boxes and a help box listing your bindings. Press an arrow key and watch the pointer glide; hold Enter and feel it slow down for precision; press `8` and something clicks. Then press **Escape** and everything disappears, your keyboard returns to normal, and you're back at your terminal — exactly as Step 28 described.

> **Where are we?** We've assembled the whole program and run it. The map is now complete:
>
> ```
> xmouseless ── a program that turns your keyboard into a mouse (on X11)
>   │
>   ├── how it works:  KEYBOARD ─► handle_key() ─► EFFECTS
>   │                    ▲            │              ├── move pointer ✓ (this step)
>   │              grabs keys    looks up binding      ├── fake click ✓ (this step)
>   │                                                 ├── scroll ✓ (this step)
>   │                                                 └── run shell command ✓ (this step)
>   │
>   ├── config.h — the editable binding tables ✓ (this step)  [exit_keys[] = Escape]
>   │
>   ├── drawing vocabulary: Display / Window / GC  ✓ (this step)
>   │
>   ├── on-screen overlays ✓ (this step): grid + help box (+ pipe, stay-on-top, avoid cursor)
>   │
>   └── exit path ✓ (this step): Escape → close_x() → graceful unwind
>        ▲
>        main() assembles it all: XInitThreads → init_x → capture → overlays → thread → loop ✓ (this step)
> ```

---

## Quick Reference

A single set of tables to keep beside your terminal. Every entry was verified against the actual `xmouseless.c`, `config.h`, and build script on this machine.

### The default key bindings (from `config.h`)

| Key(s)            | Action                                              |
| ---               | ---                                                 |
| Arrow keys        | Move the pointer while held                         |
| Hold **Enter**    | Lower speed for precision; release to restore       |
| **8 / 9 / 0**     | Left / middle / right click                         |
| **PgUp / PgDn**   | Scroll (this project maps them to wheel buttons)    |
| Grid keys `q…y` / `a…i` / `z….` | Jump pointer to that cell's center        |
| **Escape**        | Exit cleanly (on key release)                       |

### The C building blocks we met

| Concept          | Meaning                                              | Example                          |
| ---              | ---                                                  | ---                              |
| `int main(void)` | The front door every C program starts at             | our first shell                  |
| `/* */`, `//`    | Comments — notes the compiler ignores                | `/* grab keyboard */`            |
| `#include <h>`   | Borrow a standard library's functions                | `#include <stdio.h>`             |
| `#include "f"`   | Borrow a file sitting next to your source            | `#include "config.h"`            |
| variable         | A named box holding one value                        | `int speed = 500;`               |
| array            | A row of same-type boxes, indexed from `0`           | `int steps[3] = {1,-1,2};`       |
| struct + typedef | Group related fields into one named type             | `MoveBinding b = {...};`         |
| function         | A reusable bundle of instructions                    | `void handle_key(...)`           |
| thread           | Parallel worker running alongside the main loop      | `move_forever()`                 |
| pipe + dup2      | Redirect a program's output somewhere else           | capture into the help box        |

### The X11 vocabulary we met

| Term              | Meaning                                              | Example                          |
| ---               | ---                                                  | ---                              |
| `Display *dpy`    | Handle to the whole X11 session                      | `XOpenDisplay(NULL)`             |
| `Window root`     | A rectangle on screen you can draw into              | `RootWindow(dpy, screen)`        |
| `GC gc`           | A "brush" with chosen color/font                     | `XCreateGC(...)`                 |
| `XGrabKeyboard`   | Take exclusive control of the keyboard               | in `init_x()`                    |
| `XWarpPointer`    | Jump the pointer to a position                       | in `move_relative()`             |
| `XTestFakeButtonEvent` | Fake a mouse button press/release                | in `click_full()`                |
| `XCreateWindow`   | Create a window to draw on                           | grid + help box                  |
| Shape extension   | Make parts of a window see-through / click-through   | the transparent grid             |

### The core rules (say them once more)

1. Every key press goes through one place — `handle_key()` — which looks up what that key means in a binding table.
2. To change behavior, edit `config.h` and rebuild; you rarely touch the logic.
3. Effects are done by X11 calls (move/click/scroll) or by running shell commands (`fork` + `exec`).
4. Two jobs run at once — the event loop listens while a thread moves the pointer — so hold-a-key feels continuous.
5. On exit, everything unwinds in reverse: stop the thread, restore output, remove windows, release the keyboard, close the display.

---

## Where to Go Next

You now understand xmouseless completely — its flow, its C building blocks, its X11 vocabulary, its overlays, and its graceful shutdown. Here's where the journey can continue:

- **Read the real source cover to cover:** open `xmouseless.c` (about 764 lines) and `config.h` side by side with this tutorial. Every function we discussed is there; now that you know what each does, reading it feels like revisiting old friends rather than decoding a mystery.
- **Customize your bindings in `config.h`:** change which keys do what (Step 13), then rebuild with `./buildonly.sh`. Try remapping movement to `h/j/k/l`, or adding diagonal moves — each is just one more row in a binding table.
- **Tune the feel:** adjust `move_rate` and `default_speed` at the top of `config.h` (Step 19) to change how fast and smooth the pointer glides, and the Enter-to-slow value for precision work (Step 22).
- **Explore the X11 libraries directly:** `man xdotool`, and the Xlib/XTest/shape extension documentation — these are the tools behind every effect we met.
- **Compare with neighbors:** the original README mentions a successor called *mouseless* that works at the Linux device level (even on Wayland). Seeing both side by side makes xmouseless's X11-specific design even clearer.

---

## Troubleshooting

Gentle answers to the questions beginners most often meet:

**"I ran `./xmouseless` but nothing appears."**
First confirm it started without error — run it and watch your terminal for a message like "cannot open display." If you see that, xmouseless couldn't reach an X11 screen (see the Wayland note below). If there's no error but no grid either, check whether your window manager is hiding override-redirect windows; some setups need a compositor running.

**"The program says `grab keyboard failed`."**
Another program already holds exclusive control of the keyboard (a game, another key-grabbing tool, or a stuck process). Close it and try again — xmouseless retries for about 3 seconds before giving up (Step 15), so sometimes just waiting a moment helps.

**"My keys do nothing / my app isn't receiving them."**
That's the keyboard grab working as designed (Step 15): while xmouseless runs, *all* keys go to it. Press **Escape** to release the keyboard and return control to your normal apps (Step 28).

**"The pointer moves in jerks instead of smoothly."**
Smoothness comes from the move thread nudging ~100 times per second (Step 19). If it feels choppy, try raising `move_rate` in `config.h`, or check that your system isn't under heavy load. Also confirm you're *holding* a key — movement only continues while held and stops on release by design.

**"Scrolling doesn't work the first time I press."**
This is a known quirk xmouseless handles with a small workaround in `handle_key` (Step 21): it fires one scroll immediately when you start, so the very first press registers rather than waiting to accumulate. If you still see lag, your scroll speed may be too low to accumulate whole wheel steps quickly — raise the value in `scroll_bindings`.

**"I'm on Wayland and nothing shows."**
xmouseless draws directly on X11 screens (Step 14). On a pure Wayland session it won't display; you'd need an XWayland-compatible setup, or consider the *mouseless* successor mentioned in "Where to Go Next," which works at the device level. This is a platform boundary, not something to fix inside xmouseless itself.

**"I changed `config.h` but nothing changed."**
Remember: `config.h` is read at compile time (Step 13). After editing it you must rebuild with `./buildonly.sh` and then restart the program — the running copy still uses the old settings until rebuilt.

---

Every expert was once a beginner who took things slowly, one step at a time. Like our snail friend exploring the garden — inch by inch, leaf by leaf — you now understand a whole program: where its keys come from, how each one is looked up and turned into an effect, how it draws itself on screen, and how it bows out gracefully when you press Escape. You're doing wonderfully. 🐌🖱️
