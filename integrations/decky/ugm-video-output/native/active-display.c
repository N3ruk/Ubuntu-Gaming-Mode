/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 N3ruk
 * Read only gamescope_control v2 active_display_info (no control requests).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <wayland-client.h>
#include "gamescope-control-client-protocol.h"

static struct gamescope_control *control;
static char connector[128];
static void feature(void *data, struct gamescope_control *obj,
                    uint32_t feature_id, uint32_t version, uint32_t flags) {
    (void)data; (void)obj; (void)feature_id; (void)version; (void)flags;
}
static void active(void *data, struct gamescope_control *obj, const char *name,
                   const char *make, const char *model, uint32_t flags,
                   struct wl_array *rates) {
    (void)data; (void)obj; (void)make; (void)model; (void)flags; (void)rates;
    snprintf(connector, sizeof connector, "%s", name);
}
static const struct gamescope_control_listener control_listener = {
    .feature_support = feature, .active_display_info = active,
};
static void global(void *data, struct wl_registry *registry, uint32_t name,
                   const char *interface, uint32_t version) {
    (void)data;
    if (!control && version >= 2 && !strcmp(interface, "gamescope_control")) {
        control = wl_registry_bind(registry, name, &gamescope_control_interface, 2);
        gamescope_control_add_listener(control, &control_listener, NULL);
    }
}
static void removed(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data; (void)registry; (void)name;
}
static const struct wl_registry_listener registry_listener = { global, removed };
int main(int argc, char **argv) {
    (void)argv;
    if (argc != 1) return 2; /* Intentionally no write/debug-command interface. */
    alarm(1);
    const char *socket = getenv("GAMESCOPE_WAYLAND_DISPLAY");
    if (!socket || strncmp(socket, "gamescope-", 10)) return 1;
    struct wl_display *display = wl_display_connect(socket);
    if (!display) return 1;
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    int status = 1;
    if (wl_display_roundtrip(display) >= 0 && control &&
        wl_display_roundtrip(display) >= 0 && connector[0]) {
        printf("Connector Name: %s\n", connector);
        status = 0;
    }
    if (control) gamescope_control_destroy(control);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return status;
}
