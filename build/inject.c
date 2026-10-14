/* X event injector for IMRUN auto-debug: posts synthetic button events.
   usage: inject x y [b]   (b=1 left) */
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    if (argc < 3) { printf("usage: inject x y\n"); return 1; }
    Display *d = XOpenDisplay(NULL);
    if (!d) { printf("no display\n"); return 1; }
    int x = atoi(argv[1]), y = atoi(argv[2]);
    Window root = DefaultRootWindow(d);
    XEvent ev;
    memset(&ev, 0, sizeof ev);
    ev.xbutton.type = ButtonPress;
    ev.xbutton.display = d;
    ev.xbutton.window = root;
    ev.xbutton.root = root;
    ev.xbutton.x = x; ev.xbutton.y = y;
    ev.xbutton.x_root = x; ev.xbutton.y_root = y;
    ev.xbutton.button = 1;
    ev.xbutton.same_screen = True;
    XEvent ev2 = ev;
    ev2.type = ButtonRelease;
    XSendEvent(d, root, True, ButtonPressMask, &ev);
    XFlush(d);
    usleep(120000);
    XSendEvent(d, root, True, ButtonReleaseMask, &ev2);
    XFlush(d);
    printf("injected %d,%d\n", x, y);
    return 0;
}
