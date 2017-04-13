/**
 * @file main_x11.c
 * This module contains entry point and initialization for X11 variant
 * of application.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/** Window type */
typedef struct game_window_t {
    Display *display; /**< X11 connection for this window */
    Atom wm_delete_window; /**< Atom to receive "window closed" message */
    Window xwindow; /**< Native X11 window */
    int is_closed; /**< true if window is closed */
    int width; /**< Width of window's client area */
    int height; /**< Height of window's client area */
    char padding[4];
} game_window_t;

/** Single application's main window */
static game_window_t *main_window = NULL;

/** The name the program was run with */
static const char *program_name;

/** Flag that indicates to be verbose as possible */
static int verbose = 0;

/** License text to show when application is runned with --version flag */
static const char *version_text =
    PACKAGE_STRING "\n\n"
    "Copyright (C) 2017 Egor Artemov <egor.artemov@gmail.com>\n"
    "This work is free. You can redistribute it and/or modify it under the\n"
    "terms of the Do What The Fuck You Want To Public License, Version 2,\n"
    "as published by Sam Hocevar. See http://www.wtfpl.net for more details.\n";

/* Option flags and variables */
static struct option const long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {"verbose", no_argument, NULL, 'v'},
    {NULL, 0, NULL, 0}
};

/** Print usage information */
static void print_usage (void)
{
    printf ("Usage: %s [OPTION]...\n"
            "Displays Vulkan animation in X11 window\n\n"
            "Options:\n"
            "  -h, --help     display this help and exit\n"
            "  -V, --version  output version information and exit\n"
            "  --verbose      be verbose\n"
            "\nReport bugs to: <" PACKAGE_BUGREPORT ">\n", program_name);
}

/** Process all pending events
 * @param window events of this window should be processed
 */
static void window_process_events (game_window_t *window)
{
    int n_events = XPending (window->display);
    while (n_events > 0) {
        XEvent event;
        XNextEvent (window->display, &event);
        if (event.type == ClientMessage) {
            if ((Atom)event.xclient.data.l[0] == window->wm_delete_window) {
                window->is_closed = 1;
            }
        } else if (event.type == ConfigureNotify) {
            XConfigureEvent xce;
            xce = event.xconfigure;
            if ((xce.width != window->width)
                    || (xce.height != window->height)) {
                window->width = xce.width;
                window->height = xce.height;
                /*game_resize (window->width, window->height);*/
            }
        }
        n_events--;
    }
}

/** Check is window isn't closed
 * @returns non-zero if closed, 0 otherwise
 */
static int window_is_exists (game_window_t *window)
{
    return window->is_closed == 0;
}

/** Destroy window and free all related resources
 * @param window window to destroy
 */
static void window_destroy (game_window_t *window)
{
    if (window != NULL) {
        XDestroyWindow (window->display, window->xwindow);
        free (window);
    }
}

/** Get native window handle of game window
 * @param window target game window object
 */
static Window window_get_native (game_window_t *window)
{
    return window->xwindow;
}

/** Create and display new window
 * @param display The display where window should be created
 * @param caption The caption of window in Host Portable Character Encoding
 * @param width The width of the window's client area
 * @param height The height of the window's client area
 * @param visual_id VisualID of Visual that should be used to create a window
 * @returns new window object, NULL otherwise
 */
static game_window_t *window_create (Display *display, const char *caption,
                                     unsigned int width, unsigned int height)
{
    int screen = DefaultScreen (display);
    Visual *visual = DefaultVisual (display, screen);
    game_window_t *window = NULL;
    window = (game_window_t *)malloc (sizeof (game_window_t));
    if (window != NULL) {
        const unsigned long attributes_mask = CWBorderPixel | CWColormap |
                                              CWEventMask;
        XSetWindowAttributes window_attributes;
        Window root = RootWindow (display, screen);
        window->display = display;
        window->is_closed = 0;
        window->width = 0;
        window->height = 0;
        window_attributes.colormap = XCreateColormap (display, root,
                                     visual, AllocNone);
        window_attributes.background_pixmap = None;
        window_attributes.border_pixel = 0;
        window_attributes.event_mask = StructureNotifyMask;
        window->xwindow = XCreateWindow (display, root, 0, 0, width, height,
                                         0, DefaultDepth (display, screen),
                                         InputOutput, visual, attributes_mask,
                                         &window_attributes);
        XStoreName (display, window->xwindow, caption);
        XMapWindow (display, window->xwindow);
        window->wm_delete_window = XInternAtom (display, "WM_DELETE_WINDOW",
                                                False);
        XSetWMProtocols (display, window->xwindow,
                         &window->wm_delete_window, 1);
    }
    return window;
}

/** Parse command-line arguments
 * @param argc number of arguments passed to main()
 * @param argv array of arguments passed to main()
 */
static void parse_args (int argc, char *const *argv)
{
    int opt;
    program_name = argv[0];
    while ((opt = getopt_long (argc, argv, "hV", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage ();
                exit (EXIT_SUCCESS);
            case 'V':
                printf ("%s\n", version_text);
                exit (EXIT_SUCCESS);
            case 'v':
                verbose = 1;
                break;
            default:
                print_usage ();
                exit (EXIT_FAILURE);
        }
    }
}

int main (int argc, char *const *argv)
{
    Display *display = NULL;

    parse_args (argc, argv);

    display = XOpenDisplay (NULL);
    if (display == NULL) {
        fprintf (stderr, "%s: can't connect to X server\n", program_name);
        return EXIT_FAILURE;
    }

    main_window = window_create (display, "Vulkan Window", 640, 480);
    if (main_window == NULL) {
        fprintf (stderr, "%s: can't create game window\n", program_name);
        XCloseDisplay (display);
        return EXIT_FAILURE;
    }
    while (window_is_exists (main_window)) {
        window_process_events (main_window);
        /*game_tick();*/
        /* eglSwapBuffers (egl_display, window_surface);*/
    }
    window_destroy (main_window);
    XCloseDisplay (display);
    return EXIT_SUCCESS;
}
