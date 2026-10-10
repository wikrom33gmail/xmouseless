Changes made

1. config.h — resolution-independent grid

    Replaced hardcoded 1600x900 coordinates with a GRID(col,row) macro that reads screen size at runtime via xdotool getdisplaygeometry
    Same 9×3 key layout, now works on any resolution

2. Help box (in xmouseless.c)

    Enlarged: INFO_W 640, INFO_H 400
    Font bumped to 44pt with fallbacks

3. Grid overlay — hollow boxes with big key labels

    Fullscreen transparent overlay (background_pixmap = None)
    Hollow double rectangles at each grid cell center, labeled with its key
    Separate 64pt label font with fallbacks
    Click-through via XShapeCombineRectangles empty input region

4. Startup speed — font metrics cached to disk (in xmouseless.c)

    The X server here takes ~12-50 ms per font to return ascent/descent/char-width, and that was ~97% of startup time. On first launch each resolved font's metrics are written to ~/.cache/xmouseless/fonts; later launches skip the slow query entirely (only a free XLoadFont) — warm startup drops from ~60 ms to well under 1 ms for fonts.

Bugs fixed

    Help box hidden by overlay → XRaiseWindow after overlay creation
    Box disappearing / no text / not fleeing cursor → root cause was two threads sharing one X connection unsafely. Fixed with:
        XInitThreads() as first line of main()
        Skip partial Expose events (event.xexpose.count > 0)
        Explicit info_window_redraw() after XMoveWindow in avoid-cursor

