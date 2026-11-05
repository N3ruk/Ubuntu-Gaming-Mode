/* Synthetic Wayland compositor for readback tests. No real DRM or outputs. */
#include <stdio.h>
#include <stdlib.h>
#include <wayland-server.h>
#include "gamescope-control-server-protocol.h"
static int report;
static void destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}
static const struct gamescope_control_interface implementation = { .destroy = destroy };
static void bind_control(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    (void)data;
    if (version != 2) { wl_client_post_implementation_error(client, "reader must bind read-only v2"); return; }
    struct wl_resource *resource = wl_resource_create(client, &gamescope_control_interface, version, id);
    wl_resource_set_implementation(resource, &implementation, NULL, NULL);
    gamescope_control_send_feature_support(resource, 0, 0, 0);
    if (report) {
        struct wl_array rates;
        wl_array_init(&rates);
        gamescope_control_send_active_display_info(resource, "DP-7", "Test", "Monitor", 0, &rates);
        wl_array_release(&rates);
    }
}
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    report = atoi(argv[2]);
    struct wl_display *display = wl_display_create();
    if (!display || wl_display_add_socket(display, "gamescope-test") < 0) return 1;
    wl_global_create(display, &gamescope_control_interface, atoi(argv[1]), NULL, bind_control);
    puts("ready"); fflush(stdout);
    wl_display_run(display);
    return 0;
}
