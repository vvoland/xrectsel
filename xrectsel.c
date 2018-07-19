/* xrectsel.c -- print the geometry of a rectangular screen region.

   Copyright (C) 2011-2014  lolilolicon <lolilolicon@gmail.com>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>

#define die(args...) do {error(args); exit(EXIT_FAILURE); } while(0)

typedef struct Region Region;
struct Region {
  Window root;
  int x; /* offset from left of screen */
  int y; /* offset from top of screen */
  int X; /* offset from right of screen */
  int Y; /* offset from bottom of screen */
  unsigned int w; /* width */
  unsigned int h; /* height */
  unsigned int b; /* border_width */
  unsigned int d; /* depth */
};

static void error(const char *errstr, ...);
static int print_region_attr(const char *fmt, Region region);
static int select_region(Display *dpy, Window root, Region *region);

int main(int argc, const char *argv[])
{
  Display *dpy;
  Window root;
  Region sr; /* selected region */
  const char *fmt; /* format string */

  dpy = XOpenDisplay(NULL);
  if (!dpy) {
    die("failed to open display %s\n", getenv("DISPLAY"));
  }

  root = DefaultRootWindow(dpy);

  fmt = argc > 1 ? argv[1] : "%wx%h+%x+%y\n";

  /* interactively select a rectangular region */
  if (select_region(dpy, root, &sr) != EXIT_SUCCESS) {
    XCloseDisplay(dpy);
    die("failed to select a rectangular region\n");
  }

  print_region_attr(fmt, sr);

  XCloseDisplay(dpy);
  return EXIT_SUCCESS;
}

static void error(const char *errstr, ...)
{
  va_list ap;

  fprintf(stderr, "xrectsel: ");
  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
}

#define ROUND(value, rounding) \
    if (rounding > 0) \
      value = (value / rounding) * rounding

static void round_and_printu(unsigned int value, unsigned int rounding)
{
  ROUND(value, rounding);
  printf("%u", value);
}

static void round_and_printi(int value, unsigned int rounding)
{
  ROUND(value, rounding);
  printf("%i", value);
}

static unsigned int read_uint_until(const char** str_ptr, char stop_character)
{
  unsigned int value = 0;
  const char* s = *str_ptr;
  while((*(++s)) != stop_character) {
    char c = *s;
    if (c == '\0')
      die("No matching %c found", stop_character);
    if (c < '0' || c > '9')
      die("Unexpected character %c\n", c);
    int num = c - '0';
    if(value != 0)
      value *= 10;
    value += num;
  }
  (*str_ptr) = s;
  return value;
}

static int print_region_attr(const char *fmt, Region region)
{
  const char *s;
  unsigned int rounding;

  for (s = fmt; *s; ++s) {
    rounding = 0;
    if (*s == '%') {
      ++s;
      if (*s == '[') {
        rounding = read_uint_until(&s, ']');
        ++s;
      }
      switch (*s) {
        case '%':
          putchar('%');
          break;
        case 'x':
          round_and_printi(region.x, rounding);
          break;
        case 'y':
          round_and_printi(region.y, rounding);
          break;
        case 'X':
          round_and_printi(region.X, rounding);
          break;
        case 'Y':
          round_and_printi(region.Y, rounding);
          break;
        case 'w':
          round_and_printu(region.w, rounding);
          break;
        case 'h':
          round_and_printu(region.h, rounding);
          break;
        case 'b':
          round_and_printu(region.b, rounding);
          break;
        case 'd':
          round_and_printu(region.d, rounding);
          break;
      }
    } else {
      putchar(*s);
    }
  }

  return 0;
}

static int select_region(Display *dpy, Window root, Region *region)
{
  XEvent ev;

  GC sel_gc;
  XGCValues sel_gv;

  int status, done = 0, btn_pressed = 0;
  int x = 0, y = 0;
  unsigned int width = 0, height = 0;
  int start_x = 0, start_y = 0;

  Cursor cursor;
  cursor = XCreateFontCursor(dpy, XC_tcross);

  /* Grab pointer for these events */
  status = XGrabPointer(dpy, root, True,
               PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
               GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);

  if (status != GrabSuccess) {
      error("failed to grab pointer\n");
      return EXIT_FAILURE;
  }

  sel_gv.function = GXinvert;
  sel_gv.subwindow_mode = IncludeInferiors;
  sel_gv.line_width = 1;
  sel_gc = XCreateGC(dpy, root, GCFunction | GCSubwindowMode | GCLineWidth, &sel_gv);

  for (;;) {
    XNextEvent(dpy, &ev);
    switch (ev.type) {
      case ButtonPress:
        btn_pressed = 1;
        x = start_x = ev.xbutton.x_root;
        y = start_y = ev.xbutton.y_root;
        width = height = 0;
        break;
      case MotionNotify:
        /* Draw only if button is pressed */
        if (btn_pressed) {
          /* Re-draw last Rectangle to clear it */
          XDrawRectangle(dpy, root, sel_gc, x, y, width, height);

          x = ev.xbutton.x_root;
          y = ev.xbutton.y_root;

          if (x > start_x) {
            width = x - start_x;
            x = start_x;
          } else {
            width = start_x - x;
          }
          if (y > start_y) {
            height = y - start_y;
            y = start_y;
          } else {
            height = start_y - y;
          }

          /* Draw Rectangle */
          XDrawRectangle(dpy, root, sel_gc, x, y, width, height);
          XFlush(dpy);
        }
        break;
      case ButtonRelease:
        done = 1;
        break;
      default:
        break;
    }
    if (done)
      break;
  }

  /* Re-draw last Rectangle to clear it */
  XDrawRectangle(dpy, root, sel_gc, x, y, width, height);
  XFlush(dpy);

  XUngrabPointer(dpy, CurrentTime);
  XFreeCursor(dpy, cursor);
  XFreeGC(dpy, sel_gc);
  XSync(dpy, 1);

  Region rr; /* root region */
  Region sr; /* selected region */

  if (False == XGetGeometry(dpy, root, &rr.root, &rr.x, &rr.y, &rr.w, &rr.h, &rr.b, &rr.d)) {
    error("failed to get root window geometry\n");
    return EXIT_FAILURE;
  }
  sr.x = x;
  sr.y = y;
  sr.w = width;
  sr.h = height;
  /* calculate right and bottom offset */
  sr.X = rr.w - sr.x - sr.w;
  sr.Y = rr.h - sr.y - sr.h;
  /* those doesn't really make sense but should be set */
  sr.b = rr.b;
  sr.d = rr.d;
  *region = sr;
  return EXIT_SUCCESS;
}
