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
#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

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

#define MAX_PHYSICAL_DEVICES 100
#define MAX_QUEUE_FAMILY_PROPERTIES 100

static const VkApplicationInfo pApplicationInfo = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pNext = NULL,
    .pApplicationName = "VKBootstrap",
    .applicationVersion = 0x00000100,
    .pEngineName = NULL,
    .engineVersion = 0,
    .apiVersion = VK_API_VERSION_1_0,
};
static const char *const ppEnabledInstanceExtensionNames[] = {
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME
};
static const VkInstanceCreateInfo instanceCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .pApplicationInfo = &pApplicationInfo,
    .enabledLayerCount = 0,
    .ppEnabledLayerNames = NULL,
    .enabledExtensionCount = 1,
    .ppEnabledExtensionNames = ppEnabledInstanceExtensionNames,
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

static void
print_device_properties (uint32_t index, VkPhysicalDeviceProperties *properties)
{
    const char *const deviceTypes[] = {
        "other",
        "integrated gpu",
        "discrete gpu",
        "virtual gpu",
        "cpu"
    };
    printf ("Device %d\n"
            "API:            0x%08x\n"
            "driverVersion:  0x%08x\n"
            "vendorID:       0x%08x\n"
            "deviceID:       0x%08x\n"
            "deviceType:     %s\n"
            "deviceName:     %s\n",
            index,
            properties->apiVersion,
            properties->driverVersion,
            properties->vendorID,
            properties->deviceID,
            deviceTypes[properties->deviceType],
            properties->deviceName);
}

static void
print_device_queues (uint32_t queueFamilyCount,
                     VkQueueFamilyProperties *queueFamilies)
{
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        printf ("Queue Family %d\n  Flags:    ", i);
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            printf ("GRAPHICS ");
        }
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            printf ("COMPUTE ");
        }
        if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            printf ("TRANSFER ");
        }
        if (queueFamilies[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
            printf ("SPARSE_BINDING ");
        }
        printf ("\n  queueCount:    %d\n", queueFamilies[i].queueCount);
    }
}

static VkResult
create_device (VkInstance vk, VkDevice *pDevice)
{
    uint32_t enabledDeviceExtensionCount = 1;
    const char *const ppEnabledDeviceExtensionNames[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    uint32_t queueFamilyIndex = 0;
    float queuePriorities[] = {1.0f};
    VkPhysicalDeviceProperties properties;
    VkQueueFamilyProperties queueFamilyProperties[MAX_QUEUE_FAMILY_PROPERTIES];
    uint32_t queuePrioritiesCount = MAX_QUEUE_FAMILY_PROPERTIES;
    VkDeviceQueueCreateInfo pQueueCreateInfos[1] = {{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .queueFamilyIndex = queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = queuePriorities,
        }
    };
    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = pQueueCreateInfos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = enabledDeviceExtensionCount,
        .ppEnabledExtensionNames = ppEnabledDeviceExtensionNames,
        .pEnabledFeatures = NULL,
    };
    uint32_t physicalDeviceCount = MAX_PHYSICAL_DEVICES;
    VkPhysicalDevice physicalDevices[MAX_PHYSICAL_DEVICES];
    VkResult result = vkEnumeratePhysicalDevices (vk, &physicalDeviceCount,
                      physicalDevices);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        goto out;
    }
    for (uint32_t i = 0; i < physicalDeviceCount; i++) {
        vkGetPhysicalDeviceProperties (physicalDevices[i], &properties);
        if (verbose) {
            print_device_properties (i, &properties);
        }
        vkGetPhysicalDeviceQueueFamilyProperties (physicalDevices[i],
                &queuePrioritiesCount, queueFamilyProperties);
        if (verbose) {
            print_device_queues (queuePrioritiesCount, queueFamilyProperties);
        }
    }
    return vkCreateDevice (physicalDevices[0], &deviceCreateInfo, NULL, pDevice);
out:
    return result;
}

static const char *
get_vulkan_error_string (VkResult result)
{
    switch (result) {
        case VK_SUCCESS:
            return "success";
        case VK_NOT_READY:
            return "not ready";
        case VK_TIMEOUT:
            return "timeout";
        case VK_EVENT_SET:
            return "event set";
        case VK_EVENT_RESET:
            return "event reset";
        case VK_INCOMPLETE:
            return "incomplete";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "out of host memory";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "out of device memory";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "initialization failed";
        case VK_ERROR_DEVICE_LOST:
            return "device lost";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "memory map failed";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "layer not present";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "extension not present";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "feature not present";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "incompatible driver";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "too many objects";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "format not supported";
        case VK_ERROR_FRAGMENTED_POOL:
            return "fragmented pool";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "surface lost";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "native window in use";
        case VK_SUBOPTIMAL_KHR:
            return "suboptimal";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "out of date";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "incompatible display";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "validation failed";
        case VK_ERROR_INVALID_SHADER_NV:
            return "invalid shader";
        case VK_ERROR_OUT_OF_POOL_MEMORY_KHR:
            return "out of pool memory";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE_KHX:
            return "invalid external handle";
        case VK_RESULT_RANGE_SIZE:
        case VK_RESULT_MAX_ENUM:
        default:
            break;
    }
    return NULL;
}

int main (int argc, char *const *argv)
{
    int error = EXIT_SUCCESS;
    Display *display = NULL;
    VkInstance vk = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkResult result = VK_SUCCESS;
    XInitThreads();
    parse_args (argc, argv);

    display = XOpenDisplay (NULL);
    if (display == NULL) {
        fprintf (stderr, "%s: can't connect to X server\n", program_name);
        error = EXIT_FAILURE;
        goto out;
    }

    main_window = window_create (display, "Vulkan Window", 640, 480);
    if (main_window == NULL) {
        fprintf (stderr, "%s: can't create game window\n", program_name);
        error = EXIT_FAILURE;
        goto out;
    }
    if (vkCreateInstance (&instanceCreateInfo, NULL, &vk)) {
        fprintf (stderr, "%s: can't load vulkan\n", program_name);
        error = EXIT_FAILURE;
        goto out;
    }
    if ((result = create_device (vk, &device))) {
        fprintf (stderr, "%s: can't create vulkan device: %s\n", program_name,
                 get_vulkan_error_string (result));
        error = EXIT_FAILURE;
        goto out;
    }
    while (window_is_exists (main_window)) {
        window_process_events (main_window);
        /*game_tick();*/
        /* eglSwapBuffers (egl_display, window_surface);*/
    }
out:
    vkDestroyDevice (device, NULL);
    vkDestroyInstance (vk, NULL);
    window_destroy (main_window);
    XCloseDisplay (display);
    return error;
}
