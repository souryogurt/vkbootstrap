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
#include <xcb/xcb.h>
#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.h>

/** Window type */
typedef struct game_window_t {
    xcb_connection_t *connection; /**< xcb connection for this window */
    xcb_atom_t wm_delete_window; /**< Atom to receive "window closed" message */
    xcb_window_t window_id;
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
    .apiVersion = VK_MAKE_VERSION (1, 0, 3),
};
static const char *const ppEnabledInstanceExtensionNames[] = {
    VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME,
};
static const VkInstanceCreateInfo instanceCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .pApplicationInfo = &pApplicationInfo,
    .enabledLayerCount = 0,
    .ppEnabledLayerNames = NULL,
    .enabledExtensionCount = 2,
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
    xcb_generic_event_t *event = NULL;
    xcb_client_message_event_t *client_message = NULL;
    xcb_configure_notify_event_t *configure_event = NULL;
    int size_changed = 0;
    while ((event = xcb_poll_for_event (window->connection))) {
        switch (event->response_type & ~0x80) {
            case XCB_CLIENT_MESSAGE:
                client_message = (xcb_client_message_event_t *)event;
                if (client_message->data.data32[0] == window->wm_delete_window) {
                    window->is_closed = 1;
                }
                break;
            case XCB_CONFIGURE_NOTIFY:
                configure_event = (xcb_configure_notify_event_t *)event;
                size_changed = (configure_event->width != window->width)
                               || (configure_event->height != window->height);
                if (size_changed) {
                    window->width = configure_event->width;
                    window->height = configure_event->height;
                    /*game_resize (window->width, window->height);*/
                }
                break;
            default:
                break;
        }
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
        xcb_destroy_window (window->connection, window->window_id);
        free (window);
    }
}

/** Get native window handle of game window
 * @param window target game window object
 */
static xcb_window_t window_get_native (game_window_t *window)
{
    return window->window_id;
}

/** Create and display new window
 * @param connection The connection to display where window should be created
 * @param caption The caption of window in Host Portable Character Encoding
 * @param width The width of the window's client area
 * @param height The height of the window's client area
 * @returns new window object, NULL otherwise
 */
static game_window_t *window_create (xcb_connection_t *connection,
                                     const char *caption,
                                     uint16_t width, uint16_t height)
{
    const xcb_setup_t *setup = xcb_get_setup (connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator (setup);
    xcb_screen_t *screen = iter.data;
    game_window_t *window = NULL;
    window = (game_window_t *)malloc (sizeof (game_window_t));
    if (window != NULL) {
        const uint32_t mask = XCB_CW_EVENT_MASK;
        const uint32_t values[] = {XCB_EVENT_MASK_STRUCTURE_NOTIFY};
        xcb_intern_atom_cookie_t delete_cookie = xcb_intern_atom (connection,
                0, strlen ("WM_DELETE_WINDOW"), "WM_DELETE_WINDOW");
        xcb_intern_atom_cookie_t protocols_cookie = xcb_intern_atom (connection,
                0, strlen ("WM_PROTOCOLS"), "WM_PROTOCOLS");
        xcb_intern_atom_reply_t *delete_reply = xcb_intern_atom_reply (connection,
                                                delete_cookie, NULL);
        xcb_intern_atom_reply_t *protocols_reply = xcb_intern_atom_reply (connection,
                protocols_cookie, NULL);
        window->connection = connection;
        window->is_closed = 0;
        window->width = 0;
        window->height = 0;
        window->window_id = xcb_generate_id (connection);
        xcb_create_window (connection, XCB_COPY_FROM_PARENT, window->window_id,
                           screen->root, 0, 0, width, height, 0,
                           XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                           mask, values);
        xcb_change_property (connection, XCB_PROP_MODE_REPLACE,
                             window->window_id, XCB_ATOM_WM_NAME,
                             XCB_ATOM_STRING, 8, (uint32_t)strlen (caption), caption);

        window->wm_delete_window = delete_reply->atom;
        xcb_change_property (connection, XCB_PROP_MODE_REPLACE, window->window_id,
                             protocols_reply->atom, 4, 32, 1, &delete_reply->atom);
        xcb_map_window (connection, window->window_id);
        xcb_flush (connection);
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
create_device (VkInstance vk, VkPhysicalDevice *pPhysicalDevice,
               VkDevice *pDevice)
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
    /* TODO: We are just getting first physical device */
    *pPhysicalDevice = physicalDevices[0];
    return vkCreateDevice (*pPhysicalDevice, &deviceCreateInfo, NULL, pDevice);
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

static VkResult
create_surface (xcb_connection_t *connection, xcb_window_t window,
                VkInstance vk,
                VkSurfaceKHR *surface)
{
    VkXcbSurfaceCreateInfoKHR SurfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .connection = connection,
        .window = window,
    };
    return vkCreateXcbSurfaceKHR (vk, &SurfaceCreateInfo, NULL, surface);
}

static VkResult
create_swapchain (VkPhysicalDevice physicalDevice, VkDevice device,
                  VkSurfaceKHR surface, VkSwapchainKHR *swapchain)
{
    VkSurfaceCapabilitiesKHR SurfaceCapabilities = {0};
    VkResult result = VK_SUCCESS;
    uint32_t presentModeCount = 100;
    VkPresentModeKHR presentModes[100];
    const uint32_t QueueFamilyIndeces[] = {0};
    VkSwapchainCreateInfoKHR SwapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .surface = surface,
        .minImageCount = 2,
        .imageFormat = VK_FORMAT_R8G8B8A8_SRGB,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = {.width = 640, .height = 480},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = QueueFamilyIndeces,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = NULL,
    };
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR (physicalDevice, surface,
             &SurfaceCapabilities);
    if (result != VK_SUCCESS) {
        fprintf (stderr, "%s: can't get physical device capabilities: %s\n",
                 program_name,
                 get_vulkan_error_string (result));
        return result;
    }
    if (verbose) {
        printf ("minImageCount: %d\n", SurfaceCapabilities.minImageCount);
        printf ("maxImageCount: %d\n", SurfaceCapabilities.maxImageCount);
        printf ("current extent %dx%d\n",
                SurfaceCapabilities.currentExtent.width,
                SurfaceCapabilities.currentExtent.height);
        printf ("min extent %dx%d\n",
                SurfaceCapabilities.minImageExtent.width,
                SurfaceCapabilities.minImageExtent.height);
        printf ("max extent %dx%d\n",
                SurfaceCapabilities.maxImageExtent.width,
                SurfaceCapabilities.maxImageExtent.height);
        printf ("maxImageArrayLayers: %d\n", SurfaceCapabilities.maxImageArrayLayers);
        printf ("supportedTransforms: ");
        if (SurfaceCapabilities.supportedTransforms &
                VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
            printf ("identity  ");
        }
        if (SurfaceCapabilities.supportedTransforms &
                VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR) {
            printf ("rotate 90  ");
        }
        if (SurfaceCapabilities.supportedTransforms &
                VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR) {
            printf ("rotate 180  ");
        }
        if (SurfaceCapabilities.supportedTransforms &
                VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {
            printf ("rotate 270  ");
        }
        if (SurfaceCapabilities.supportedTransforms &
                VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR) {
            printf ("horizontal mirror  ");
        }
        if (SurfaceCapabilities.supportedTransforms &
                VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR) {
            printf ("horizontal mirror rotate 90 ");
        }
        if (SurfaceCapabilities.supportedTransforms &
                VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR) {
            printf ("rotate horizontal mirror rotate 180 ");
        }
        if (SurfaceCapabilities.supportedTransforms &
                VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR) {
            printf ("rotate horizontal mirror rotate 270 ");
        }
        if (SurfaceCapabilities.supportedTransforms &
                VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR) {
            printf ("inherit ");
        }
        printf ("\n");
        printf ("currentTransform: ");
        if (SurfaceCapabilities.currentTransform &
                VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
            printf ("identity  ");
        }
        if (SurfaceCapabilities.currentTransform &
                VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR) {
            printf ("rotate 90  ");
        }
        if (SurfaceCapabilities.currentTransform &
                VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR) {
            printf ("rotate 180  ");
        }
        if (SurfaceCapabilities.currentTransform &
                VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {
            printf ("rotate 270  ");
        }
        if (SurfaceCapabilities.currentTransform &
                VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR) {
            printf ("horizontal mirror  ");
        }
        if (SurfaceCapabilities.currentTransform &
                VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR) {
            printf ("horizontal mirror rotate 90 ");
        }
        if (SurfaceCapabilities.currentTransform &
                VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR) {
            printf ("rotate horizontal mirror rotate 180 ");
        }
        if (SurfaceCapabilities.currentTransform &
                VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR) {
            printf ("rotate horizontal mirror rotate 270 ");
        }
        if (SurfaceCapabilities.currentTransform &
                VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR) {
            printf ("inherit ");
        }
        printf ("\n");
        /*
            VkCompositeAlphaFlagsKHR         supportedCompositeAlpha;
            VkImageUsageFlags                supportedUsageFlags;
            */
    }
    if (SwapchainCreateInfo.minImageCount < SurfaceCapabilities.minImageCount) {
        SwapchainCreateInfo.minImageCount = SurfaceCapabilities.minImageCount;
    } else if (SwapchainCreateInfo.minImageCount >
               SurfaceCapabilities.maxImageCount) {
        SwapchainCreateInfo.minImageCount = SurfaceCapabilities.maxImageCount;
    }
    //SwapchainCreateInfo.imageExtent = SurfaceCapabilities.currentExtent;

    /*
    if (SurfaceCapabilities.currentExtent.width != -1
            && SurfaceCapabilities.currentExtent.height != -1) {
        SwapchainCreateInfo.imageExtent = SurfaceCapabilities.currentExtent;
    }
    */
    result = vkGetPhysicalDeviceSurfacePresentModesKHR (physicalDevice, surface,
             &presentModeCount, presentModes);
    if (result != VK_SUCCESS) {
        fprintf (stderr, "%s: can't get physical device present modes: %s\n",
                 program_name,
                 get_vulkan_error_string (result));
        return result;
    }
    for (uint32_t i = 0; i < presentModeCount; i++) {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            SwapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
        if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            SwapchainCreateInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }
    return vkCreateSwapchainKHR (device, &SwapchainCreateInfo, NULL, swapchain);
}

int main (int argc, char *const *argv)
{
    int error = EXIT_SUCCESS;
    xcb_connection_t *connection = NULL;
    VkInstance vk = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkResult result = VK_SUCCESS;
    parse_args (argc, argv);

    connection = xcb_connect (NULL, NULL);
    if (connection == NULL) {
        fprintf (stderr, "%s: can't connect to X server\n", program_name);
        error = EXIT_FAILURE;
        goto out;
    }

    main_window = window_create (connection, "Vulkan Window", 640, 480);
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
    if ((result = create_device (vk, &physicalDevice, &device))) {
        fprintf (stderr, "%s: can't create vulkan device: %s\n", program_name,
                 get_vulkan_error_string (result));
        error = EXIT_FAILURE;
        goto out;
    }
    if ((result = create_surface (connection, window_get_native (main_window), vk,
                                  &surface))) {
        fprintf (stderr, "%s: can't create surface: %s\n", program_name,
                 get_vulkan_error_string (result));
        error = EXIT_FAILURE;
        goto out;
    }
    vkGetDeviceQueue (device, 0, 0, &queue);
    if ((result = create_swapchain (physicalDevice, device, surface, &swapchain))) {
        fprintf (stderr, "%s: can't create swapchain: %s\n", program_name,
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
    vkDestroySwapchainKHR (device, swapchain, NULL);
    vkDestroySurfaceKHR (vk, surface, NULL);
    vkDestroyDevice (device, NULL);
    vkDestroyInstance (vk, NULL);
    window_destroy (main_window);
    xcb_disconnect (connection);
    return error;
}
